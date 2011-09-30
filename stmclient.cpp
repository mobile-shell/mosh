#include <locale.h>
#include <string.h>
#include <langinfo.h>
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

#include "stmclient.hpp"
#include "swrite.hpp"
#include "networktransport.hpp"
#include "completeterminal.hpp"
#include "user.hpp"

void STMClient::init( void )
{
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
}

void STMClient::shutdown( void )
{
  /* Restore terminal and terminal-driver state */
  swrite( STDOUT_FILENO, Terminal::Emulator::close().c_str() );
  
  if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
    perror( "tcsetattr" );
    exit( 1 );
  }
}

void STMClient::main( void )
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

  /* establish fd for shutdown signals */
  assert( sigemptyset( &signal_mask ) == 0 );
  assert( sigaddset( &signal_mask, SIGTERM ) == 0 );
  assert( sigaddset( &signal_mask, SIGINT ) == 0 );
  assert( sigaddset( &signal_mask, SIGHUP ) == 0 );
  assert( sigaddset( &signal_mask, SIGPIPE ) == 0 );

  /* don't let signals kill us */
  assert( sigprocmask( SIG_BLOCK, &signal_mask, NULL ) == 0 );

  int shutdown_signal_fd = signalfd( -1, &signal_mask, 0 );
  if ( shutdown_signal_fd < 0 ) {
    perror( "signalfd" );
    return;
  }

  /* get initial window size */
  struct winsize window_size;
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCGWINSZ" );
    return;
  }

  /* local state */
  Terminal::Complete terminal( window_size.ws_col, window_size.ws_row );

  /* initialize screen */
  string init = Terminal::Display::new_frame( false, terminal.get_fb(), terminal.get_fb() );
  swrite( STDOUT_FILENO, init.data(), init.size() );

  /* open network */
  Network::UserStream blank;
  Network::Transport< Network::UserStream, Terminal::Complete > network( blank, terminal,
									 key.c_str(), ip.c_str(), port );

  /* tell server the size of the terminal */
  network.get_current_state().push_back( Parser::Resize( window_size.ws_col, window_size.ws_row ) );

  /* prepare to poll for events */
  struct pollfd pollfds[ 4 ];

  pollfds[ 0 ].fd = network.fd();
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = STDIN_FILENO;
  pollfds[ 1 ].events = POLLIN;

  pollfds[ 2 ].fd = winch_fd;
  pollfds[ 2 ].events = POLLIN;

  pollfds[ 3 ].fd = shutdown_signal_fd;
  pollfds[ 3 ].events = POLLIN;

  uint64_t last_remote_num = network.get_remote_state_num();

  while ( 1 ) {
    try {
      int active_fds = poll( pollfds, 4, network.wait_time() );
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

	if ( !network.shutdown_in_progress() ) {
	  for ( int i = 0; i < bytes_read; i++ ) {
	    network.get_current_state().push_back( Parser::UserByte( buf[ i ] ) );
	  }
	}
      }

      if ( pollfds[ 2 ].revents & POLLIN ) {
	/* resize */
	struct signalfd_siginfo info;
	assert( read( winch_fd, &info, sizeof( info ) ) == sizeof( info ) );
	assert( info.ssi_signo == SIGWINCH );
	
	/* get new size */
	if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
	  perror( "ioctl TIOCGWINSZ" );
	  return;
	}
	
	/* tell remote emulator */
	Parser::Resize res( window_size.ws_col, window_size.ws_row );

	if ( !network.shutdown_in_progress() ) {
	  network.get_current_state().push_back( res );
	}

	/* tell local emulator -- there is probably a safer way to do this */
	for ( list< Network::TimestampedState<Terminal::Complete> >::iterator i = network.begin();
	      i != network.end();
	      i++ ) {
	  i->state.act( &res );
	}
      }

      if ( pollfds[ 3 ].revents & POLLIN ) {
	/* shutdown signal */
	if ( network.attached() ) {
	  network.start_shutdown();
	} else {
	  break;
	}
      }

      if ( (pollfds[ 0 ].revents)
	   & (POLLERR | POLLHUP | POLLNVAL) ) {
	/* network problem */
	break;
      }

      if ( (pollfds[ 1 ].revents)
	   & (POLLERR | POLLHUP | POLLNVAL) ) {
	/* user problem */
	network.start_shutdown();
      }

      /* quit if our shutdown has been acknowledged */
      if ( network.shutdown_in_progress() && network.shutdown_acknowledged() ) {
	break;
      }

      /* quit if we received and acknowledged a shutdown request */
      if ( network.counterparty_shutdown_ack_sent() ) {
	break;
      }

      network.tick();
    } catch ( Network::NetworkException e ) {
      fprintf( stderr, "%s: %s\r\n", e.function.c_str(), strerror( e.the_errno ) );
      sleep( 1 );
    }
  }
}
