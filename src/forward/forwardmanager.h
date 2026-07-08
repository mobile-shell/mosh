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

#ifndef FORWARD_MANAGER_HPP
#define FORWARD_MANAGER_HPP

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/network/network.h"
#include "src/protobufs/forward.pb.h"

namespace Forward {

class ForwardManager
{
public:
  enum Side
  {
    CLIENT,
    SERVER
  };

  enum SpecType
  {
    LOCAL_DIRECT,
    LOCAL_DYNAMIC
  };

  struct ForwardSpec
  {
    SpecType type;
    std::string bind_host;
    uint16_t bind_port;
    std::string target_host;
    uint16_t target_port;

    ForwardSpec( void );
  };

  ForwardManager( Side side, const char* desired_ip, const char* desired_port );
  ForwardManager( Side side, const char* key_str, const char* ip, const char* port );
  ~ForwardManager();

  void add_local_forward( const std::string& bind_host,
                          uint16_t bind_port,
                          const std::string& target_host,
                          uint16_t target_port );
  void add_dynamic_forward( const std::string& bind_host, uint16_t bind_port );
  uint64_t open_stdio_stream( int read_fd, int write_fd );
  uint64_t open_stderr_stream( int write_fd );
  void accept_stdio_fds( int read_fd, int write_fd );
  void accept_stderr_fd( int read_fd );
  bool stream_closed( uint64_t stream_id ) const;
  bool all_streams_closed( void ) const
  {
    return streams.empty() && pending_stdio_read_fd < 0 && pending_stdio_write_fd < 0 && pending_stderr_read_fd < 0;
  }

  std::vector<int> read_fds( void ) const;
  std::vector<int> write_fds( void ) const;
  int wait_time( void ) const;
  void handle_readable( int fd );
  void handle_writable( int fd );
  void tick( void );

  std::string port( void ) const;
  std::string key( void ) const;
  std::string describe_listeners( void ) const;
  bool has_listeners( void ) const { return !listeners.empty(); }

  static std::vector<ForwardSpec> parse_spec_list( const std::string& specs );

private:
  struct Listener;
  struct Segment;
  struct Stream;

  Side side;
  std::unique_ptr<Network::Connection> connection;
  std::vector<Listener> listeners;
  std::map<uint64_t, Stream> streams;
  std::deque<ForwardBuffers::ForwardFrame> control_frames;
  uint64_t next_stream_id;
  uint64_t next_packet_id;
  uint64_t last_hello;
  uint64_t last_ping;
  bool peer_ready;
  int pending_stdio_read_fd;
  int pending_stdio_write_fd;
  int pending_stderr_read_fd;

  void add_listener( const ForwardSpec& spec );
  void accept_listener( Listener& listener );
  void handle_network_readable( void );
  void process_frame( const ForwardBuffers::ForwardFrame& frame );
  void process_open( const ForwardBuffers::ForwardFrame& frame );
  void process_open_result( const ForwardBuffers::ForwardFrame& frame );
  void process_data( const ForwardBuffers::ForwardFrame& frame );
  void process_acklike( const ForwardBuffers::ForwardFrame& frame );
  void process_fin( const ForwardBuffers::ForwardFrame& frame );
  void process_rst( const ForwardBuffers::ForwardFrame& frame );

  void handle_stream_readable( Stream& stream );
  void handle_stream_writable( Stream& stream );
  bool finish_connect( Stream& stream, std::string& error_message );
  bool flush_local_write( Stream& stream );
  void parse_socks( Stream& stream );
  void open_direct_stream( Stream& stream, const std::string& host, uint16_t port );
  uint64_t open_special_stream( int read_fd, int write_fd, ForwardBuffers::OpenRequest::Kind kind );
  void accept_special_fds( int read_fd, int write_fd, ForwardBuffers::OpenRequest::Kind kind );
  void send_open_result( uint64_t stream_id, bool success, const std::string& error_message );
  void send_rst( uint64_t stream_id, const std::string& error_message );
  void queue_ack( Stream& stream );
  void queue_fin( Stream& stream );
  void close_stream( uint64_t stream_id );
  void close_closed_streams( void );

  bool send_frame( const ForwardBuffers::ForwardFrame& frame );
  bool send_control_frames( unsigned int& budget );
  bool send_ack_frames( unsigned int& budget );
  bool send_retransmissions( unsigned int& budget );
  bool send_new_data( unsigned int& budget );
  bool send_fin_frames( unsigned int& budget );
  uint32_t receive_window( const Stream& stream ) const;
  size_t max_data_payload( void ) const;
  size_t global_buffered( void ) const;
  bool is_network_fd( int fd ) const;
  Stream* stream_for_fd( int fd );
  Listener* listener_for_fd( int fd );

  ForwardManager( const ForwardManager& );
  ForwardManager& operator=( const ForwardManager& );
};

}

#endif
