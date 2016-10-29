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

#ifndef NETWORK_TRANSPORT_IMPL_HPP
#define NETWORK_TRANSPORT_IMPL_HPP

#include "networktransport.h"

#include "transportsender-impl.h"

using namespace Network;
using namespace std;

template <class MyState, class RemoteState>
Transport<MyState, RemoteState>::Transport( MyState &initial_state, RemoteState &initial_remote,
					    const char *desired_ip, const char *desired_port )
  : connection( desired_ip, desired_port ),
    sender( &connection, initial_state ),
    received_states( 1, TimestampedState<RemoteState>( timestamp(), 0, initial_remote ) ),
    receiver_quench_timer( 0 ),
    last_receiver_state( initial_remote ),
    fragments(),
    verbose( 0 )
{
  /* server */
}

template <class MyState, class RemoteState>
Transport<MyState, RemoteState>::Transport( MyState &initial_state, RemoteState &initial_remote,
					    const char *key_str, const char *ip, const char *port )
  : connection( key_str, ip, port ),
    sender( &connection, initial_state ),
    received_states( 1, TimestampedState<RemoteState>( timestamp(), 0, initial_remote ) ),
    receiver_quench_timer( 0 ),
    last_receiver_state( initial_remote ),
    fragments(),
    verbose( 0 )
{
  /* client */
}

template <class MyState, class RemoteState>
void Transport<MyState, RemoteState>::recv( void )
{
  string s( connection.recv() );
  Fragment frag( s );

  if ( fragments.add_fragment( frag ) ) { /* complete packet */
    Instruction inst = fragments.get_assembly();

    if ( inst.protocol_version() != MOSH_PROTOCOL_VERSION ) {
      throw NetworkException( "mosh protocol version mismatch", 0 );
    }

    sender.process_acknowledgment_through( inst.ack_num() );

    /* inform network layer of roundtrip (end-to-end-to-end) connectivity */
    connection.set_last_roundtrip_success( sender.get_sent_state_acked_timestamp() );

    /* first, make sure we don't already have the new state */
    for ( typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
	  i != received_states.end();
	  i++ ) {
      if ( inst.new_num() == i->num ) {
	return;
      }
    }
    
    /* now, make sure we do have the old state */
    bool found = 0;
    typename list< TimestampedState<RemoteState> >::iterator reference_state = received_states.begin();
    while ( reference_state != received_states.end() ) {
      if ( inst.old_num() == reference_state->num ) {
	found = true;
	break;
      }
      reference_state++;
    }
    
    if ( !found ) {
      //    fprintf( stderr, "Ignoring out-of-order packet. Reference state %d has been discarded or hasn't yet been received.\n", int(inst.old_num) );
      return; /* this is security-sensitive and part of how we enforce idempotency */
    }
    
    /* Do not accept state if our queue is full */
    /* This is better than dropping states from the middle of the
       queue (as sender does), because we don't want to ACK a state
       and then discard it later. */

    process_throwaway_until( inst.throwaway_num() );

    if ( received_states.size() > 1024 ) { /* limit on state queue */
      uint64_t now = timestamp();
      if ( now < receiver_quench_timer ) { /* deny letting state grow further */
	if ( verbose ) {
	  fprintf( stderr, "[%u] Receiver queue full, discarding %d (malicious sender or long-unidirectional connectivity?)\n",
		   (unsigned int)(timestamp() % 100000), (int)inst.new_num() );
	}
	return;
      } else {
	receiver_quench_timer = now + 15000;
      }
    }

    /* apply diff to reference state */
    TimestampedState<RemoteState> new_state = *reference_state;
    new_state.timestamp = timestamp();
    new_state.num = inst.new_num();

    if ( !inst.diff().empty() ) {
      new_state.state.apply_string( inst.diff() );
    }

    /* Insert new state in sorted place */
    for ( typename list< TimestampedState<RemoteState> >::iterator i = received_states.begin();
	  i != received_states.end();
	  i++ ) {
      if ( i->num > new_state.num ) {
	received_states.insert( i, new_state );
	if ( verbose ) {
	  fprintf( stderr, "[%u] Received OUT-OF-ORDER state %d [ack %d]\n",
		   (unsigned int)(timestamp() % 100000), (int)new_state.num, (int)inst.ack_num() );
	}
	return;
      }
    }
    if ( verbose ) {
      fprintf( stderr, "[%u] Received state %d [coming from %d, ack %d]\n",
	       (unsigned int)(timestamp() % 100000), (int)new_state.num, (int)inst.old_num(), (int)inst.ack_num() );
    }
    received_states.push_back( new_state );
    sender.set_ack_num( received_states.back().num );

    sender.remote_heard( new_state.timestamp );
    if ( !inst.diff().empty() ) {
      sender.set_data_ack();
    }
  }
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

  fatal_assert( received_states.size() > 0 );
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

#endif
