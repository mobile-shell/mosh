#ifndef NETWORK_TRANSPORT_HPP
#define NETWORK_TRANSPORT_HPP

#include <google/dense_hash_map>

using google::dense_hash_map;

namespace Network {
  template <class MyState, class RemoteState>
  class Transport
  {
  private:
    Connection connection;

    typedef dense_hash_map< uint64_t, MyState > StateMapper;

    dense_hash_map< uint64_t, MyState > sent;
    dense_hash_map< uint64_t, RemoteState > received;

    uint64_t known_receiver_state;
    uint64_t assumed_receiver_state;
    uint64_t last_sent_state;

  public:
    Transport();
    Transport( const char *key_str, const char *ip, int port );

    
  };
};

#endif
