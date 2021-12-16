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

#include "config.h"

#include <errno.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <exception>

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif

#include "swrite.h"
#include "completeterminal.h"
#include "user.h"
#include "terminaloverlay.h"
#include "locale_utils.h"
#include "fatal_assert.h"

const int ITERATIONS = 100000;

using namespace Terminal;

int main( int argc, char **argv )
{
  try {
    int fbmod = 0;
    int width = 80, height = 24;
    int iterations = ITERATIONS;
    if (argc > 1) {
      iterations = atoi(argv[1]);
      if (iterations < 1 || iterations > 1000000000) {
	fprintf(stderr, "bogus iteration count\n");
	exit(1);
      }
    }
    if (argc > 3) {
      width = atoi(argv[2]);
      height = atoi(argv[3]);
      if (width < 1 || width > 1000 || height < 1 || height > 1000) {
	fprintf(stderr, "bogus window size\n");
	exit(1);
      }
    }
    Framebuffer local_framebuffers[ 2 ] = { Framebuffer(width,height), Framebuffer(width,height) };
    Framebuffer *local_framebuffer = &(local_framebuffers[ fbmod ]);
    Framebuffer *new_state = &(local_framebuffers[ !fbmod ]);
    Overlay::OverlayManager overlays;
    Display display( true );
    Complete local_terminal( width, height );

    /* Adopt native locale */
    set_native_locale();
    fatal_assert( is_utf8_locale() );

    for ( int i = 0; i < iterations; i++ ) {
      /* type a character */
      overlays.get_prediction_engine().new_user_byte( i + 'x', *local_framebuffer );

      /* fetch target state */
      *new_state = local_terminal.get_fb();

      /* apply local overlays */
      overlays.apply( *new_state );

      /* calculate minimal difference from where we are */
      const string diff( display.new_frame( false,
					    *local_framebuffer,
					    *new_state ) );

      /* make sure to use diff */
      if ( diff.size() > INT_MAX ) {
	exit( 1 );
      }

      fbmod = !fbmod;
      local_framebuffer = &(local_framebuffers[ fbmod ]);
      new_state = &(local_framebuffers[ !fbmod ]);
    }
  } catch ( const std::exception &e ) {
    fprintf( stderr, "Exception caught: %s\n", e.what() );
    return 1;
  }
  return 0;
}
