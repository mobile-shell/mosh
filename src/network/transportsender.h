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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
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
  /* timing parameters */
  const int SEND_INTERVAL_MIN = 20; /* ms between frames */
  const int SEND_INTERVAL_MAX = 250; /* ms between frames */
  const int ACK_INTERVAL = 3000; /* ms between empty acks */
  const int ACK_DELAY = 100; /* ms before delayed ack */
  const int SHUTDOWN_RETRIES = 16; /* number of shutdown packets to send before giving up */
  const int ACTIVE_RETRY_TIMEOUT = 10000; /* attempt to resend at frame rate */

  template <class MyState>
  class TransportSender
  {
  private:
    /* helper methods for tick() */
    void update_assumed_receiver_state( void );
    void attempt_prospective_resend_optimization( string &proposed_diff );
    void rationalize_states( void );
    void send_to_receiver( const string & diff );
    void send_empty_ack( void );
    void send_in_fragments( const string & diff, uint64_t new_num );
    void add_sent_state( uint64_t the_timestamp, uint64_t num, MyState &state );

    /* state of sender */
    Connection *connection;

    MyState current_state;

    typedef list< TimestampedState<MyState> > sent_states_type;
    sent_states_type sent_states;
    /* first element: known, acknowledged receiver state */
    /* last element: last sent state */

    /* somewhere in the middle: the assumed state of the receiver */
    typename sent_states_type::iterator assumed_receiver_state;

    /* for fragment creation */
    Fragmenter fragmenter;

    /* timing state */
    uint64_t next_ack_time;
    uint64_t next_send_time;

    void calculate_timers( void );

    unsigned int verbose;
    bool shutdown_in_progress;
    int shutdown_tries;
    uint64_t shutdown_start;

    /* information about receiver state */
    uint64_t ack_num;
    bool pending_data_ack;

    unsigned int SEND_MINDELAY; /* ms to collect all input */

    uint64_t last_heard; /* last time received new state */

    /* chaff to disguise instruction length */
    PRNG prng;
    const string make_chaff( void );

    uint64_t mindelay_clock; /* time of first pending change to current state */

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
    void start_shutdown( void ) { if ( !shutdown_in_progress ) { shutdown_start = timestamp(); shutdown_in_progress = true; } }

    /* Misc. getters and setters */
    /* Cannot modify current_state while shutdown in progress */
    MyState &get_current_state( void ) { assert( !shutdown_in_progress ); return current_state; }
    void set_current_state( const MyState &x )
    {
      assert( !shutdown_in_progress );
      current_state = x;
      current_state.reset_input();
    }
    void set_verbose( unsigned int s_verbose ) { verbose = s_verbose; }

    bool get_shutdown_in_progress( void ) const { return shutdown_in_progress; }
    bool get_shutdown_acknowledged( void ) const { return sent_states.front().num == uint64_t(-1); }
    bool get_counterparty_shutdown_acknowledged( void ) const { return fragmenter.last_ack_sent() == uint64_t(-1); }
    uint64_t get_sent_state_acked_timestamp( void ) const { return sent_states.front().timestamp; }
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
