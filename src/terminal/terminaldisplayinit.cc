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

/* This is in its own file because otherwise the ncurses #defines
   alias our own variable names. */

#include "terminaldisplay.h"

#include <string>

#include <curses.h>
#include <term.h>

using namespace Terminal;

Display::Display( bool use_environment )
  : has_ech( true ), has_bce( true ), posterize_colors( false )
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
