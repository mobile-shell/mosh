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
    uint64_t throwaway_num;

    string diff;

    Instruction( uint64_t s_old_num, uint64_t s_new_num,
		 uint64_t s_ack_num, uint64_t s_throwaway_num, string s_diff )
      : old_num( s_old_num ), new_num( s_new_num ),
	ack_num( s_ack_num ), throwaway_num( s_throwaway_num ), diff( s_diff )
    {}

    Instruction( string &x );

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
    static const int SEND_INTERVAL = 50; /* ms between frames */
    static const int ACK_INTERVAL = 1000; /* ms between empty acks */
    static const int HEADER_LEN = 120;

    /* helper methods for tick() */
    void update_assumed_receiver_state( void );
    void rationalize_states( void );
    void send_to_receiver( void );

    /* helper methods for recv() */
    void process_acknowledgment_through( uint64_t ack_num );
    void process_throwaway_until( uint64_t throwaway_num );

    Connection connection;
    bool server;

    /* sender */
    MyState current_state;

    list< TimestampedState<MyState> > sent_states;
    /* first element: known, acknowledged receiver state */
    /* last element: last sent state */
    /* somewhere in the middle: the assumed state of the receiver */

    typename list< TimestampedState<MyState> >::iterator assumed_receiver_state;

    /* simple receiver */
    list< TimestampedState<RemoteState> > received_states;

  public:
    Transport( MyState &initial_state, RemoteState &initial_remote );
    Transport( MyState &initial_state, RemoteState &initial_remote,
	       const char *key_str, const char *ip, int port );

    int tick( void );

    void recv( void );

    int port( void ) { return connection.port(); }
    string get_key( void ) { return connection.get_key(); }

    MyState &get_current_state( void ) { return current_state; }
    RemoteState &get_remote_state( void ) { return received_states.back().state; }
    uint64_t get_remote_state_num( void ) { return received_states.back().num; }

    int fd( void ) { return connection.fd(); }
  };
}

#endif
