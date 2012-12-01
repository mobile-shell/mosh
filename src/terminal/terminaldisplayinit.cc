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

/* This is in its own file because otherwise the ncurses #defines
   alias our own variable names. */

#include "config.h"
#include "terminaldisplay.h"

#include <string>

#if defined HAVE_NCURSESW_CURSES_H
#  include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
#  include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
#  include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#  include <ncurses.h>
#elif defined HAVE_CURSES_H
#  include <curses.h>
#else
#  error "SysV or X/Open-compatible Curses header file required"
#endif
#include <term.h>
#include <stdlib.h>
#include <string.h>

using namespace Terminal;

Display::Display( bool use_environment )
  : has_ech( true ), has_bce( true ), has_title( true ), posterize_colors( false )
{
  if ( use_environment ) {
    int errret = -2;
    int ret = setupterm( (char *)0, 1, &errret );

    if ( ret != OK ) {
      switch ( errret ) {
      case 1:
	throw std::string( "Terminal is hardcopy and cannot be used by curses applications." );
	break;
      case 0:
	throw std::string( "Unknown terminal type." );
	break;
      case -1:
	throw std::string( "Terminfo database could not be found." );
	break;
      default:
	throw std::string( "Unknown terminfo error." );
	break;
      } 
    }

    /* check for ECH */
    char ech_name[] = "ech";
    char *val = tigetstr( ech_name );
    if ( val == (char *)-1 ) {
      throw std::string( "Invalid terminfo string capability " ) + ech_name;
    } else if ( val == 0 ) {
      has_ech = false;
    }

    /* check for BCE */
    char bce_name[] = "bce";
    int bce_val = tigetflag( bce_name );
    if ( bce_val == -1 ) {
      throw std::string( "Invalid terminfo boolean capability " ) + bce_name;
    } else if ( bce_val == 0 ) {
      has_bce = false;
    }

    /* Check if we can set the window title and icon name.  terminfo does not
       have reliable information on this, so we hardcode a whitelist of
       terminal type prefixes.  This is the list from Debian's default
       screenrc, plus "screen" itself (which also covers tmux). */
    static const char * const title_term_types[] = {
      "xterm", "rxvt", "kterm", "Eterm", "screen"
    };

    has_title = false;
    const char *term_type = getenv( "TERM" );
    if ( term_type ) {
      for ( size_t i = 0;
            i < sizeof( title_term_types ) / sizeof( const char * );
            i++ ) {
        if ( 0 == strncmp( term_type, title_term_types[ i ],
                           strlen( title_term_types[ i ] ) ) ) {
          has_title = true;
          break;
        }
      }
    }

    /* posterization disabled because server now only advertises
       xterm-256color when client has colors = 256 */
    /*
    char colors_name[] = "colors";
    int color_val = tigetnum( colors_name );
    if ( color_val == -2 ) {
      throw std::string( "Invalid terminfo numeric capability " ) + colors_name;
    } else if ( color_val < 256 ) {
      posterize_colors = true;
    }
    */
  }
}
