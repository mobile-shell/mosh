#ifndef NETWORK_TRANSPORT_HPP
#define NETWORK_TRANSPORT_HPP

#include <string>
#include <signal.h>
#include <time.h>
#include <list>
#include <vector>

#include "network.hpp"
#include "transportsender.hpp"
#include "transportfragment.hpp"

using namespace std;

namespace Network {
  template <class MyState, class RemoteState>
  class Transport
  {
  private:
    /* the underlying, encrypted network connection */
    Connection connection;

    /* sender side */
    TransportSender<MyState> sender;

    /* helper methods for recv() */
    void process_throwaway_until( uint64_t throwaway_num );

    /* simple receiver */
    list< TimestampedState<RemoteState> > received_states;
    RemoteState last_receiver_state; /* the state we were in when user last queried state */

    FragmentAssembly fragments;

  public:
    Transport( MyState &initial_state, RemoteState &initial_remote );
    Transport( MyState &initial_state, RemoteState &initial_remote,
	       const char *key_str, const char *ip, int port );

    /* Send data or an ack if necessary. */
    void tick( void ) { sender.tick(); }

    /* Returns the number of ms to wait until next possible event. */
    int wait_time( void ) { return sender.wait_time(); }

    /* Blocks waiting for a packet. */
    void recv( void );

    int port( void ) { return connection.port(); }
    string get_key( void ) { return connection.get_key(); }

    MyState &get_current_state( void ) { return sender.get_current_state(); }
    void set_current_state( const MyState &x ) { sender.set_current_state( x ); }

    string get_remote_diff( void );

    typename list< TimestampedState<RemoteState > >::iterator begin( void ) { return received_states.begin(); }
    typename list< TimestampedState<RemoteState > >::iterator end( void ) { return received_states.end(); }

    uint64_t get_remote_state_num( void ) { return received_states.back().num; }

    int fd( void ) { return connection.fd(); }

    void set_verbose( void ) { sender.set_verbose(); }
  };
}

#endif
