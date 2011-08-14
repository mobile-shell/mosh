#include "completeterminal.hpp"

using namespace std;
using namespace Parser;
using namespace Terminal;

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
  return Terminal::Display::new_frame( true, existing.get_fb(), terminal.get_fb() );
}

void Complete::apply_string( string diff )
{
  string terminal_to_host = act( diff );
  assert( terminal_to_host.empty() );
}

bool Complete::operator==( Complete const &x ) const
{
  //  assert( parser == x.parser ); /* parser state is irrelevant for us */
  return terminal == x.terminal;
}
