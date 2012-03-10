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
  : has_ech( true )
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

    char *val = tigetstr( "ech" );
    if ( val <= 0 ) {
      has_ech = false;
    }
  }
}
