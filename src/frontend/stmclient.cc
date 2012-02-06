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
#include <time.h>

#include "stmclient.h"
#include "swrite.h"
#include "completeterminal.h"
#include "user.h"

#include "networktransport.cc"

void STMClient::init( void )
{
  /* Verify locale calls for UTF-8 */
  if ( strcmp( nl_langinfo( CODESET ), "UTF-8" ) != 0 ) {
    fprintf( stderr, "mosh requires a UTF-8 locale.\n" );
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
    /* SSH should also convey IUTF8 across connection. */
    //    fprintf( stderr, "Warning: Locale is UTF-8 but termios IUTF8 flag not set. Setting IUTF8 flag.\n" );
    raw_termios.c_iflag |= IUTF8;
  }

  cfmakeraw( &raw_termios );

  if ( tcsetattr( STDIN_FILENO, TCSANOW, &raw_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
  }

  /* Put terminal in application-cursor-key mode */
  swrite( STDOUT_FILENO, Terminal::Emulator::open().c_str() );

  /* Add our name to window title */
  overlays.set_title_prefix( wstring( L"[mosh] " ) );
}

void STMClient::shutdown( void )
{
  /* Restore screen state */
  overlays.get_notification_engine().set_notification_string( wstring( L"" ) );
  overlays.get_notification_engine().server_heard( timestamp() );
  overlays.set_title_prefix( wstring( L"" ) );
  output_new_frame();

  /* Restore terminal and terminal-driver state */
  swrite( STDOUT_FILENO, Terminal::Emulator::close().c_str() );
  
  if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
    perror( "tcsetattr" );
    exit( 1 );
  }
}

void STMClient::main_init( void )
{
  /* establish WINCH fd and start listening for signal */
  sigset_t signal_mask;
  assert( sigemptyset( &signal_mask ) == 0 );
  assert( sigaddset( &signal_mask, SIGWINCH ) == 0 );

  /* stop "ignoring" WINCH signal */
  assert( sigprocmask( SIG_BLOCK, &signal_mask, NULL ) == 0 );

  winch_fd = signalfd( -1, &signal_mask, 0 );
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
  assert( sigaddset( &signal_mask, SIGTSTP ) == 0 );
  assert( sigaddset( &signal_mask, SIGSTOP ) == 0 );
  assert( sigaddset( &signal_mask, SIGCONT ) == 0 );

  /* don't let signals kill us */
  assert( sigprocmask( SIG_BLOCK, &signal_mask, NULL ) == 0 );

  shutdown_signal_fd = signalfd( -1, &signal_mask, 0 );
  if ( shutdown_signal_fd < 0 ) {
    perror( "signalfd" );
    return;
  }

  /* get initial window size */
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCGWINSZ" );
    return;
  }  

  /* local state */
  local_framebuffer = new Terminal::Framebuffer( window_size.ws_col, window_size.ws_row );

  /* initialize screen */
  string init = Terminal::Display::new_frame( false, *local_framebuffer, *local_framebuffer );
  swrite( STDOUT_FILENO, init.data(), init.size() );

  /* open network */
  Network::UserStream blank;
  Terminal::Complete local_terminal( window_size.ws_col, window_size.ws_row );
  network = new Network::Transport< Network::UserStream, Terminal::Complete >( blank, local_terminal,
									       key.c_str(), ip.c_str(), port );

  network->set_send_delay( 1 ); /* minimal delay on outgoing keystrokes */

  /* tell server the size of the terminal */
  network->get_current_state().push_back( Parser::Resize( window_size.ws_col, window_size.ws_row ) );
}

void STMClient::output_new_frame( void )
{
  /* fetch target state */
  Terminal::Framebuffer new_state( network->get_latest_remote_state().state.get_fb() );

  /* apply local overlays */
  overlays.apply( new_state );

  /* calculate minimal difference from where we are */
  const string diff( Terminal::Display::new_frame( !repaint_requested,
						   *local_framebuffer,
						   new_state ) );
  swrite( STDOUT_FILENO, diff.data(), diff.size() );
  *local_framebuffer = new_state;  

  repaint_requested = false;
}

bool STMClient::process_network_input( void )
{
  network->recv();
  
  /* Now give hints to the overlays */
  overlays.get_notification_engine().server_heard( network->get_latest_remote_state().timestamp );

  overlays.get_prediction_engine().set_local_frame_acked( network->get_sent_state_acked() );
  overlays.get_prediction_engine().set_send_interval( network->send_interval() );
  overlays.get_prediction_engine().set_local_frame_late_acked( network->get_sent_state_late_acked() );

  return true;
}

bool STMClient::process_user_input( int fd )
{
  const int buf_size = 16384;
  char buf[ buf_size ];

  /* fill buffer if possible */
  ssize_t bytes_read = read( fd, buf, buf_size );
  if ( bytes_read == 0 ) { /* EOF */
    return false;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return false;
  }

  if ( !network->shutdown_in_progress() ) {
    overlays.get_prediction_engine().set_local_frame_sent( network->get_sent_state_last() );

    for ( int i = 0; i < bytes_read; i++ ) {
      char the_byte = buf[ i ];

      overlays.get_prediction_engine().new_user_byte( the_byte, *local_framebuffer );

      if ( quit_sequence_started ) {
	if ( the_byte == '.' ) { /* Quit sequence is Ctrl-^ . */
	  if ( network->attached() && (!network->shutdown_in_progress()) ) {
	    overlays.get_notification_engine().set_notification_string( wstring( L"Exiting on user request..." ) );
	    network->start_shutdown();
	    return true;
	  } else {
	    return false;
	  }
	} else if ( the_byte == '^' ) {
	  /* Emulation sequence to type Ctrl-^ is Ctrl-^ ^ */
	  network->get_current_state().push_back( Parser::UserByte( 0x1E ) );
	} else {
	  /* Ctrl-^ followed by anything other than . and ^ gets sent literally */
	  network->get_current_state().push_back( Parser::UserByte( 0x1E ) );
	  network->get_current_state().push_back( Parser::UserByte( the_byte ) );	  
	}

	quit_sequence_started = false;
	continue;
      }

      quit_sequence_started = (the_byte == 0x1E);
      if ( quit_sequence_started ) {
	continue;
      }

      if ( the_byte == 0x0C ) { /* Ctrl-L */
	repaint_requested = true;
      }

      network->get_current_state().push_back( Parser::UserByte( the_byte ) );		
    }
  }

  return true;
}

bool STMClient::process_resize( void )
{
  struct signalfd_siginfo info;
  assert( read( winch_fd, &info, sizeof( info ) ) == sizeof( info ) );
  assert( info.ssi_signo == SIGWINCH );
  
  /* get new size */
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCGWINSZ" );
    return false;
  }
  
  /* tell remote emulator */
  Parser::Resize res( window_size.ws_col, window_size.ws_row );
  
  if ( !network->shutdown_in_progress() ) {
    network->get_current_state().push_back( res );
  }

  /* note remote emulator will probably reply with its own Resize to adjust our state */
  
  /* tell prediction engine */
  overlays.get_prediction_engine().reset();

  return true;
}

void STMClient::main( void )
{
  /* initialize signal handling and structures */
  main_init();

  /* prepare to poll for events */
  struct pollfd pollfds[ 4 ];

  pollfds[ 0 ].fd = network->fd();
  pollfds[ 0 ].events = POLLIN;

  pollfds[ 1 ].fd = STDIN_FILENO;
  pollfds[ 1 ].events = POLLIN;

  pollfds[ 2 ].fd = winch_fd;
  pollfds[ 2 ].events = POLLIN;

  pollfds[ 3 ].fd = shutdown_signal_fd;
  pollfds[ 3 ].events = POLLIN;

  while ( 1 ) {
    try {
      output_new_frame();

      int active_fds = poll( pollfds, 4, min( network->wait_time(), overlays.wait_time() ) );
      if ( active_fds < 0 ) {
	perror( "poll" );
	break;
      }

      if ( pollfds[ 0 ].revents & POLLIN ) {
	/* packet received from the network */
	if ( !process_network_input() ) { return; }
      }
    
      if ( pollfds[ 1 ].revents & POLLIN ) {
	/* input from the user needs to be fed to the network */
	if ( !process_user_input( pollfds[ 1 ].fd ) ) {
	  if ( !network->attached() ) {
	    break;
	  } else if ( !network->shutdown_in_progress() ) {
	    overlays.get_notification_engine().set_notification_string( wstring( L"Exiting..." ) );
	    network->start_shutdown();
	  }
	}
      }

      if ( pollfds[ 2 ].revents & POLLIN ) {
	/* resize */
	if ( !process_resize() ) { return; }
      }

      if ( pollfds[ 3 ].revents & POLLIN ) {
	/* shutdown signal */
	struct signalfd_siginfo the_siginfo;
	ssize_t bytes_read = read( pollfds[ 3 ].fd, &the_siginfo, sizeof( the_siginfo ) );
	if ( bytes_read == 0 ) {
	  break;
	} else if ( bytes_read < 0 ) {
	  perror( "read" );
	  break;
	}

	if ( !network->attached() ) {
	  break;
	} else if ( !network->shutdown_in_progress() ) {
	  overlays.get_notification_engine().set_notification_string( wstring( L"Signal received, shutting down..." ) );
	  network->start_shutdown();
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
	if ( !network->attached() ) {
	  break;
	} else if ( !network->shutdown_in_progress() ) {
	  overlays.get_notification_engine().set_notification_string( wstring( L"Exiting..." ) );
	  network->start_shutdown();
	}
      }

      /* quit if our shutdown has been acknowledged */
      if ( network->shutdown_in_progress() && network->shutdown_acknowledged() ) {
	break;
      }

      /* quit after shutdown acknowledgement timeout */
      if ( network->shutdown_in_progress() && network->shutdown_ack_timed_out() ) {
	break;
      }

      /* quit if we received and acknowledged a shutdown request */
      if ( network->counterparty_shutdown_ack_sent() ) {
	break;
      }

      static const wstring connecting_notification( L"Connecting..." );
      if ( (network->get_remote_state_num() == 0) && (!network->shutdown_in_progress()) ) {
	overlays.get_notification_engine().set_notification_string( connecting_notification );
      } else if ( (network->get_remote_state_num() != 0)
		  && (overlays.get_notification_engine().get_notification_string()
		      == connecting_notification) ) {
	overlays.get_notification_engine().set_notification_string( L"" );
      }

      network->tick();
    } catch ( Network::NetworkException e ) {
      if ( !network->shutdown_in_progress() ) {
	wchar_t tmp[ 128 ];
	swprintf( tmp, 128, L"%s: %s", e.function.c_str(), strerror( e.the_errno ) );
	overlays.get_notification_engine().set_notification_string( wstring( tmp ) );
      }

      struct timespec req;
      req.tv_sec = 0;
      req.tv_nsec = 200000000; /* 0.2 sec */
      nanosleep( &req, NULL );
    } catch ( Crypto::CryptoException e ) {
      wchar_t tmp[ 128 ];
      swprintf( tmp, 128, L"Crypto exception: %s", e.text.c_str() );
      overlays.get_notification_engine().set_notification_string( wstring( tmp ) );
    }
  }
}

