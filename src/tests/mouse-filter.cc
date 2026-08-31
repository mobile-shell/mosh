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

#include "src/include/config.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/frontend/mousefilter.h"

static std::string run( MouseFilter::Filter& f, const std::string& in )
{ return f.filter( in.data(), in.size() ); }

static void one_shot( const std::string& in, const std::string& expected )
{
  MouseFilter::Filter f;
  const std::string got = run( f, in );
  if ( got != expected ) {
    fprintf( stderr, "filter(%s): got <%s>, expected <%s>\n", in.c_str(), got.c_str(), expected.c_str() );
    abort();
  }
}

int main( void )
{
  /* wheel reports are dropped, in both directions and with modifiers */
  one_shot( "\033[<64;14;20M", "" );
  one_shot( "\033[<65;14;20M", "" );
  one_shot( "\033[<69;14;20M", "" ); /* shift+wheel-down: 65|4 */
  one_shot( "\033[<80;14;20M", "" ); /* ctrl+wheel-up: 64|16 */
  one_shot( "\033[<66;14;20M", "" ); /* horizontal wheel */
  one_shot( "\033[<67;14;20M", "" );

  /* so is everything else the terminal reports, since the application never
     asked for the mouse -- a click would arrive as literal keystrokes */
  one_shot( "\033[<0;14;20M", "" );  /* button 1 press */
  one_shot( "\033[<0;14;20m", "" );  /* button 1 release */
  one_shot( "\033[<32;14;20M", "" ); /* drag */

  /* surrounding input survives, and repeated reports all go */
  one_shot( "ab\033[<64;1;1Mcd", "abcd" );
  one_shot( "\033[<64;1;1M\033[<65;2;2M\033[<64;3;3M", "" );

  /* ordinary keystrokes are untouched */
  one_shot( "hello", "hello" );
  one_shot( "\033[A", "\033[A" ); /* cursor up */
  one_shot( "\033", "\033" );     /* a bare Escape must never be held back */
  one_shot( "\033[", "\033[" );   /* nor a partial CSI */
  one_shot( "\033[<0;1;1", "" );  /* a real partial report is held */

  /* malformed sequences are passed on rather than eaten */
  one_shot( "\033[<abc;1;1M", "\033[<abc;1;1M" );
  one_shot( "\033[<64;1M", "\033[<64;1M" );   /* too few parameters */
  one_shot( "\033[<64;;1M", "\033[<64;;1M" ); /* empty parameter */
  one_shot( "\033[<64;1;2;3M", "\033[<64;1;2;3M" );

  /* an over-long body is not a report even when it terminates in one read */
  {
    const std::string junk = "\033[<" + std::string( MouseFilter::MAX_REPORT + 1, '9' ) + "M";
    one_shot( junk, junk );
  }

  /* a report split across two reads is still recognised */
  {
    MouseFilter::Filter f;
    assert( run( f, "xy\033[<64;14" ) == "xy" );
    assert( run( f, ";20Mz" ) == "z" );
  }

  /* but a read that ends inside the three-byte prefix lets the report
     through: holding a trailing ESC would delay the user's Escape key, and
     that is the worse trade.  Pinned so it cannot silently get worse. */
  {
    MouseFilter::Filter f;
    assert( run( f, "\033[" ) == "\033[" );
    assert( run( f, "<64;14;20M" ) == "<64;14;20M" );
  }

  /* an over-long run that never terminates is released, not hoarded */
  {
    MouseFilter::Filter f;
    const std::string junk = "\033[<" + std::string( MouseFilter::MAX_REPORT, '9' );
    assert( run( f, junk ) == junk );
  }

  printf( "mouse-filter: all checks passed\n" );
  return 0;
}
