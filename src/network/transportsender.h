/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef TRANSPORT_SENDER_HPP
#define TRANSPORT_SENDER_HPP

#include <string>
#include <list>

#include "network.h"
#include "transportinstruction.pb.h"
#include "transportstate.h"
#include "transportfragment.h"
#include "prng.h"

using std::list;
using std::pair;
using namespace TransportBuffers;

namespace Network {
  template <class MyState>
  class TransportSender
  {
  private:
    /* timing parameters */
    static const int SEND_INTERVAL_MIN = 20; /* ms between frames */
    static const int SEND_INTERVAL_MAX = 250; /* ms between frames */
    static const int ACK_INTERVAL = 3000; /* ms between empty acks */
    static const int ACK_DELAY = 100; /* ms before delayed ack */
    static const int SHUTDOWN_RETRIES = 3; /* number of shutdown packets to send before giving up */
    static const int ACTIVE_RETRY_TIMEOUT = 10000; /* attempt to resend at frame rate */

    /* helper methods for tick() */
    void update_assumed_receiver_state( void );
    void rationalize_states( void );
    void send_to_receiver( string diff );
    void send_empty_ack( void );
    void send_in_fragments( string diff, uint64_t new_num );
    void add_sent_state( uint64_t the_timestamp, uint64_t num, MyState &state );

    /* state of sender */
    Connection *connection;

    MyState current_state;

    typedef list< TimestampedState<MyState> > sent_states_t;
    sent_states_t sent_states;
    /* first element: known, acknowledged receiver state */
    /* last element: last sent state */

    /* somewhere in the middle: the assumed state of the receiver */
    typename sent_states_t::iterator assumed_receiver_state;

    /* for fragment creation */
    Fragmenter fragmenter;

    /* timing state */
    uint64_t next_ack_time;
    uint64_t next_send_time;

    void calculate_timers( void );

    bool verbose;
    bool shutdown_in_progress;
    int shutdown_tries;

    /* information about receiver state */
    uint64_t ack_num;
    bool pending_data_ack;

    unsigned int SEND_MINDELAY; /* ms to collect all input */

    uint64_t last_heard; /* last time received new state */

    /* chaff to disguise instruction length */
    PRNG prng;
    const string make_chaff( void );

  public:
    /* constructor */
    TransportSender( Connection *s_connection, MyState &initial_state );

    /* Send data or an ack if necessary */
    void tick( void );

    /* Returns the number of ms to wait until next possible event. */
    int wait_time( void );

    /* Executed upon receipt of ack */
    void process_acknowledgment_through( uint64_t ack_num );

    /* Executed upon entry to new receiver state */
    void set_ack_num( uint64_t s_ack_num );

    /* Accelerate reply ack */
    void set_data_ack( void ) { pending_data_ack = true; }

    /* Received something */
    void remote_heard( uint64_t ts ) { last_heard = ts; }

    /* Starts shutdown sequence */
    void start_shutdown( void ) { shutdown_in_progress = true; }

    /* Misc. getters and setters */
    /* Cannot modify current_state while shutdown in progress */
    MyState &get_current_state( void ) { assert( !shutdown_in_progress ); return current_state; }
    void set_current_state( const MyState &x ) { assert( !shutdown_in_progress ); current_state = x; }
    void set_verbose( void ) { verbose = true; }

    bool get_shutdown_in_progress( void ) const { return shutdown_in_progress; }
    bool get_shutdown_acknowledged( void ) const { return sent_states.front().num == uint64_t(-1); }
    bool get_counterparty_shutdown_acknowledged( void ) const { return fragmenter.last_ack_sent() == uint64_t(-1); }
    uint64_t get_sent_state_acked( void ) const { return sent_states.front().num; }
    uint64_t get_sent_state_last( void ) const { return sent_states.back().num; }

    bool shutdown_ack_timed_out( void ) const;

    void set_send_delay( int new_delay ) { SEND_MINDELAY = new_delay; }

    unsigned int send_interval( void ) const;

    /* nonexistent methods to satisfy -Weffc++ */
    TransportSender( const TransportSender &x );
    TransportSender & operator=( const TransportSender &x );
  };
}

#endif
