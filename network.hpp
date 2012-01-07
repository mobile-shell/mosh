#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <stdint.h>
#include <deque>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <math.h>

#include "crypto.hpp"

using namespace std;
using namespace Crypto;

namespace Network {
  static const unsigned int MOSH_PROTOCOL_VERSION = 1;

  uint64_t timestamp( void );
  uint16_t timestamp16( void );
  uint16_t timestamp_diff( uint16_t tsnew, uint16_t tsold );

  class NetworkException {
  public:
    string function;
    int the_errno;
    NetworkException( string s_function, int s_errno ) : function( s_function ), the_errno( s_errno ) {}
  };

  enum Direction {
    TO_SERVER = 0,
    TO_CLIENT = 1
  };

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
    static const int RECEIVE_MTU = 2048;
    static const int SEND_MTU = 1400;
    static const uint64_t MIN_RTO = 50; /* ms */
    static const uint64_t MAX_RTO = 1000; /* ms */

    int sock;
    struct sockaddr_in remote_addr;

    bool server;
    bool attached;

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

    Packet new_packet( string &s_payload );

    void update_MTU( void );

  public:
    Connection();
    Connection( const char *key_str, const char *ip, int port );
    
    void send( string s );
    string recv( void );
    int fd( void ) const { return sock; }
    int get_MTU( void ) const { return MTU; }

    int port( void ) const;
    string get_key( void ) const { return key.printable_key(); }
    bool get_attached( void ) const { return attached; }

    uint64_t timeout( void ) const;
    double get_SRTT( void ) const { return SRTT; }
  };
}

#endif
