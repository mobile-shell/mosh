#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "dos_assert.hpp"
#include "network.hpp"
#include "crypto.hpp"

using namespace std;
using namespace Network;
using namespace Crypto;

/* Read in packet from coded string */
Packet::Packet( string coded_packet, Session *session )
  : seq( -1 ),
    direction( TO_SERVER ),
    payload()
{
  Message message = session->decrypt( coded_packet );

  direction = (message.nonce.val() & 8000000000000000) ? TO_CLIENT : TO_SERVER;
  seq = message.nonce.val() & 0x7FFFFFFFFFFFFFFF;
  payload = message.text;
}

/* Output coded string from packet */
string Packet::tostring( Session *session )
{
  uint64_t direction_seq = (uint64_t( direction == TO_CLIENT ) << 63) | (seq & 0x7FFFFFFFFFFFFFFF);

  return session->encrypt( Message( direction_seq, payload ) );
}

Packet Connection::new_packet( string &s_payload )
{
  return Packet( next_seq++, direction, s_payload );
}

void Connection::setup( void )
{
  /* create socket */
  sock = socket( AF_INET, SOCK_DGRAM, 0 );
  if ( sock < 0 ) {
    throw NetworkException( "socket", errno );
  }

  /* Bind to free local port.
     This usage does not seem to be endorsed by POSIX. */
  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons( 0 );
  local_addr.sin_addr.s_addr = INADDR_ANY;
  if ( bind( sock, (sockaddr *)&local_addr, sizeof( local_addr ) ) < 0 ) {
    throw NetworkException( "bind", errno );
  }

  /* Enable path MTU discovery */
  char flag = IP_PMTUDISC_DO;
  socklen_t optlen = sizeof( flag );
  if ( setsockopt( sock, IPPROTO_IP, IP_MTU_DISCOVER, &flag, optlen ) < 0 ) {
    throw NetworkException( "setsockopt", errno );
  }  
}

Connection::Connection() /* server */
  : sock( -1 ),
    remote_addr(),
    server( true ),
    attached( false ),
    MTU( RECEIVE_MTU ),
    key(),
    session( key ),
    direction( TO_CLIENT ),
    next_seq( 0 )
{
  setup();
}

Connection::Connection( const char *key_str, const char *ip, int port ) /* client */
  : sock( -1 ),
    remote_addr(),
    server( false ),
    attached( false ),
    MTU( RECEIVE_MTU ),
    key( key_str ),
    session( key ),
    direction( TO_SERVER ),
    next_seq( 0 )
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

  if ( connect( sock, (sockaddr *)&remote_addr, sizeof( remote_addr ) ) < 0 ) {
    throw NetworkException( "connect", errno );
  }

  attached = true;
}

void Connection::update_MTU( void )
{
  socklen_t optlen = sizeof( MTU );
  if ( getsockopt( sock, IPPROTO_IP, IP_MTU, &MTU, &optlen ) < 0 ) {
    throw NetworkException( "getsockopt", errno );
  }

  if ( optlen != sizeof( MTU ) ) {
    throw NetworkException( "Error getting path MTU", errno );
  }

  fprintf( stderr, "Path MTU: %d\n", MTU );  
}

void Connection::send( string &s )
{
  assert( attached );

  string p = new_packet( s ).tostring( &session );

  ssize_t bytes_sent = sendto( sock, p.data(), p.size(), 0,
			       (sockaddr *)&remote_addr, sizeof( remote_addr ) );

  if ( (bytes_sent < 0) && (errno == EMSGSIZE) ) {
    update_MTU();
    throw MTUException( MTU );
  } else if ( bytes_sent == static_cast<int>( p.size() ) ) {
    return;
  } else {
    throw NetworkException( "sendto", errno );
  }
}

string Connection::recv( void )
{
  struct sockaddr_in packet_remote_addr;

  char buf[ RECEIVE_MTU ];

  socklen_t addrlen = sizeof( packet_remote_addr );

  ssize_t received_len = recvfrom( sock, buf, RECEIVE_MTU, 0, (sockaddr *)&packet_remote_addr, &addrlen );

  if ( received_len < 0 ) {
    throw NetworkException( "recvfrom", errno );
  }

  if ( received_len > RECEIVE_MTU ) {
    char buffer[ 2048 ];
    snprintf( buffer, 2048, "Received oversize datagram (size %d) and limit is %d\n",
	      static_cast<int>( received_len ), RECEIVE_MTU );
    throw NetworkException( buffer, errno );
  }

  Packet p( string( buf, received_len ), &session );
  dos_assert( p.direction == (server ? TO_SERVER : TO_CLIENT) ); /* prevent malicious playback to sender */

  /* server auto-adjusts to client */
  if ( server ) {
    attached = true;

    if ( (remote_addr.sin_addr.s_addr != packet_remote_addr.sin_addr.s_addr)
	 || (remote_addr.sin_port != packet_remote_addr.sin_port) ) {
      remote_addr = packet_remote_addr;
      fprintf( stderr, "Server now attached to client at %s:%d\n",
	       inet_ntoa( remote_addr.sin_addr ),
	       ntohs( remote_addr.sin_port ) );
    }
  }

  return p.payload;
}

int Connection::port( void )
{
  struct sockaddr_in local_addr;
  socklen_t addrlen = sizeof( local_addr );

  if ( getsockname( sock, (sockaddr *)&local_addr, &addrlen ) < 0 ) {
    throw NetworkException( "getsockname", errno );
  }

  return ntohs( local_addr.sin_port );
}
