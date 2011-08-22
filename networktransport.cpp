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
    received_states( 1, TimestampedState<RemoteState>( timestamp(), 0, initial_remote ) ),
    last_receiver_state( initial_remote ),
    fragments(),
    verbose( false ),
    next_ack_time( timestamp() ),
    next_send_time( timestamp() )
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
    received_states( 1, TimestampedState<RemoteState>( timestamp(), 0, initial_remote ) ),
    last_receiver_state( initial_remote ),
    fragments(),
    verbose( false ),
    next_ack_time( timestamp() ),
    next_send_time( timestamp() )
{
  /* client */
}

/* Try to send roughly two frames per RTT, bounded by limits on frame rate */
template <class MyState, class RemoteState>
unsigned int Transport<MyState, RemoteState>::send_interval( void )
{
  int SEND_INTERVAL = lrint( ceil( (connection.get_SRTT() - ACK_DELAY) / 2.0 ) );
  if ( SEND_INTERVAL < SEND_INTERVAL_MIN ) {
    SEND_INTERVAL = SEND_INTERVAL_MIN;
  } else if ( SEND_INTERVAL > SEND_INTERVAL_MAX ) {
    SEND_INTERVAL = SEND_INTERVAL_MAX;
  }

  return SEND_INTERVAL;
}

/* How many ms can the caller wait before we will have an event (empty ack or next frame)? */
template <class MyState, class RemoteState>
int Transport<MyState, RemoteState>::wait_time( void )
{
  if ( connection.pending_timestamp() && ( next_ack_time > timestamp() + ACK_DELAY ) ) {
    next_ack_time = timestamp() + ACK_DELAY;
  }

  uint64_t next_wakeup = next_ack_time;

  if ( !(current_state == sent_states.back().state) ) { /* pending data to send */
    if ( next_send_time > timestamp() + SEND_MINDELAY ) {
      next_send_time = timestamp() + SEND_MINDELAY;
    }

    if ( next_send_time < sent_states.back().timestamp + send_interval() ) {
      next_send_time = sent_states.back().timestamp + send_interval();
    }

    if ( next_send_time < next_wakeup ) {
      next_wakeup = next_send_time;
    }
  }

  if ( !connection.get_attached() ) {
    return -1;
  }

  if ( next_wakeup > timestamp() ) {
    return next_wakeup - timestamp();
  } else {
    return 0;
  }
}

/* Send data or an empty ack if necessary */
template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::tick( void )
{
  wait_time();

  if ( !connection.get_attached() ) {
    return;
  }

  if ( (timestamp() < next_ack_time)
       && (timestamp() < next_send_time) ) {
    return;
  }

  /* Determine if a new diff or empty ack needs to be sent */
  /* Update assumed receiver state */
  update_assumed_receiver_state();
    
  /* Cut out common prefix of all states */
  rationalize_states();

  string diff = current_state.diff_from( assumed_receiver_state->state );

  if ( diff.empty() && (timestamp() >= next_ack_time) ) {
    /*
    if ( verbose )
      fprintf( stderr, "Sending empty ack (ts=%d, next_send=%d, next_ack=%d)\n",
	       (int)timestamp() % 100000,
	       (int)next_send_time % 100000,
	       (int)next_ack_time % 100000 );
    */
    send_empty_ack();
    return;
  }

  if ( !diff.empty() && ( (timestamp() >= next_send_time)
			  || (timestamp() >= next_ack_time) ) ) {
    /* Send diffs or ack */
    /*
    if ( verbose )
      fprintf( stderr, "Sending packet (ts=%d, next_send=%d, next_ack=%d)\n",
	       (int)timestamp() % 100000,
	       (int)next_send_time % 100000,
	       (int)next_ack_time % 100000 );
    */
    send_to_receiver( diff );
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::send_empty_ack( void )
{
  assert ( timestamp() >= next_ack_time );

  uint64_t new_num = sent_states.back().num + 1;

  send_in_fragments( "", new_num, false );
  sent_states.push_back( TimestampedState<MyState>( sent_states.back().timestamp, new_num, current_state ) );
  next_ack_time = timestamp() + ACK_INTERVAL;
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::send_to_receiver( string diff )
{
  uint64_t new_num;
  if ( current_state == sent_states.back().state ) { /* previously sent */
    new_num = sent_states.back().num;
  } else { /* new state */
    new_num = sent_states.back().num + 1;
  }

  bool done = false;
  int MTU_tries = 0;
  while ( !done ) {
    MTU_tries++;

    if ( MTU_tries > 20 ) {
      fprintf( stderr, "Error, could not send fragments after 20 tries (MTU = %d).\n",
	       connection.get_MTU() );
    }

    try {
      send_in_fragments( diff, new_num );
      done = true;
    } catch ( MTUException m ) {
      fprintf( stderr, "Caught Path MTU exception, MTU now = %d\n", connection.get_MTU() );
      done = false;
    }

    if ( new_num == sent_states.back().num ) {
      sent_states.back().timestamp = timestamp();
    } else {
      sent_states.push_back( TimestampedState<MyState>( timestamp(), new_num, current_state ) );
    }

    new_num++;
  }

  /* successfully sent, probably */
  /* ("probably" because the FIRST size-exceeded datagram doesn't get an error) */
  assumed_receiver_state = sent_states.end();
  assumed_receiver_state--;
  next_ack_time = timestamp() + ACK_INTERVAL;
  next_send_time = -1;
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::update_assumed_receiver_state( void )
{
  uint64_t now = timestamp();

  /* start from what is known and give benefit of the doubt to unacknowledged states
     transmitted recently enough ago */
  assumed_receiver_state = sent_states.begin();

  typename list< TimestampedState<MyState> >::iterator i = sent_states.begin();
  i++;

  while ( i != sent_states.end() ) {
    assert( now >= i->timestamp );

    if ( int(now - i->timestamp) < connection.timeout() + ACK_DELAY ) {
      assumed_receiver_state = i;
    } else {
      return;
    }

    i++;
  }
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::rationalize_states( void )
{
  const MyState * known_receiver_state = &sent_states.front().state;

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
  Instruction frag( s );

  if ( fragments.add_fragment( frag ) ) { /* complete packet */
    Instruction inst = fragments.get_assembly();

    process_acknowledgment_through( inst.ack_num );
    
    /* first, make sure we don't already have the new state */
    for ( typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
	  i != received_states.end();
	  i++ ) {
      if ( inst.new_num == i->num ) {
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
      return; /* this is security-sensitive and part of how we enforce idempotency */
    }
    
    /* apply diff to reference state */
    TimestampedState<RemoteState> new_state = *reference_state;
    new_state.timestamp = timestamp();
    new_state.num = inst.new_num;
    new_state.state.apply_string( inst.diff );
    
    process_throwaway_until( inst.throwaway_num );

    /* Insert new state in sorted place */
    for ( typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
	  i != received_states.end();
	  i++ ) {
      if ( i->num > new_state.num ) {
	received_states.insert( i, new_state );
	return;
      }
    }
    /*
    if ( verbose )
      fprintf( stderr, "[%d] Received state %d [ack %d]\n",
	       (int)timestamp() % 100000, (int)new_state.num, (int)inst.ack_num );
    */
    received_states.push_back( new_state );
  }
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
  //  assert( sent_states.front().num == ack_num );
}

/* The sender uses throwaway_num to tell us the earliest received state that we need to keep around */
template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::process_throwaway_until( uint64_t throwaway_num )
{
  typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
  while ( i != received_states.end() ) {
    typename list< TimestampedState<RemoteState> >::iterator inext = i;
    inext++;
    if ( i->num < throwaway_num ) {
      received_states.erase( i );
    }
    i = inext;
  }

  assert( received_states.size() > 0 );
}

template <class MyState, class RemoteState>
string Transport<MyState, RemoteState>::get_remote_diff( void )
{
  /* find diff between last receiver state and current remote state, then rationalize states */

  string ret( received_states.back().state.diff_from( last_receiver_state ) );

  const RemoteState *oldest_receiver_state = &received_states.front().state;

  for ( typename list< TimestampedState<RemoteState> >::reverse_iterator i = received_states.rbegin();
	i != received_states.rend();
	i++ ) {
    i->state.subtract( oldest_receiver_state );
  }  

  last_receiver_state = received_states.back().state;

  return ret;
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::send_in_fragments( string diff, uint64_t new_num, bool send_timestamp )
{
  uint16_t fragment_num = 0;

  do {
    string this_fragment;
    
    assert( fragment_num <= 32767 );

    bool final = false;

    if ( int( diff.size() + HEADER_LEN ) > connection.get_MTU() ) {
      this_fragment = string( diff.begin(), diff.begin() + connection.get_MTU() - HEADER_LEN );
      diff = string( diff.begin() + connection.get_MTU() - HEADER_LEN, diff.end() );
    } else {
      this_fragment = diff;
      diff.clear();
      final = true;
    }

    Instruction inst( assumed_receiver_state->num,
		      new_num,
		      received_states.back().num,
		      sent_states.front().num,
		      fragment_num++, final,
		      this_fragment );
    string s = inst.tostring();

    connection.send( s, send_timestamp );

    if ( verbose ) {
      fprintf( stderr, "[%d] Sent [%d=>%d] frag %d, ack=%d, throwaway=%d, len=%d, frame rate=%.2f, timeout=%d\n",
	       (int)(timestamp() % 100000), (int)inst.old_num, (int)inst.new_num, (int)inst.fragment_num,
	       (int)inst.ack_num, (int)inst.throwaway_num, (int)inst.diff.size(),
	       1000.0 / (double)send_interval(),
	       (int)connection.timeout() );
    }
  } while ( !diff.empty() );
}
