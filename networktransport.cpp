#include <assert.h>

#include "networktransport.hpp"

using namespace Network;
using namespace std;

template <class MyState, class RemoteState>
uint64_t Transport<MyState, RemoteState>::timestamp( void )
{
  struct timespec tp;

  if ( clock_gettime( CLOCK_MONOTONIC, &tp ) < 0 ) {
    throw NetworkException( "clock_gettime", errno );
  }

  uint64_t millis = tp.tv_nsec / 1000000;
  millis += uint64_t( tp.tv_sec ) * 1000000;

  return millis;
}

template <class MyState, class RemoteState>
Transport<MyState, RemoteState>::Transport( MyState &initial_state )
  : connection(),
    server( true ),
    current_state( initial_state ),
    sent_states( 1, TimestampedState<MyState>( timestamp(), 0, initial_state ) ),
    assumed_receiver_state( sent_states.begin() ),
    timeout( INITIAL_TIMEOUT ),
    highest_state_received( 0 )
{
  /* server */
}

template <class MyState, class RemoteState>
Transport<MyState, RemoteState>::Transport( MyState &initial_state,
					    const char *key_str, const char *ip, int port )
  : connection( key_str, ip, port ),
    server( false ),
    current_state( initial_state ),
    sent_states( 1, TimestampedState<MyState>( timestamp(), 0, initial_state ) ),
    assumed_receiver_state( sent_states.begin() ),
    timeout( INITIAL_TIMEOUT ),
    highest_state_received( 0 )
{
  /* client */
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::new_state( MyState &s )
{
  current_state = s;

  tick();
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::tick( void )
{
  /* Update assumed receiver state */
  update_assumed_receiver_state();

  /* Cut out common prefix of all states */
  rationalize_states();

  /* Determine if a new diff or empty ack needs to be sent */
  if ( timestamp() - sent_states.back().timestamp >= int64_t( SEND_INTERVAL ) ) {
    /* Send diffs or ack */
    send_to_receiver();
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::send_to_receiver( void )
{
  if ( assumed_receiver_state->state == sent_states.back().state ) {
    /* send empty ack */
    Instruction inst( assumed_receiver_state->num,
		      assumed_receiver_state->num,
		      "",
		      highest_state_received );
    string s = inst.tostring();
    connection.send( s );
    sent_states.back().timestamp = timestamp();
    return;
  }

  /* Otherwise, send sequence of diffs between assumed receiver state and current state */

  /* We don't want to assume that this sequence of diffs will
     necessarily bring the receiver to the _actual_ current
     state. That requires perfect round-trip stability of the diff
     mechanism -- stronger than we need (and probably too fragile).
     Instead, we produce the full diff, unlimited by MTU, between
     the assumed receiver state and current state, and apply that
     diff to produce a target receiver state. Then we produce a
     sequence of diffs (this time limited by MTU) that bring us to
     that state. */

  MyState target_receiver_state( assumed_receiver_state->state );
  target_receiver_state.apply_string( current_state.diff_from( target_receiver_state, -1 ) );

  while ( !(assumed_receiver_state->state == target_receiver_state) ) {
    Instruction inst( assumed_receiver_state->num, -1,
		      current_state.diff_from( assumed_receiver_state->state,
					       connection.get_MTU() - HEADER_LEN ),
		      highest_state_received );
    MyState new_state = assumed_receiver_state->state;
    new_state.apply_string( inst.diff );

    /* Find the right "new_num" for this instruction. */
    /* Has this state previously been sent? */
    /* should replace with hash table if this becomes demanding */
    typename list< TimestampedState<MyState> >::iterator previously_sent = sent_states.begin();
    while ( ( previously_sent != sent_states.end() )
	    && ( !(previously_sent->state == new_state) ) ) {
      previously_sent++;
    }

    if ( previously_sent == sent_states.end() ) { /* not previously sent */
      inst.new_num = sent_states.back().num + 1;
      sent_states.push_back( TimestampedState<MyState>( timestamp(), inst.new_num, new_state ) );
      previously_sent = sent_states.end();
      previously_sent--;
    } else {
      inst.new_num = previously_sent->num;
      previously_sent->timestamp = timestamp();
    }

    /* send instruction */
    /* XXX what about MTU problem? */
    string s = inst.tostring();

    try {
      connection.send( s );
    } catch ( MTUException m ) {
      continue;
    }

    /* successfully sent, probably */
    /* ("probably" because the FIRST size-exceeded datagram doesn't get an error) */
    assumed_receiver_state = previously_sent;
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::update_assumed_receiver_state( void )
{
  uint64_t now = timestamp();

  /* start from what is known and give benefit of the doubt to unacknowledged states
     transmitted recently enough ago */
  assumed_receiver_state = sent_states.begin();

  for ( typename list< TimestampedState<MyState> >::iterator i = sent_states.begin();
	i != sent_states.end();
	i++ ) {
    assert( now >= i->timestamp );

    if ( now - i->timestamp < int64_t(timeout) ) {
      assumed_receiver_state = i;
    }
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::rationalize_states( void )
{
  MyState * const known_receiver_state = &sent_states.front().state;

  for ( typename list< TimestampedState<MyState> >::iterator i = sent_states.begin();
	i != sent_states.end();
	i++ ) {
    i->state.subtract( known_receiver_state );
  }
}
