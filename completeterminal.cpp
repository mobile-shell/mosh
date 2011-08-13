#include "completeterminal.hpp"

using namespace std;
using namespace Parser;

string Terminal::Complete::act( const string &str )
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

string Terminal::Complete::act( const Action *act )
{
  /* apply action to terminal */
  act->act_on_terminal( &terminal );
  return terminal.read_octets_to_host();
}
