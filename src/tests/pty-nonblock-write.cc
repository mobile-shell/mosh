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

/* Regression test for the issue #1356 deadlock. The server writes client input
   to the pty master; if the child stops reading (e.g. a large paste into a
   program blocked writing echo to a full stdout), a *blocking* write to the
   master hangs the whole server. The fix makes the master non-blocking and
   buffers what it can't accept. This test forks a child under a pty that never
   reads, then confirms that a write to the master returns EAGAIN promptly
   rather than blocking. An alarm() watchdog fails the test (by terminating it)
   if a blocking write ever sneaks back in. */

#include "src/include/config.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif

#if FORKPTY_IN_LIBUTIL
#include <libutil.h>
#endif

#include "src/util/pty_compat.h"

int main( void )
{
  int master = -1;
  pid_t pid = forkpty( &master, NULL, NULL, NULL );
  if ( pid < 0 ) {
    perror( "forkpty" );
    return EXIT_FAILURE;
  }

  if ( pid == 0 ) {
    /* Child: never read stdin, so the master's write side fills up. */
    for ( ;; ) {
      pause();
    }
    _exit( 0 );
  }

  /* Parent: make the master non-blocking, exactly as mosh-server does. */
  int flags = fcntl( master, F_GETFL );
  if ( flags < 0 || fcntl( master, F_SETFL, flags | O_NONBLOCK ) < 0 ) {
    perror( "fcntl O_NONBLOCK" );
    kill( pid, SIGKILL );
    return EXIT_FAILURE;
  }

  /* If a write blocks (i.e. the master is not really non-blocking), this alarm
     terminates the test process, which the harness scores as a failure. */
  alarm( 10 );

  char buf[4096];
  memset( buf, 'x', sizeof( buf ) );

  bool got_eagain = false;
  for ( int i = 0; i < 100000 && !got_eagain; i++ ) {
    ssize_t n = write( master, buf, sizeof( buf ) );
    if ( n < 0 ) {
      if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
        got_eagain = true;
      } else if ( errno == EINTR ) {
        continue;
      } else {
        perror( "write" );
        break;
      }
    }
  }

  alarm( 0 );
  kill( pid, SIGKILL );
  int status;
  waitpid( pid, &status, 0 );

  if ( !got_eagain ) {
    fprintf( stderr, "FAIL: master accepted 400 MB without ever blocking or returning EAGAIN\n" );
    return EXIT_FAILURE;
  }

  /* The non-blocking master returned EAGAIN instead of hanging: the property
     the #1356 fix depends on holds. */
  return EXIT_SUCCESS;
}
