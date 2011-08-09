#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <stdint.h>
#include <deque>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

#include "crypto.hpp"

using namespace std;
using namespace Crypto;

namespace Network {
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
    string payload;
    
    Packet( uint64_t s_seq, Direction s_direction, string s_payload )
      : seq( s_seq ), direction( s_direction ), payload( s_payload )
    {}
    
    Packet( string coded_packet, Session *session );
    
    string tostring( Session *session );
  };

  class Connection {
  private:
    static const int RECEIVE_MTU = 2048;

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
    Packet new_packet( string &s_payload );

  public:
    Connection();
    Connection( const char *key_str, const char *ip, int port );
    
    void send( string &s );
    string recv( void );
    int fd( void ) { return sock; }
    int get_MTU( void ) { return MTU; }

    int port( void );
    string get_key( void ) { return key.printable_key(); }
  };
}

#endif
