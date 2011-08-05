#ifndef NETWORK_TRANSPORT_HPP
#define NETWORK_TRANSPORT_HPP

#include <google/dense_hash_map>

using google::dense_hash_map;

namespace Network {
  template <class MyState, class RemoteState>
  class Transport
  {
  private:
    Connection<typename MyState::Conveyance, typename RemoteState::Conveyance> connection;

    uint64_t last_acknowledged_state;
    uint64_t assumed_receiver_state;
    uint64_t last_sent_state;

  public:
    Transport();
    Transport( const char *key_str, const char *ip, int port );

    
  };
};

#endif
