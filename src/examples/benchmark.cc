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

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif

#include "sigfd.h"
#include "swrite.h"
#include "completeterminal.h"
#include "user.h"
#include "terminaloverlay.h"
#include "locale_utils.h"
#include "fatal_assert.h"

/* For newer skalibs */
extern "C" {
  const char *PROG = "benchmark";
}

const int ITERATIONS = 100000;

using namespace Terminal;

int main( void )
{
  int fbmod = 0;
  Framebuffer local_framebuffers[ 2 ] = { Framebuffer(80,24), Framebuffer(80,24) };
  Framebuffer *local_framebuffer = &(local_framebuffers[ fbmod ]);
  Framebuffer *new_state = &(local_framebuffers[ !fbmod ]);
  Overlay::OverlayManager overlays;
  Display display( true );
  Complete local_terminal( 80, 24 );

  /* Adopt native locale */
  set_native_locale();
  fatal_assert( is_utf8_locale() );

  for ( int i = 0; i < ITERATIONS; i++ ) {
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

  return 0;
}
