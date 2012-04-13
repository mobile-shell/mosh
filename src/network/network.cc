/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#if HAVE_CLOCK_GETTIME
 #include <time.h>
#elif HAVE_MACH_ABSOLUTE_TIME
 #include <mach/mach_time.h>
#elif HAVE_GETTIMEOFDAY
 #include <sys/time.h>
#endif

#include "dos_assert.h"
#include "byteorder.h"
#include "network.h"
#include "crypto.h"

using namespace std;
using namespace Network;
using namespace Crypto;

const uint64_t DIRECTION_MASK = uint64_t(1) << 63;
const uint64_t SEQUENCE_MASK = uint64_t(-1) ^ DIRECTION_MASK;

/* Read in packet from coded string */
Packet::Packet( string coded_packet, Session *session )
  : seq( -1 ),
    direction( TO_SERVER ),
    timestamp( -1 ),
    timestamp_reply( -1 ),
    payload()
{
  Message message = session->decrypt( coded_packet );

  direction = (message.nonce.val() & DIRECTION_MASK) ? TO_CLIENT : TO_SERVER;
  seq = message.nonce.val() & SEQUENCE_MASK;

  dos_assert( message.text.size() >= 2 * sizeof( uint16_t ) );

  uint16_t *data = (uint16_t *)message.text.data();
  timestamp = be16toh( data[ 0 ] );
  timestamp_reply = be16toh( data[ 1 ] );

  payload = string( message.text.begin() + 2 * sizeof( uint16_t ), message.text.end() );
}

/* Output coded string from packet */
string Packet::tostring( Session *session )
{
  uint64_t direction_seq = (uint64_t( direction == TO_CLIENT ) << 63) | (seq & SEQUENCE_MASK);

  uint16_t ts_net[ 2 ] = { htobe16( timestamp ), htobe16( timestamp_reply ) };

  string timestamps = string( (char *)ts_net, 2 * sizeof( uint16_t ) );

  return session->encrypt( Message( Nonce( direction_seq ), timestamps + payload ) );
}

Packet Connection::new_packet( string &s_payload )
{
  uint16_t outgoing_timestamp_reply = -1;

  uint64_t now = timestamp();

  if ( now - saved_timestamp_received_at < 1000 ) { /* we have a recent received timestamp */
    /* send "corrected" timestamp advanced by how long we held it */
    outgoing_timestamp_reply = saved_timestamp + (now - saved_timestamp_received_at);
    saved_timestamp = -1;
    saved_timestamp_received_at = 0;
  }

  Packet p( next_seq++, direction, timestamp16(), outgoing_timestamp_reply, s_payload );

  return p;
}

void Connection::setup( void )
{
  /* create socket */
  sock = socket( AF_INET, SOCK_DGRAM, 0 );
  if ( sock < 0 ) {
    throw NetworkException( "socket", errno );
  }

  /* Disable path MTU discovery */
#ifdef HAVE_IP_MTU_DISCOVER
  char flag = IP_PMTUDISC_DONT;
  socklen_t optlen = sizeof( flag );
  if ( setsockopt( sock, IPPROTO_IP, IP_MTU_DISCOVER, &flag, optlen ) < 0 ) {
    throw NetworkException( "setsockopt", errno );
  }
#endif

 /* set diffserv values to AF42 + ECT */
  uint8_t dscp = 0x92;
  if ( setsockopt( sock, IPPROTO_IP, IP_TOS, &dscp, 1) < 0 ) {
    throw NetworkException( "setsockopt on tos setting", errno );
  }
}

Connection::Connection( const char *desired_ip, const char *desired_port ) /* server */
  : sock( -1 ),
    has_remote_addr( false ),
    remote_addr(),
    server( true ),
    MTU( SEND_MTU ),
    key(),
    session( key ),
    direction( TO_CLIENT ),
    next_seq( 0 ),
    saved_timestamp( -1 ),
    saved_timestamp_received_at( 0 ),
    expected_receiver_seq( 0 ),
    RTT_hit( false ),
    SRTT( 1000 ),
    RTTVAR( 500 ),
    have_send_exception( false ),
    send_exception(),
    have_recv_exception( false ),
    recv_exception()
{
  setup();

  /* The mosh wrapper always gives an IP request, in order
     to deal with multihomed servers. The port is optional. */

  /* If an IP request is given, we try to bind to that IP, but we also
     try INADDR_ANY. If a port request is given, we bind only to that port. */

  /* convert port number */
  long int desired_port_no = 0;

  if ( desired_port ) {
    char *end;
    errno = 0;
    desired_port_no = strtol( desired_port, &end, 10 );
    if ( (errno != 0) || (end != desired_port + strlen( desired_port )) ) {
      throw NetworkException( "Invalid port number", errno );
    }
  }

  if ( (desired_port_no < 0) || (desired_port_no > 65535) ) {
    throw NetworkException( "Port number outside valid range [0..65535]", 0 );
  }

  /* convert desired IP */
  uint32_t desired_ip_addr = INADDR_ANY;

  if ( desired_ip ) {
    struct in_addr sin_addr;
    if ( inet_aton( desired_ip, &sin_addr ) == 0 ) {
      throw NetworkException( "Invalid IP address", errno );
    }
    desired_ip_addr = sin_addr.s_addr;
  }

  /* try to bind to desired IP first */
  if ( desired_ip_addr != INADDR_ANY ) {
    try {
      if ( try_bind( sock, desired_ip_addr, desired_port_no ) ) { return; }
    } catch ( const NetworkException& e ) {
      struct in_addr sin_addr;
      sin_addr.s_addr = desired_ip_addr;
      fprintf( stderr, "Error binding to IP %s: %s: %s\n",
	       inet_ntoa( sin_addr ),
	       e.function.c_str(), strerror( e.the_errno ) );
    }
  }

  /* now try any local interface */
  try {
    if ( try_bind( sock, INADDR_ANY, desired_port_no ) ) { return; }
  } catch ( const NetworkException& e ) {
    fprintf( stderr, "Error binding to any interface: %s: %s\n",
	     e.function.c_str(), strerror( e.the_errno ) );
    throw; /* this time it's fatal */
  }

  assert( false );
  throw NetworkException( "Could not bind", errno );
}

bool Connection::try_bind( int socket, uint32_t s_addr, int port )
{
  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_addr.s_addr = s_addr;

  int search_low = PORT_RANGE_LOW, search_high = PORT_RANGE_HIGH;

  if ( port != 0 ) { /* port preference */
    search_low = search_high = port;
  }

  for ( int i = search_low; i <= search_high; i++ ) {
    local_addr.sin_port = htons( i );

    if ( bind( socket, (sockaddr *)&local_addr, sizeof( local_addr ) ) == 0 ) {
      return true;
    } else if ( i == search_high ) { /* last port to search */
      fprintf( stderr, "Failed binding to %s:%d\n",
	       inet_ntoa( local_addr.sin_addr ),
	       ntohs( local_addr.sin_port ) );
      throw NetworkException( "bind", errno );
    }
  }

  assert( false );
  return false;
}

Connection::Connection( const char *key_str, const char *ip, int port ) /* client */
  : sock( -1 ),
    has_remote_addr( false ),
    remote_addr(),
    server( false ),
    MTU( SEND_MTU ),
    key( key_str ),
    session( key ),
    direction( TO_SERVER ),
    next_seq( 0 ),
    saved_timestamp( -1 ),
    saved_timestamp_received_at( 0 ),
    expected_receiver_seq( 0 ),
    RTT_hit( false ),
    SRTT( 1000 ),
    RTTVAR( 500 ),
    have_send_exception( false ),
    send_exception(),
    have_recv_exception( false ),
    recv_exception()
{
  setup();

  /* associate socket with remote host and port */
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons( port );
  if ( !inet_aton( ip, &remote_addr.sin_addr ) ) {
    int saved_errno = errno;
    char buffer[ 2048 ];
    snprintf( buffer, 2048, "Bad IP address (%s)", ip );
    throw NetworkException( buffer, saved_errno );
  }

  has_remote_addr = true;
}

void Connection::send( string s )
{
  assert( has_remote_addr );

  Packet px = new_packet( s );

  string p = px.tostring( &session );

  ssize_t bytes_sent = sendto( sock, p.data(), p.size(), 0,
			       (sockaddr *)&remote_addr, sizeof( remote_addr ) );

  if ( bytes_sent == static_cast<ssize_t>( p.size() ) ) {
    have_send_exception = false;
  } else {
    /* Notify the frontend on sendto() failure, but don't alter control flow.
       sendto() success is not very meaningful because packets can be lost in
       flight anyway. */
    have_send_exception = true;
    send_exception = NetworkException( "sendto", errno );
  }
}

string Connection::recv( void )
{
  struct sockaddr_in packet_remote_addr;

  char buf[ Session::RECEIVE_MTU ];

  socklen_t addrlen = sizeof( packet_remote_addr );

  ssize_t received_len = recvfrom( sock, buf, Session::RECEIVE_MTU, 0, (sockaddr *)&packet_remote_addr, &addrlen );

  if ( received_len < 0 ) {
    throw NetworkException( "recvfrom", errno );
  }

  if ( received_len > Session::RECEIVE_MTU ) {
    char buffer[ 2048 ];
    snprintf( buffer, 2048, "Received oversize datagram (size %d) and limit is %d\n",
	      static_cast<int>( received_len ), Session::RECEIVE_MTU );
    throw NetworkException( buffer, errno );
  }

  Packet p( string( buf, received_len ), &session );

  dos_assert( p.direction == (server ? TO_SERVER : TO_CLIENT) ); /* prevent malicious playback to sender */

  if ( p.seq < expected_receiver_seq ) {
    have_recv_exception = true;
    recv_exception = NetworkException( "Out-of-order or duplicated packet received", 0 );
  }

  if ( p.seq >= expected_receiver_seq ) { /* don't use out-of-order packets for timestamp or targeting */
    expected_receiver_seq = p.seq + 1; /* this is security-sensitive because a replay attack could otherwise
					  screw up the timestamp and targeting */

    if ( p.timestamp != uint16_t(-1) ) {
      saved_timestamp = p.timestamp;
      saved_timestamp_received_at = timestamp();
    }

    if ( p.timestamp_reply != uint16_t(-1) ) {
      uint16_t now = timestamp16();
      double R = timestamp_diff( now, p.timestamp_reply );

      if ( R < 5000 ) { /* ignore large values, e.g. server was Ctrl-Zed */
	if ( !RTT_hit ) { /* first measurement */
	  SRTT = R;
	  RTTVAR = R / 2;
	  RTT_hit = true;
	} else {
	  const double alpha = 1.0 / 8.0;
	  const double beta = 1.0 / 4.0;
	  
	  RTTVAR = (1 - beta) * RTTVAR + ( beta * fabs( SRTT - R ) );
	  SRTT = (1 - alpha) * SRTT + ( alpha * R );
	}
      }
    }

    /* auto-adjust to remote host */
    has_remote_addr = true;

    if ( server ) { /* only client can roam */
      if ( (remote_addr.sin_addr.s_addr != packet_remote_addr.sin_addr.s_addr)
	   || (remote_addr.sin_port != packet_remote_addr.sin_port) ) {
	remote_addr = packet_remote_addr;
	fprintf( stderr, "Server now attached to client at %s:%d\n",
		 inet_ntoa( remote_addr.sin_addr ),
		 ntohs( remote_addr.sin_port ) );
      }
    }

    have_recv_exception = false;
  }

  return p.payload; /* we do return out-of-order or duplicated packets to caller */
}

int Connection::port( void ) const
{
  struct sockaddr_in local_addr;
  socklen_t addrlen = sizeof( local_addr );

  if ( getsockname( sock, (sockaddr *)&local_addr, &addrlen ) < 0 ) {
    throw NetworkException( "getsockname", errno );
  }

  return ntohs( local_addr.sin_port );
}

uint64_t Network::timestamp( void )
{
#if HAVE_CLOCK_GETTIME
  struct timespec tp;

  if ( clock_gettime( CLOCK_MONOTONIC, &tp ) < 0 ) {
    throw NetworkException( "clock_gettime", errno );
  }

  uint64_t millis = tp.tv_nsec / 1000000;
  millis += uint64_t( tp.tv_sec ) * 1000;

  return millis;
#elif HAVE_MACH_ABSOLUTE_TIME
  static mach_timebase_info_data_t s_timebase_info;

  if (s_timebase_info.denom == 0) {
    mach_timebase_info(&s_timebase_info);
  }

  // NB: mach_absolute_time() returns "absolute time units"
  // We need to apply a conversion to get milliseconds.
  return ((mach_absolute_time() * s_timebase_info.numer) / (1000000 * s_timebase_info.denom));
#elif HAVE_GETTIMEOFDAY
  // NOTE: If time steps backwards, timeouts may be confused.
  struct timeval tv;
  if ( gettimeofday(&tv, NULL) ) {
    throw NetworkException( "gettimeofday", errno );
  }

  uint64_t millis = tv.tv_usec / 1000;
  millis += uint64_t( tv.tv_sec ) * 1000;

  return millis;
#else
# error "Don't know how to get a timestamp on this platform"
#endif
}

uint16_t Network::timestamp16( void )
{
  uint16_t ts = timestamp() % 65536;
  if ( ts == uint16_t(-1) ) {
    ts++;
  }
  return ts;
}

uint16_t Network::timestamp_diff( uint16_t tsnew, uint16_t tsold )
{
  int diff = tsnew - tsold;
  if ( diff < 0 ) {
    diff += 65536;
  }
  
  assert( diff >= 0 );
  assert( diff <= 65535 );

  return diff;
}

uint64_t Connection::timeout( void ) const
{
  uint64_t RTO = lrint( ceil( SRTT + 4 * RTTVAR ) );
  if ( RTO < MIN_RTO ) {
    RTO = MIN_RTO;
  } else if ( RTO > MAX_RTO ) {
    RTO = MAX_RTO;
  }
  return RTO;
}

Connection::~Connection()
{
  if ( close( sock ) < 0 ) {
    throw NetworkException( "close", errno );
  }
}
