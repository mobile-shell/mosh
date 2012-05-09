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
