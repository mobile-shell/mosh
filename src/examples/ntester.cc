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
#include "networktransport-impl.h"
#include "select.h"

using namespace Network;

int main( int argc, char *argv[] )
{
  bool server = true;
  char *key;
  char *ip;
  char *port;

  UserStream me, remote;

  Transport<UserStream, UserStream> *n;

  try {
    if ( argc > 1 ) {
      server = false;
      /* client */
      
      key = argv[ 1 ];
      ip = argv[ 2 ];
      port = argv[ 3 ];
      
      n = new Transport<UserStream, UserStream>( me, remote, key, ip, port );
    } else {
      n = new Transport<UserStream, UserStream>( me, remote, NULL, NULL );
    }
    fprintf( stderr, "Port bound is %s, key is %s\n", n->port().c_str(), n->get_key().c_str() );
  } catch ( const std::exception &e ) {
    fprintf( stderr, "Fatal startup error: %s\n", e.what() );
    exit( 1 );
  }

  if ( server ) {
    Select &sel = Select::get_instance();
    uint64_t last_num = n->get_remote_state_num();
    while ( true ) {
      try {
	sel.clear_fds();
	std::vector< int > fd_list( n->fds() );
	assert( fd_list.size() == 1 ); /* servers don't hop */
	int network_fd = fd_list.back();
	sel.add_fd( network_fd );
	if ( sel.select( n->wait_time() ) < 0 ) {
	  perror( "select" );
	  exit( 1 );
	}
	
	n->tick();

	if ( sel.read( network_fd ) ) {
	  n->recv();

	  if ( n->get_remote_state_num() != last_num ) {
	    fprintf( stderr, "[%d=>%d %s]", (int)last_num, (int)n->get_remote_state_num(), n->get_remote_diff().c_str() );
	    last_num = n->get_remote_state_num();
	  }
	}
      } catch ( const std::exception &e ) {
	fprintf( stderr, "Server error: %s\n", e.what() );
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

    while( true ) {
      sel.clear_fds();
      sel.add_fd( STDIN_FILENO );

      std::vector< int > fd_list( n->fds() );
      for ( std::vector< int >::const_iterator it = fd_list.begin();
	    it != fd_list.end();
	    it++ ) {
	sel.add_fd( *it );
      }

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

	bool network_ready_to_read = false;
	for ( std::vector< int >::const_iterator it = fd_list.begin();
	      it != fd_list.end();
	      it++ ) {
	  if ( sel.read( *it ) ) {
	    /* packet received from the network */
	    /* we only read one socket each run */
	    network_ready_to_read = true;
	  }
	}

	if ( network_ready_to_read ) {
	  n->recv();
	}
      } catch ( const std::exception &e ) {
	fprintf( stderr, "Client error: %s\n", e.what() );
      }
    }

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }    
  }

  delete n;
}
