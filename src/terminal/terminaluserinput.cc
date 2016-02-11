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
			 bool application_mode_cursor_keys )
{
  /* The user will always be in application mode. If stm is not in
     application mode, convert user's cursor control function to an
     ANSI cursor control sequence */

  /* We need to look ahead one byte in the SS3 state to see if
     the next byte will be A, B, C, or D (cursor control keys). */

  switch ( state ) {
  case Ground:
    if ( act->c == 0x1b ) { /* ESC */
      state = ESC;
    }
    return string( &act->c, 1 );
    break;

  case ESC:
    if ( act->c == 'O' ) { /* ESC O = 7-bit SS3 */
      state = SS3;
      return string();
    } else {
      state = Ground;
      return string( &act->c, 1 );
    }
    break;

  case SS3:
    state = Ground;
    if ( (!application_mode_cursor_keys)
	 && (act->c >= 'A')
	 && (act->c <= 'D') ) {
      char translated_cursor[ 2 ] = { '[', act->c };
      return string( translated_cursor, 2 );
    } else {
      char original_cursor[ 2 ] = { 'O', act->c };
      return string( original_cursor, 2 );
    }
    break;
  }

  /* This doesn't handle the 8-bit SS3 C1 control, which would be
     two octets in UTF-8. Fortunately nobody seems to send this. */

  assert( false );
  return string();
}
