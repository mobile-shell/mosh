#include <pty.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#include <wchar.h>
#include <assert.h>
#include <wctype.h>
#include <iostream>
#include <typeinfo>

#include "terminal.hpp"

#ifndef __STDC_ISO_10646__
#error "Must have __STDC_ISO_10646__"
#endif

const size_t buf_size = 1024;

void emulate_terminal( int fd );
int copy( int src, int dest );
int termemu( int fd, Terminal::Emulator *terminal );

int main( int argc __attribute__((unused)),
	  char *argv[] __attribute__((unused)),
	  char *envp[] )
{
  int master;
  struct termios saved_termios, raw_termios, child_termios;

  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
    exit( 1 );
  }

  if ( strcmp( nl_langinfo( CODESET ), "UTF-8" ) != 0 ) {
    fprintf( stderr, "rtm requires a UTF-8 locale.\n" );
    exit( 1 );
  }

  if ( tcgetattr( STDIN_FILENO, &saved_termios ) < 0 ) {
    perror( "tcgetattr" );
    exit( 1 );
  }

  child_termios = saved_termios;

  if ( !(child_termios.c_iflag & IUTF8) ) {
    fprintf( stderr, "Warning: Locale is UTF-8 but termios IUTF8 flag not set. Setting IUTF8 flag.\n" );
    child_termios.c_iflag |= IUTF8;
  }

  pid_t child = forkpty( &master, NULL, &child_termios, NULL );

  if ( child == -1 ) {
    perror( "forkpty" );
    exit( 1 );
  }

  if ( child == 0 ) {
    /* child */
    if ( chdir( "/" ) < 0 ) {
      perror( "chdir" );
      exit( 1 );
    }

    char *my_argv[ 2 ];
    my_argv[ 0 ] = strdup( "/bin/bash" );
    assert( my_argv[ 0 ] );

    my_argv[ 1 ] = NULL;

    if ( execve( "/bin/bash", my_argv, envp ) < 0 ) {
      perror( "execve" );
      exit( 1 );
    }
    exit( 0 );
  } else {
    /* parent */
    raw_termios = saved_termios;

    cfmakeraw( &raw_termios );

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &raw_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }

    emulate_terminal( master );

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }
  }

  return 0;
}

void emulate_terminal( int fd )
{
  Terminal::Emulator terminal( 80, 24 );
  struct pollfd pollfds[ 2 ];

  pollfds[ 0 ].fd = STDIN_FILENO;
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = fd;
  pollfds[ 1 ].events = POLLIN;

  while ( 1 ) {
    int active_fds = poll( pollfds, 2, -1 );
    if ( active_fds <= 0 ) {
      perror( "poll" );
      return;
    }

    if ( pollfds[ 0 ].revents & POLLIN ) {
      if ( copy( STDIN_FILENO, fd ) < 0 ) {
	return;
      }
    } else if ( pollfds[ 1 ].revents & POLLIN ) {
      if ( termemu( fd, &terminal ) < 0 ) {
	return;
      }
    } else if ( (pollfds[ 0 ].revents | pollfds[ 1 ].revents)
		& (POLLERR | POLLHUP | POLLNVAL) ) {
      return;
    } else {
      fprintf( stderr, "poll mysteriously woken up\n" );
    }
  }
}

int copy( int src, int dest )
{
  char buf[ buf_size ];

  ssize_t bytes_read = read( src, buf, buf_size );
  if ( bytes_read == 0 ) { /* EOF */
    return -1;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return -1;
  } else {
    ssize_t total_bytes_written = 0;
    while ( total_bytes_written < bytes_read ) {
      ssize_t bytes_written = write( dest, buf + total_bytes_written,
				     bytes_read - total_bytes_written );
      if ( bytes_written <= 0 ) {
	perror( "write" );
	return -1;
      } else {
	total_bytes_written += bytes_written;
      }
    }
  }

  return 0;
}

int termemu( int fd, Terminal::Emulator *terminal )
{
  char buf[ buf_size ];

  /* fill buffer if possible */
  ssize_t bytes_read = read( fd, buf, buf_size );
  if ( bytes_read == 0 ) { /* EOF */
    return -1;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return -1;
  }

  /* feed to terminal */
  for ( int i = 0; i < bytes_read; i++ ) {
    terminal->input( buf[ i ] );
  }

  return 0;
}
