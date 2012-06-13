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

#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <stdint.h>
#include <deque>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <math.h>

#include "crypto.h"

using namespace Crypto;

namespace Network {
  static const unsigned int MOSH_PROTOCOL_VERSION = 2; /* bumped for echo-ack */

  uint64_t timestamp( void );
  uint16_t timestamp16( void );
  uint16_t timestamp_diff( uint16_t tsnew, uint16_t tsold );

  class NetworkException {
  public:
    string function;
    int the_errno;
    NetworkException( string s_function, int s_errno ) : function( s_function ), the_errno( s_errno ) {}
    NetworkException() : function( "<none>" ), the_errno( 0 ) {}
  };

  enum Direction {
    TO_SERVER = 0,
    TO_CLIENT = 1
  };

  void make_firewall_hole(int signum);

  class Packet {
  public:
    uint64_t seq;
    Direction direction;
    uint16_t timestamp, timestamp_reply;
    string payload;
    
    Packet( uint64_t s_seq, Direction s_direction,
	    uint16_t s_timestamp, uint16_t s_timestamp_reply, string s_payload )
      : seq( s_seq ), direction( s_direction ),
	timestamp( s_timestamp ), timestamp_reply( s_timestamp_reply ), payload( s_payload )
    {}
    
    Packet( string coded_packet, Session *session );
    
    string tostring( Session *session );
  };

  class Connection {
  private:
    static const int SEND_MTU = 1400;
    static const uint64_t MIN_RTO = 50; /* ms */
    static const uint64_t MAX_RTO = 1000; /* ms */

    static const int PORT_RANGE_LOW  = 60001;
    static const int PORT_RANGE_HIGH = 60999;
    
    static bool try_bind( int socket, uint32_t s_addr, int port );

    int sock;
    bool has_remote_addr;
    struct sockaddr_in remote_addr;

    bool server;

    int MTU;

    Base64Key key;
    Session session;

    void setup( void );

    Direction direction;
    uint64_t next_seq;
    uint16_t saved_timestamp;
    uint64_t saved_timestamp_received_at;
    uint64_t expected_receiver_seq;

    bool RTT_hit;
    double SRTT;
    double RTTVAR;

    /* Exception from send(), to be delivered if the frontend asks for it,
       without altering control flow. */
    bool have_send_exception;
    NetworkException send_exception;

    Packet new_packet( string &s_payload );

  public:
    Connection( const char *desired_ip, const char *desired_port ); /* server */
    Connection( const char *key_str, const char *ip, int port, int client_port ); /* client */
    ~Connection();

    void send( string s );
    string recv( void );
    int fd( void ) const { return sock; }
    int get_MTU( void ) const { return MTU; }

    int port( void ) const;
    string get_key( void ) const { return key.printable_key(); }
    bool get_has_remote_addr( void ) const { return has_remote_addr; }

    uint64_t timeout( void ) const;
    double get_SRTT( void ) const { return SRTT; }
    
    const struct in_addr & get_remote_ip( void ) const { return remote_addr.sin_addr; }

    const NetworkException *get_send_exception( void ) const
    {
      return have_send_exception ? &send_exception : NULL;
    }
  };
}

#endif
