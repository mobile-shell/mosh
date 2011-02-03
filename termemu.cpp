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
#include <sys/stat.h>
#include <fcntl.h>

#include "parser.hpp"
#include "terminal.hpp"
#include "swrite.hpp"

const size_t buf_size = 1024;

void emulate_terminal( int fd, int debug_fd );
int copy( int src, int dest );
int termemu( int host_fd, int src_fd, bool user, int debug_fd,
	     Parser::UTF8Parser *parser,
	     Terminal::Emulator *terminal );

int main( int argc,
	  char *argv[],
	  char *envp[] )
{
  int debug_fd;
  if ( argc == 1 ) {
    debug_fd = -1;
  } else if ( argc == 2 ) {
    debug_fd = open( argv[ 1 ], O_WRONLY );
    if ( debug_fd < 0 ) {
      perror( "open" );
      exit( 1 );
    }
  } else {
    fprintf( stderr, "Usage: %s [debugfd]\n", argv[ 0 ] );
    exit( 1 );
  }

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

    emulate_terminal( master, debug_fd );

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }
  }

  printf( "[rtm is exiting.]\n" );

  return 0;
}

void emulate_terminal( int fd, int debug_fd )
{
  Parser::UTF8Parser parser;
  Terminal::Emulator terminal( 80, 24 );
  struct pollfd pollfds[ 2 ];

  pollfds[ 0 ].fd = STDIN_FILENO;
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = fd;
  pollfds[ 1 ].events = POLLIN;

  swrite( STDOUT_FILENO, terminal.open().c_str() );

  while ( 1 ) {
    int active_fds = poll( pollfds, 2, -1 );
    if ( active_fds <= 0 ) {
      perror( "poll" );
      break;
    }

    if ( pollfds[ 0 ].revents & POLLIN ) {
      if ( termemu( fd, STDIN_FILENO, true, debug_fd, &parser, &terminal ) < 0 ) {
	break;
      }
    } else if ( pollfds[ 1 ].revents & POLLIN ) {
      if ( termemu( fd, fd, false, debug_fd, &parser, &terminal ) < 0 ) {
	break;
      }
    } else if ( (pollfds[ 0 ].revents | pollfds[ 1 ].revents)
		& (POLLERR | POLLHUP | POLLNVAL) ) {
      break;
    } else {
      fprintf( stderr, "poll mysteriously woken up\n" );
    }
  }

  swrite( STDOUT_FILENO, terminal.close().c_str() );
}

int termemu( int host_fd, int src_fd, bool user, int debug_fd,
	     Parser::UTF8Parser *parser,
	     Terminal::Emulator *terminal )
{
  char buf[ buf_size ];

  /* fill buffer if possible */
  ssize_t bytes_read = read( src_fd, buf, buf_size );
  if ( bytes_read == 0 ) { /* EOF */
    return -1;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return -1;
  }

  for ( int i = 0; i < bytes_read; i++ ) {
    /* feed bytes to parser */
    std::list<Parser::Action *> actions;
    if ( user ) {
      actions.push_back( new Parser::UserByte( buf[ i ] ) );
    } else {
      actions = parser->input( buf[ i ] );
    }

    /* apply actions to terminal */
    for ( std::list<Parser::Action *>::iterator i = actions.begin();
	  i != actions.end();
	  i++ ) {
      Parser::Action *act = *i;
      /* apply action to terminal */
      act->act_on_terminal( terminal );

      /* print out action for debugging */
      if ( (debug_fd > 0) && (!act->handled) ) {
	char actsum[ 64 ];
	snprintf( actsum, 64, "%s ", act->str().c_str() );
	swrite( debug_fd, actsum );
      }

      delete *i;
    }
  }

  terminal->debug_printout( STDOUT_FILENO );

  /* write writeback */
  std::string terminal_to_host = terminal->read_octets_to_host();
  return swrite( host_fd, terminal_to_host.c_str(), terminal_to_host.length() );
}
