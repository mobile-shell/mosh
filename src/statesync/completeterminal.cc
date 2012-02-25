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

#include <boost/typeof/typeof.hpp>
#include <boost/lambda/lambda.hpp>

#include "completeterminal.h"

#include "hostinput.pb.h"

using namespace std;
using namespace Parser;
using namespace Terminal;
using namespace HostBuffers;
using namespace boost::lambda;

string Complete::act( const string &str )
{
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    /* parse octet into up to three actions */
    list<Action *> actions( parser.input( str[ i ] ) );
    
    /* apply actions to terminal and delete them */
    for ( list<Action *>::iterator it = actions.begin();
	  it != actions.end();
	  it++ ) {
      Action *act = *it;
      act->act_on_terminal( &terminal );
      delete act;
    }
  }

  return terminal.read_octets_to_host();
}

string Complete::act( const Action *act )
{
  /* apply action to terminal */
  act->act_on_terminal( &terminal );
  return terminal.read_octets_to_host();
}

/* interface for Network::Transport */
string Complete::diff_from( const Complete &existing ) const
{
  HostBuffers::HostMessage output;

  if ( existing.get_echo_ack() != get_echo_ack() ) {
    assert( get_echo_ack() >= existing.get_echo_ack() );
    Instruction *new_echo = output.add_instruction();
    new_echo->MutableExtension( echoack )->set_echo_ack_num( get_echo_ack() );
  }

  if ( !(existing.get_fb() == get_fb()) ) {
    if ( (existing.get_fb().ds.get_width() != terminal.get_fb().ds.get_width())
	 || (existing.get_fb().ds.get_height() != terminal.get_fb().ds.get_height()) ) {
      Instruction *new_res = output.add_instruction();
      new_res->MutableExtension( resize )->set_width( terminal.get_fb().ds.get_width() );
      new_res->MutableExtension( resize )->set_height( terminal.get_fb().ds.get_height() );
    }
    Instruction *new_inst = output.add_instruction();
    new_inst->MutableExtension( hostbytes )->set_hoststring( Terminal::Display::new_frame( true, existing.get_fb(), terminal.get_fb() ) );
  }
  
  return output.SerializeAsString();
}

void Complete::apply_string( string diff )
{
  HostBuffers::HostMessage input;
  assert( input.ParseFromString( diff ) );

  for ( int i = 0; i < input.instruction_size(); i++ ) {
    if ( input.instruction( i ).HasExtension( hostbytes ) ) {
      string terminal_to_host = act( input.instruction( i ).GetExtension( hostbytes ).hoststring() );
      assert( terminal_to_host.empty() ); /* server never interrogates client terminal */
    } else if ( input.instruction( i ).HasExtension( resize ) ) {
      act( new Resize( input.instruction( i ).GetExtension( resize ).width(),
		       input.instruction( i ).GetExtension( resize ).height() ) );
    } else if ( input.instruction( i ).HasExtension( echoack ) ) {
      uint64_t inst_echo_ack_num = input.instruction( i ).GetExtension( echoack ).echo_ack_num();
      assert( inst_echo_ack_num >= echo_ack );
      echo_ack = inst_echo_ack_num;
    }
  }
}

bool Complete::operator==( Complete const &x ) const
{
  //  assert( parser == x.parser ); /* parser state is irrelevant for us */
  return (terminal == x.terminal) && (echo_ack == x.echo_ack);
}

void Complete::set_echo_ack( uint64_t now )
{
  uint64_t newest_echo_ack = 0;

  for ( BOOST_AUTO( i, input_history.begin() ); i != input_history.end(); i++ ) {
    if ( i->second < now - ECHO_TIMEOUT ) {
      newest_echo_ack = i->first;
    }
  }

  input_history.remove_if( (&_1)->*&pair<uint64_t, uint64_t>::first < newest_echo_ack );

  echo_ack = newest_echo_ack;
}

void Complete::register_input_frame( uint64_t n, uint64_t now )
{
  input_history.push_back( make_pair( n, now ) );
}
