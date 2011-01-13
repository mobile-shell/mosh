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

#include "parser.hpp"

#ifndef __STDC_ISO_10646__
#error "Must have __STDC_ISO_10646__"
#endif

const size_t buf_size = 1024;

class stripstate {
public:
  int src_fd, dest_fd;
  mbstate_t ps;
  char buf[ buf_size ];
  size_t buf_len;
  Parser::Parser parser;

  stripstate() : src_fd(-1), dest_fd(-1), ps(),
		 buf(), buf_len(0), parser() {}
};

void emulate_terminal( int fd );
int copy( int src, int dest );
int vt_parser( struct stripstate *state );

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

    /*
    if ( setenv( "TERM", "vt220", true ) < 0 ) {
      perror( "setenv" );
      exit( 1 );
    }
    */

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
  struct stripstate output_stripstate;
  struct pollfd pollfds[ 2 ];

  pollfds[ 0 ].fd = STDIN_FILENO;
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = fd;
  pollfds[ 1 ].events = POLLIN;

  output_stripstate.src_fd = fd;
  output_stripstate.dest_fd = STDOUT_FILENO;
  output_stripstate.buf_len = 0;
  memset( &output_stripstate.ps, 0, sizeof( output_stripstate.ps ) );

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
      if ( vt_parser( &output_stripstate ) < 0 ) {
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

int vt_parser( struct stripstate *state )
{
  /* fill buffer if possible */
  ssize_t bytes_read = read( state->src_fd, state->buf + state->buf_len,
			     buf_size - state->buf_len );
  if ( bytes_read == 0 ) { /* EOF */
    return -1;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return -1;
  }

  state->buf_len += bytes_read;

  /* translate buffer from UTF-8 to wide characters */
  wchar_t out_buffer[ buf_size ];
  size_t in_index = 0, out_index = 0;

  while ( 1 ) {
    assert( in_index <= state->buf_len );
    if ( in_index == state->buf_len ) {
      state->buf_len = 0;
      break;
    }

    wchar_t pwc;
    size_t bytes_parsed = mbrtowc( &pwc, state->buf + in_index,
				   state->buf_len - in_index,
				   &state->ps );
    /* this returns 0 when n = 0! */

    /* This function annoying returns a size_t so we have to check
       the negative values first before the "> 0" branch */

    if ( bytes_parsed == 0 ) {
      /* character was NUL */
      in_index++; /* this relies on knowing UTF-8 NUL is one byte! */
      assert( out_index < buf_size );
      out_buffer[ out_index++ ] = L'\0';
    } else if ( bytes_parsed == (size_t) -1 ) {
      /* invalid sequence */
      assert( errno == EILSEQ );
      in_index++;
      assert( out_index < buf_size );
      out_buffer[ out_index++ ] = (wchar_t) 0xFFFD;
      memset( &state->ps, 0, sizeof( state->ps ) );
    } else if ( bytes_parsed == (size_t) -2 ) {
      /* can't parse complete multibyte character */
      memmove( state->buf, state->buf + in_index,
	       state->buf_len - in_index );
      state->buf_len = state->buf_len - in_index;
      break;
    } else if ( bytes_parsed > 0 ) {
      /* parsed something */
      in_index += bytes_parsed;
      assert( out_index < buf_size );
      out_buffer[ out_index++ ] = pwc;
    } else {
      fprintf( stderr, "Unknown return value %d from mbrtowc\n",
	       bytes_parsed );
      exit( 1 );
    }
  }

  /* feed to vtparse */
  for ( size_t i = 0; i < out_index; i++ ) {
    std::vector<Parser::Action *> actions = state->parser.input( out_buffer[ i ] );
    for ( std::vector<Parser::Action *>::iterator j = actions.begin();
	  j != actions.end();
	  j++ ) {

      Parser::Action *act = *j;
      assert( act );

      if ( act->char_present ) {
	printf( "%s(0x%02x=%lc) ", act->name.c_str(), act->ch, act->ch );
      } else {
	printf( "[%s] ", act->name.c_str() );
      }

      delete act;

      fflush( stdout );
    }
  }

  return 0;
}
