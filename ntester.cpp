#include <termios.h>
#include <unistd.h>

#include "network.hpp"
#include "keystroke.hpp"

int main( int argc, char *argv[] )
{
  bool server = true;
  char *key;
  char *ip;
  int port;

  Network::Connection<KeyStroke, KeyStroke> *n;

  try {
    if ( argc > 1 ) {
      server = false;
      /* client */
      
      key = argv[ 1 ];
      ip = argv[ 2 ];
      port = atoi( argv[ 3 ] );
      
      n = new Network::Connection<KeyStroke, KeyStroke>( key, ip, port );
    } else {
      n = new Network::Connection<KeyStroke, KeyStroke>();
    }
  } catch ( CryptoException e ) {
    fprintf( stderr, "Fatal error: %s\n", e.text.c_str() );
    exit( 1 );
  }

  fprintf( stderr, "Port bound is %d, key is %s\n", n->port(), n->get_key().c_str() );

  if ( server ) {
    while ( true ) {
      try {
	KeyStroke s = n->recv();
	printf( "%c", s.letter );
	fflush( NULL );
      } catch ( CryptoException e ) {
	fprintf( stderr, "Error: %s\n", e.text.c_str() );
      }
    }
  } else {
      struct termios the_termios;

      if ( tcgetattr( STDIN_FILENO, &the_termios ) < 0 ) {
	perror( "tcgetattr" );
	exit( 1 );
      }

      cfmakeraw( &the_termios );

      if ( tcsetattr( STDIN_FILENO, TCSANOW, &the_termios ) < 0 ) {
	perror( "tcsetattr" );
	exit( 1 );
      }

    while( true ) {
      char x = getchar();

      KeyStroke t( string( &x, 1 ) );

      n->send( t );
    }
  }
}
