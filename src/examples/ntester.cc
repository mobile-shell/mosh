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

#include <termios.h>
#include <unistd.h>

#include "user.h"
#include "fatal_assert.h"
#include "pty_compat.h"
#include "networktransport.cc"
#include "select.h"

using namespace Network;

int main( int argc, char *argv[] )
{
  bool server = true;
  char *key;
  char *ip;
  int port;

  UserStream me, remote;

  Transport<UserStream, UserStream> *n;

  try {
    if ( argc > 1 ) {
      server = false;
      /* client */
      
      key = argv[ 1 ];
      ip = argv[ 2 ];
      port = atoi( argv[ 3 ] );
      
      n = new Transport<UserStream, UserStream>( me, remote, key, ip, port );
    } else {
      n = new Transport<UserStream, UserStream>( me, remote, NULL, NULL );
    }
  } catch ( CryptoException e ) {
    fprintf( stderr, "Fatal error: %s\n", e.text.c_str() );
    exit( 1 );
  }

  fprintf( stderr, "Port bound is %d, key is %s\n", n->port(), n->get_key().c_str() );

  if ( server ) {
    Select &sel = Select::get_instance();
    sel.add_fd( n->fd() );
    uint64_t last_num = n->get_remote_state_num();
    while ( true ) {
      try {
	if ( sel.select( n->wait_time() ) < 0 ) {
	  perror( "select" );
	  exit( 1 );
	}
	
	n->tick();

	if ( sel.read( n->fd() ) ) {
	  n->recv();

	  if ( n->get_remote_state_num() != last_num ) {
	    fprintf( stderr, "[%d=>%d %s]", (int)last_num, (int)n->get_remote_state_num(), n->get_remote_diff().c_str() );
	    last_num = n->get_remote_state_num();
	  }
	}
      } catch ( CryptoException e ) {
	fprintf( stderr, "Cryptographic error: %s\n", e.text.c_str() );
      }
    }
  } else {
    struct termios saved_termios;
    struct termios the_termios;

    if ( tcgetattr( STDIN_FILENO, &the_termios ) < 0 ) {
      perror( "tcgetattr" );
      exit( 1 );
    }

    saved_termios = the_termios;

    cfmakeraw( &the_termios );

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &the_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }

    Select &sel = Select::get_instance();
    sel.add_fd( STDIN_FILENO );
    sel.add_fd( n->fd() );

    while( true ) {
      try {
	if ( sel.select( n->wait_time() ) < 0 ) {
	  perror( "select" );
	}

	n->tick();

	if ( sel.read( STDIN_FILENO ) ) {
	  char x;
	  fatal_assert( read( STDIN_FILENO, &x, 1 ) == 1 );
	  n->get_current_state().push_back( Parser::UserByte( x ) );
	}

	if ( sel.read( n->fd() ) ) {
	  n->recv();
	}
      } catch ( NetworkException e ) {
	fprintf( stderr, "%s: %s\r\n", e.function.c_str(), strerror( e.the_errno ) );
	break;
      } catch ( CryptoException e ) {
	fprintf( stderr, "Cryptographic error: %s\n", e.text.c_str() );
      }
    }

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }    
  }

  delete n;
}
