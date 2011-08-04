#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <stdint.h>
#include <deque>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

using namespace std;

namespace Network {
  enum Direction {
    TO_SERVER = 0,
    TO_CLIENT = 1
  };

  template <class Payload>
  class Flow {
  public:
    class Packet {
    private:
      class DecodingCache
      {
      public:
	Direction direction;
	uint64_t seq;
	string payload_string;

	DecodingCache( string coded_packet );
	DecodingCache() : direction( TO_CLIENT ), seq( -1 ), payload_string() {}
      };

      DecodingCache decoding_cache;

    public:
      uint64_t seq;
      Direction direction;
      Payload payload;
      
      Packet( uint64_t s_seq, Direction s_direction, Payload s_payload )
	: decoding_cache(), seq( s_seq ), direction( s_direction ), payload( s_payload )
      {}
      
      Packet( string coded_packet );
      
      string tostring( void );
    };

    uint64_t next_seq;
    const Direction direction;

    Flow( Direction s_direction )
      : next_seq( 0 ), direction( s_direction )
    {}

    Packet new_packet( Payload &s_payload );
  };

  template <class Outgoing, class Incoming>
  class Connection {
  private:
    static const int RECEIVE_MTU = 2048;

    Flow<Outgoing> flow;

    int sock;
    struct sockaddr_in remote_addr;

    bool server;
    bool attached;

    int MTU;

    void update_MTU( void );

  public:
    Connection( bool s_server );
    
    void client_connect( const char *ip, int port );
    bool send( Outgoing &s );
    Incoming recv( void );
    int fd( void ) { return sock; }
    int port( void );
  };
}

#endif
