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
