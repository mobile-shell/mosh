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
#include <sys/signalfd.h>
#include <termios.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/time.h>

#include "parser.hpp"
#include "completeterminal.hpp"
#include "swrite.hpp"

const size_t buf_size = 16384;

void emulate_terminal( int fd );

int main( void )
{
  int master;
  struct termios saved_termios, raw_termios, child_termios;

  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
    exit( 1 );
  }

  if ( strcmp( nl_langinfo( CODESET ), "UTF-8" ) != 0 ) {
    fprintf( stderr, "stm requires a UTF-8 locale.\n" );
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
    if ( setenv( "TERM", "xterm", true ) < 0 ) {
      perror( "setenv" );
      exit( 1 );
    }

    /* ask ncurses to send UTF-8 instead of ISO 2022 for line-drawing chars */
    if ( setenv( "NCURSES_NO_UTF8_ACS", "1", true ) < 0 ) {
      perror( "setenv" );
      exit( 1 );
    }

    /* get shell name */
    struct passwd *pw = getpwuid( geteuid() );
    if ( pw == NULL ) {
      perror( "getpwuid" );
      exit( 1 );
    }

    char *my_argv[ 2 ];
    my_argv[ 0 ] = strdup( pw->pw_shell );
    assert( my_argv[ 0 ] );
    my_argv[ 1 ] = NULL;

    if ( execve( pw->pw_shell, my_argv, environ ) < 0 ) {
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

  printf( "[stm is exiting.]\n" );

  return 0;
}

/* Print a frame */
void tick( Terminal::Framebuffer &state, const Terminal::Framebuffer &new_frame )
{
  static bool initialized = false;

  if ( !(state == new_frame) ) {
    Terminal::Display::debug_print( new_frame );
  }

  std::string update = Terminal::Display::new_frame( initialized, state, new_frame );
  swrite( STDOUT_FILENO, update.c_str() );

  state = new_frame;

  initialized = true;
}

/* This is the main loop.

   1. New bytes from the user get applied to the terminal emulator
      as "UserByte" actions.

   2. New bytes from the host get sent to the Parser, and then
      those actions are applied to the terminal.

   3. Resize events (from a SIGWINCH signal) get turned into
      "Resize" actions and applied to the terminal.

   At every event from poll(), we run the tick() function to
   possibly print a new frame (if we haven't printed one in the
   last 1/50 second). The new frames are "differential" -- they
   assume the previous frame was sent to the real terminal.
*/

void emulate_terminal( int fd )
{
  /* establish WINCH fd and start listening for signal */
  sigset_t signal_mask;
  assert( sigemptyset( &signal_mask ) == 0 );
  assert( sigaddset( &signal_mask, SIGWINCH ) == 0 );

  /* stop "ignoring" WINCH signal */
  assert( sigprocmask( SIG_BLOCK, &signal_mask, NULL ) == 0 );

  int winch_fd = signalfd( -1, &signal_mask, 0 );
  if ( winch_fd < 0 ) {
    perror( "signalfd" );
    return;
  }

  /* get current window size */
  struct winsize window_size;
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCGWINSZ" );
    return;
  }

  /* tell child process */
  if ( ioctl( fd, TIOCSWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCSWINSZ" );
    return;
  }

  /* open parser and terminal */
  Terminal::Complete complete( window_size.ws_col, window_size.ws_row );
  Terminal::Framebuffer state( window_size.ws_col, window_size.ws_row );

  struct pollfd pollfds[ 3 ];

  pollfds[ 0 ].fd = STDIN_FILENO;
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = fd;
  pollfds[ 1 ].events = POLLIN;

  pollfds[ 2 ].fd = winch_fd;
  pollfds[ 2 ].events = POLLIN;

  swrite( STDOUT_FILENO, Terminal::Emulator::open().c_str() );

  while ( 1 ) {
    int active_fds = poll( pollfds, 3, -1 );
    if ( active_fds < 0 ) {
      perror( "poll" );
      break;
    }

    if ( pollfds[ 0 ].revents & POLLIN ) {
      /* input from user */
      char buf[ buf_size ];

      /* fill buffer if possible */
      ssize_t bytes_read = read( pollfds[ 0 ].fd, buf, buf_size );
      if ( bytes_read == 0 ) { /* EOF */
	return;
      } else if ( bytes_read < 0 ) {
	perror( "read" );
	return;
      }
      
      std::string terminal_to_host;
      
      for ( int i = 0; i < bytes_read; i++ ) {
	Parser::UserByte ub( buf[ i ] );
	terminal_to_host += complete.act( &ub );
      }
      
      if ( swrite( fd, terminal_to_host.c_str(), terminal_to_host.length() ) < 0 ) {
	break;
      }
    } else if ( pollfds[ 1 ].revents & POLLIN ) {
      /* input from host */
      char buf[ buf_size ];

      /* fill buffer if possible */
      ssize_t bytes_read = read( pollfds[ 1 ].fd, buf, buf_size );
      if ( bytes_read == 0 ) { /* EOF */
	return;
      } else if ( bytes_read < 0 ) {
	perror( "read" );
	return;
      }
      
      std::string terminal_to_host = complete.act( std::string( buf, bytes_read ) );
      if ( swrite( fd, terminal_to_host.c_str(), terminal_to_host.length() ) < 0 ) {
	break;
      }
    } else if ( pollfds[ 2 ].revents & POLLIN ) {
      /* resize */
      struct signalfd_siginfo info;
      assert( read( winch_fd, &info, sizeof( info ) ) == sizeof( info ) );
      assert( info.ssi_signo == SIGWINCH );

      /* get new size */
      if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
	perror( "ioctl TIOCGWINSZ" );
	return;
      }

      /* tell emulator */
      Parser::Resize r( window_size.ws_col, window_size.ws_row );
      complete.act( &r );

      /* tell child process */
      if ( ioctl( fd, TIOCSWINSZ, &window_size ) < 0 ) {
	perror( "ioctl TIOCSWINSZ" );
	return;
      }
    } else if ( (pollfds[ 0 ].revents | pollfds[ 1 ].revents)
		& (POLLERR | POLLHUP | POLLNVAL) ) {
      break;
    }

    tick( state, complete.get_fb() );
  }

  std::string update = Terminal::Display::new_frame( true, state, complete.get_fb() );
  swrite( STDOUT_FILENO, update.c_str() );

  swrite( STDOUT_FILENO, Terminal::Emulator::close().c_str() );
}
