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
#include <stdlib.h>
#include <unistd.h>

#include "stmclient.h"
#include "chwidth.h"
#include "crypto.h"
#include "locale_utils.h"
#include "fatal_assert.h"

/* These need to be included last because of conflicting defines. */
/*
 * stmclient.h includes termios.h, and that will break termio/termios pull in on Solaris.
 * The solution is to include termio.h also.
 * But Mac OS X doesn't have termio.h, so this needs a guard.
 */
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif

#if defined HAVE_NCURSESW_CURSES_H
#  include <ncursesw/curses.h>
#  include <ncursesw/term.h>
#elif defined HAVE_NCURSESW_H
#  include <ncursesw.h>
#  include <term.h>
#elif defined HAVE_NCURSES_CURSES_H
#  include <ncurses/curses.h>
#  include <ncurses/term.h>
#elif defined HAVE_NCURSES_H
#  include <ncurses.h>
#  include <term.h>
#elif defined HAVE_CURSES_H
#  include <curses.h>
#  include <term.h>
#else
#  error "SysV or X/Open-compatible Curses header file required"
#endif

static void print_version( FILE *file )
{
  fputs( "mosh-client (" PACKAGE_STRING ") [build " BUILD_VERSION "]\n"
	 "Copyright 2012 Keith Winstein <mosh-devel@mit.edu>\n"
	 "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
	 "This is free software: you are free to change and redistribute it.\n"
	 "There is NO WARRANTY, to the extent permitted by law.\n", file );
}

static void print_usage( FILE *file, const char *argv0 )
{
  print_version( file );
  fprintf( file, "\nUsage: %s [-v] [-w fd] [-W] [-# 'ARGS'] IP PORT\n"
	   "       %s -c\n", argv0, argv0 );
}

static void print_colorcount( void )
{
  /* check colors */
  setupterm((char *)0, 1, (int *)0);

  char colors_name[] = "colors";
  int color_val = tigetnum( colors_name );
  if ( color_val == -2 ) {
    fprintf( stderr, "Invalid terminfo numeric capability: %s\n",
	     colors_name );
  }

  printf( "%d\n", color_val );
}

#ifdef NACL
int mosh_main( int argc, char *argv[] )
#else
int main( int argc, char *argv[] )
#endif
{
  unsigned int verbose = 0;
  int chwidth_fd = -1;
  bool eaw_wide = false;
  /* For security, make sure we don't dump core */
  Crypto::disable_dumping_core();

  /* Detect edge case */
  fatal_assert( argc > 0 );

  /* Get arguments */
  for ( int i = 1; i < argc; i++ ) {
    if ( 0 == strcmp( argv[ i ], "--help" ) ) {
      print_usage( stdout, argv[ 0 ] );
      exit( 0 );
    }
    if ( 0 == strcmp( argv[ i ], "--version" ) ) {
      print_version( stdout );
      exit( 0 );
    }
  }

  int opt;
  while ( (opt = getopt( argc, argv, "#:cvw:W" )) != -1 ) {
    switch ( opt ) {
    case '#':
      // Ignore the original arguments to mosh wrapper
      break;
    case 'c':
      print_colorcount();
      exit( 0 );
      break;
    case 'v':
      verbose++;
      break;
    case 'w':
      chwidth_fd = atoi( optarg );
      break;
    case 'W':
      eaw_wide = true;
      break;
    default:
      print_usage( stderr, argv[ 0 ] );
      exit( 1 );
      break;
    }
  }

  char *ip, *desired_port;

  if ( argc - optind != 2 ) {
    print_usage( stderr, argv[ 0 ] );
    exit( 1 );
  }

  ip = argv[ optind ];
  desired_port = argv[ optind + 1 ];

  /* Sanity-check arguments */
  if ( desired_port
       && ( strspn( desired_port, "0123456789" ) != strlen( desired_port ) ) ) {
    fprintf( stderr, "%s: Bad UDP port (%s)\n\n", argv[ 0 ], desired_port );
    print_usage( stderr, argv[ 0 ] );
    exit( 1 );
  }

  /* Read key from environment */
  char *env_key = getenv( "MOSH_KEY" );
  if ( env_key == NULL ) {
    fputs( "MOSH_KEY environment variable not found.\n", stderr );
    exit( 1 );
  }

  /* Read prediction preference */
  char *predict_mode = getenv( "MOSH_PREDICTION_DISPLAY" );
  /* can be NULL */

  /* Read prediction insertion preference */
  char *predict_overwrite = getenv( "MOSH_PREDICTION_OVERWRITE" );
  /* can be NULL */

  string key( env_key );

  if ( unsetenv( "MOSH_KEY" ) < 0 ) {
    perror( "unsetenv" );
    exit( 1 );
  }

  /* Adopt native locale */
  set_native_locale();

  /* Install our default character width table. */
  ChWidthPtr chwidths = shared::make_shared<ChWidth>();
  if ( !chwidths->apply_diff( ChWidth::get_default() )) {
    errx( 1, "bad default table" );
  }

  /* Load and apply a user character width table if provided. */
  std::string user_map;
  if ( chwidth_fd != -1 ) {
    /* Read character width table. */
    /* Dup it to see if it's actually open. */
    int chwidth_fd2 = dup( chwidth_fd );
    if ( chwidth_fd2 == -1 ) {
      err( 1, "dup chwidth fd" );
    }
    close( chwidth_fd2 );
    /* C++ streams offers no standard way to attach to an fd.  So use stdio. */
    FILE *chwidth_file = fdopen( chwidth_fd, "r" );
    if ( !chwidth_file ) {
      err( 1, "fdopen chwidth fd" );
    }
    /* Read characters. */
    user_map.reserve( 65536 );
    int ch;
    while ( EOF != ( ch = getc_unlocked( chwidth_file ) ) ) {
      user_map.push_back( static_cast<char>( ch ) );
    }
    fclose( chwidth_file );
    /* Apply changes to default table.  This may replace it entirely. */
    if ( !chwidths->apply_diff( user_map )) {
      errx( 1, "bad user character widths table" );
    }
  }
  /* Apply East Asian width-wide delta if requested. */
  if ( eaw_wide ) {
    if ( !chwidths->apply_diff( ChWidth::get_eaw_delta() )) {
      errx( 1, "bad delta table" );
    }
  }

  bool success = false;
  try {
    STMClient client( ip, desired_port, key.c_str(), predict_mode, verbose, predict_overwrite, chwidths );
    client.init();

    try {
      success = client.main();
    } catch ( ... ) {
      client.shutdown();
      throw;
    }

    client.shutdown();
  } catch ( const Network::NetworkException &e ) {
    fprintf( stderr, "Network exception: %s\r\n",
	     e.what() );
    success = false;
  } catch ( const Crypto::CryptoException &e ) {
    fprintf( stderr, "Crypto exception: %s\r\n",
	     e.what() );
    success = false;
  } catch ( const std::exception &e ) {
    fprintf( stderr, "Error: %s\r\n", e.what() );
    success = false;
  }

  printf( "[mosh is exiting.]\n" );

  return !success;
}
