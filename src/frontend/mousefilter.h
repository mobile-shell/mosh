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

#ifndef MOUSEFILTER_HPP
#define MOUSEFILTER_HPP

#include <cstddef>
#include <string>

/* Drop the mouse reports that arrive while mosh-client is the one holding the
   terminal's mouse.

   Terminals turn the wheel into cursor-key presses while the alternate screen
   is active ("alternate scroll"), which mosh has no way to tell apart from the
   user actually pressing Up or Down -- so scrolling types into whatever is
   running.  Asking the terminal for mouse reporting suppresses that
   substitution, and the events arrive as SGR (DECSET 1006) reports instead.

   Nobody is waiting for those events: the remote application never asked for
   the mouse, or we would have left it in charge and would not be filtering at
   all.  So every well-formed report is discarded -- clicks and drags as much
   as the wheel, since a click delivered to an application that did not enable
   mouse reporting is the same garbage on the command line that the cursor
   keys were.  Anything that is not a report passes through byte for byte. */

namespace MouseFilter {
/* Longest report we will hold on to before deciding it isn't one.  A real one
   is "\033[<" plus three decimal fields; 24 bytes leaves room to spare. */
static const size_t MAX_REPORT = 24;

/* True if body is the parameter list of a well-formed SGR report, that is
   three non-empty runs of decimal digits. */
inline bool sgr_report( const std::string& body )
{
  if ( body.empty() || body.size() > MAX_REPORT ) {
    return false;
  }

  int fields = 1;
  size_t digits = 0;
  for ( std::string::const_iterator i = body.begin(); i != body.end(); i++ ) {
    if ( *i == ';' ) {
      if ( digits == 0 ) {
        return false;
      }
      digits = 0;
      fields++;
    } else if ( ( *i >= '0' ) && ( *i <= '9' ) ) {
      digits++;
    } else {
      return false;
    }
  }

  return ( fields == 3 ) && ( digits > 0 );
}

class Filter
{
private:
  std::string pending;

public:
  Filter() : pending() {}

  void reset( void ) { pending.clear(); }

  /* Returns the input with mouse reports removed. */
  std::string filter( const char* buf, size_t len )
  {
    std::string data;
    data.swap( pending );
    data.append( buf, len );

    std::string out;
    size_t i = 0;

    while ( i < data.size() ) {
      const size_t start = data.find( "\033[<", i );
      if ( start == std::string::npos ) {
        out.append( data, i, std::string::npos );
        break;
      }

      out.append( data, i, start - i );

      const size_t end = data.find_first_of( "Mm", start + 3 );
      if ( end == std::string::npos ) {
        /* Incomplete report.  Only ever hold back something that already
           starts with the whole "\033[<" prefix: a read that ends in the
           middle of the prefix lets that one report through, which is the
           price of not holding a bare ESC and delaying the user's own Escape
           key until the next keystroke. */
        if ( data.size() - start <= MAX_REPORT ) {
          pending.assign( data, start, std::string::npos );
        } else {
          out.append( data, start, std::string::npos );
        }
        break;
      }

      if ( !sgr_report( std::string( data, start + 3, end - start - 3 ) ) ) {
        out.append( data, start, end - start + 1 );
      }
      i = end + 1;
    }

    return out;
  }
};
}

#endif
