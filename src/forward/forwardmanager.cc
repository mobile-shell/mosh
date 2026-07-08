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

#include "src/include/config.h"

#include "src/forward/forwardmanager.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "src/util/timestamp.h"

using namespace Forward;

namespace {

const uint32_t FORWARD_PROTOCOL_VERSION = 1;
const size_t PER_STREAM_SEND_CAP = 4 * 1024 * 1024;
const size_t PER_STREAM_RECV_CAP = 4 * 1024 * 1024;
const size_t GLOBAL_BUFFER_CAP = 64 * 1024 * 1024;
const size_t MAX_STREAMS = 256;
const size_t MAX_SOCKS_BUFFER = 4096;
const uint64_t CONTROL_RETRY_MS = 500;
const uint64_t DEFAULT_RTO_MS = 250;
const uint64_t MAX_RTO_MS = 1000;
const uint64_t HELLO_INTERVAL_MS = 250;
const uint64_t PING_INTERVAL_MS = 5000;
const uint32_t ERROR_SUCCESS = 0;
const uint32_t ERROR_GENERIC = 1;

int set_nonblocking( int fd )
{
  int flags = fcntl( fd, F_GETFL, 0 );
  if ( flags < 0 ) {
    return -1;
  }
  return fcntl( fd, F_SETFL, flags | O_NONBLOCK );
}

void close_if_open( int& fd )
{
  if ( fd >= 0 ) {
    close( fd );
    fd = -1;
  }
}

bool would_block( void )
{
  return errno == EAGAIN || errno == EWOULDBLOCK;
}

std::string errno_string( const char* prefix )
{
  std::string ret( prefix );
  ret += ": ";
  ret += strerror( errno );
  return ret;
}

std::string port_to_string( uint16_t port )
{
  char buf[16];
  snprintf( buf, sizeof buf, "%u", static_cast<unsigned int>( port ) );
  return std::string( buf );
}

uint16_t parse_u16_port( const std::string& text )
{
  if ( text.empty() ) {
    throw std::runtime_error( "empty port" );
  }

  char* end = NULL;
  errno = 0;
  unsigned long port = strtoul( text.c_str(), &end, 10 );
  if ( errno != 0 || !end || *end != '\0' || port > 65535 ) {
    throw std::runtime_error( "invalid port: " + text );
  }

  return static_cast<uint16_t>( port );
}

std::vector<std::string> split_tabs( const std::string& line )
{
  std::vector<std::string> ret;
  std::string::size_type start = 0;
  while ( true ) {
    std::string::size_type pos = line.find( '\t', start );
    if ( pos == std::string::npos ) {
      ret.push_back( line.substr( start ) );
      break;
    }
    ret.push_back( line.substr( start, pos - start ) );
    start = pos + 1;
  }
  return ret;
}

int create_listener_socket( const std::string& bind_host, uint16_t bind_port, uint16_t& actual_port )
{
  struct addrinfo hints;
  memset( &hints, 0, sizeof hints );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  std::string service = port_to_string( bind_port );
  struct addrinfo* res = NULL;
  int gai = getaddrinfo( bind_host.empty() ? NULL : bind_host.c_str(), service.c_str(), &hints, &res );
  if ( gai != 0 ) {
    throw std::runtime_error( std::string( "getaddrinfo: " ) + gai_strerror( gai ) );
  }

  int fd = -1;
  int saved_errno = 0;
  for ( struct addrinfo* ai = res; ai; ai = ai->ai_next ) {
    fd = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
    if ( fd < 0 ) {
      saved_errno = errno;
      continue;
    }

    int one = 1;
    setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one );

    if ( bind( fd, ai->ai_addr, ai->ai_addrlen ) == 0 && listen( fd, 128 ) == 0 && set_nonblocking( fd ) == 0 ) {
      struct sockaddr_storage ss;
      socklen_t ss_len = sizeof ss;
      if ( getsockname( fd, reinterpret_cast<struct sockaddr*>( &ss ), &ss_len ) == 0 ) {
        if ( ss.ss_family == AF_INET ) {
          actual_port = ntohs( reinterpret_cast<struct sockaddr_in*>( &ss )->sin_port );
        } else if ( ss.ss_family == AF_INET6 ) {
          actual_port = ntohs( reinterpret_cast<struct sockaddr_in6*>( &ss )->sin6_port );
        }
      }
      break;
    }

    saved_errno = errno;
    close( fd );
    fd = -1;
  }

  freeaddrinfo( res );

  if ( fd < 0 ) {
    errno = saved_errno ? saved_errno : errno;
    throw std::runtime_error( errno_string( "listen" ) );
  }

  return fd;
}

int connect_nonblocking( const std::string& host, uint16_t port, bool& connecting, std::string& error_message )
{
  struct addrinfo hints;
  memset( &hints, 0, sizeof hints );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string service = port_to_string( port );
  struct addrinfo* res = NULL;
  int gai = getaddrinfo( host.c_str(), service.c_str(), &hints, &res );
  if ( gai != 0 ) {
    error_message = std::string( "getaddrinfo: " ) + gai_strerror( gai );
    return -1;
  }

  int fd = -1;
  for ( struct addrinfo* ai = res; ai; ai = ai->ai_next ) {
    fd = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
    if ( fd < 0 ) {
      error_message = errno_string( "socket" );
      continue;
    }

    int one = 1;
    setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one );

    if ( set_nonblocking( fd ) < 0 ) {
      error_message = errno_string( "fcntl" );
      close( fd );
      fd = -1;
      continue;
    }

    if ( connect( fd, ai->ai_addr, ai->ai_addrlen ) == 0 ) {
      connecting = false;
      break;
    }

    if ( errno == EINPROGRESS ) {
      connecting = true;
      break;
    }

    error_message = errno_string( "connect" );
    close( fd );
    fd = -1;
  }

  freeaddrinfo( res );
  return fd;
}

std::string socks_reply( uint8_t status )
{
  char data[10];
  memset( data, 0, sizeof data );
  data[0] = 0x05;
  data[1] = static_cast<char>( status );
  data[2] = 0x00;
  data[3] = 0x01;
  return std::string( data, sizeof data );
}

uint64_t now_ms( void )
{
  return Network::timestamp();
}

}

struct ForwardManager::Segment
{
  uint64_t seq;
  std::string data;
  uint64_t last_sent;
  uint64_t rto;

  Segment( void ) : seq( 0 ), data(), last_sent( 0 ), rto( DEFAULT_RTO_MS ) {}
  Segment( uint64_t s_seq, const std::string& s_data )
    : seq( s_seq ), data( s_data ), last_sent( 0 ), rto( DEFAULT_RTO_MS )
  {}
};

struct ForwardManager::Listener
{
  int fd;
  ForwardSpec spec;

  Listener( void ) : fd( -1 ), spec() {}
  Listener( int s_fd, const ForwardSpec& s_spec ) : fd( s_fd ), spec( s_spec ) {}
};

struct ForwardManager::Stream
{
  uint64_t id;
  int read_fd;
  int write_fd;
  bool opened;
  bool connecting;
  bool waiting_open_result;
  bool is_dynamic;
  ForwardBuffers::OpenRequest::Kind kind;
  int socks_state;
  bool local_eof;
  bool remote_fin;
  bool fin_pending;
  bool fin_sent;
  bool fin_acked;
  uint64_t fin_seq;
  uint64_t fin_last_sent;
  bool ack_pending;
  bool close_after_write;
  bool reset;
  uint64_t send_next;
  uint64_t recv_next;
  uint64_t peer_window;
  std::string local_read_buffer;
  std::string local_write_buffer;
  std::string socks_buffer;
  std::map<uint64_t, std::string> out_of_order;
  std::deque<Segment> sent_segments;
  ForwardBuffers::ForwardFrame open_frame;
  bool open_sent;
  uint64_t open_last_sent;
  bool open_result_known;
  bool open_result_success;
  std::string open_result_message;

  Stream( void )
    : id( 0 ), read_fd( -1 ), write_fd( -1 ), opened( false ), connecting( false ), waiting_open_result( false ),
      is_dynamic( false ), kind( ForwardBuffers::OpenRequest::DIRECT_TCP ), socks_state( 0 ),
      local_eof( false ), remote_fin( false ), fin_pending( false ), fin_sent( false ),
      fin_acked( false ), fin_seq( 0 ), fin_last_sent( 0 ), ack_pending( false ),
      close_after_write( false ), reset( false ), send_next( 0 ), recv_next( 0 ),
      peer_window( PER_STREAM_RECV_CAP ), local_read_buffer(), local_write_buffer(), socks_buffer(),
      out_of_order(), sent_segments(), open_frame(), open_sent( false ), open_last_sent( 0 ),
      open_result_known( false ), open_result_success( false ), open_result_message()
  {}

  size_t bytes_in_flight( void ) const
  {
    size_t ret = 0;
    for ( std::deque<Segment>::const_iterator it = sent_segments.begin(); it != sent_segments.end(); ++it ) {
      ret += it->data.size();
    }
    return ret;
  }

  size_t receive_buffered( void ) const
  {
    size_t ret = local_write_buffer.size();
    for ( std::map<uint64_t, std::string>::const_iterator it = out_of_order.begin(); it != out_of_order.end(); ++it ) {
      ret += it->second.size();
    }
    return ret;
  }
};

ForwardManager::ForwardSpec::ForwardSpec( void )
  : type( LOCAL_DIRECT ), bind_host( "127.0.0.1" ), bind_port( 0 ), target_host(), target_port( 0 )
{}

ForwardManager::ForwardManager( Side s_side, const char* desired_ip, const char* desired_port )
  : side( s_side ), connection( new Network::Connection( desired_ip, desired_port ) ), listeners(), streams(),
    control_frames(), next_stream_id( s_side == CLIENT ? 1 : 2 ), next_packet_id( 1 ), last_hello( 0 ),
    last_ping( 0 ), peer_ready( false ), pending_stdio_read_fd( -1 ), pending_stdio_write_fd( -1 ),
    pending_stderr_read_fd( -1 )
{}

ForwardManager::ForwardManager( Side s_side, const char* key_str, const char* ip, const char* port )
  : side( s_side ), connection( new Network::Connection( key_str, ip, port ) ), listeners(), streams(),
    control_frames(), next_stream_id( s_side == CLIENT ? 1 : 2 ), next_packet_id( 1 ), last_hello( 0 ),
    last_ping( 0 ), peer_ready( false ), pending_stdio_read_fd( -1 ), pending_stdio_write_fd( -1 ),
    pending_stderr_read_fd( -1 )
{}

ForwardManager::~ForwardManager()
{
  for ( std::vector<Listener>::iterator it = listeners.begin(); it != listeners.end(); ++it ) {
    close_if_open( it->fd );
  }
  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end(); ++it ) {
    close_if_open( it->second.read_fd );
    if ( it->second.write_fd != it->second.read_fd ) {
      close_if_open( it->second.write_fd );
    }
  }
  close_if_open( pending_stdio_read_fd );
  close_if_open( pending_stdio_write_fd );
  close_if_open( pending_stderr_read_fd );
}

void ForwardManager::add_local_forward( const std::string& bind_host,
                                        uint16_t bind_port,
                                        const std::string& target_host,
                                        uint16_t target_port )
{
  ForwardSpec spec;
  spec.type = LOCAL_DIRECT;
  spec.bind_host = bind_host.empty() ? std::string( "127.0.0.1" ) : bind_host;
  spec.bind_port = bind_port;
  spec.target_host = target_host;
  spec.target_port = target_port;
  add_listener( spec );
}

void ForwardManager::add_dynamic_forward( const std::string& bind_host, uint16_t bind_port )
{
  ForwardSpec spec;
  spec.type = LOCAL_DYNAMIC;
  spec.bind_host = bind_host.empty() ? std::string( "127.0.0.1" ) : bind_host;
  spec.bind_port = bind_port;
  add_listener( spec );
}

uint64_t ForwardManager::open_stdio_stream( int read_fd, int write_fd )
{
  return open_special_stream( read_fd, write_fd, ForwardBuffers::OpenRequest::STDIO );
}

uint64_t ForwardManager::open_stderr_stream( int write_fd )
{
  return open_special_stream( -1, write_fd, ForwardBuffers::OpenRequest::STDERR );
}

void ForwardManager::accept_stdio_fds( int read_fd, int write_fd )
{
  accept_special_fds( read_fd, write_fd, ForwardBuffers::OpenRequest::STDIO );
}

void ForwardManager::accept_stderr_fd( int fd )
{
  accept_special_fds( fd, -1, ForwardBuffers::OpenRequest::STDERR );
}

bool ForwardManager::stream_closed( uint64_t stream_id ) const
{
  return stream_id == 0 || streams.find( stream_id ) == streams.end();
}

void ForwardManager::add_listener( const ForwardSpec& spec )
{
  uint16_t actual_port = spec.bind_port;
  int fd = create_listener_socket( spec.bind_host, spec.bind_port, actual_port );
  ForwardSpec actual = spec;
  actual.bind_port = actual_port;
  listeners.push_back( Listener( fd, actual ) );
}

std::vector<int> ForwardManager::read_fds( void ) const
{
  std::vector<int> ret = connection->fds();

  for ( std::vector<Listener>::const_iterator it = listeners.begin(); it != listeners.end(); ++it ) {
    if ( it->fd >= 0 ) {
      ret.push_back( it->fd );
    }
  }

  for ( std::map<uint64_t, Stream>::const_iterator it = streams.begin(); it != streams.end(); ++it ) {
    const Stream& stream = it->second;
    if ( stream.read_fd < 0 || stream.reset || stream.local_eof || stream.connecting || stream.waiting_open_result ) {
      continue;
    }
    if ( stream.is_dynamic && stream.socks_state < 2 ) {
      ret.push_back( stream.read_fd );
    } else if ( stream.opened
                && stream.local_read_buffer.size() + stream.bytes_in_flight() < PER_STREAM_SEND_CAP
                && global_buffered() < GLOBAL_BUFFER_CAP ) {
      ret.push_back( stream.read_fd );
    }
  }

  return ret;
}

std::vector<int> ForwardManager::write_fds( void ) const
{
  std::vector<int> ret;
  for ( std::map<uint64_t, Stream>::const_iterator it = streams.begin(); it != streams.end(); ++it ) {
    const Stream& stream = it->second;
    if ( stream.write_fd < 0 || stream.reset ) {
      continue;
    }
    if ( stream.connecting || !stream.local_write_buffer.empty() ) {
      ret.push_back( stream.write_fd );
    }
  }
  return ret;
}

int ForwardManager::wait_time( void ) const
{
  return 50;
}

void ForwardManager::handle_readable( int fd )
{
  if ( is_network_fd( fd ) ) {
    handle_network_readable();
    return;
  }

  Listener* listener = listener_for_fd( fd );
  if ( listener ) {
    accept_listener( *listener );
    return;
  }

  Stream* stream = stream_for_fd( fd );
  if ( stream ) {
    handle_stream_readable( *stream );
  }
}

void ForwardManager::handle_writable( int fd )
{
  Stream* stream = stream_for_fd( fd );
  if ( !stream ) {
    return;
  }

  if ( stream->connecting ) {
    std::string error_message;
    if ( finish_connect( *stream, error_message ) ) {
      if ( side == SERVER ) {
        send_open_result( stream->id, true, std::string() );
      }
      stream->opened = true;
    } else {
      if ( side == SERVER ) {
        send_open_result( stream->id, false, error_message );
      }
      stream->reset = true;
    }
  }

  if ( !stream->reset ) {
    flush_local_write( *stream );
  }
}

void ForwardManager::tick( void )
{
  uint64_t now = now_ms();
  if ( !peer_ready && now - last_hello >= HELLO_INTERVAL_MS ) {
    ForwardBuffers::ForwardFrame hello;
    hello.set_type( ForwardBuffers::ForwardFrame::HELLO );
    send_frame( hello );
    last_hello = now;
  }

  if ( peer_ready && ( !listeners.empty() || !streams.empty() ) && now - last_ping >= PING_INTERVAL_MS ) {
    ForwardBuffers::ForwardFrame ping;
    ping.set_type( ForwardBuffers::ForwardFrame::PING );
    send_frame( ping );
    last_ping = now;
  }

  unsigned int budget = 32;
  send_control_frames( budget );
  send_ack_frames( budget );
  send_retransmissions( budget );
  send_new_data( budget );
  send_fin_frames( budget );
  close_closed_streams();
}

std::string ForwardManager::port( void ) const
{
  return connection->port();
}

std::string ForwardManager::key( void ) const
{
  return connection->get_key();
}

std::string ForwardManager::describe_listeners( void ) const
{
  std::ostringstream out;
  for ( std::vector<Listener>::const_iterator it = listeners.begin(); it != listeners.end(); ++it ) {
    if ( it != listeners.begin() ) {
      out << ", ";
    }
    out << ( it->spec.type == LOCAL_DYNAMIC ? "D " : "L " ) << it->spec.bind_host << ":" << it->spec.bind_port;
    if ( it->spec.type == LOCAL_DIRECT ) {
      out << ":" << it->spec.target_host << ":" << it->spec.target_port;
    }
  }
  return out.str();
}

std::vector<ForwardManager::ForwardSpec> ForwardManager::parse_spec_list( const std::string& specs )
{
  std::vector<ForwardSpec> ret;
  std::string::size_type start = 0;
  while ( start <= specs.size() ) {
    std::string::size_type end = specs.find( '\n', start );
    std::string line = specs.substr( start, end == std::string::npos ? std::string::npos : end - start );
    if ( !line.empty() ) {
      std::vector<std::string> fields = split_tabs( line );
      if ( fields.size() != 5 ) {
        throw std::runtime_error( "bad forwarding spec: " + line );
      }
      ForwardSpec spec;
      if ( fields[0] == "L" ) {
        spec.type = LOCAL_DIRECT;
        spec.target_host = fields[3];
        spec.target_port = parse_u16_port( fields[4] );
      } else if ( fields[0] == "D" ) {
        spec.type = LOCAL_DYNAMIC;
      } else {
        throw std::runtime_error( "bad forwarding type: " + fields[0] );
      }
      spec.bind_host = fields[1].empty() ? std::string( "127.0.0.1" ) : fields[1];
      spec.bind_port = parse_u16_port( fields[2] );
      ret.push_back( spec );
    }
    if ( end == std::string::npos ) {
      break;
    }
    start = end + 1;
  }
  return ret;
}

void ForwardManager::accept_listener( Listener& listener )
{
  while ( true ) {
    if ( streams.size() >= MAX_STREAMS || global_buffered() >= GLOBAL_BUFFER_CAP ) {
      return;
    }

    struct sockaddr_storage ss;
    socklen_t ss_len = sizeof ss;
    int fd = accept( listener.fd, reinterpret_cast<struct sockaddr*>( &ss ), &ss_len );
    if ( fd < 0 ) {
      if ( would_block() || errno == EINTR ) {
        return;
      }
      throw std::runtime_error( errno_string( "accept" ) );
    }

    set_nonblocking( fd );
    int one = 1;
    setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one );

    Stream stream;
    stream.id = next_stream_id;
    next_stream_id += 2;
    stream.read_fd = fd;
    stream.write_fd = fd;
    stream.peer_window = PER_STREAM_RECV_CAP;
    stream.is_dynamic = listener.spec.type == LOCAL_DYNAMIC;

    if ( stream.is_dynamic ) {
      stream.socks_state = 0;
    } else {
      open_direct_stream( stream, listener.spec.target_host, listener.spec.target_port );
    }

    streams[stream.id] = stream;
  }
}

void ForwardManager::handle_network_readable( void )
{
  std::string payload = connection->recv();
  ForwardBuffers::ForwardPacket packet;
  if ( !packet.ParseFromString( payload ) ) {
    return;
  }
  if ( packet.version() != FORWARD_PROTOCOL_VERSION ) {
    return;
  }
  for ( int i = 0; i < packet.frame_size(); i++ ) {
    process_frame( packet.frame( i ) );
  }
}

void ForwardManager::process_frame( const ForwardBuffers::ForwardFrame& frame )
{
  switch ( frame.type() ) {
    case ForwardBuffers::ForwardFrame::HELLO: {
      peer_ready = true;
      ForwardBuffers::ForwardFrame reply;
      reply.set_type( ForwardBuffers::ForwardFrame::HELLO_OK );
      control_frames.push_back( reply );
      break;
    }
    case ForwardBuffers::ForwardFrame::HELLO_OK:
      peer_ready = true;
      break;
    case ForwardBuffers::ForwardFrame::OPEN:
      process_open( frame );
      break;
    case ForwardBuffers::ForwardFrame::OPEN_RESULT:
      process_open_result( frame );
      break;
    case ForwardBuffers::ForwardFrame::DATA:
      process_data( frame );
      break;
    case ForwardBuffers::ForwardFrame::ACK:
    case ForwardBuffers::ForwardFrame::WINDOW_UPDATE:
      process_acklike( frame );
      break;
    case ForwardBuffers::ForwardFrame::FIN:
      process_fin( frame );
      break;
    case ForwardBuffers::ForwardFrame::RST:
      process_rst( frame );
      break;
    case ForwardBuffers::ForwardFrame::PING: {
      ForwardBuffers::ForwardFrame pong;
      pong.set_type( ForwardBuffers::ForwardFrame::PONG );
      control_frames.push_back( pong );
      break;
    }
    case ForwardBuffers::ForwardFrame::PONG:
      peer_ready = true;
      break;
    default:
      break;
  }
}

void ForwardManager::process_open( const ForwardBuffers::ForwardFrame& frame )
{
  if ( !frame.has_stream_id() || !frame.has_open() ) {
    return;
  }

  uint64_t id = frame.stream_id();
  std::map<uint64_t, Stream>::iterator existing = streams.find( id );
  if ( existing != streams.end() ) {
    Stream& stream = existing->second;
    if ( stream.open_result_known ) {
      send_open_result( id, stream.open_result_success, stream.open_result_message );
    }
    return;
  }

  if ( streams.size() >= MAX_STREAMS || global_buffered() >= GLOBAL_BUFFER_CAP ) {
    send_open_result( id, false, "forwarding resource limit reached" );
    return;
  }

  const ForwardBuffers::OpenRequest& open = frame.open();
  if ( open.kind() == ForwardBuffers::OpenRequest::STDIO || open.kind() == ForwardBuffers::OpenRequest::STDERR ) {
    int* pending_read_fd
      = open.kind() == ForwardBuffers::OpenRequest::STDIO ? &pending_stdio_read_fd : &pending_stderr_read_fd;
    int* pending_write_fd = open.kind() == ForwardBuffers::OpenRequest::STDIO ? &pending_stdio_write_fd : NULL;
    if ( *pending_read_fd < 0 && ( pending_write_fd == NULL || *pending_write_fd < 0 ) ) {
      send_open_result( id, false, "stdio stream is not available" );
      return;
    }

    Stream stream;
    stream.id = id;
    stream.read_fd = *pending_read_fd;
    stream.write_fd = pending_write_fd ? *pending_write_fd : -1;
    if ( stream.read_fd < 0 ) {
      stream.local_eof = true;
      stream.fin_pending = true;
    }
    *pending_read_fd = -1;
    if ( pending_write_fd ) {
      *pending_write_fd = -1;
    }
    stream.opened = true;
    stream.kind = open.kind();
    stream.peer_window = frame.has_recv_window() ? frame.recv_window() : PER_STREAM_RECV_CAP;
    streams[id] = stream;
    send_open_result( id, true, std::string() );
    return;
  }

  if ( open.kind() != ForwardBuffers::OpenRequest::DIRECT_TCP ) {
    send_open_result( id, false, "unsupported open kind" );
    return;
  }

  Stream stream;
  stream.id = id;
  stream.kind = ForwardBuffers::OpenRequest::DIRECT_TCP;
  stream.peer_window = frame.has_recv_window() ? frame.recv_window() : PER_STREAM_RECV_CAP;

  bool connecting = false;
  std::string error_message;
  int fd = connect_nonblocking( open.host(), static_cast<uint16_t>( open.port() ), connecting, error_message );
  if ( fd < 0 ) {
    send_open_result( id, false, error_message );
    return;
  }

  stream.read_fd = fd;
  stream.write_fd = fd;
  stream.connecting = connecting;
  stream.opened = !connecting;
  streams[id] = stream;
  if ( !connecting ) {
    send_open_result( id, true, std::string() );
  }
}

void ForwardManager::process_open_result( const ForwardBuffers::ForwardFrame& frame )
{
  if ( !frame.has_stream_id() ) {
    return;
  }
  std::map<uint64_t, Stream>::iterator it = streams.find( frame.stream_id() );
  if ( it == streams.end() ) {
    return;
  }

  Stream& stream = it->second;
  stream.waiting_open_result = false;
  stream.peer_window = frame.has_recv_window() ? frame.recv_window() : PER_STREAM_RECV_CAP;

  if ( frame.error_code() == ERROR_SUCCESS ) {
    stream.opened = true;
    if ( stream.is_dynamic ) {
      stream.socks_state = 2;
      stream.local_write_buffer += socks_reply( 0x00 );
      stream.local_read_buffer += stream.socks_buffer;
      stream.socks_buffer.clear();
    }
  } else {
    if ( stream.is_dynamic ) {
      stream.local_write_buffer += socks_reply( 0x05 );
      stream.close_after_write = true;
    } else {
      stream.reset = true;
    }
  }
}

void ForwardManager::process_data( const ForwardBuffers::ForwardFrame& frame )
{
  if ( !frame.has_stream_id() || !frame.has_seq() ) {
    return;
  }

  std::map<uint64_t, Stream>::iterator it = streams.find( frame.stream_id() );
  if ( it == streams.end() ) {
    return;
  }

  Stream& stream = it->second;
  const std::string& data = frame.data();
  uint64_t seq = frame.seq();

  if ( seq < stream.recv_next ) {
    stream.ack_pending = true;
    return;
  }

  if ( stream.receive_buffered() + data.size() > PER_STREAM_RECV_CAP
       || global_buffered() + data.size() > GLOBAL_BUFFER_CAP ) {
    stream.ack_pending = true;
    return;
  }

  if ( seq == stream.recv_next ) {
    stream.local_write_buffer += data;
    stream.recv_next += data.size();

    while ( true ) {
      std::map<uint64_t, std::string>::iterator next = stream.out_of_order.find( stream.recv_next );
      if ( next == stream.out_of_order.end() ) {
        break;
      }
      stream.local_write_buffer += next->second;
      stream.recv_next += next->second.size();
      stream.out_of_order.erase( next );
    }
  } else {
    stream.out_of_order[seq] = data;
  }

  stream.ack_pending = true;
}

void ForwardManager::process_acklike( const ForwardBuffers::ForwardFrame& frame )
{
  if ( !frame.has_stream_id() ) {
    return;
  }
  std::map<uint64_t, Stream>::iterator it = streams.find( frame.stream_id() );
  if ( it == streams.end() ) {
    return;
  }

  Stream& stream = it->second;
  if ( frame.has_recv_window() ) {
    stream.peer_window = frame.recv_window();
  }

  if ( frame.has_ack() ) {
    uint64_t ack = frame.ack();
    while ( !stream.sent_segments.empty() ) {
      const Segment& segment = stream.sent_segments.front();
      if ( segment.seq + segment.data.size() > ack ) {
        break;
      }
      stream.sent_segments.pop_front();
    }
    if ( stream.fin_sent && ack > stream.fin_seq ) {
      stream.fin_acked = true;
    }
  }
}

void ForwardManager::process_fin( const ForwardBuffers::ForwardFrame& frame )
{
  if ( !frame.has_stream_id() || !frame.has_seq() ) {
    return;
  }
  std::map<uint64_t, Stream>::iterator it = streams.find( frame.stream_id() );
  if ( it == streams.end() ) {
    return;
  }

  Stream& stream = it->second;
  if ( frame.seq() == stream.recv_next ) {
    stream.recv_next++;
    stream.remote_fin = true;
  }
  stream.ack_pending = true;
}

void ForwardManager::process_rst( const ForwardBuffers::ForwardFrame& frame )
{
  if ( !frame.has_stream_id() ) {
    return;
  }
  std::map<uint64_t, Stream>::iterator it = streams.find( frame.stream_id() );
  if ( it == streams.end() ) {
    return;
  }
  it->second.reset = true;
}

void ForwardManager::handle_stream_readable( Stream& stream )
{
  if ( stream.is_dynamic && stream.socks_state < 2 ) {
    parse_socks( stream );
    return;
  }

  char buf[16384];
  while ( stream.local_read_buffer.size() + stream.bytes_in_flight() < PER_STREAM_SEND_CAP
          && global_buffered() < GLOBAL_BUFFER_CAP ) {
    ssize_t n = read( stream.read_fd, buf, sizeof buf );
    if ( n > 0 ) {
      stream.local_read_buffer.append( buf, n );
      continue;
    }
    if ( n == 0 ) {
      stream.local_eof = true;
      stream.fin_pending = true;
      return;
    }
    if ( errno == EINTR ) {
      continue;
    }
    if ( would_block() ) {
      return;
    }
    send_rst( stream.id, errno_string( "read" ) );
    stream.reset = true;
    return;
  }
}

void ForwardManager::handle_stream_writable( Stream& stream )
{
  flush_local_write( stream );
}

bool ForwardManager::finish_connect( Stream& stream, std::string& error_message )
{
  int err = 0;
  socklen_t err_len = sizeof err;
  if ( getsockopt( stream.write_fd, SOL_SOCKET, SO_ERROR, &err, &err_len ) < 0 ) {
    error_message = errno_string( "getsockopt" );
    return false;
  }
  if ( err != 0 ) {
    error_message = std::string( "connect: " ) + strerror( err );
    return false;
  }
  stream.connecting = false;
  return true;
}

bool ForwardManager::flush_local_write( Stream& stream )
{
  while ( !stream.local_write_buffer.empty() ) {
    ssize_t n = write( stream.write_fd, stream.local_write_buffer.data(), stream.local_write_buffer.size() );
    if ( n > 0 ) {
      stream.local_write_buffer.erase( 0, n );
      stream.ack_pending = true;
      continue;
    }
    if ( n < 0 && errno == EINTR ) {
      continue;
    }
    if ( n < 0 && would_block() ) {
      return true;
    }
    send_rst( stream.id, errno_string( "write" ) );
    stream.reset = true;
    return false;
  }

  if ( stream.close_after_write ) {
    stream.reset = true;
    return true;
  }

  if ( stream.remote_fin ) {
    if ( stream.read_fd == stream.write_fd ) {
      shutdown( stream.write_fd, SHUT_WR );
    } else {
      close_if_open( stream.write_fd );
    }
  }

  return true;
}

void ForwardManager::parse_socks( Stream& stream )
{
  char buf[512];
  while ( stream.socks_buffer.size() < MAX_SOCKS_BUFFER ) {
    ssize_t n = read( stream.read_fd, buf, sizeof buf );
    if ( n > 0 ) {
      stream.socks_buffer.append( buf, n );
      continue;
    }
    if ( n == 0 ) {
      stream.reset = true;
      return;
    }
    if ( errno == EINTR ) {
      continue;
    }
    if ( would_block() ) {
      break;
    }
    stream.reset = true;
    return;
  }

  if ( stream.socks_state == 0 ) {
    if ( stream.socks_buffer.size() < 2 ) {
      return;
    }
    const unsigned char* data = reinterpret_cast<const unsigned char*>( stream.socks_buffer.data() );
    if ( data[0] != 0x05 ) {
      stream.reset = true;
      return;
    }
    size_t needed = 2 + data[1];
    if ( stream.socks_buffer.size() < needed ) {
      return;
    }

    bool no_auth = false;
    for ( size_t i = 2; i < needed; i++ ) {
      no_auth = no_auth || data[i] == 0x00;
    }

    char reply[2] = { 0x05, static_cast<char>( no_auth ? 0x00 : 0xff ) };
    stream.local_write_buffer.append( reply, sizeof reply );
    stream.socks_buffer.erase( 0, needed );
    if ( !no_auth ) {
      stream.close_after_write = true;
      return;
    }
    stream.socks_state = 1;
  }

  if ( stream.socks_state == 1 ) {
    if ( stream.socks_buffer.size() < 4 ) {
      return;
    }
    const unsigned char* data = reinterpret_cast<const unsigned char*>( stream.socks_buffer.data() );
    if ( data[0] != 0x05 || data[2] != 0x00 ) {
      stream.local_write_buffer += socks_reply( 0x01 );
      stream.close_after_write = true;
      return;
    }
    if ( data[1] != 0x01 ) {
      stream.local_write_buffer += socks_reply( 0x07 );
      stream.close_after_write = true;
      return;
    }

    size_t addr_offset = 4;
    std::string host;
    size_t needed = 0;
    if ( data[3] == 0x01 ) {
      needed = addr_offset + 4 + 2;
      if ( stream.socks_buffer.size() < needed ) {
        return;
      }
      char text[INET_ADDRSTRLEN];
      inet_ntop( AF_INET, data + addr_offset, text, sizeof text );
      host = text;
      addr_offset += 4;
    } else if ( data[3] == 0x03 ) {
      if ( stream.socks_buffer.size() < addr_offset + 1 ) {
        return;
      }
      size_t len = data[addr_offset];
      needed = addr_offset + 1 + len + 2;
      if ( stream.socks_buffer.size() < needed ) {
        return;
      }
      host.assign( reinterpret_cast<const char*>( data + addr_offset + 1 ), len );
      addr_offset += 1 + len;
    } else if ( data[3] == 0x04 ) {
      needed = addr_offset + 16 + 2;
      if ( stream.socks_buffer.size() < needed ) {
        return;
      }
      char text[INET6_ADDRSTRLEN];
      inet_ntop( AF_INET6, data + addr_offset, text, sizeof text );
      host = text;
      addr_offset += 16;
    } else {
      stream.local_write_buffer += socks_reply( 0x08 );
      stream.close_after_write = true;
      return;
    }

    uint16_t port = static_cast<uint16_t>( data[addr_offset] << 8 | data[addr_offset + 1] );
    stream.socks_buffer.erase( 0, needed );
    open_direct_stream( stream, host, port );
  }
}

void ForwardManager::open_direct_stream( Stream& stream, const std::string& host, uint16_t port )
{
  stream.waiting_open_result = true;
  stream.kind = ForwardBuffers::OpenRequest::DIRECT_TCP;
  stream.open_frame.Clear();
  stream.open_frame.set_type( ForwardBuffers::ForwardFrame::OPEN );
  stream.open_frame.set_stream_id( stream.id );
  stream.open_frame.set_recv_window( receive_window( stream ) );
  ForwardBuffers::OpenRequest* open = stream.open_frame.mutable_open();
  open->set_kind( ForwardBuffers::OpenRequest::DIRECT_TCP );
  open->set_host( host );
  open->set_port( port );
}

uint64_t ForwardManager::open_special_stream( int read_fd, int write_fd, ForwardBuffers::OpenRequest::Kind kind )
{
  if ( read_fd < 0 && write_fd < 0 ) {
    return 0;
  }
  if ( read_fd >= 0 ) {
    set_nonblocking( read_fd );
  }
  if ( write_fd >= 0 && write_fd != read_fd ) {
    set_nonblocking( write_fd );
  }

  Stream stream;
  stream.id = next_stream_id;
  next_stream_id += 2;
  stream.read_fd = read_fd;
  stream.write_fd = write_fd;
  stream.kind = kind;
  if ( stream.read_fd < 0 ) {
    stream.local_eof = true;
    stream.fin_pending = true;
  }
  stream.peer_window = PER_STREAM_RECV_CAP;
  stream.waiting_open_result = true;
  stream.open_frame.Clear();
  stream.open_frame.set_type( ForwardBuffers::ForwardFrame::OPEN );
  stream.open_frame.set_stream_id( stream.id );
  stream.open_frame.set_recv_window( receive_window( stream ) );
  ForwardBuffers::OpenRequest* open = stream.open_frame.mutable_open();
  open->set_kind( kind );

  uint64_t id = stream.id;
  streams[id] = stream;
  return id;
}

void ForwardManager::accept_special_fds( int read_fd, int write_fd, ForwardBuffers::OpenRequest::Kind kind )
{
  if ( read_fd >= 0 ) {
    set_nonblocking( read_fd );
  }
  if ( write_fd >= 0 && write_fd != read_fd ) {
    set_nonblocking( write_fd );
  }
  if ( kind == ForwardBuffers::OpenRequest::STDIO ) {
    close_if_open( pending_stdio_read_fd );
    close_if_open( pending_stdio_write_fd );
    pending_stdio_read_fd = read_fd;
    pending_stdio_write_fd = write_fd;
  } else if ( kind == ForwardBuffers::OpenRequest::STDERR ) {
    close_if_open( pending_stderr_read_fd );
    pending_stderr_read_fd = read_fd;
  } else {
    if ( read_fd >= 0 ) {
      close( read_fd );
    }
    if ( write_fd >= 0 && write_fd != read_fd ) {
      close( write_fd );
    }
  }
}

void ForwardManager::send_open_result( uint64_t stream_id, bool success, const std::string& error_message )
{
  std::map<uint64_t, Stream>::iterator it = streams.find( stream_id );
  if ( it != streams.end() ) {
    it->second.open_result_known = true;
    it->second.open_result_success = success;
    it->second.open_result_message = error_message;
  }

  ForwardBuffers::ForwardFrame frame;
  frame.set_type( ForwardBuffers::ForwardFrame::OPEN_RESULT );
  frame.set_stream_id( stream_id );
  frame.set_error_code( success ? ERROR_SUCCESS : ERROR_GENERIC );
  frame.set_recv_window( PER_STREAM_RECV_CAP );
  if ( !success ) {
    frame.set_error_message( error_message );
  }
  control_frames.push_back( frame );
}

void ForwardManager::send_rst( uint64_t stream_id, const std::string& error_message )
{
  ForwardBuffers::ForwardFrame frame;
  frame.set_type( ForwardBuffers::ForwardFrame::RST );
  frame.set_stream_id( stream_id );
  frame.set_error_code( ERROR_GENERIC );
  frame.set_error_message( error_message );
  control_frames.push_back( frame );
}

void ForwardManager::queue_ack( Stream& stream )
{
  stream.ack_pending = true;
}

void ForwardManager::queue_fin( Stream& stream )
{
  stream.fin_pending = true;
}

void ForwardManager::close_stream( uint64_t stream_id )
{
  std::map<uint64_t, Stream>::iterator it = streams.find( stream_id );
  if ( it == streams.end() ) {
    return;
  }
  close_if_open( it->second.read_fd );
  if ( it->second.write_fd != it->second.read_fd ) {
    close_if_open( it->second.write_fd );
  }
  streams.erase( it );
}

void ForwardManager::close_closed_streams( void )
{
  std::vector<uint64_t> to_close;
  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end(); ++it ) {
    Stream& stream = it->second;
    if ( stream.reset ) {
      to_close.push_back( stream.id );
      continue;
    }
    if ( stream.remote_fin && stream.local_eof && stream.fin_sent && stream.fin_acked
         && stream.local_write_buffer.empty() && stream.local_read_buffer.empty() && stream.sent_segments.empty() ) {
      to_close.push_back( stream.id );
    }
  }

  for ( std::vector<uint64_t>::const_iterator it = to_close.begin(); it != to_close.end(); ++it ) {
    close_stream( *it );
  }
}

bool ForwardManager::send_frame( const ForwardBuffers::ForwardFrame& frame )
{
  ForwardBuffers::ForwardPacket packet;
  packet.set_version( FORWARD_PROTOCOL_VERSION );
  packet.set_packet_id( next_packet_id++ );
  ForwardBuffers::ForwardFrame* out = packet.add_frame();
  *out = frame;

  std::string payload;
  packet.SerializeToString( &payload );
  return connection->send_datagram( payload );
}

bool ForwardManager::send_control_frames( unsigned int& budget )
{
  bool sent_any = false;
  uint64_t now = now_ms();

  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end() && budget > 0; ++it ) {
    Stream& stream = it->second;
    if ( stream.waiting_open_result && ( !stream.open_sent || now - stream.open_last_sent >= CONTROL_RETRY_MS ) ) {
      if ( send_frame( stream.open_frame ) ) {
        stream.open_sent = true;
        stream.open_last_sent = now;
        sent_any = true;
        budget--;
      }
    }
  }

  while ( !control_frames.empty() && budget > 0 ) {
    ForwardBuffers::ForwardFrame frame = control_frames.front();
    control_frames.pop_front();
    if ( send_frame( frame ) ) {
      sent_any = true;
      budget--;
    }
  }

  return sent_any;
}

bool ForwardManager::send_ack_frames( unsigned int& budget )
{
  bool sent_any = false;
  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end() && budget > 0; ++it ) {
    Stream& stream = it->second;
    if ( !stream.ack_pending ) {
      continue;
    }
    ForwardBuffers::ForwardFrame frame;
    frame.set_type( ForwardBuffers::ForwardFrame::ACK );
    frame.set_stream_id( stream.id );
    frame.set_ack( stream.recv_next );
    frame.set_recv_window( receive_window( stream ) );
    if ( send_frame( frame ) ) {
      stream.ack_pending = false;
      sent_any = true;
      budget--;
    }
  }
  return sent_any;
}

bool ForwardManager::send_retransmissions( unsigned int& budget )
{
  bool sent_any = false;
  uint64_t now = now_ms();
  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end() && budget > 0; ++it ) {
    Stream& stream = it->second;
    for ( std::deque<Segment>::iterator segment = stream.sent_segments.begin();
          segment != stream.sent_segments.end() && budget > 0;
          ++segment ) {
      if ( segment->last_sent == 0 || now - segment->last_sent < segment->rto ) {
        continue;
      }
      ForwardBuffers::ForwardFrame frame;
      frame.set_type( ForwardBuffers::ForwardFrame::DATA );
      frame.set_stream_id( stream.id );
      frame.set_seq( segment->seq );
      frame.set_data( segment->data );
      frame.set_recv_window( receive_window( stream ) );
      if ( send_frame( frame ) ) {
        segment->last_sent = now;
        segment->rto = std::min<uint64_t>( MAX_RTO_MS, segment->rto * 2 );
        sent_any = true;
        budget--;
      }
    }
  }
  return sent_any;
}

bool ForwardManager::send_new_data( unsigned int& budget )
{
  bool sent_any = false;
  size_t payload_limit = max_data_payload();
  uint64_t now = now_ms();

  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end() && budget > 0; ++it ) {
    Stream& stream = it->second;
    if ( !stream.opened || stream.reset || stream.local_read_buffer.empty() ) {
      continue;
    }

    while ( !stream.local_read_buffer.empty() && budget > 0 ) {
      size_t in_flight = stream.bytes_in_flight();
      if ( stream.peer_window <= in_flight ) {
        break;
      }
      size_t window_left = static_cast<size_t>( stream.peer_window - in_flight );
      size_t len = std::min( payload_limit, std::min( window_left, stream.local_read_buffer.size() ) );
      if ( len == 0 ) {
        break;
      }

      std::string chunk = stream.local_read_buffer.substr( 0, len );
      ForwardBuffers::ForwardFrame frame;
      frame.set_type( ForwardBuffers::ForwardFrame::DATA );
      frame.set_stream_id( stream.id );
      frame.set_seq( stream.send_next );
      frame.set_data( chunk );
      frame.set_recv_window( receive_window( stream ) );
      if ( !send_frame( frame ) ) {
        break;
      }

      Segment segment( stream.send_next, chunk );
      segment.last_sent = now;
      stream.sent_segments.push_back( segment );
      stream.send_next += chunk.size();
      stream.local_read_buffer.erase( 0, chunk.size() );
      sent_any = true;
      budget--;
    }
  }

  return sent_any;
}

bool ForwardManager::send_fin_frames( unsigned int& budget )
{
  bool sent_any = false;
  uint64_t now = now_ms();
  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end() && budget > 0; ++it ) {
    Stream& stream = it->second;
    if ( stream.reset || !stream.opened || !stream.local_read_buffer.empty() ) {
      continue;
    }
    if ( stream.fin_pending && !stream.fin_sent ) {
      stream.fin_seq = stream.send_next;
      stream.fin_sent = true;
      stream.fin_pending = false;
    }
    if ( stream.fin_sent && !stream.fin_acked
         && ( stream.fin_last_sent == 0 || now - stream.fin_last_sent >= DEFAULT_RTO_MS ) ) {
      ForwardBuffers::ForwardFrame frame;
      frame.set_type( ForwardBuffers::ForwardFrame::FIN );
      frame.set_stream_id( stream.id );
      frame.set_seq( stream.fin_seq );
      frame.set_recv_window( receive_window( stream ) );
      if ( send_frame( frame ) ) {
        stream.fin_last_sent = now;
        sent_any = true;
        budget--;
      }
    }
  }
  return sent_any;
}

uint32_t ForwardManager::receive_window( const Stream& stream ) const
{
  size_t buffered = stream.receive_buffered();
  size_t global = global_buffered();
  if ( buffered >= PER_STREAM_RECV_CAP || global >= GLOBAL_BUFFER_CAP ) {
    return 0;
  }
  return static_cast<uint32_t>( std::min( PER_STREAM_RECV_CAP - buffered, GLOBAL_BUFFER_CAP - global ) );
}

size_t ForwardManager::max_data_payload( void ) const
{
  int mtu = connection->get_MTU();
  if ( mtu < 256 ) {
    return 128;
  }
  return static_cast<size_t>( mtu - 128 );
}

size_t ForwardManager::global_buffered( void ) const
{
  size_t ret = 0;
  for ( std::map<uint64_t, Stream>::const_iterator it = streams.begin(); it != streams.end(); ++it ) {
    ret += it->second.local_read_buffer.size();
    ret += it->second.socks_buffer.size();
    ret += it->second.receive_buffered();
    ret += it->second.bytes_in_flight();
  }
  return ret;
}

bool ForwardManager::is_network_fd( int fd ) const
{
  std::vector<int> fds = connection->fds();
  return std::find( fds.begin(), fds.end(), fd ) != fds.end();
}

ForwardManager::Stream* ForwardManager::stream_for_fd( int fd )
{
  for ( std::map<uint64_t, Stream>::iterator it = streams.begin(); it != streams.end(); ++it ) {
    if ( it->second.read_fd == fd || it->second.write_fd == fd ) {
      return &it->second;
    }
  }
  return NULL;
}

ForwardManager::Listener* ForwardManager::listener_for_fd( int fd )
{
  for ( std::vector<Listener>::iterator it = listeners.begin(); it != listeners.end(); ++it ) {
    if ( it->fd == fd ) {
      return &*it;
    }
  }
  return NULL;
}
