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

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include "pty_compat.h"
#include "swrite.h"

int main( int argc, char *argv[] )
{
  if (argc < 2) {
    fprintf( stderr, "usage: inpty COMMAND [ARGS...]\n" );
    return 1;
  }

  struct winsize winsize;
  memset( &winsize, 0, sizeof( winsize ) );
  winsize.ws_col = 80;
  winsize.ws_row = 24;

  int saved_stderr = dup(STDERR_FILENO);
  if ( saved_stderr < 0 ) {
    perror( "dup" );
    return 1;
  }

  int master;
  pid_t child = forkpty( &master, NULL, NULL, &winsize );
  if ( child == -1 ) {
    perror( "forkpty" );
    /* The Debian and Ubuntu build systems fail to set up a working
     * /dev/ptmx (https://bugs.debian.org/817236).  There is not much
     * we can do about that except skip the test.  In the future when
     * this is fixed, we should turn this into an failure.
     */
    return 77;
  } else if ( child == 0 ) {
    if ( dup2( saved_stderr, STDERR_FILENO ) < 0 ) {
      perror( "dup2" );
      exit( 1 );
    }
    if ( close( saved_stderr ) < 0 ) {
      perror( "close" );
      exit( 1 );
    }
    if ( execvp( argv[1], argv + 1 ) < 0 ) {
      perror( "execve" );
      exit( 1 );
    }
    exit( 0 );
  }

  while ( 1 ) {
    char buf[ 1024 ];
    ssize_t bytes_read = read( master, buf, sizeof( buf ) );
    if ( bytes_read == 0 || ( bytes_read < 0 && errno == EIO ) ) { /* EOF */
      break;
    } else if ( bytes_read < 0 ) {
      perror( "read" );
      return 1;
    }
    swrite( STDOUT_FILENO, buf, bytes_read );
  }

  int wstatus;
  if ( waitpid( child, &wstatus, 0 ) < 0 ) {
    perror( "waitpid" );
    return 1;
  }

  if ( WIFSIGNALED( wstatus ) ) {
    fprintf( stderr, "inpty: child exited with signal %d\n", WTERMSIG( wstatus ) );
    raise( WTERMSIG( wstatus ) );
    return -1;
  } else {
    return WIFEXITED( wstatus ) ? WEXITSTATUS( wstatus ) : -1;
  }
}
