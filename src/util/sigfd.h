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

/* Uses either signalfd or libstddjb's selfpipe to receive signals as part of
   an event loop.

   selfpipe already does a fine job of interfacing to signalfd.  But Debian and
   Ubuntu want us to depend on the skalibs-dev package rather than build
   libstddjb ourselves.  That would be fine except that skalibs-dev has static
   libraries only, and they aren't built with -fPIC.  This interferes with
   building mosh-{client,server} as position-independent executables, which
   is a desirable security measure.

   So we have our own wrapper, which invokes either signalfd or selfpipe.  And
   we build it ourselves with our own flags, because it's part of the Mosh
   project proper. */

#ifndef SIGFD_HPP
#define SIGFD_HPP

#if USE_LIBSTDDJB

extern "C" {
#include "selfpipe.h"
}

#define sigfd_init selfpipe_init
#define sigfd_trap selfpipe_trap
#define sigfd_read selfpipe_read

#else

int sigfd_init( void );
int sigfd_trap( int sig );
int sigfd_read( void );

#endif

#endif
