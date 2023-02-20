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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#include <cstring>

#include "stdfds.h"

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

void open_stdfds()
{
  /* Make sure all standard i/o fds are open on something. */
  for ( int fd = 0; fd <= STDERR_FILENO; fd++) {
    if ( ::fcntl( fd, F_GETFD ) < 0 ) {
      if ( ::open( _PATH_DEVNULL, O_RDWR ) != fd ) {
	/* given the circumstances, even writing an error may fail */
	const char* stdErr = "cannot open standard file descriptor\n";
	if ( ::write( STDERR_FILENO, stdErr, strlen(stdErr) ) < static_cast<ssize_t>( strlen(stdErr) ) ) {
	  ::abort();
	}
	::exit(1);
      }
    }
  }
}

void detach_stdfds()
{
  /* Necessary to properly detach on old versions of sshd (e.g. RHEL/CentOS 5.0). */
  int nullfd;

  nullfd = ::open( _PATH_DEVNULL, O_RDWR );
  if ( nullfd == -1 ) {
    ::perror( "open" );
    ::exit( 1 );
  }

  if ( ::dup2 ( nullfd, STDIN_FILENO ) < 0 ||
       ::dup2 ( nullfd, STDOUT_FILENO ) < 0 ||
       ::dup2 ( nullfd, STDERR_FILENO ) < 0 ) {
    ::perror( "dup2" );
    ::exit( 1 );
  }

  if ( nullfd > STDERR_FILENO && ::close( nullfd ) < 0 ) {
    /*
     * This goes to /dev/null, but do it anyway, because it will
     * show up on system call traces.
     */
    ::perror( "close" );
    ::exit( 1 );
  }
}
