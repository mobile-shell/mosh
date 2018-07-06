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

#include <assert.h>
#include "terminaluserinput.h"

using namespace Terminal;
using namespace std;

string UserInput::input( const Parser::UserByte *act,
			 bool application_mode_cursor_keys,
			 string & client_type,
			 string & client_version )
{
  /* Transform incoming user bytes or capture information (e.g. if the
     user bytes are carrying a response sequence from the client
     terminal application) */

  /* The user will always be in application mode. If stm is not in
     application mode, convert user's cursor control function to an
     ANSI cursor control sequence */

  /* We need to look ahead one byte in the SS3 state to see if
     the next byte will be A, B, C, or D (cursor control keys). */

  /* This doesn't handle the 8-bit SS3 C1 control or CSI, which would
     be two octets in UTF-8. Fortunately nobody seems to send this. */

  switch ( state ) {
  case Ground:
    if ( act->c == 0x1b ) { /* ESC */
      state = ESC;
      return string();
    }
    return string( &act->c, 1 );

  case ESC:
    if ( act->c == 'O' ) { /* ESC O = 7-bit SS3 */
      state = SS3;
      return string();
    }
    if ( act->c == '[' ) { /* ESC [ = 7-bit CSI */
      state = CSI;
      return string();
    }
    state = Ground;
    {
      char return_to_ground[ 2 ] = { '\x1b', act->c };
      return string( return_to_ground, 2 );
    }

  case SS3:
    state = Ground;
    if ( (!application_mode_cursor_keys)
	 && (act->c >= 'A')
	 && (act->c <= 'D') ) {
      char translated_cursor[ 3 ] = { 0x1b, '[', act->c };
      return string( translated_cursor, 3 );
    } else {
      char original_cursor[ 3 ] = { 0x1b, 'O', act->c };
      return string( original_cursor, 3 );
    }

  case CSI:
    if ( act->c == '>' ) {
      char collector_init[ 3 ] = { '\x1b', '[', '>' };
      collector.assign( collector_init, 3 );
      ps.clear();
      state = CSI_AB;
      return string();
    }
    state = Ground;
    {
      char return_to_ground[ 3 ] = { 0x1b, '[', act->c };
      return string( return_to_ground, 3 );
    }

  case CSI_AB:
    /* A CSI followed by '>' must be response to a "send secondary
       device attributes" request. It should contain three numeric
       parameters and end in 'c'. Parse the parameters until the 'c'
       is found. */

    collector.push_back( act->c );

    if ( act->c >= '0' && act->c <= '9' ) {
      if ( ps.empty() )
	ps.push_back("");
      ps.back().push_back( act->c );
      return string();
    }
    if ( act->c == ';' ) {
      ps.push_back("");
      return string();
    }
    if ( act->c == 'c' ) {
      /* store the terminal type and version, then return to ground */

      if ( ps.size() > 1 ) {
	client_type = ps[0];
	client_version = ps[1];
      }

      state = Ground;
      return string();
    }

    /* Quit parsing the CSI function on any unknown character */

    state = Ground;
    return collector;

  default:
    assert( !"unexpected state" );
    state = Ground;
    return string();
  }
}
