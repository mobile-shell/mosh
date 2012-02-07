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
*/

#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include "user.h"
#include "networktransport.cc"

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
      n = new Transport<UserStream, UserStream>( me, remote, NULL );
    }
  } catch ( CryptoException e ) {
    fprintf( stderr, "Fatal error: %s\n", e.text.c_str() );
    exit( 1 );
  }

  fprintf( stderr, "Port bound is %d, key is %s\n", n->port(), n->get_key().c_str() );

  if ( server ) {
    struct pollfd my_pollfd;
    my_pollfd.fd = n->fd();
    my_pollfd.events = POLLIN;
    uint64_t last_num = n->get_remote_state_num();
    while ( true ) {
      try {
	if ( poll( &my_pollfd, 1, n->wait_time() ) < 0 ) {
	  perror( "poll" );
	  exit( 1 );
	}
	
	n->tick();

	if ( my_pollfd.revents & POLLIN ) {
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

    struct pollfd fds[ 2 ];
    fds[ 0 ].fd = STDIN_FILENO;
    fds[ 0 ].events = POLLIN;

    fds[ 1 ].fd = n->fd();
    fds[ 1 ].events = POLLIN;

    while( true ) {
      try {
	if ( poll( fds, 2, n->wait_time() ) < 0 ) {
	  perror( "poll" );
	}

	n->tick();

	if ( fds[ 0 ].revents & POLLIN ) {
	  char x;
	  assert( read( STDIN_FILENO, &x, 1 ) == 1 );
	  n->get_current_state().push_back( Parser::UserByte( x ) );
	}

	if ( fds[ 1 ].revents & POLLIN ) {
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
