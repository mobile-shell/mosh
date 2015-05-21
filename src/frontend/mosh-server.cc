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
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <typeinfo>
#include <signal.h>
#ifdef HAVE_UTEMPTER
#include <utempter.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif

#if FORKPTY_IN_LIBUTIL
#include <libutil.h>
#endif

#include "completeterminal.h"
#include "swrite.h"
#include "user.h"
#include "fatal_assert.h"
#include "locale_utils.h"
#include "pty_compat.h"
#include "select.h"
#include "agent.h"
#include "timestamp.h"
#include "fatal_assert.h"

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif

#include "networktransport.cc"

typedef Network::Transport< Terminal::Complete, Network::UserStream > ServerConnection;

void serve( int host_fd,
	    Terminal::Complete &terminal,
	    ServerConnection &network,
	    Agent::ProxyAgent &agent );

int run_server( const char *desired_ip, const char *desired_port,
		const string &command_path, char *command_argv[],
		const int colors, bool verbose, bool with_motd,
		bool with_agent_fwd );

using namespace std;

void print_usage( const char *argv0 )
{
  fprintf( stderr, "Usage: %s new [-s] [-v] [-i LOCALADDR] [-p PORT[:PORT2]] [-c COLORS] [-l NAME=VALUE] [-- COMMAND...]\n", argv0 );
}

void print_motd( void );
void chdir_homedir( void );
bool motd_hushed( void );
void warn_unattached( const string & ignore_entry );

/* Simple spinloop */
void spin( void )
{
  static unsigned int spincount = 0;
  spincount++;

  if ( spincount > 10 ) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 100000000; /* 0.1 sec */
    nanosleep( &req, NULL );
    freeze_timestamp();
  }
}

string get_SSH_IP( void )
{
  const char *SSH_CONNECTION = getenv( "SSH_CONNECTION" );
  if ( !SSH_CONNECTION ) { /* Older sshds don't set this */
    fprintf( stderr, "Warning: SSH_CONNECTION not found; binding to any interface.\n" );
    return string( "0.0.0.0" );
  }
  istringstream ss( SSH_CONNECTION );
  string dummy, local_interface_IP;
  ss >> dummy >> dummy >> local_interface_IP;
  if ( !ss ) {
    fprintf( stderr, "Warning: Could not parse SSH_CONNECTION; binding to any interface.\n" );
    return string( "0.0.0.0" );
  }

  /* Strip IPv6 prefix. */
  const char IPv6_prefix[] = "::ffff:";

  if ( ( local_interface_IP.length() > strlen( IPv6_prefix ) )
       && ( 0 == strncasecmp( local_interface_IP.c_str(), IPv6_prefix, strlen( IPv6_prefix ) ) ) ) {
    return local_interface_IP.substr( strlen( IPv6_prefix ) );
  }

  return local_interface_IP;
}

int main( int argc, char *argv[] )
{
  /* For security, make sure we don't dump core */
  Crypto::disable_dumping_core();

  /* Detect edge case */
  fatal_assert( argc > 0 );

  const char *desired_ip = NULL;
  string desired_ip_str;
  const char *desired_port = NULL;
  string command_path;
  char **command_argv = NULL;
  int colors = 0;
  bool with_agent_fwd = false;
  bool verbose = false; /* don't close stdin/stdout/stderr */
  /* Will cause mosh-server not to correctly detach on old versions of sshd. */
  list<string> locale_vars;

  /* strip off command */
  for ( int i = 0; i < argc; i++ ) {
    if ( 0 == strcmp( argv[ i ], "--" ) ) { /* -- is mandatory */
      if ( i != argc - 1 ) {
	command_argv = argv + i + 1;
      }
      argc = i; /* rest of options before -- */
      break;
    }
  }

  /* Parse new command-line syntax */
  if ( (argc >= 2)
       && (strcmp( argv[ 1 ], "new" ) == 0) ) {
    /* new option syntax */
    int opt;
    while ( (opt = getopt( argc - 1, argv + 1, "i:p:c:svl:A" )) != -1 ) {
      switch ( opt ) {
      case 'A':
	with_agent_fwd = true;
        break;
      case 'i':
	desired_ip = optarg;
	break;
      case 'p':
	desired_port = optarg;
	break;
      case 's':
	desired_ip_str = get_SSH_IP();
	desired_ip = desired_ip_str.c_str();
	fatal_assert( desired_ip );
	break;
      case 'c':
	colors = myatoi( optarg );
	break;
      case 'v':
	verbose = true;
	break;
      case 'l':
	locale_vars.push_back( string( optarg ) );
	break;
      default:
	print_usage( argv[ 0 ] );
	/* don't die on unknown options */
      }
    }
  } else if ( argc == 1 ) {
    /* legacy argument parsing for older client wrapper script */
    /* do nothing */
  } else if ( argc == 2 ) {
    desired_ip = argv[ 1 ];
  } else if ( argc == 3 ) {
    desired_ip = argv[ 1 ];
    desired_port = argv[ 2 ];
  } else {
    print_usage( argv[ 0 ] );
    exit( 1 );
  }

  /* Sanity-check arguments */
  int dpl, dph;
  if ( desired_port && ! Connection::parse_portrange( desired_port, dpl, dph ) ) {
    fprintf( stderr, "%s: Bad UDP port range (%s)\n", argv[ 0 ], desired_port );
    print_usage( argv[ 0 ] );
    exit( 1 );
  }

  bool with_motd = false;

  /* Get shell */
  char *my_argv[ 2 ];
  string shell_name;
  if ( !command_argv ) {
    /* get shell name */
    struct passwd *pw = getpwuid( geteuid() );
    if ( pw == NULL ) {
      perror( "getpwuid" );
      exit( 1 );
    }

    string shell_path( pw->pw_shell );
    if ( shell_path.empty() ) { /* empty shell means Bourne shell */
      shell_path = _PATH_BSHELL;
    }

    command_path = shell_path;

    size_t shell_slash( shell_path.rfind('/') );
    if ( shell_slash == string::npos ) {
      shell_name = shell_path;
    } else {
      shell_name = shell_path.substr(shell_slash + 1);
    }

    /* prepend '-' to make login shell */
    shell_name = '-' + shell_name;

    my_argv[ 0 ] = const_cast<char *>( shell_name.c_str() );
    my_argv[ 1 ] = NULL;
    command_argv = my_argv;

    with_motd = true;
  }

  if ( command_path.empty() ) {
    command_path = command_argv[0];
  }

  /* Adopt implementation locale */
  set_native_locale();
  if ( !is_utf8_locale() ) {
    /* save details for diagnostic */
    LocaleVar native_ctype = get_ctype();
    string native_charset( locale_charset() );

    /* apply locale-related environment variables from client */
    clear_locale_variables();
    for ( list<string>::const_iterator i = locale_vars.begin();
	  i != locale_vars.end();
	  i++ ) {
      char *env_string = strdup( i->c_str() );
      fatal_assert( env_string );
      if ( 0 != putenv( env_string ) ) {
	perror( "putenv" );
      }
    }

    /* check again */
    set_native_locale();
    if ( !is_utf8_locale() ) {
      LocaleVar client_ctype = get_ctype();
      string client_charset( locale_charset() );

      fprintf( stderr, "mosh-server needs a UTF-8 native locale to run.\n\n" );
      fprintf( stderr, "Unfortunately, the local environment (%s) specifies\nthe character set \"%s\",\n\n", native_ctype.str().c_str(), native_charset.c_str() );
      fprintf( stderr, "The client-supplied environment (%s) specifies\nthe character set \"%s\".\n\n", client_ctype.str().c_str(), client_charset.c_str() );
      int unused __attribute((unused)) = system( "locale" );
      exit( 1 );
    }
  }

  try {
    return run_server( desired_ip, desired_port, command_path, command_argv, colors, verbose, with_motd, with_agent_fwd );
  } catch ( const Network::NetworkException& e ) {
    fprintf( stderr, "Network exception: %s: %s\n",
	     e.function.c_str(), strerror( e.the_errno ) );
    return 1;
  } catch ( const Crypto::CryptoException& e ) {
    fprintf( stderr, "Crypto exception: %s\n",
	     e.text.c_str() );
    return 1;
  }
}

int run_server( const char *desired_ip, const char *desired_port,
		const string &command_path, char *command_argv[],
		const int colors, bool verbose, bool with_motd,
		bool with_agent_fwd ) {
  /* get initial window size */
  struct winsize window_size;
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ||
       window_size.ws_col == 0 ||
       window_size.ws_row == 0 ) {
    fprintf( stderr, "Server started without pseudo-terminal. Opening 80x24 terminal.\n" );

    /* Fill in sensible defaults. */
    /* They will be overwritten by client on first connection. */
    memset( &window_size, 0, sizeof( window_size ) );
    window_size.ws_col = 80;
    window_size.ws_row = 24;
  }

  /* open parser and terminal */
  Terminal::Complete terminal( window_size.ws_col, window_size.ws_row );

  /* open network */
  Network::UserStream blank;
  ServerConnection *network = new ServerConnection( terminal, blank, desired_ip, desired_port );

  if ( verbose ) {
    network->set_verbose();
  }

  printf( "\nMOSH CONNECT %s %s\n", network->port().c_str(), network->get_key().c_str() );
  fflush( stdout );

  /* don't let signals kill us */
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  fatal_assert( 0 == sigfillset( &sa.sa_mask ) );
  fatal_assert( 0 == sigaction( SIGHUP, &sa, NULL ) );
  fatal_assert( 0 == sigaction( SIGPIPE, &sa, NULL ) );


  /* detach from terminal */
  pid_t the_pid = fork();
  if ( the_pid < 0 ) {
    perror( "fork" );
  } else if ( the_pid > 0 ) {
    _exit( 0 );
  }

  fprintf( stderr, "\nmosh-server (%s)\n", PACKAGE_STRING );
  fprintf( stderr, "Copyright 2012 Keith Winstein <mosh-devel@mit.edu>\n" );
  fprintf( stderr, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n\n" );

  fprintf( stderr, "[mosh-server detached, pid = %d]\n", (int)getpid() );

  /* initialize agent listener if requested */
  Agent::ProxyAgent agent( true, ! with_agent_fwd );
  if ( with_agent_fwd && (! agent.active()) ) {
    fprintf( stderr, "Warning: Agent listener initialization failed. Disabling agent forwarding.\n" );
    with_agent_fwd = false;
  }

  int master;

#ifndef HAVE_IUTF8
  fprintf( stderr, "\nWarning: termios IUTF8 flag not defined.\nCharacter-erase of multibyte character sequence\nprobably does not work properly on this platform.\n" );
#endif /* HAVE_IUTF8 */

  /* close file descriptors */
  if ( !verbose ) {
    /* Necessary to properly detach on old versions of sshd (e.g. RHEL/CentOS 5.0). */
    int nullfd;

    nullfd = open( "/dev/null", O_RDWR );
    if ( nullfd == -1 ) {
      perror( "open" );
      exit( 1 );
    }

    if ( dup2 ( nullfd, STDIN_FILENO ) < 0 ||
         dup2 ( nullfd, STDOUT_FILENO ) < 0 ||
         dup2 ( nullfd, STDERR_FILENO ) < 0 ) {
      perror( "dup2" );
      exit( 1 );
    }

    if ( close( nullfd ) < 0 ) {
      perror( "close" );
      exit( 1 );
    }
  }

  char utmp_entry[ 64 ] = { 0 };
  snprintf( utmp_entry, 64, "mosh [%d]", getpid() );

  /* Fork child process */
  pid_t child = forkpty( &master, NULL, NULL, &window_size );

  if ( child == -1 ) {
    perror( "forkpty" );
    exit( 1 );
  }

  if ( child == 0 ) {
    /* child */

    /* reenable signals */
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    fatal_assert( 0 == sigfillset( &sa.sa_mask ) );
    fatal_assert( 0 == sigaction( SIGHUP, &sa, NULL ) );
    fatal_assert( 0 == sigaction( SIGPIPE, &sa, NULL ) );

    /* close server-related file descriptors */
    delete network;

    /* set IUTF8 if available */
#ifdef HAVE_IUTF8
    struct termios child_termios;
    if ( tcgetattr( STDIN_FILENO, &child_termios ) < 0 ) {
      perror( "tcgetattr" );
      exit( 1 );
    }

    child_termios.c_iflag |= IUTF8;

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &child_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }
#endif /* HAVE_IUTF8 */

    /* set TERM */
    const char default_term[] = "xterm";
    const char color_term[] = "xterm-256color";

    if ( setenv( "TERM", (colors == 256) ? color_term : default_term, true ) < 0 ) {
      perror( "setenv" );
      exit( 1 );
    }

    /* set SSH_AUTH_SOCK */
    if ( agent.active() ) {
      if ( setenv( "SSH_AUTH_SOCK", agent.listener_path().c_str(), true ) < 0 ) {
	perror( "setenv" );
	exit( 1 );
      }
    }

    /* ask ncurses to send UTF-8 instead of ISO 2022 for line-drawing chars */
    if ( setenv( "NCURSES_NO_UTF8_ACS", "1", true ) < 0 ) {
      perror( "setenv" );
      exit( 1 );
    }

    /* clear STY environment variable so GNU screen regards us as top level */
    if ( unsetenv( "STY" ) < 0 ) {
      perror( "unsetenv" );
      exit( 1 );
    }

    chdir_homedir();

    if ( with_motd && (!motd_hushed()) ) {
      print_motd();
      warn_unattached( utmp_entry );
    }

    Crypto::reenable_dumping_core();

    if ( execvp( command_path.c_str(), command_argv ) < 0 ) {
      perror( "execvp" );
      _exit( 1 );
    }
  } else {
    /* parent */

#ifdef HAVE_UTEMPTER
    /* make utmp entry */
    utempter_add_record( master, utmp_entry );
#endif

    try {
      serve( master, terminal, *network, agent );
    } catch ( const Network::NetworkException& e ) {
      fprintf( stderr, "Network exception: %s: %s\n",
	       e.function.c_str(), strerror( e.the_errno ) );
    } catch ( const Crypto::CryptoException& e ) {
      fprintf( stderr, "Crypto exception: %s\n",
	       e.text.c_str() );
    }

    #ifdef HAVE_UTEMPTER
    utempter_remove_record( master );
    #endif

    if ( close( master ) < 0 ) {
      perror( "close" );
      exit( 1 );
    }

    delete network;
  }

  printf( "\n[mosh-server is exiting.]\n" );

  return 0;
}

void serve( int host_fd, Terminal::Complete &terminal, ServerConnection &network, Agent::ProxyAgent &agent )
{
  /* prepare to poll for events */
  Select &sel = Select::get_instance();
  sel.add_signal( SIGTERM );
  sel.add_signal( SIGINT );

  uint64_t last_remote_num = network.get_remote_state_num();

  #ifdef HAVE_UTEMPTER
  bool connected_utmp = false;

  Addr saved_addr;
  socklen_t saved_addr_len = 0;
  #endif

  agent.attach_oob(network.oob());

  while ( 1 ) {
    try {
      uint64_t now = Network::timestamp();

      const int timeout_if_no_client = 60000;
      int timeout = min( network.wait_time(), terminal.wait_time( now ) );
      if ( (!network.get_remote_state_num())
	   || network.shutdown_in_progress() ) {
        timeout = min( timeout, 5000 );
      }

      /* poll for events */
      sel.clear_fds();
      std::vector< int > fd_list( network.fds() );
      assert( fd_list.size() == 1 ); /* servers don't hop */
      int network_fd = fd_list.back();
      sel.add_fd( network_fd );
      if ( !network.shutdown_in_progress() ) {
	sel.add_fd( host_fd );
      }

      network.oob()->pre_poll();

      int active_fds = sel.select( timeout );
      if ( active_fds < 0 ) {
	perror( "select" );
	break;
      }

      now = Network::timestamp();
      uint64_t time_since_remote_state = now - network.get_latest_remote_state().timestamp;

      if ( sel.read( network_fd ) ) {
	/* packet received from the network */
	network.recv();
	
	/* is new user input available for the terminal? */
	if ( network.get_remote_state_num() != last_remote_num ) {
	  last_remote_num = network.get_remote_state_num();

	  string terminal_to_host;
	  
	  Network::UserStream us;
	  us.apply_string( network.get_remote_diff() );
	  /* apply userstream to terminal */
	  for ( size_t i = 0; i < us.size(); i++ ) {
	    terminal_to_host += terminal.act( us.get_action( i ) );
	    if ( typeid( *us.get_action( i ) ) == typeid( Parser::Resize ) ) {
	      /* tell child process of resize */
	      const Parser::Resize *res = static_cast<const Parser::Resize *>( us.get_action( i ) );
	      struct winsize window_size;
	      if ( ioctl( host_fd, TIOCGWINSZ, &window_size ) < 0 ) {
		perror( "ioctl TIOCGWINSZ" );
		return;
	      }
	      window_size.ws_col = res->width;
	      window_size.ws_row = res->height;
	      if ( ioctl( host_fd, TIOCSWINSZ, &window_size ) < 0 ) {
		perror( "ioctl TIOCSWINSZ" );
		return;
	      }
	    }
	  }

	  if ( !us.empty() ) {
	    /* register input frame number for future echo ack */
	    terminal.register_input_frame( last_remote_num, now );
	  }

	  /* update client with new state of terminal */
	  if ( !network.shutdown_in_progress() ) {
	    network.set_current_state( terminal );
	  }
	  
	  /* write any writeback octets back to the host */
	  if ( swrite( host_fd, terminal_to_host.c_str(), terminal_to_host.length() ) < 0 ) {
	    break;
	  }

	  #ifdef HAVE_UTEMPTER
	  /* update utmp entry if we have become "connected" */
	  if ( (!connected_utmp)
	       || saved_addr_len != network.get_remote_addr_len()
	       || memcmp( &saved_addr, &network.get_remote_addr(),
			  saved_addr_len ) != 0 ) {
	    utempter_remove_record( host_fd );

	    saved_addr = network.get_remote_addr();
	    saved_addr_len = network.get_remote_addr_len();

	    char host[ NI_MAXHOST ];
	    int errcode = getnameinfo( &saved_addr.sa, saved_addr_len,
				       host, sizeof( host ), NULL, 0,
				       NI_NUMERICHOST );
	    if ( errcode != 0 ) {
	      throw NetworkException( std::string( "serve: getnameinfo: " ) + gai_strerror( errcode ), 0 );
	    }

	    char tmp[ 64 ];
	    snprintf( tmp, 64, "%s via mosh [%d]", host, getpid() );
	    utempter_add_record( host_fd, tmp );

	    connected_utmp = true;
	  }
	  #endif
	}
      }
      
      if ( (!network.shutdown_in_progress()) && sel.read( host_fd ) ) {
	/* input from the host needs to be fed to the terminal */
	const int buf_size = 16384;
	char buf[ buf_size ];
	
	/* fill buffer if possible */
	ssize_t bytes_read = read( host_fd, buf, buf_size );

        /* If the pty slave is closed, reading from the master can fail with
           EIO (see #264).  So we treat errors on read() like EOF. */
        if ( bytes_read <= 0 ) {
	  network.oob()->shutdown();
	  network.start_shutdown();
	} else {
	  string terminal_to_host = terminal.act( string( buf, bytes_read ) );
	
	  /* update client with new state of terminal */
	  network.set_current_state( terminal );

	  /* write any writeback octets back to the host */
	  if ( swrite( host_fd, terminal_to_host.c_str(), terminal_to_host.length() ) < 0 ) {
	    break;
	  }
	}
      }

      if ( sel.any_signal() ) {
	/* shutdown signal */
	if ( network.has_remote_addr() && (!network.shutdown_in_progress()) ) {
	  network.oob()->shutdown();
	  network.start_shutdown();
	} else {
	  break;
	}
      }
      
      if ( sel.error( network_fd ) ) {
	/* network problem */
	break;
      }

      if ( (!network.shutdown_in_progress()) && sel.error( host_fd ) ) {
	/* host problem */
	network.oob()->shutdown();
	network.start_shutdown();
      }

      /* quit if our shutdown has been acknowledged */
      if ( network.shutdown_in_progress() && network.shutdown_acknowledged() ) {
	break;
      }

      /* quit after shutdown acknowledgement timeout */
      if ( network.shutdown_in_progress() && network.shutdown_ack_timed_out() ) {
	break;
      }

      /* quit if we received and acknowledged a shutdown request */
      if ( network.counterparty_shutdown_ack_sent() ) {
	break;
      }

      #ifdef HAVE_UTEMPTER
      /* update utmp if has been more than 30 seconds since heard from client */
      if ( connected_utmp ) {
	if ( time_since_remote_state > 30000 ) {
	  utempter_remove_record( host_fd );

	  char tmp[ 64 ];
	  snprintf( tmp, 64, "mosh [%d]", getpid() );
	  utempter_add_record( host_fd, tmp );

	  connected_utmp = false;
	}
      }
      #endif

      if ( terminal.set_echo_ack( now ) ) {
	/* update client with new echo ack */
	if ( !network.shutdown_in_progress() ) {
	  network.set_current_state( terminal );
	}
      }

      if ( !network.get_remote_state_num()
           && time_since_remote_state >= uint64_t( timeout_if_no_client ) ) {
        fprintf( stderr, "No connection within %d seconds.\n",
                 timeout_if_no_client / 1000 );
	network.oob()->shutdown();
        break;
      }

      if ( time_since_remote_state > (AGENT_IDLE_TIMEOUT * 1000) || time_since_remote_state > 30000 ) {
	network.oob()->close_sessions();
      }
      network.oob()->post_poll();

      network.tick();

      network.oob()->post_tick();

    } catch ( const Network::NetworkException& e ) {
      fprintf( stderr, "%s: %s\n", e.function.c_str(), strerror( e.the_errno ) );
      spin();
    } catch ( const Crypto::CryptoException& e ) {
      if ( e.fatal ) {
	network.oob()->shutdown();
        throw;
      } else {
        fprintf( stderr, "Crypto exception: %s\n", e.text.c_str() );
      }
    }
  }
}

/* OpenSSH prints the motd on startup, so we will too */
void print_motd( void )
{
  FILE *motd = fopen( "/etc/motd", "r" );
  if ( !motd ) {
    return; /* don't report error on missing or forbidden motd */
  }

  const int BUFSIZE = 256;

  char buffer[ BUFSIZE ];
  while ( 1 ) {
    size_t bytes_read = fread( buffer, 1, BUFSIZE, motd );
    if ( bytes_read == 0 ) {
      break; /* don't report error */
    }
    size_t bytes_written = fwrite( buffer, 1, bytes_read, stdout );
    if ( bytes_written == 0 ) {
      break;
    }
  }

  fclose( motd );
}

void chdir_homedir( void )
{
  struct passwd *pw = getpwuid( geteuid() );
  if ( pw == NULL ) {
    perror( "getpwuid" );
    return; /* non-fatal */
  }

  if ( chdir( pw->pw_dir ) < 0 ) {
    perror( "chdir" );
  }

  if ( setenv( "PWD", pw->pw_dir, 1 ) < 0 ) {
    perror( "setenv" );
  }
}

bool motd_hushed( void )
{
  /* must be in home directory already */
  struct stat buf;
  return (0 == lstat( ".hushlogin", &buf ));
}

bool device_exists( const char *ut_line )
{
  string device_name = string( "/dev/" ) + string( ut_line );
  struct stat buf;
  return (0 == lstat( device_name.c_str(), &buf ));
}

string mosh_read_line( FILE *file )
{
  string ret;
  while ( !feof( file ) ) {
    char next = getc( file );
    if ( next == '\n' ) {
      return ret;
    }
    ret.push_back( next );
  }
  return ret;
}

void warn_unattached( const string & ignore_entry )
{
#ifdef HAVE_UTMPX_H
  /* get username */
  const struct passwd *pw = getpwuid( geteuid() );
  if ( pw == NULL ) {
    perror( "getpwuid" );
    /* non-fatal */
    return;
  }

  const string username( pw->pw_name );

  /* look for unattached sessions */
  vector< string > unattached_mosh_servers;

  while ( struct utmpx *entry = getutxent() ) {
    if ( (entry->ut_type == USER_PROCESS)
	 && (username == string( entry->ut_user )) ) {
      /* does line show unattached mosh session */
      string text( entry->ut_host );
      if ( (text.size() >= 5)
	   && (text.substr( 0, 5 ) == "mosh ")
	   && (text[ text.size() - 1 ] == ']')
	   && (text != ignore_entry)
	   && device_exists( entry->ut_line ) ) {
	unattached_mosh_servers.push_back( text );
      }
    }
  }

  /* print out warning if necessary */
  if ( unattached_mosh_servers.empty() ) {
    return;
  } else if ( unattached_mosh_servers.size() == 1 ) {
    printf( "\033[37;44mMosh: You have a detached Mosh session on this server (%s).\033[m\n\n",
	    unattached_mosh_servers.front().c_str() );
  } else {
    string pid_string;

    for ( vector< string >::const_iterator it = unattached_mosh_servers.begin();
	  it != unattached_mosh_servers.end();
	  it++ ) {
      pid_string += "        - " + *it + "\n";
    }

    printf( "\033[37;44mMosh: You have %d detached Mosh sessions on this server, with PIDs:\n%s\033[m\n",
	    (int)unattached_mosh_servers.size(), pid_string.c_str() );
  }
#endif /* HAVE_UTMPX_H */
}
