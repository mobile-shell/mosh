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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <assert.h>
#include <wctype.h>
#include <typeinfo>
#include <termios.h>

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#elif HAVE_LIBUTIL_H
#include <libutil.h>
#endif

#include "parser.h"
#include "swrite.h"
#include "locale_utils.h"
#include "fatal_assert.h"
#include "pty_compat.h"
#include "select.h"

const size_t buf_size = 1024;

static void emulate_terminal( int fd );
static int copy( int src, int dest );
static int vt_parser( int fd, Parser::UTF8Parser *parser );

int main( int argc __attribute__((unused)),
	  char *argv[] __attribute__((unused)),
	  char *envp[] )
{
  int master;
  struct termios saved_termios, raw_termios, child_termios;

  set_native_locale();
  fatal_assert( is_utf8_locale() );

  if ( tcgetattr( STDIN_FILENO, &saved_termios ) < 0 ) {
    perror( "tcgetattr" );
    exit( 1 );
  }

  child_termios = saved_termios;

#ifdef HAVE_IUTF8
  if ( !(child_termios.c_iflag & IUTF8) ) {
    fprintf( stderr, "Warning: Locale is UTF-8 but termios IUTF8 flag not set. Setting IUTF8 flag.\n" );
    child_termios.c_iflag |= IUTF8;
  }
#else
  fprintf( stderr, "Warning: termios IUTF8 flag not defined. Character-erase of multibyte character sequence probably does not work properly on this platform.\n" );
#endif /* HAVE_IUTF8 */

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

    emulate_terminal( master );

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }
  }

  return 0;
}

static void emulate_terminal( int fd )
{
  Parser::UTF8Parser parser;

  Select &sel = Select::get_instance();
  sel.add_fd( STDIN_FILENO );
  sel.add_fd( fd );

  while ( 1 ) {
    int active_fds = sel.select( -1 );
    if ( active_fds <= 0 ) {
      perror( "select" );
      return;
    }

    if ( sel.read( STDIN_FILENO ) ) {
      if ( copy( STDIN_FILENO, fd ) < 0 ) {
	return;
      }
    } else if ( sel.read( fd ) ) {
      if ( vt_parser( fd, &parser ) < 0 ) {
	return;
      }
    } else {
      fprintf( stderr, "select mysteriously woken up\n" );
    }
  }
}

static int copy( int src, int dest )
{
  char buf[ buf_size ];

  ssize_t bytes_read = read( src, buf, buf_size );
  if ( bytes_read == 0 ) { /* EOF */
    return -1;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return -1;
  }

  return swrite( dest, buf, bytes_read );
}

static int vt_parser( int fd, Parser::UTF8Parser *parser )
{
  char buf[ buf_size ];

  /* fill buffer if possible */
  ssize_t bytes_read = read( fd, buf, buf_size );
  if ( bytes_read == 0 ) { /* EOF */
    return -1;
  } else if ( bytes_read < 0 ) {
    perror( "read" );
    return -1;
  }

  /* feed to parser */
  Parser::Actions actions;
  for ( int i = 0; i < bytes_read; i++ ) {
    parser->input( buf[ i ], actions );
    for ( Parser::Actions::iterator j = actions.begin();
	  j != actions.end();
	  j++ ) {

      Parser::Action *act = *j;
      assert( act );

      if ( act->char_present ) {
	if ( iswprint( act->ch ) ) {
	  printf( "%s(0x%02x=%lc) ", act->name().c_str(), (unsigned int)act->ch, (wint_t)act->ch );
	} else {
	  printf( "%s(0x%02x) ", act->name().c_str(), (unsigned int)act->ch );
	}
      } else {
	printf( "[%s] ", act->name().c_str() );
      }

      delete act;

      fflush( stdout );
    }
    actions.clear();
  }

  return 0;
}
