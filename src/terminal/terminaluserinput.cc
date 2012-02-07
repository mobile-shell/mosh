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

#include "terminaluserinput.h"

using namespace Terminal;

std::string UserInput::input( const Parser::UserByte *act,
			      bool application_mode_cursor_keys )
{
  char translated_str[ 2 ] = { act->c, 0 };

  /* The user will always be in application mode. If stm is not in
     application mode, convert user's cursor control function to an
     ANSI cursor control sequence */

  /* We don't need lookahead to do this for 7-bit. */

  if ( (!application_mode_cursor_keys)
       && (last_byte == 0x1b) /* ESC */
       && (act->c == 'O') ) { /* ESC O = 7-bit SS3 = application mode */
    translated_str[ 0 ] = '[';
  }

  /* This doesn't handle the 8-bit SS3 C1 control, which would be
     two octets in UTF-8. Fortunately nobody seems to send this. */

  last_byte = act->c;

  act->handled = true;

  return std::string( translated_str, 1 );
}
