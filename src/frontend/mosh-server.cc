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
#include "version.h"

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <strings.h>
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
#include <inttypes.h>

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
#include "timestamp.h"
#include "fatal_assert.h"

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif

#include "networktransport-impl.h"

typedef Network::Transport< Terminal::Complete, Network::UserStream > ServerConnection;

static void serve( int host_fd,
		   Terminal::Complete &terminal,
		   ServerConnection &network,
		   long network_timeout,
		   long network_signaled_timeout );

static int run_server( const char *desired_ip, const char *desired_port,
		       const string &command_path, char *command_argv[],
		       const int colors, unsigned int verbose, bool with_motd );

using namespace std;

static void print_version( FILE *file )
{
  fprintf( file, "mosh-server (%s) [build %s]\n", PACKAGE_STRING, BUILD_VERSION );
  fprintf( file, "Copyright 2012 Keith Winstein <mosh-devel@mit.edu>\n" );
  fprintf( file, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n" );
}

static void print_usage( FILE *stream, const char *argv0 )
{
  fprintf( stream, "Usage: %s new [-s] [-v] [-i LOCALADDR] [-p PORT[:PORT2]] [-c COLORS] [-l NAME=VALUE] [-- COMMAND...]\n", argv0 );
}

static bool print_motd( const char *filename );
static void chdir_homedir( void );
static bool motd_hushed( void );
static void warn_unattached( const string & ignore_entry );

/* Simple spinloop */
static void spin( void )
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

static string get_SSH_IP( void )
{
  const char *SSH_CONNECTION = getenv( "SSH_CONNECTION" );
  if ( !SSH_CONNECTION ) { /* Older sshds don't set this */
    fprintf( stderr, "Warning: SSH_CONNECTION not found; binding to any interface.\n" );
    return string( "" );
  }
  istringstream ss( SSH_CONNECTION );
  string dummy, local_interface_IP;
  ss >> dummy >> dummy >> local_interface_IP;
  if ( !ss ) {
    fprintf( stderr, "Warning: Could not parse SSH_CONNECTION; binding to any interface.\n" );
    return string( "" );
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
  unsigned int verbose = 0; /* don't close stdin/stdout/stderr */
  /* Will cause mosh-server not to correctly detach on old versions of sshd. */
  list<string> locale_vars;

  /* strip off command */
  for ( int i = 1; i < argc; i++ ) {
    if ( 0 == strcmp( argv[ i ], "--help" ) || 0 == strcmp( argv[ i ], "-h" ) ) {
      print_usage( stdout, argv[ 0 ] );
      exit( 0 );
    }
    if ( 0 == strcmp( argv[ i ], "--version" ) ) {
      print_version( stdout );
      exit( 0 );
    }
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
    while ( (opt = getopt( argc - 1, argv + 1, "@:i:p:c:svl:" )) != -1 ) {
      switch ( opt ) {
	/*
	 * This undocumented option does nothing but eat its argument.
	 * Useful in scripting where you prepend something to a
	 * mosh-server argv, and might end up with something like
	 * "mosh-server new -v new -c 256", now you can say
	 * "mosh-server new -v -@ new -c 256" to discard the second
	 * "new".
	 */
      case '@':
	break;
      case 'i':
	desired_ip = optarg;
	break;
      case 'p':
	desired_port = optarg;
	break;
      case 's':
	desired_ip = NULL;
	desired_ip_str = get_SSH_IP();
	if ( !desired_ip_str.empty() ) {
	  desired_ip = desired_ip_str.c_str();
	  fatal_assert( desired_ip );
	}
	break;
      case 'c':
	try {
	  colors = myatoi( optarg );
	} catch ( const CryptoException & ) {
	  fprintf( stderr, "%s: Bad number of colors (%s)\n", argv[ 0 ], optarg );
	  print_usage( stderr, argv[ 0 ] );
	  exit( 1 );
	}
	break;
      case 'v':
	verbose++;
	break;
      case 'l':
	locale_vars.push_back( string( optarg ) );
	break;
      default:
	print_usage( stderr, argv[ 0 ] );
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
    print_usage( stderr, argv[ 0 ] );
    exit( 1 );
  }

  /* Sanity-check arguments */
  int dpl, dph;
  if ( desired_port && ! Connection::parse_portrange( desired_port, dpl, dph ) ) {
    fprintf( stderr, "%s: Bad UDP port range (%s)\n", argv[ 0 ], desired_port );
    print_usage( stderr, argv[ 0 ] );
    exit( 1 );
  }

  bool with_motd = false;

  /* Get shell */
  char *my_argv[ 2 ];
  string shell_name;
  if ( !command_argv ) {
    /* get shell name */
    const char *shell = getenv( "SHELL" );
    if ( shell == NULL ) {
      struct passwd *pw = getpwuid( getuid() );
      if ( pw == NULL ) {
	perror( "getpwuid" );
	exit( 1 );
      }
      shell = pw->pw_shell;
    }

    string shell_path( shell );
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
    return run_server( desired_ip, desired_port, command_path, command_argv, colors, verbose, with_motd );
  } catch ( const Network::NetworkException &e ) {
    fprintf( stderr, "Network exception: %s\n",
	     e.what() );
    return 1;
  } catch ( const Crypto::CryptoException &e ) {
    fprintf( stderr, "Crypto exception: %s\n",
	     e.what() );
    return 1;
  }
}

static int run_server( const char *desired_ip, const char *desired_port,
		       const string &command_path, char *command_argv[],
		       const int colors, unsigned int verbose, bool with_motd ) {
  /* get network idle timeout */
  long network_timeout = 0;
  char *timeout_envar = getenv( "MOSH_SERVER_NETWORK_TMOUT" );
  if ( timeout_envar && *timeout_envar ) {
    errno = 0;
    char *endptr;
    network_timeout = strtol( timeout_envar, &endptr, 10 );
    if ( *endptr != '\0' || ( network_timeout == 0 && errno == EINVAL ) ) {
      fprintf( stderr, "MOSH_SERVER_NETWORK_TMOUT not a valid integer, ignoring\n" );
    } else if ( network_timeout < 0 ) {
      fprintf( stderr, "MOSH_SERVER_NETWORK_TMOUT is negative, ignoring\n" );
      network_timeout = 0;
    }
  }
  /* get network signaled idle timeout */
  long network_signaled_timeout = 0;
  char *signal_envar = getenv( "MOSH_SERVER_SIGNAL_TMOUT" );
  if ( signal_envar && *signal_envar ) {
    errno = 0;
    char *endptr;
    network_signaled_timeout = strtol( signal_envar, &endptr, 10 );
    if ( *endptr != '\0' || ( network_signaled_timeout == 0 && errno == EINVAL ) ) {
      fprintf( stderr, "MOSH_SERVER_SIGNAL_TMOUT not a valid integer, ignoring\n" );
    } else if ( network_signaled_timeout < 0 ) {
      fprintf( stderr, "MOSH_SERVER_SIGNAL_TMOUT is negative, ignoring\n" );
      network_signaled_timeout = 0;
    }
  }
  /* get initial window size */
  struct winsize window_size;
  if ( ioctl( STDIN_FILENO, TIOCGWINSZ, &window_size ) < 0 ||
       window_size.ws_col == 0 ||
       window_size.ws_row == 0 ) {
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

  network->set_verbose( verbose );
  Select::set_verbose( verbose );

  /*
   * If mosh-server is run on a pty, then typeahead may echo and break mosh.pl's
   * detection of the MOSH CONNECT message.  Print it on a new line to bodge
   * around that.
   */
  if ( isatty( STDIN_FILENO ) ) {
    puts( "\r\n" );
  }
  printf( "MOSH CONNECT %s %s\n", network->port().c_str(), network->get_key().c_str() );

  /* don't let signals kill us */
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  fatal_assert( 0 == sigfillset( &sa.sa_mask ) );
  fatal_assert( 0 == sigaction( SIGHUP, &sa, NULL ) );
  fatal_assert( 0 == sigaction( SIGPIPE, &sa, NULL ) );


  /* detach from terminal */
  fflush( stdout );
  fflush( stderr );
  pid_t the_pid = fork();
  if ( the_pid < 0 ) {
    perror( "fork" );
  } else if ( the_pid > 0 ) {
    fprintf( stderr, "\nmosh-server (%s) [build %s]\n", PACKAGE_STRING, BUILD_VERSION );
    fprintf( stderr, "Copyright 2012 Keith Winstein <mosh-devel@mit.edu>\n" );
    fprintf( stderr, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n\n" );

    fprintf( stderr, "[mosh-server detached, pid = %d]\n", static_cast<int>(the_pid) );
#ifndef HAVE_IUTF8
    fprintf( stderr, "\nWarning: termios IUTF8 flag not defined.\nCharacter-erase of multibyte character sequence\nprobably does not work properly on this platform.\n" );
#endif /* HAVE_IUTF8 */

    fflush( stdout );
    fflush( stderr );
    if ( isatty( STDOUT_FILENO ) ) {
      tcdrain( STDOUT_FILENO );
    }
    if ( isatty( STDERR_FILENO ) ) {
      tcdrain( STDERR_FILENO );
    }
    _exit( 0 );
  }

  int master;

  /* close file descriptors */
  if ( verbose == 0 ) {
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
  snprintf( utmp_entry, 64, "mosh [%ld]", static_cast<long int>( getpid() ) );

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
      // On illumos motd is printed by /etc/profile.
#ifndef __sun
      // For Ubuntu, try and print one of {,/var}/run/motd.dynamic.
      // This file is only updated when pam_motd is run, but when
      // mosh-server is run in the usual way with ssh via the script,
      // this always happens.
      // XXX Hackish knowledge of Ubuntu PAM configuration.
      // But this seems less awful than build-time detection with autoconf.
      if (!print_motd("/run/motd.dynamic")) {
	print_motd("/var/run/motd.dynamic");
      }
      // Always print traditional /etc/motd.
      print_motd("/etc/motd");
#endif
      warn_unattached( utmp_entry );
    }

    /* Wait for parent to release us. */
    char linebuf[81];
    if (fgets(linebuf, sizeof linebuf, stdin) == NULL) {
      perror( "parent signal" );
      _exit( 1 );
    }

    Crypto::reenable_dumping_core();

    if ( execvp( command_path.c_str(), command_argv ) < 0 ) {
      perror( "execvp" );
      _exit( 1 );
    }
  } else {
    /* parent */

    /* Drop unnecessary privileges */
#ifdef HAVE_PLEDGE
    /* OpenBSD pledge() syscall */
    if ( pledge( "stdio inet tty", NULL )) {
      perror( "pledge() failed" );
      exit( 1 );
    }
#endif

#ifdef HAVE_UTEMPTER
    /* make utmp entry */
    utempter_add_record( master, utmp_entry );
#endif

    try {
      serve( master, terminal, *network, network_timeout, network_signaled_timeout );
    } catch ( const Network::NetworkException &e ) {
      fprintf( stderr, "Network exception: %s\n",
	       e.what() );
    } catch ( const Crypto::CryptoException &e ) {
      fprintf( stderr, "Crypto exception: %s\n",
	       e.what() );
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

static void serve( int host_fd, Terminal::Complete &terminal, ServerConnection &network, long network_timeout, long network_signaled_timeout )
{
  /* scale timeouts */
  const uint64_t network_timeout_ms = static_cast<uint64_t>( network_timeout ) * 1000;
  const uint64_t network_signaled_timeout_ms = static_cast<uint64_t>( network_signaled_timeout ) * 1000;
  /* prepare to poll for events */
  Select &sel = Select::get_instance();
  sel.add_signal( SIGTERM );
  sel.add_signal( SIGINT );
  sel.add_signal( SIGUSR1 );

  uint64_t last_remote_num = network.get_remote_state_num();

  #ifdef HAVE_UTEMPTER
  bool connected_utmp = false;

  Addr saved_addr;
  socklen_t saved_addr_len = 0;
  #endif

  bool child_released = false;

  while ( 1 ) {
    try {
      static const uint64_t timeout_if_no_client = 60000;
      int timeout = INT_MAX;
      uint64_t now = Network::timestamp();

      timeout = min( timeout, network.wait_time() );
      timeout = min( timeout, terminal.wait_time( now ) );
      if ( (!network.get_remote_state_num())
	   || network.shutdown_in_progress() ) {
        timeout = min( timeout, 5000 );
      }
      /*
       * The server goes completely asleep if it has no remote peer.
       * We may want to wake up sooner.
       */
      if ( network_timeout_ms ) {
	int64_t network_sleep = network_timeout_ms -
	  ( now - network.get_latest_remote_state().timestamp );
	if ( network_sleep < 0 ) {
	  network_sleep = 0;
	} else if ( network_sleep > INT_MAX ) {
	  /* 24 days might be too soon.  That's OK. */
	  network_sleep = INT_MAX;
	}
	timeout = min( timeout, static_cast<int>(network_sleep) );
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

      int active_fds = sel.select( timeout );
      if ( active_fds < 0 ) {
	perror( "select" );
	break;
      }

      now = Network::timestamp();
      uint64_t time_since_remote_state = now - network.get_latest_remote_state().timestamp;
      string terminal_to_host;

      if ( sel.read( network_fd ) ) {
	/* packet received from the network */
	network.recv();
	
	/* is new user input available for the terminal? */
	if ( network.get_remote_state_num() != last_remote_num ) {
	  last_remote_num = network.get_remote_state_num();

	  
	  Network::UserStream us;
	  us.apply_string( network.get_remote_diff() );
	  /* apply userstream to terminal */
	  for ( size_t i = 0; i < us.size(); i++ ) {
	    const Parser::Action *action = us.get_action( i );
	    if ( typeid( *action ) == typeid( Parser::Resize ) ) {
	      /* apply only the last consecutive Resize action */
	      while ( i < us.size() - 1 &&
		   typeid( us.get_action( i + 1 ) ) == typeid( Parser::Resize ) ) {
		i++;
	      }
	      /* tell child process of resize */
	      const Parser::Resize *res = static_cast<const Parser::Resize *>( action );
	      struct winsize window_size;
	      if ( ioctl( host_fd, TIOCGWINSZ, &window_size ) < 0 ) {
		perror( "ioctl TIOCGWINSZ" );
		network.start_shutdown();
	      }
	      window_size.ws_col = res->width;
	      window_size.ws_row = res->height;
	      if ( ioctl( host_fd, TIOCSWINSZ, &window_size ) < 0 ) {
		perror( "ioctl TIOCSWINSZ" );
		network.start_shutdown();
	      }
	    }
	    terminal_to_host += terminal.act( action );
	  }

	  if ( !us.empty() ) {
	    /* register input frame number for future echo ack */
	    terminal.register_input_frame( last_remote_num, now );
	  }

	  /* update client with new state of terminal */
	  if ( !network.shutdown_in_progress() ) {
	    network.set_current_state( terminal );
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

	  /* Tell child to start login session. */
	  if ( !child_released ) {
	    if ( swrite( host_fd, "\n", 1 ) < 0) {
	      perror( "child release" );
	      _exit( 1 );
	    }
	    child_released = true;
	  }
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
	  network.start_shutdown();
	} else {
	  terminal_to_host += terminal.act( string( buf, bytes_read ) );
	
	  /* update client with new state of terminal */
	  network.set_current_state( terminal );
	}
      }

      /* write user input and terminal writeback to the host */
      if ( swrite( host_fd, terminal_to_host.c_str(), terminal_to_host.length() ) < 0 ) {
	network.start_shutdown();
      }

      bool idle_shutdown = false;
      if ( network_timeout_ms &&
	   network_timeout_ms <= time_since_remote_state ) {
	idle_shutdown = true;
	fprintf( stderr, "Network idle for %llu seconds.\n", 
		 static_cast<unsigned long long>( time_since_remote_state / 1000 ) );
      }
      if ( sel.signal( SIGUSR1 ) ) {
	if ( !network_signaled_timeout_ms || network_signaled_timeout_ms <= time_since_remote_state ) {
	  idle_shutdown = true;
	  fprintf( stderr, "Network idle for %llu seconds when SIGUSR1 received\n",
		   static_cast<unsigned long long>( time_since_remote_state / 1000 ) );
	}
      }

      if ( sel.any_signal() || idle_shutdown ) {
	/* shutdown signal */
	if ( network.has_remote_addr() && (!network.shutdown_in_progress()) ) {
	  network.start_shutdown();
	} else {
	  break;
	}
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
           && time_since_remote_state >= timeout_if_no_client ) {
        fprintf( stderr, "No connection within %llu seconds.\n",
                 static_cast<unsigned long long>( timeout_if_no_client / 1000 ) );
        break;
      }

      network.tick();
    } catch ( const Network::NetworkException &e ) {
      fprintf( stderr, "%s\n", e.what() );
      spin();
    } catch ( const Crypto::CryptoException &e ) {
      if ( e.fatal ) {
        throw;
      } else {
        fprintf( stderr, "Crypto exception: %s\n", e.what() );
      }
    }
  }
}

/* Print the motd from a given file, if available */
static bool print_motd( const char *filename )
{
  FILE *motd = fopen( filename, "r" );
  if ( !motd ) {
    return false;
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
  return true;
}

static void chdir_homedir( void )
{
  const char *home = getenv( "HOME" );
  if ( home == NULL ) {
    struct passwd *pw = getpwuid( getuid() );
    if ( pw == NULL ) {
      perror( "getpwuid" );
      return; /* non-fatal */
    }
    home = pw->pw_dir;
  }

  if ( chdir( home ) < 0 ) {
    perror( "chdir" );
  }

  if ( setenv( "PWD", home, 1 ) < 0 ) {
    perror( "setenv" );
  }
}

static bool motd_hushed( void )
{
  /* must be in home directory already */
  struct stat buf;
  return (0 == lstat( ".hushlogin", &buf ));
}

static bool device_exists( const char *ut_line )
{
  string device_name = string( "/dev/" ) + string( ut_line );
  struct stat buf;
  return (0 == lstat( device_name.c_str(), &buf ));
}

static void warn_unattached( const string & ignore_entry )
{
#ifdef HAVE_UTMPX_H
  /* get username */
  const struct passwd *pw = getpwuid( getuid() );
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
