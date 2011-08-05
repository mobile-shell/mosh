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

  Network::Connection *n;

  try {
    if ( argc > 1 ) {
      server = false;
      /* client */
      
      key = argv[ 1 ];
      ip = argv[ 2 ];
      port = atoi( argv[ 3 ] );
      
      n = new Network::Connection( key, ip, port );
    } else {
      n = new Network::Connection();
    }
  } catch ( CryptoException e ) {
    fprintf( stderr, "Fatal error: %s\n", e.text.c_str() );
    exit( 1 );
  }

  fprintf( stderr, "Port bound is %d, key is %s\n", n->port(), n->get_key().c_str() );

  if ( server ) {
    while ( true ) {
      try {
	string s = n->recv();
	printf( "%s", s.c_str() );
	fflush( NULL );
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

    while( true ) {
      char x = getchar();
      
      string prefix = "Key(" + string( &x, 1 ) + ")";
      
      try {
	n->send( prefix );
      } catch ( Network::NetworkException e ) {
	fprintf( stderr, "%s: %s\r\n", e.function.c_str(), strerror( e.the_errno ) );
	break;
      }
    }

    if ( tcsetattr( STDIN_FILENO, TCSANOW, &saved_termios ) < 0 ) {
      perror( "tcsetattr" );
      exit( 1 );
    }    
  }
}
