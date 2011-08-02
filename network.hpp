#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <stdint.h>
#include <deque>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

namespace Network {
  template <class Payload>
  class Connection {
  private:
    class Packet {
    public:
      int64_t seq;
      int64_t reference_seq;

      std::string tag;

      int64_t ack;

      Payload state;

      Packet( int64_t s_seq, int64_t s_ack, Packet *s_previous, Payload s_state );
      Packet( std::string wire );
    };

    int64_t next_seq;
    int64_t next_ack;
    int sequence_increment;

    int sock;
    struct sockaddr_in addr;

    std::deque<Packet> send_queue;
    std::deque<Packet> recv_queue;

  public:
    Connection( const char *ip, const char *port, bool server );
  };
}

#endif
