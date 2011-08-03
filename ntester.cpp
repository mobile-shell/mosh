#include "network.hpp"
#include "keystroke.hpp"

int main( int argc, char *argv[] )
{
  bool server = true;
  char *ip;
  int port;

  if ( argc > 1 ) {
    server = false;

    ip = argv[ 1 ];
    port = atoi( argv[ 2 ] );
  }

  Network::Connection<KeyStroke, KeyStroke> n( server );
  fprintf( stderr, "Port bound is %d\n", n.port() );

  if ( !server ) {
    n.client_connect( ip, port );
  }

  if ( server ) {
    while ( true ) {
      KeyStroke s = n.recv();

      fprintf( stderr, "Got KeyStroke: %c\n", s.letter );
    }
  } else {
    while( true ) {
      sleep( 1 );

      KeyStroke t( string( "x", 1 ) );

      n.send( t );
    }
  }
}
