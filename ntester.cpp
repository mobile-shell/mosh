#include "network.hpp"
#include "keystroke.hpp"

int main( int argc, char *argv[] )
{
  bool server = true;
  char *key;
  char *ip;
  int port;

  Network::Connection<KeyStroke, KeyStroke> *n;

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

  fprintf( stderr, "Port bound is %d, key is %s\n", n->port(), n->get_key().c_str() );

  if ( server ) {
    while ( true ) {
      KeyStroke s = n->recv();

      fprintf( stderr, "Got KeyStroke: %c\n", s.letter );
    }
  } else {
    while( true ) {
      sleep( 1 );

      KeyStroke t( string( "x", 1 ) );

      n->send( t );
    }
  }
}
