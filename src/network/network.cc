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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include "config.h"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <map>
#include "winsock2.h"
#include "windows.h"
#include "mswsock.h"

#include <io.h>
#endif

#include "dos_assert.h"
#include "fatal_assert.h"
#include "byteorder.h"
#include "network.h"
#include "crypto.h"

#include "timestamp.h"

#ifndef MSG_DONTWAIT
#ifndef _WIN32
#define MSG_DONTWAIT MSG_NONBLOCK
#else
#define MSG_DONTWAIT 0
#endif
#endif

#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

using namespace Network;
using namespace Crypto;

const uint64_t DIRECTION_MASK = uint64_t(1) << 63;
const uint64_t SEQUENCE_MASK = uint64_t(-1) ^ DIRECTION_MASK;

#ifdef _WIN32
int dup_socket(int fd) {
    WSAPROTOCOL_INFO info = {};

    int rc = WSADuplicateSocket(fd, GetCurrentProcessId(), &info);
    if(rc != 0)
        return -1;

    SOCKET sock = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &info, 0, 0);
    if(sock == INVALID_SOCKET)
        return -1;

    return (int)sock;
}
#endif

/* Read in packet */
Packet::Packet( const Message & message )
  : seq( message.nonce.val() & SEQUENCE_MASK ),
    direction( (message.nonce.val() & DIRECTION_MASK) ? TO_CLIENT : TO_SERVER ),
    timestamp( -1 ),
    timestamp_reply( -1 ),
    payload()
{
  dos_assert( message.text.size() >= 2 * sizeof( uint16_t ) );

  const uint16_t *data = (uint16_t *)message.text.data();
  timestamp = be16toh( data[ 0 ] );
  timestamp_reply = be16toh( data[ 1 ] );

  payload = string( message.text.begin() + 2 * sizeof( uint16_t ), message.text.end() );
}

/* Output from packet */
Message Packet::toMessage( void )
{
  uint64_t direction_seq = (uint64_t( direction == TO_CLIENT ) << 63) | (seq & SEQUENCE_MASK);

  uint16_t ts_net[ 2 ] = { static_cast<uint16_t>( htobe16( timestamp ) ),
                           static_cast<uint16_t>( htobe16( timestamp_reply ) ) };

  string timestamps = string( (char *)ts_net, 2 * sizeof( uint16_t ) );

  return Message( Nonce( direction_seq ), timestamps + payload );
}

Packet Connection::new_packet( const string &s_payload )
{
  uint16_t outgoing_timestamp_reply = -1;

  uint64_t now = timestamp();

  if ( now - saved_timestamp_received_at < 1000 ) { /* we have a recent received timestamp */
    /* send "corrected" timestamp advanced by how long we held it */
    outgoing_timestamp_reply = saved_timestamp + (now - saved_timestamp_received_at);
    saved_timestamp = -1;
    saved_timestamp_received_at = 0;
  }

  Packet p( direction, timestamp16(), outgoing_timestamp_reply, s_payload );

  return p;
}

void Connection::hop_port( void )
{
  assert( !server );

  setup();
  assert( remote_addr_len != 0 );
  socks.push_back( Socket( remote_addr.sa.sa_family ) );

  prune_sockets();
}

void Connection::prune_sockets( void )
{
  /* don't keep old sockets if the new socket has been working for long enough */
  if ( socks.size() > 1 ) {
    if ( timestamp() - last_port_choice > MAX_OLD_SOCKET_AGE ) {
      int num_to_kill = socks.size() - 1;
      for ( int i = 0; i < num_to_kill; i++ ) {
	socks.pop_front();
      }
    }
  } else {
    return;
  }

  /* make sure we don't have too many receive sockets open */
  if ( socks.size() > MAX_PORTS_OPEN ) {
    int num_to_kill = socks.size() - MAX_PORTS_OPEN;
    for ( int i = 0; i < num_to_kill; i++ ) {
      socks.pop_front();
    }
  }
}

Connection::Socket::Socket( int family )
#ifndef _WIN32
  : _fd( socket( family, SOCK_DGRAM, 0 ) )
#else
  : _fd( WSASocket( family, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED) )
#endif
{
  #ifndef _WIN32
  if ( _fd < 0 ) {
    throw NetworkException( "socket", errno );
  }
  #else
  if (_fd == INVALID_SOCKET)  {
    throw WSAException( "WSASocket", WSAGetLastError() );
  }
  #endif

  /* Disable path MTU discovery */
#ifdef HAVE_IP_MTU_DISCOVER
  int flag = IP_PMTUDISC_DONT;
  if ( setsockopt( _fd, IPPROTO_IP, IP_MTU_DISCOVER, &flag, sizeof flag ) < 0 ) {
    throw NetworkException( "setsockopt", errno );
  }
#endif

  #ifdef _WIN32
  u_long mode = 1;
  if(ioctlsocket(_fd, FIONBIO, &mode) == INVALID_SOCKET) {
      throw WSAException("ioctlsocket", WSAGetLastError());
  }
  #endif

  //  int dscp = 0x92; /* OS X does not have IPTOS_DSCP_AF42 constant */
  int dscp = 0x02; /* ECN-capable transport only */
  #ifndef _WIN32
  if ( setsockopt( _fd, IPPROTO_IP, IP_TOS, &dscp, sizeof dscp ) < 0 ) {
    //    perror( "setsockopt( IP_TOS )" );
  }
  #else
  if ( setsockopt( _fd, IPPROTO_IP, IP_TOS, (const char*)&dscp, sizeof dscp ) < 0 ) {
        perror( "setsockopt( IP_TOS )" );
  }
  #endif

  /* request explicit congestion notification on received datagrams */
#ifdef HAVE_IP_RECVTOS
  int tosflag = true;
  if ( setsockopt( _fd, IPPROTO_IP, IP_RECVTOS, &tosflag, sizeof tosflag ) < 0
       && family == IPPROTO_IP ) { /* FreeBSD disallows this option on IPv6 sockets. */
    perror( "setsockopt( IP_RECVTOS )" );
  }
#endif
}

void Connection::setup( void )
{
  last_port_choice = timestamp();
}

const std::vector< int > Connection::fds( void ) const
{
  std::vector< int > ret;

  for ( std::deque< Socket >::const_iterator it = socks.begin();
	it != socks.end();
	it++ ) {
    ret.push_back( it->fd() );
  }

  return ret;
}

void Connection::set_MTU( int family )
{
  switch ( family ) {
  case AF_INET:
    MTU = DEFAULT_IPV4_MTU - IPV4_HEADER_LEN;
    break;
  case AF_INET6:
    MTU = DEFAULT_IPV6_MTU - IPV6_HEADER_LEN;
    break;
  default:
    throw NetworkException( "Unknown address family", 0 );
  }
}

#ifdef _WIN32
/* called before any other calls to socketio_ functions */
int
socketio_initialize()
{
    WSADATA wsaData = { 0 };
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}
#endif

class AddrInfo {
public:
  struct addrinfo *res;
  AddrInfo( const char *node, const char *service,
	    const struct addrinfo *hints ) :
    res( NULL ) {
    #ifndef _WIN32
    int errcode = getaddrinfo( node, service, hints, &res );
    #else
    int errcode = socketio_initialize();
    errcode = getaddrinfo( node, service, hints, &res );
    #endif
    if ( errcode != 0 ) {
      throw NetworkException( std::string( "Bad IP address (" ) + (node != NULL ? node : "(null)") + "): " + gai_strerror( errcode ), 0 );
    }
  }
  ~AddrInfo() { freeaddrinfo(res); }
private:
  AddrInfo(const AddrInfo &);
  AddrInfo &operator=(const AddrInfo &);
};

Connection::Connection( const char *desired_ip, const char *desired_port ) /* server */
  : socks(),
    has_remote_addr( false ),
    remote_addr(),
    remote_addr_len( 0 ),
    server( true ),
    MTU( DEFAULT_SEND_MTU ),
    key(),
    session( key ),
    direction( TO_CLIENT ),
    saved_timestamp( -1 ),
    saved_timestamp_received_at( 0 ),
    expected_receiver_seq( 0 ),
    last_heard( -1 ),
    last_port_choice( -1 ),
    last_roundtrip_success( -1 ),
    RTT_hit( false ),
    SRTT( 1000 ),
    RTTVAR( 500 ),
    send_error()
{
  setup();

  /* The mosh wrapper always gives an IP request, in order
     to deal with multihomed servers. The port is optional. */

  /* If an IP request is given, we try to bind to that IP, but we also
     try INADDR_ANY. If a port request is given, we bind only to that port. */

  /* convert port numbers */
  int desired_port_low = -1;
  int desired_port_high = -1;

  if ( desired_port && !parse_portrange( desired_port, desired_port_low, desired_port_high ) ) {
    throw NetworkException("Invalid port range", 0);
  }

  /* try to bind to desired IP first */
  if ( desired_ip ) {
    try {
      if ( try_bind( desired_ip, desired_port_low, desired_port_high ) ) { return; }
    } catch ( const NetworkException &e ) {
      fprintf( stderr, "Error binding to IP %s: %s\n",
	       desired_ip,
	       e.what() );
    }
  }

  /* now try any local interface */
  try {
    if ( try_bind( NULL, desired_port_low, desired_port_high ) ) { return; }
  } catch ( const NetworkException &e ) {
    fprintf( stderr, "Error binding to any interface: %s\n",
	     e.what() );
    throw; /* this time it's fatal */
  }

  throw NetworkException( "Could not bind", errno );
}

bool Connection::try_bind( const char *addr, int port_low, int port_high )
{
  struct addrinfo hints;
  memset( &hints, 0, sizeof( hints ) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;
  AddrInfo ai( addr, "0", &hints );

  Addr local_addr;
  socklen_t local_addr_len = ai.res->ai_addrlen;
  memcpy( &local_addr.sa, ai.res->ai_addr, local_addr_len );

  int search_low = PORT_RANGE_LOW, search_high = PORT_RANGE_HIGH;

  if ( port_low != -1 ) { /* low port preference */
    search_low = port_low;
  }
  if ( port_high != -1 ) { /* high port preference */
    search_high = port_high;
  }

  socks.push_back( Socket( local_addr.sa.sa_family ) );
  for ( int i = search_low; i <= search_high; i++ ) {
    switch (local_addr.sa.sa_family) {
    case AF_INET:
      local_addr.sin.sin_port = htons( i );
      break;
    case AF_INET6:
      local_addr.sin6.sin6_port = htons( i );
      break;
    default:
      throw NetworkException( "Unknown address family", 0 );
    }

    if ( local_addr.sa.sa_family == AF_INET6
      && memcmp(&local_addr.sin6.sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0 ) {
      #ifndef _WIN32
      const int off = 0;
      if ( setsockopt( sock(), IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off) ) ) {
      #else
      const char* off = "0";
      if ( setsockopt( sock(), IPPROTO_IPV6, IPV6_V6ONLY, off, sizeof(off) ) ) {
      #endif
        perror( "setsockopt( IPV6_V6ONLY, off )" );
      }
    }

    if ( ::bind( sock(), &local_addr.sa, local_addr_len ) == 0 ) {
      set_MTU( local_addr.sa.sa_family );
      return true;
    } // else fallthrough to below code, on last iteration.
  }
  int saved_errno = errno;
  socks.pop_back();
  char host[ NI_MAXHOST ], serv[ NI_MAXSERV ];
  int errcode = getnameinfo( &local_addr.sa, local_addr_len,
			     host, sizeof( host ), serv, sizeof( serv ),
			     NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV );
  if ( errcode != 0 ) {
    throw NetworkException( std::string( "bind: getnameinfo: " ) + gai_strerror( errcode ), 0 );
  }
  fprintf( stderr, "Failed binding to %s:%s\n",
	   host, serv );
  throw NetworkException( "bind", saved_errno );
}

Connection::Connection( const char *key_str, const char *ip, const char *port ) /* client */
  : socks(),
    has_remote_addr( false ),
    remote_addr(),
    remote_addr_len( 0 ),
    server( false ),
    MTU( DEFAULT_SEND_MTU ),
    key( key_str ),
    session( key ),
    direction( TO_SERVER ),
    saved_timestamp( -1 ),
    saved_timestamp_received_at( 0 ),
    expected_receiver_seq( 0 ),
    last_heard( -1 ),
    last_port_choice( -1 ),
    last_roundtrip_success( -1 ),
    RTT_hit( false ),
    SRTT( 1000 ),
    RTTVAR( 500 ),
    send_error()
{
  setup();

  /* associate socket with remote host and port */
  struct addrinfo hints;
  memset( &hints, 0, sizeof( hints ) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  AddrInfo ai( ip, port, &hints );
  fatal_assert( static_cast<size_t>( ai.res->ai_addrlen ) <= sizeof( remote_addr ) );
  remote_addr_len = ai.res->ai_addrlen;
  memcpy( &remote_addr.sa, ai.res->ai_addr, remote_addr_len );

  has_remote_addr = true;

  socks.push_back( Socket( remote_addr.sa.sa_family ) );

  set_MTU( remote_addr.sa.sa_family );
}

void Connection::send( const string & s )
{
  if ( !has_remote_addr ) {
    return;
  }

  Packet px = new_packet( s );

  string p = session.encrypt( px.toMessage() );

  ssize_t bytes_sent = sendto( sock(), p.data(), p.size(), MSG_DONTWAIT,
			       &remote_addr.sa, remote_addr_len );

  if ( bytes_sent != static_cast<ssize_t>( p.size() ) ) {
    /* Make sendto() failure available to the frontend. */
    send_error = "sendto: ";
    send_error += strerror( errno );

    if ( errno == EMSGSIZE ) {
      MTU = DEFAULT_SEND_MTU; /* payload MTU of last resort */
    }
  }

  uint64_t now = timestamp();
  if ( server ) {
    if ( now - last_heard > SERVER_ASSOCIATION_TIMEOUT ) {
      has_remote_addr = false;
      fprintf( stderr, "Server now detached from client.\n" );
    }
  } else { /* client */
    if ( ( now - last_port_choice > PORT_HOP_INTERVAL )
	 && ( now - last_roundtrip_success > PORT_HOP_INTERVAL ) ) {
      hop_port();
    }
  }
}

string Connection::recv( void )
{
  assert( !socks.empty() );
  for ( std::deque< Socket >::const_iterator it = socks.begin();
	it != socks.end();
	it++ ) {
    string payload;
    try {
      payload = recv_one( it->fd());
    } catch ( NetworkException & e ) {
      #ifndef _WIN32
      if ( (e.the_errno == EAGAIN)
	   || (e.the_errno == EWOULDBLOCK) ) {
	continue;
      } else {
	throw;
      }
      #else
      // TODO(MaxRis): Allow certain socket read failures (error codes between Windows and Linux don't match)
      continue;
      #endif
    }

    /* succeeded */
    prune_sockets();
    return payload;
  }
  throw NetworkException( "No packet received" );
}

#ifdef _WIN32
std::map<int, LPFN_WSARECVMSG> recvMsgMap;
#endif

string Connection::recv_one( int sock_to_recv )
{
  /* receive source address, ECN, and payload in msghdr structure */
  Addr packet_remote_addr;
  #ifndef _WIN32
  struct msghdr header;
  struct iovec msg_iovec;
  #else
  GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
  LPFN_WSARECVMSG WSARecvMsg = nullptr;
  WSAMSG msg;
  WSABUF WSABuf;
  DWORD received_len = 0;
  int nResult = 0;

  WSARecvMsg = recvMsgMap[sock_to_recv];
  if (!WSARecvMsg) {
      nResult = WSAIoctl(sock_to_recv, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &WSARecvMsg_GUID, sizeof WSARecvMsg_GUID,
               &WSARecvMsg, sizeof WSARecvMsg,
               &received_len, NULL, NULL);
      if (nResult == SOCKET_ERROR) {
          int errorCode = WSAGetLastError();
          WSARecvMsg = NULL;
          return "";
      }
      recvMsgMap[sock_to_recv] = WSARecvMsg;
  }
  #endif

  char msg_payload[ Session::RECEIVE_MTU ];
  char msg_control[ Session::RECEIVE_MTU ];

  /* receive source address */
  #ifndef _WIN32
  header.msg_name = &packet_remote_addr;
  header.msg_namelen = sizeof packet_remote_addr;
  #else
  msg.name = &packet_remote_addr.sa;
  msg.namelen = sizeof packet_remote_addr.sa;
  #endif

  /* receive payload */
  #ifndef _WIN32
  msg_iovec.iov_base = msg_payload;
  msg_iovec.iov_len = sizeof msg_payload;
  header.msg_iov = &msg_iovec;
  header.msg_iovlen = 1;
  #else
  WSABuf.buf = msg_payload;
  WSABuf.len = sizeof msg_payload;
  msg.lpBuffers = &WSABuf;
  msg.dwBufferCount = 1;
  #endif

  /* receive explicit congestion notification */
  #ifndef _WIN32
  header.msg_control = msg_control;
  header.msg_controllen = sizeof msg_control;
  #else
  msg.Control.len = sizeof msg_control;
  msg.Control.buf = msg_control;
  #endif

  /* receive flags */
  #ifndef _WIN32
  header.msg_flags = 0;

  ssize_t received_len = recvmsg( sock_to_recv, &header, MSG_DONTWAIT );

  if ( received_len < 0 ) {
    throw NetworkException( "recvmsg", errno );
  }

  if ( header.msg_flags & MSG_TRUNC ) {
    throw NetworkException( "Received oversize datagram", errno );
  }
  #else
  msg.dwFlags = 0;

  nResult = WSARecvMsg(sock_to_recv, &msg, &received_len, NULL, NULL);
  if (nResult == SOCKET_ERROR) {
      throw WSAException( "WSARecvMsg", WSAGetLastError() );
  }

  if ( received_len < 0 ) {
    throw WSAException( "recvmsg", WSAGetLastError() );
  }

  if ( msg.dwFlags & MSG_TRUNC ) {
    throw WSAException( "Received oversize datagram", WSAGetLastError() );
  }
  #endif

  /* receive ECN */
  bool congestion_experienced = false;

  #ifndef _WIN32
  struct cmsghdr *ecn_hdr = CMSG_FIRSTHDR( &header );
  if ( ecn_hdr
       && ecn_hdr->cmsg_level == IPPROTO_IP
       && ( ecn_hdr->cmsg_type == IP_TOS
#ifdef IP_RECVTOS
	    || ecn_hdr->cmsg_type == IP_RECVTOS
#endif
	    ) ) {
    /* got one */
    uint8_t *ecn_octet_p = (uint8_t *)CMSG_DATA( ecn_hdr );
    assert( ecn_octet_p );

    congestion_experienced = (*ecn_octet_p & 0x03) == 0x03;
  }
  #else
  WSACMSGHDR *pCMsgHdr = WSA_CMSG_FIRSTHDR(&msg);
  /*IN_PKTINFO *pPktInfo = nullptr;
  if (pCMsgHdr) {
      switch (pCMsgHdr->cmsg_type) {
          case IP_PKTINFO: {

                  //CSocketAddressIn DestinationAddress;
                  pPktInfo = (IN_PKTINFO *)WSA_CMSG_DATA(pCMsgHdr);
                  //DestinationAddress.SetHostAddress(pPktInfo->ipi_addr.S_un.S_addr);
                  //DestinationAddress.GetAddress(Address);
                  //cout << "Destination address: " << Address
                  //    << ", interface index: " << pPktInfo->ipi_ifindex << '\n';
                  }
              break;
          default:
              //cout << "Unknown message type: " << pCMsgHdr->cmsg_type
              //    << "; level: " << pCMsgHdr->cmsg_level << '\n';
              break;
          }
  }

  if (!pPktInfo) {
      return "";
  }*/
  #endif

  Packet p( session.decrypt( msg_payload, received_len ) );

  dos_assert( p.direction == (server ? TO_SERVER : TO_CLIENT) ); /* prevent malicious playback to sender */

  if ( p.seq < expected_receiver_seq ) { /* don't use (but do return) out-of-order packets for timestamp or targeting */
    return p.payload;
  }
  expected_receiver_seq = p.seq + 1; /* this is security-sensitive because a replay attack could otherwise
					screw up the timestamp and targeting */

  if ( p.timestamp != uint16_t(-1) ) {
    saved_timestamp = p.timestamp;
    saved_timestamp_received_at = timestamp();

    if ( congestion_experienced ) {
      /* signal counterparty to slow down */
      /* this will gradually slow the counterparty down to the minimum frame rate */
      saved_timestamp -= CONGESTION_TIMESTAMP_PENALTY;
      if ( server ) {
	fprintf( stderr, "Received explicit congestion notification.\n" );
      }
    }
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
  last_heard = timestamp();

  if ( server && /* only client can roam */
       #ifndef _WIN32
       ( remote_addr_len != header.msg_namelen ||
       #else
       ( remote_addr_len != msg.namelen ||
       #endif
	 memcmp( &remote_addr, &packet_remote_addr, remote_addr_len ) != 0 ) ) {
    remote_addr = packet_remote_addr;
    #ifndef _WIN32
    remote_addr_len = header.msg_namelen;
    #else
    remote_addr_len = msg.namelen;
    #endif
    char host[ NI_MAXHOST ], serv[ NI_MAXSERV ];
    int errcode = getnameinfo( &remote_addr.sa, remote_addr_len,
			       host, sizeof( host ), serv, sizeof( serv ),
			       NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV );
    if ( errcode != 0 ) {
      throw NetworkException( std::string( "recv_one: getnameinfo: " ) + gai_strerror( errcode ), 0 );
    }
    fprintf( stderr, "Server now attached to client at %s:%s\n",
	     host, serv );
  }
  return p.payload;
}

std::string Connection::port( void ) const
{
  Addr local_addr;
  socklen_t addrlen = sizeof( local_addr );

  if ( getsockname( sock(), &local_addr.sa, &addrlen ) < 0 ) {
    throw NetworkException( "getsockname", errno );
  }

  char serv[ NI_MAXSERV ];
  int errcode = getnameinfo( &local_addr.sa, addrlen,
			     NULL, 0, serv, sizeof( serv ),
			     NI_DGRAM | NI_NUMERICSERV );
  if ( errcode != 0 ) {
    throw NetworkException( std::string( "port: getnameinfo: " ) + gai_strerror( errcode ), 0 );
  }

  return std::string( serv );
}

uint64_t Network::timestamp( void )
{
  return frozen_timestamp();
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

Connection::Socket::~Socket()
{
  #ifndef _WIN32
  fatal_assert ( close( _fd ) == 0 );
  #else
  fatal_assert ( closesocket( _fd ) == 0 );
  #endif
}

Connection::Socket::Socket( const Socket & other )
#ifndef _WIN32
  : _fd( dup( other._fd ) )
#else
  : _fd( dup_socket( other._fd ) )
#endif
{
  if ( _fd < 0 ) {
    throw NetworkException( "socket", errno );
  }
}

Connection::Socket & Connection::Socket::operator=( const Socket & other )
{
    //TODO: This will not work on Windows because dup2() and dup() cannot be used for socket handles on that OS.
  if ( dup2( other._fd, _fd ) < 0 ) {
    throw NetworkException( "socket", errno );
  }

  return *this;
}

bool Connection::parse_portrange( const char * desired_port, int & desired_port_low, int & desired_port_high )
{
  /* parse "port" or "portlow:porthigh" */
  desired_port_low = desired_port_high = 0;
  char *end;
  long value;

  /* parse first (only?) port */
  errno = 0;
  value = strtol( desired_port, &end, 10 );
  if ( (errno != 0) || (*end != '\0' && *end != ':') ) {
    fprintf( stderr, "Invalid (low) port number (%s)\n", desired_port );
    return false;
  }
  if ( (value < 0) || (value > 65535) ) {
    fprintf( stderr, "(Low) port number %ld outside valid range [0..65535]\n", value );
    return false;
  }

  desired_port_low = (int)value;
  if (*end == '\0') { /* not a port range */
    desired_port_high = desired_port_low;
    return true;
  }
  /* port range; parse high port */
  const char * cp = end + 1;
  errno = 0;
  value = strtol( cp, &end, 10 );
  if ( (errno != 0) || (*end != '\0') ) {
    fprintf( stderr, "Invalid high port number (%s)\n", cp );
    return false;
  }
  if ( (value < 0) || (value > 65535) ) {
    fprintf( stderr, "High port number %ld outside valid range [0..65535]\n", value );
    return false;
  }

  desired_port_high = (int)value;
  if ( desired_port_low > desired_port_high ) {
    fprintf( stderr, "Low port %d greater than high port %d\n", desired_port_low, desired_port_high );
    return false;
  }

  if ( desired_port_low == 0 ) {
    fprintf( stderr, "Low port 0 incompatible with port ranges\n" );
    return false;
  }


  return true;
}
