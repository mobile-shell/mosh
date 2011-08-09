#ifndef NETWORK_TRANSPORT_HPP
#define NETWORK_TRANSPORT_HPP

#include <string>
#include <signal.h>
#include <time.h>
#include <list>

#include "network.hpp"

using namespace std;

namespace Network {
  class Instruction
  {
  public:
    uint64_t old_num, new_num;
    uint64_t ack_num;

    string diff;

    Instruction( uint64_t s_old_num, uint64_t s_new_num, uint64_t s_ack_num, string s_diff )
      : old_num( s_old_num ), new_num( s_new_num ), ack_num( s_ack_num ), diff( s_diff )
    {}

    string tostring( void );
  };

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

  template <class MyState, class RemoteState>
  class Transport
  {
  private:
    static const int INITIAL_TIMEOUT = 1000; /* ms, same as TCP */
    static const int SEND_INTERVAL = 20; /* ms between frames */
    static const int HEADER_LEN = 40;

    /* helper methods for tick() */
    void update_assumed_receiver_state( void );
    void rationalize_states( void );
    void send_to_receiver( void );

    Connection connection;
    bool server;

    uint64_t timestamp( void );

    /* sender */
    MyState current_state;

    list< TimestampedState<MyState> > sent_states;
    /* first element: known, acknowledged receiver state */
    /* last element: last sent state */
    /* somewhere in the middle: the assumed state of the receiver */

    typename list< TimestampedState<MyState> >::iterator assumed_receiver_state;

    int timeout;

    /* simple receiver */
    uint64_t highest_state_received;

  public:
    Transport( MyState &initial_state );
    Transport( MyState &initial_state, const char *key_str, const char *ip, int port );

    void tick( void );

    int port( void ) { return connection.port(); }
    string get_key( void ) { return connection.get_key(); }

    MyState &get_current_state( void ) { return current_state; }
  };
}

#endif
