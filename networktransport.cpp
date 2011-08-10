#include <assert.h>
#include <iostream>

#include "networktransport.hpp"

using namespace Network;
using namespace std;

template <class MyState, class RemoteState>
Transport<MyState, RemoteState>::Transport( MyState &initial_state, RemoteState &initial_remote )
  : connection(),
    server( true ),
    current_state( initial_state ),
    sent_states( 1, TimestampedState<MyState>( timestamp(), 0, initial_state ) ),
    assumed_receiver_state( sent_states.begin() ),
    received_states( 1, TimestampedState<RemoteState>( timestamp(), 0, initial_remote ) )
{
  /* server */
}

template <class MyState, class RemoteState>
Transport<MyState, RemoteState>::Transport( MyState &initial_state, RemoteState &initial_remote,
					    const char *key_str, const char *ip, int port )
  : connection( key_str, ip, port ),
    server( false ),
    current_state( initial_state ),
    sent_states( 1, TimestampedState<MyState>( timestamp(), 0, initial_state ) ),
    assumed_receiver_state( sent_states.begin() ),
    received_states( 1, TimestampedState<RemoteState>( timestamp(), 0, initial_remote ) )
{
  /* client */
}

/* Returns the number of ms to wait until next (possible) event */
template <class MyState, class RemoteState>
int Transport<MyState, RemoteState>::tick( void )
{
  /* Determine if a new diff or empty ack needs to be sent */
  if ( timestamp() - sent_states.back().timestamp >= int64_t( SEND_INTERVAL ) ) {
    /* Update assumed receiver state */
    update_assumed_receiver_state();

    /* Cut out common prefix of all states */
    rationalize_states();

    /* Send diffs or ack */
    send_to_receiver();

    return SEND_INTERVAL;
  } 

  int64_t wait = sent_states.back().timestamp + SEND_INTERVAL - timestamp();
  if ( wait < 0 ) {
    wait = 0;
  }
  return wait;
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::send_to_receiver( void )
{
  /* We don't want to assume that this sequence of diffs will
     necessarily bring the receiver to the _actual_ current
     state. That requires perfect round-trip stability of the diff
     mechanism -- stronger than we need (and probably too fragile).
     Instead, we produce the full diff, unlimited by MTU, between
     the assumed receiver state and current state, and apply that
     diff to produce a target receiver state. Then we produce a
     sequence of diffs (this time limited by MTU) that bring us to
     that state. */

  if ( !connection.get_attached() ) {
    return;
  }

  MyState target_receiver_state( assumed_receiver_state->state );
  target_receiver_state.apply_string( current_state.diff_from( target_receiver_state, -1 ) );

  if ( assumed_receiver_state->state == target_receiver_state ) {
    /* send empty ack */
    if ( (!connection.pending_timestamp())
	 && (timestamp() - sent_states.back().timestamp < int64_t( ACK_INTERVAL )) ) {
      return;
    }

    Instruction inst( assumed_receiver_state->num,
		      assumed_receiver_state->num,
		      received_states.back().num,
		      sent_states.front().num,
		      "" );
    string s = inst.tostring();
    connection.send( s, false );
    assumed_receiver_state->timestamp = timestamp();

    return;
  }

  int tries = 0;
  while ( !(assumed_receiver_state->state == target_receiver_state) ) {
    if ( tries++ > 1024 ) {
      fprintf( stderr, "BUG: Convergence limit exceeded.\n" );
      exit( 1 );
    }

    Instruction inst( assumed_receiver_state->num,
		      -1,
		      received_states.back().num,
		      sent_states.front().num,
		      current_state.diff_from( assumed_receiver_state->state,
					       connection.get_MTU() - HEADER_LEN ) );
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

    /* Reusing state numbers is only for intermediate states */
    /* If this is the final diff in a sequence, make sure it does get the highest
       state number (even if we've retread to previously-seen ground ) */
    /* This will force the client to update to this state */
    if ( new_state == target_receiver_state ) {
      if ( new_state == sent_states.back().state ) {
	previously_sent = sent_states.end();
	previously_sent--;
      } else {
	previously_sent = sent_states.end();
      }
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
      fprintf( stderr, "Sent instruction (timeout %d, queues %d/%d) from %d => %d (terminal %d): %s\r\n", connection.timeout(), (int)sent_states.size(), (int)received_states.size(), int(inst.old_num), int(inst.new_num), int(sent_states.back().num), inst.diff.c_str() );
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

    if ( int(now - i->timestamp) < connection.timeout() ) {
      assumed_receiver_state = i;
    }
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::rationalize_states( void )
{
  MyState * const known_receiver_state = &sent_states.front().state;

  current_state.subtract( known_receiver_state );

  for ( typename list< TimestampedState<MyState> >::reverse_iterator i = sent_states.rbegin();
	i != sent_states.rend();
	i++ ) {
    i->state.subtract( known_receiver_state );
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::recv( void )
{
  string s( connection.recv() );
  Instruction inst( s );
  
  process_acknowledgment_through( inst.ack_num );

  /* first, make sure we don't already have the new state */
  for ( typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
	i != received_states.end();
	i++ ) {
    if ( inst.new_num == i->num ) {
      i->timestamp = timestamp();
      return;
    }
  }

  /* now, make sure we do have the old state */
  bool found = 0;
  typename list< TimestampedState<RemoteState> >::iterator reference_state = received_states.begin();
  while ( reference_state != received_states.end() ) {
    if ( inst.old_num == reference_state->num ) {
      found = true;
      break;
    }
    reference_state++;
  }

  if ( !found ) {
    //    fprintf( stderr, "Ignoring out-of-order packet. Reference state %d has been discarded or hasn't yet been received.\n", int(inst.old_num) );
    return;
  }

  /* apply diff to reference state */
  TimestampedState<RemoteState> new_state = *reference_state;
  new_state.timestamp = timestamp();
  new_state.num = inst.new_num;
  new_state.state.apply_string( inst.diff );

  if ( new_state.num > received_states.back().num ) {
    process_throwaway_until( inst.throwaway_num );
  }

  /* Insert new state in sorted place */
  for ( typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
	i != received_states.end();
	i++ ) {
    if ( i->num > new_state.num ) {
      received_states.insert( i, new_state );
      return;
    }
  }
  received_states.push_back( new_state );
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::process_acknowledgment_through( uint64_t ack_num )
{
  typename list< TimestampedState<MyState> >::iterator i = sent_states.begin();
  while ( i != sent_states.end() ) {
    typename list< TimestampedState<MyState> >::iterator inext = i;
    inext++;
    if ( i->num < ack_num ) {
      sent_states.erase( i );
    }
    i = inext;
  }

  assert( sent_states.size() > 0 );
  assert( sent_states.front().num == ack_num );
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::process_throwaway_until( uint64_t throwaway_num )
{
  typename list< TimestampedState<MyState> >::iterator i = received_states.begin();
  while ( i != received_states.end() ) {
    typename list< TimestampedState<MyState> >::iterator inext = i;
    inext++;
    if ( i->num < throwaway_num ) {
      sent_states.erase( i );
    }
    i = inext;
  }

  assert( received_states.size() > 0 );
}
