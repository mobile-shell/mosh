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

#if !defined(HAVE_FORKPTY) || !defined(HAVE_CFMAKERAW)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>
#include <termios.h>

#include "pty_compat.h"

#ifndef HAVE_FORKPTY
pid_t my_forkpty( int *amaster, char *name,
		  const struct termios *termp,
		  const struct winsize *winp )
{ 
  /* For Solaris */
  int master, slave; 
  char *slave_name; 
  pid_t pid; 
   
  master = open( "/dev/ptmx", O_RDWR | O_NOCTTY );
  if ( master < 0 ) {
    perror( "open(/dev/ptmx)" );
    return -1;
  }

  if ( grantpt( master ) < 0 ) {
    perror( "grantpt" );
    close( master );
    return -1;
  }

  if ( unlockpt(master) < 0 ) {
    perror( "unlockpt" );
    close( master );
    return -1;
  }

  slave_name = ptsname( master );
  if ( slave_name == NULL ) {
    perror( "ptsname" );
    close( master );
    return -1;
  }

  slave = open( slave_name, O_RDWR | O_NOCTTY );
  if ( slave < 0 ) {
    perror( "open(slave)" );
    close( master );
    return -1;
  }

  if ( ioctl(slave, I_PUSH, "ptem") < 0 ||
       ioctl(slave, I_PUSH, "ldterm") < 0 ) {
    perror( "ioctl(I_PUSH)" );
    close( slave );
    close( master );
    return -1;
  }

  if ( amaster != NULL )
    *amaster = master;

  if ( name != NULL)
    strcpy( name, slave_name );

  if ( termp != NULL ) {
    if ( tcsetattr( slave, TCSAFLUSH, termp ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }
  }

  // we need to set initial window size, or TIOCGWINSZ fails
  struct winsize w;
  w.ws_row = 25;
  w.ws_col = 80;
  w.ws_xpixel = 0;
  w.ws_ypixel = 0;
  if ( ioctl( slave, TIOCSWINSZ, &w) < 0 ) {
    perror( "ioctl TIOCSWINSZ" );
    exit( 1 );
  }
  if ( winp != NULL ) {
    if ( ioctl( slave, TIOCGWINSZ, winp ) < 0 ) {
      perror( "ioctl TIOCGWINSZ" );
      exit( 1 );
    }
  }

  pid = fork();
  switch ( pid ) {
  case -1: /* Error */
    perror( "fork()" );
    return -1;
  case 0: /* Child */
    if ( setsid() < 0 )
      perror( "setsid" );
    if ( ioctl( slave, TIOCSCTTY, NULL ) < 0 ) {
      perror( "ioctl" );
      return -1;
    }
    close( master );
    dup2( slave, STDIN_FILENO );
    dup2( slave, STDOUT_FILENO );
    dup2( slave, STDERR_FILENO );
    return 0;
  default: /* Parent */
    close( slave );
    return pid;
  }
}
#endif

#ifndef HAVE_CFMAKERAW
void my_cfmakeraw( struct termios *termios_p )
{
  termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
			  | INLCR | IGNCR | ICRNL | IXON);
  termios_p->c_oflag &= ~OPOST;
  termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  termios_p->c_cflag &= ~(CSIZE | PARENB);
  termios_p->c_cflag |= CS8;

  termios_p->c_cc[VMIN] = 1; // read() is satisfied after 1 char
  termios_p->c_cc[VTIME] = 0; // No timer
}
#endif
#endif
