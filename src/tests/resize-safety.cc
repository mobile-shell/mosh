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

/* Regression test for issue #1386 and the resize memory-exhaustion guard:
   Framebuffer::resize() must not abort on a non-positive size (e.g. a 0x0
   tty) or attempt an unbounded allocation for an absurd size.  Invalid
   requests are ignored (the current size is kept); valid ones take effect. */

#include <cstdio>
#include <cstdlib>

#include "src/terminal/terminalframebuffer.h"

using namespace Terminal;

static int failures = 0;

static void expect_size( const Framebuffer& fb, int width, int height, const char* what )
{
  if ( fb.ds.get_width() != width || fb.ds.get_height() != height ) {
    fprintf( stderr,
             "FAIL: %s: expected %dx%d, got %dx%d\n",
             what,
             width,
             height,
             fb.ds.get_width(),
             fb.ds.get_height() );
    failures++;
  }
}

int main( void )
{
  Framebuffer fb( 80, 24 );
  expect_size( fb, 80, 24, "initial" );

  /* zero size (issue #1386): a 0x0 tty must not abort; size is kept */
  fb.resize( 0, 0 );
  expect_size( fb, 80, 24, "resize 0x0" );

  /* one zero dimension */
  fb.resize( 100, 0 );
  expect_size( fb, 80, 24, "resize 100x0" );
  fb.resize( 0, 100 );
  expect_size( fb, 80, 24, "resize 0x100" );

  /* negative dimensions (a hostile peer sends a negative int32) */
  fb.resize( -5, -5 );
  expect_size( fb, 80, 24, "resize -5x-5" );

  /* absurd size: would allocate many gigabytes; must be ignored */
  fb.resize( 100000, 100000 );
  expect_size( fb, 80, 24, "resize 100000x100000" );

  /* a legitimate resize still takes effect */
  fb.resize( 132, 43 );
  expect_size( fb, 132, 43, "resize 132x43" );

  /* shrink back down */
  fb.resize( 40, 12 );
  expect_size( fb, 40, 12, "resize 40x12" );

  /* minimum valid size */
  fb.resize( 1, 1 );
  expect_size( fb, 1, 1, "resize 1x1" );

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
