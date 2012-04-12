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

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/signalfd.h>

#include "fatal_assert.h"

static sigset_t caught;
static int fd = -1;

#define SIGNALFD_FLAGS ( SFD_NONBLOCK | SFD_CLOEXEC )

int sigfd_init( void )
{
  if ( fd != -1 ) {
    errno = EBUSY;
    return -1;
  }
  sigemptyset( &caught );
  fd = signalfd( -1, &caught, SIGNALFD_FLAGS );
  return fd;
}

int sigfd_trap( int sig )
{
  /* The callers all fatal_assert on error, so we can do the same here.
     We still return 'int' in order to have the same API as libstddjb. */
  fatal_assert( 0 <= fd );
  fatal_assert( 0 <= sigaddset( &caught, sig ) );
  fatal_assert( 0 <= sigprocmask( SIG_BLOCK, &caught, NULL ) );
  fatal_assert( 0 <= signalfd( fd, &caught, SIGNALFD_FLAGS ) );
  return 0;
}

int sigfd_read( void )
{
  int r;
  struct signalfd_siginfo si;

  do {
    r = read( fd, &si, sizeof( si ) );
  } while ( ( r == -1 ) && ( errno == EINTR ) );

  if ( r == -1 ) {
    if ( errno == EAGAIN || errno == EWOULDBLOCK )
      /* No signal available */
      return 0;
    return -1;
  } else if ( r != sizeof( si ) ) {
    /* Should never happen?
       Includes r = 0, i.e. end of file */
    errno = EPIPE;
    return -1;
  }

  return (int) si.ssi_signo;
}
