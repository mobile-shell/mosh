#ifndef TRANSPORT_STATE_HPP
#define TRANSPORT_STATE_HPP

namespace Network {
  template <class State>
  class TimestampedState
  {
  public:
    uint64_t timestamp;
    uint64_t num;
    State state;
    
    TimestampedState( uint64_t s_timestamp, uint64_t s_num, State &s_state )
      : timestamp( s_timestamp ), num( s_num ), state( s_state )
    {}
  };
}

#endif
