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

#include <errno.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif

#include "stmclient.h"
#include "swrite.h"
#include "completeterminal.h"
#include "user.h"
#include "fatal_assert.h"
#include "locale_utils.h"
#include "pty_compat.h"
#include "select.h"
#include "timestamp.h"

#include "networktransport.cc"

void STMClient::resume( void )
{
  /* Restore termios state */
  if ( tcsetattr( STDIN_FILENO, TCSANOW, &raw_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
  }

  /* Put terminal in application-cursor-key mode */
  swrite( STDOUT_FILENO, Terminal::Emulator::open().c_str() );

  /* Flag that outer terminal state is unknown */
  repaint_requested = true;
}

void STMClient::init( void )
{
  if ( !is_utf8_locale() ) {
    LocaleVar native_ctype = get_ctype();
    string native_charset( locale_charset() );

    fprintf( stderr, "mosh-client needs a UTF-8 native locale to run.\n\n" );
    fprintf( stderr, "Unfortunately, the client's environment (%s) specifies\nthe character set \"%s\".\n\n", native_ctype.str().c_str(), native_charset.c_str() );
    int unused __attribute((unused)) = system( "locale" );
    exit( 1 );
  }

  /* Verify terminal configuration */
  if ( tcgetattr( STDIN_FILENO, &saved_termios ) < 0 ) {
    perror( "tcgetattr" );
    exit( 1 );
  }

  /* Put terminal driver in raw mode */
  raw_termios = saved_termios;

#ifdef HAVE_IUTF8
  if ( !(raw_termios.c_iflag & IUTF8) ) {
    //    fprintf( stderr, "Warning: Locale is UTF-8 but termios IUTF8 flag not set. Setting IUTF8 flag.\n" );
    /* Probably not really necessary since we are putting terminal driver into raw mode anyway. */
    raw_termios.c_iflag |= IUTF8;
  }
#endif /* HAVE_IUTF8 */

  cfmakeraw( &raw_termios );

  if ( tcsetattr( STDIN_FILENO, TCSANOW, &raw_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
  }

  /* Put terminal in application-cursor-key mode */
  swrite( STDOUT_FILENO, Terminal::Emulator::open().c_str() );

  /* Add our name to window title */
  if ( !getenv( "MOSH_TITLE_NOPREFIX" ) ) {
    overlays.set_title_prefix( wstring( L"[mosh] " ) );
  }

  wchar_t tmp[ 128 ];
  swprintf( tmp, 128, L"Nothing received from server on UDP port %d.", port );
  connecting_notification = wstring( tmp );
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

  if ( still_connecting() ) {
    fprintf( stderr, "\nmosh did not make a successful connection to %s:%d.\n", ip.c_str(), port );
    fprintf( stderr, "Please verify that UDP port %d is not firewalled and can reach the server.\n\n", port );
    fprintf( stderr, "(By default, mosh uses a UDP port between 60000 and 61000. The -p option\nselects a specific UDP port number.)\n" );
  } else if ( network ) {
    if ( !clean_shutdown ) {
      fprintf( stderr, "\n\nmosh did not shut down cleanly. Please note that the\nmosh-server process may still be running on the server.\n" );
    }
  }
}

void STMClient::main_init( void )
{
  Select &sel = Select::get_instance();
  sel.add_signal( SIGWINCH );
  sel.add_signal( SIGTERM );
  sel.add_signal( SIGINT );
  sel.add_signal( SIGHUP );
  sel.add_signal( SIGPIPE );
  sel.add_signal( SIGCONT );

  /* get initial window size */
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ) {
    perror( "ioctl TIOCGWINSZ" );
    return;
  }  

  /* local state */
  local_framebuffer = new Terminal::Framebuffer( window_size.ws_col, window_size.ws_row );
  new_state = new Terminal::Framebuffer( 1, 1 );

  /* initialize screen */
  string init = display.new_frame( false, *local_framebuffer, *local_framebuffer );
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
  if ( !network ) { /* clean shutdown even when not initialized */
    return;
  }

  /* fetch target state */
  *new_state = network->get_latest_remote_state().state.get_fb();

  /* apply local overlays */
  overlays.apply( *new_state );

  /* apply any mutations */
  display.downgrade( *new_state );

  /* calculate minimal difference from where we are */
  const string diff( display.new_frame( !repaint_requested,
					*local_framebuffer,
					*new_state ) );
  swrite( STDOUT_FILENO, diff.data(), diff.size() );

  repaint_requested = false;

  /* switch pointers */
  Terminal::Framebuffer *tmp = new_state;
  new_state = local_framebuffer;
  local_framebuffer = tmp;
}

bool STMClient::process_network_input( void )
{
  network->recv();
  
  /* Now give hints to the overlays */
  overlays.get_notification_engine().server_heard( network->get_latest_remote_state().timestamp );
  overlays.get_notification_engine().server_acked( network->get_sent_state_acked_timestamp() );

  overlays.get_prediction_engine().set_local_frame_acked( network->get_sent_state_acked() );
  overlays.get_prediction_engine().set_send_interval( network->send_interval() );
  overlays.get_prediction_engine().set_local_frame_late_acked( network->get_latest_remote_state().state.get_echo_ack() );

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
	  if ( network->has_remote_addr() && (!network->shutdown_in_progress()) ) {
	    overlays.get_notification_engine().set_notification_string( wstring( L"Exiting on user request..." ), true );
	    network->start_shutdown();
	    return true;
	  } else {
	    return false;
	  }
	} else if ( the_byte == 0x1a ) { /* Suspend sequence is Ctrl-^ Ctrl-Z */
	  /* Restore terminal and terminal-driver state */
	  swrite( STDOUT_FILENO, Terminal::Emulator::close().c_str() );

	  if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
	    perror( "tcsetattr" );
	    exit( 1 );
	  }

	  /* clear screen */
	  printf( "\033[H\033[2J" );
	  printf( "\033[37;44m[mosh is suspended.]\n\033[m" );

	  fflush( NULL );

	  /* actually suspend */
	  raise( SIGTSTP );
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
  Select &sel = Select::get_instance();

  while ( 1 ) {
    try {
      output_new_frame();

      int wait_time = min( network->wait_time(), overlays.wait_time() );

      /* Handle startup "Connecting..." message */
      if ( still_connecting() ) {
	wait_time = min( 250, wait_time );
      }

      /* poll for events */
      /* network->fd() can in theory change over time */
      sel.clear_fds();
      std::vector< int > fd_list( network->fds() );
      for ( std::vector< int >::const_iterator it = fd_list.begin();
	    it != fd_list.end();
	    it++ ) {
	sel.add_fd( *it );
      }
      sel.add_fd( STDIN_FILENO );

      int active_fds = sel.select( wait_time );
      if ( active_fds < 0 ) {
	perror( "select" );
	break;
      }

      bool network_ready_to_read = false;

      for ( std::vector< int >::const_iterator it = fd_list.begin();
	    it != fd_list.end();
	    it++ ) {
	if ( sel.read( *it ) ) {
	  /* packet received from the network */
	  /* we only read one socket each run */
	  network_ready_to_read = true;
	}

	if ( sel.error( *it ) ) {
	  /* network problem */
	  break;
	}
      }

      if ( network_ready_to_read ) {
	if ( !process_network_input() ) { return; }
      }
    
      if ( sel.read( STDIN_FILENO ) ) {
	/* input from the user needs to be fed to the network */
	if ( !process_user_input( STDIN_FILENO ) ) {
	  if ( !network->has_remote_addr() ) {
	    break;
	  } else if ( !network->shutdown_in_progress() ) {
	    overlays.get_notification_engine().set_notification_string( wstring( L"Exiting..." ), true );
	    network->start_shutdown();
	  }
	}
      }

      if ( sel.signal( SIGWINCH ) ) {
        /* resize */
        if ( !process_resize() ) { return; }
      }

      if ( sel.signal( SIGCONT ) ) {
	resume();
      }

      if ( sel.signal( SIGTERM )
           || sel.signal( SIGINT )
           || sel.signal( SIGHUP )
           || sel.signal( SIGPIPE ) ) {
        /* shutdown signal */
        if ( !network->has_remote_addr() ) {
          break;
        } else if ( !network->shutdown_in_progress() ) {
          overlays.get_notification_engine().set_notification_string( wstring( L"Signal received, shutting down..." ), true );
          network->start_shutdown();
        }
      }

      if ( sel.error( STDIN_FILENO ) ) {
	/* user problem */
	if ( !network->has_remote_addr() ) {
	  break;
	} else if ( !network->shutdown_in_progress() ) {
	  overlays.get_notification_engine().set_notification_string( wstring( L"Exiting..." ), true );
	  network->start_shutdown();
	}
      }

      /* quit if our shutdown has been acknowledged */
      if ( network->shutdown_in_progress() && network->shutdown_acknowledged() ) {
	clean_shutdown = true;
	break;
      }

      /* quit after shutdown acknowledgement timeout */
      if ( network->shutdown_in_progress() && network->shutdown_ack_timed_out() ) {
	break;
      }

      /* quit if we received and acknowledged a shutdown request */
      if ( network->counterparty_shutdown_ack_sent() ) {
	clean_shutdown = true;
	break;
      }

      /* write diagnostic message if can't reach server */
      if ( still_connecting()
	   && (!network->shutdown_in_progress())
	   && (timestamp() - network->get_latest_remote_state().timestamp > 250) ) {
	if ( timestamp() - network->get_latest_remote_state().timestamp > 15000 ) {
	  if ( !network->shutdown_in_progress() ) {
	    overlays.get_notification_engine().set_notification_string( wstring( L"Timed out waiting for server..." ), true );
	    network->start_shutdown();
	  }
	} else {
	  overlays.get_notification_engine().set_notification_string( connecting_notification );
	}
      } else if ( (network->get_remote_state_num() != 0)
		  && (overlays.get_notification_engine().get_notification_string()
		      == connecting_notification) ) {
	overlays.get_notification_engine().set_notification_string( L"" );
      }

      network->tick();

      const Network::NetworkException *exn = network->get_send_exception();
      if ( exn ) {
        overlays.get_notification_engine().set_network_exception( *exn );
      } else {
        overlays.get_notification_engine().clear_network_exception();
      }
    } catch ( Network::NetworkException e ) {
      if ( !network->shutdown_in_progress() ) {
        overlays.get_notification_engine().set_network_exception( e );
      }

      struct timespec req;
      req.tv_sec = 0;
      req.tv_nsec = 200000000; /* 0.2 sec */
      nanosleep( &req, NULL );
      freeze_timestamp();
    } catch ( Crypto::CryptoException e ) {
      if ( e.fatal ) {
        throw;
      } else {
        wchar_t tmp[ 128 ];
        swprintf( tmp, 128, L"Crypto exception: %s", e.text.c_str() );
        overlays.get_notification_engine().set_notification_string( wstring( tmp ) );
      }
    }
  }
}

