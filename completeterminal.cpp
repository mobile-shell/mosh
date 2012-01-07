#include "completeterminal.hpp"

#include "hostinput.pb.h"

using namespace std;
using namespace Parser;
using namespace Terminal;
using namespace HostBuffers;

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
string Complete::diff_from( const Complete &existing )
{
  HostBuffers::HostMessage output;

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
    }
  }
}

bool Complete::operator==( Complete const &x ) const
{
  //  assert( parser == x.parser ); /* parser state is irrelevant for us */
  return terminal == x.terminal;
}
