#include <locale.h>
#include <string.h>
#include <langinfo.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <pty.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <sys/signalfd.h>

#include "networktransport.hpp"
#include "completeterminal.hpp"
#include "swrite.hpp"
#include "user.hpp"

void client( const char *ip, int port, const char *key );

using namespace std;

int main( int argc, char *argv[] )
{
  /* Get arguments */
  char *ip, *key;
  int port;

  if ( argc != 4 ) {
    fprintf( stderr, "Usage: %s IP PORT KEY\n", argv[ 0 ] );
    exit( 1 );
  }

  ip = argv[ 1 ];
  port = atoi( argv[ 2 ] );
  key = argv[ 3 ];

  struct termios saved_termios, raw_termios;

  /* Adopt implementation locale */
  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
    exit( 1 );
  }

  /* Verify locale calls for UTF-8 */
  if ( strcmp( nl_langinfo( CODESET ), "UTF-8" ) != 0 ) {
    fprintf( stderr, "stm requires a UTF-8 locale.\n" );
    exit( 1 );
  }

  /* Verify terminal configuration */
  if ( tcgetattr( STDIN_FILENO, &saved_termios ) < 0 ) {
    perror( "tcgetattr" );
    exit( 1 );
  }

  /* Put terminal driver in raw mode */
  raw_termios = saved_termios;
  if ( !(raw_termios.c_iflag & IUTF8) ) {
    fprintf( stderr, "Warning: Locale is UTF-8 but termios IUTF8 flag not set. Setting IUTF8 flag.\n" );
    raw_termios.c_iflag |= IUTF8;
  }

  cfmakeraw( &raw_termios );

  if ( tcsetattr( STDIN_FILENO, TCSANOW, &raw_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
  }

  /* Put terminal in application-cursor-key mode */
  swrite( STDOUT_FILENO, Terminal::Emulator::open().c_str() );

  client( ip, port, key );

  /* Restore terminal and terminal-driver state */
  swrite( STDOUT_FILENO, Terminal::Emulator::close().c_str() );

  if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
    perror( "tcsetattr" );
    exit( 1 );
  }

  printf( "[stm is exiting.]\n" );

  return 0;
}

void client( const char *ip, int port, const char *key )
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

  /* get initial window size */
  struct winsize window_size;
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCGWINSZ" );
    return;
  }

  /* XXX transmit initial resize and initialize */

  /* local state */
  Terminal::Complete terminal( window_size.ws_col, window_size.ws_row );
  Terminal::Framebuffer state( window_size.ws_col, window_size.ws_row );

  /* open network */
  Network::UserStream blank;
  Network::Transport< Network::UserStream, Terminal::Complete > network( blank, terminal,
									 key, ip, port );

  /* prepare to poll for events */
  struct pollfd pollfds[ 3 ];

  pollfds[ 0 ].fd = network.fd();
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = STDIN_FILENO;
  pollfds[ 1 ].events = POLLIN;

  pollfds[ 2 ].fd = winch_fd;
  pollfds[ 2 ].events = POLLIN;

  uint64_t last_remote_num = network.get_remote_state_num();

  while ( 1 ) {
    int active_fds = poll( pollfds, 3, network.tick() );
    if ( active_fds < 0 ) {
      perror( "poll" );
      break;
    }

    if ( pollfds[ 0 ].revents & POLLIN ) {
      /* packet received from the network */
      network.recv();

      /* is a new frame available from the terminal? */
      if ( network.get_remote_state_num() != last_remote_num ) {
	string diff = network.get_remote_diff();
	swrite( STDOUT_FILENO, diff.data(), diff.size() );
      }
    }
    
    if ( pollfds[ 1 ].revents & POLLIN ) {
      /* input from the user needs to be fed to the network */
      const int buf_size = 16384;
      char buf[ buf_size ];

      /* fill buffer if possible */
      ssize_t bytes_read = read( pollfds[ 1 ].fd, buf, buf_size );
      if ( bytes_read == 0 ) { /* EOF */
	return;
      } else if ( bytes_read < 0 ) {
	perror( "read" );
	return;
      }

      for ( int i = 0; i < bytes_read; i++ ) {
	network.get_current_state().push_back( Parser::UserByte( buf[ i ] ) );
      }
    }

    if ( pollfds[ 2 ].revents & POLLIN ) {
      /* handle resize */
    }

    if ( (pollfds[ 0 ].revents | pollfds[ 1 ].revents)
	 & (POLLERR | POLLHUP | POLLNVAL) ) {
      break;
    }
  }
}
