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
  uint64_t timestamp( void );

  class MTUException {
  public:
    int MTU;
    MTUException( int s_MTU ) : MTU( s_MTU ) {};
  };

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
    uint64_t timestamp, timestamp_reply;
    string payload;
    
    Packet( uint64_t s_seq, Direction s_direction,
	    uint64_t s_timestamp, uint64_t s_timestamp_reply, string s_payload )
      : seq( s_seq ), direction( s_direction ),
	timestamp( s_timestamp ), timestamp_reply( s_timestamp_reply ), payload( s_payload )
    {}
    
    Packet( string coded_packet, Session *session );
    
    string tostring( Session *session );
  };

  class Connection {
  private:
    static const int RECEIVE_MTU = 2048;
    static const uint64_t MIN_RTO = 50; /* ms */

    int sock;
    struct sockaddr_in remote_addr;

    bool server;
    bool attached;

    int MTU;

    Base64Key key;
    Session session;

    void update_MTU( void );

    void setup( void );

    Direction direction;
    uint64_t next_seq;
    uint64_t saved_timestamp;
    uint64_t expected_receiver_seq;

    bool RTT_hit;
    double SRTT;
    double RTTVAR;

    Packet new_packet( string &s_payload );

  public:
    Connection();
    Connection( const char *key_str, const char *ip, int port );
    
    void send( string &s, bool send_timestamp = true );
    string recv( void );
    int fd( void ) { return sock; }
    int get_MTU( void ) { return MTU; }

    int port( void );
    string get_key( void ) { return key.printable_key(); }
    bool get_attached( void ) { return attached; }

    uint64_t timeout( void );
    double get_SRTT( void ) { return SRTT; }
    bool pending_timestamp( void ) { return ( saved_timestamp != uint64_t(-1) ); }
  };
}

#endif
