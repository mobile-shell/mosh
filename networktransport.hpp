#ifndef NETWORK_TRANSPORT_HPP
#define NETWORK_TRANSPORT_HPP

#include <string>
#include <signal.h>
#include <time.h>
#include <list>
#include <vector>

#include "network.hpp"

using namespace std;

namespace Network {
  class Instruction
  {
  private:
    static const size_t inst_header_len = 4 * sizeof( uint64_t ) + 1 * sizeof( uint16_t );

  public:
    uint64_t old_num, new_num;
    uint64_t ack_num;
    uint64_t throwaway_num;
    uint16_t fragment_num;
    bool final;

    string diff;

    Instruction() : old_num( -1 ), new_num( -1 ), ack_num( -1 ), throwaway_num( -1 ), fragment_num( -1 ), final( false ),
		    diff( "" )
    {}

    Instruction( uint64_t s_old_num, uint64_t s_new_num,
		 uint64_t s_ack_num, uint64_t s_throwaway_num, uint16_t s_fragment_num, bool s_final,
		 string s_diff )
      : old_num( s_old_num ), new_num( s_new_num ),
	ack_num( s_ack_num ), throwaway_num( s_throwaway_num ), fragment_num( s_fragment_num ), final( s_final ),
	diff( s_diff )
    {}

    Instruction( string &x );

    string tostring( void );

    bool operator==( const Instruction &x );
  };

  class FragmentAssembly
  {
  private:
    vector<Instruction> fragments;
    Instruction current_template;
    int fragments_arrived, fragments_total;

  public:
    FragmentAssembly() : fragments(), current_template(), fragments_arrived( 0 ), fragments_total( -1 ) {}
    static bool same_template( Instruction &a, Instruction &b );
    bool add_fragment( Instruction &inst );
    Instruction get_assembly( void );
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
    void send_in_fragments( string diff, uint64_t new_num, bool send_timestamp = true );

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
    RemoteState last_receiver_state; /* the state we were in when user last queried state */

    FragmentAssembly fragments;

    bool verbose;

  public:
    Transport( MyState &initial_state, RemoteState &initial_remote );
    Transport( MyState &initial_state, RemoteState &initial_remote,
	       const char *key_str, const char *ip, int port );

    /* Send data or an ack if necessary.
       Returns the number of ms to wait until next event. */
    int tick( void );

    /* Blocks waiting for a packet. */
    void recv( void );

    int port( void ) { return connection.port(); }
    string get_key( void ) { return connection.get_key(); }

    MyState &get_current_state( void ) { return current_state; }
    void set_current_state( const MyState &x ) { current_state = x; }

    string get_remote_diff( void );

    typename list< TimestampedState<RemoteState > >::iterator begin( void ) { return received_states.begin(); }
    typename list< TimestampedState<RemoteState > >::iterator end( void ) { return received_states.end(); }

    uint64_t get_remote_state_num( void ) { return received_states.back().num; }

    int fd( void ) { return connection.fd(); }

    void set_verbose( void ) { verbose = true; }
  };
}

#endif
