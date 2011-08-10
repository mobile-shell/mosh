#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include "keystroke.hpp"
#include "networktransport.hpp"

int main( int argc, char *argv[] )
{
  bool server = true;
  char *key;
  char *ip;
  int port;

  KeyStroke me, remote;

  Network::Transport<KeyStroke, KeyStroke> *n;

  try {
    if ( argc > 1 ) {
      server = false;
      /* client */
      
      key = argv[ 1 ];
      ip = argv[ 2 ];
      port = atoi( argv[ 3 ] );
      
      n = new Network::Transport<KeyStroke, KeyStroke>( me, remote, key, ip, port );
    } else {
      n = new Network::Transport<KeyStroke, KeyStroke>( me, remote );
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
	if ( poll( &my_pollfd, 1, n->tick() ) < 0 ) {
	  perror( "poll" );
	  exit( 1 );
	}
	
	if ( my_pollfd.revents & POLLIN ) {
	  n->recv();

	  if ( n->get_remote_state_num() != last_num ) {
	    fprintf( stderr, "Num: %d. Contents: ",
		     (int)n->get_remote_state_num() );
	    for ( size_t i = 0; i < n->get_remote_state().user_bytes.size(); i++ ) {
	      fprintf( stderr, "%c", n->get_remote_state().user_bytes[ i ] );
	    }
	    fprintf( stderr, "\n" );
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
	if ( poll( fds, 2, n->tick() ) < 0 ) {
	  perror( "poll" );
	}

	if ( fds[ 0 ].revents & POLLIN ) {
	  char x = getchar();
	  n->get_current_state().key_hit( x );
	}

	if ( fds[ 1 ].revents & POLLIN ) {
	  n->recv();
	}
      } catch ( Network::NetworkException e ) {
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
}
