#include "stmclient.hpp"
#include "crypto.hpp"

#include <iostream>

int main( int argc, char *argv[] )
{
  /* Get arguments */
  char *ip;
  int port;

  if ( argc != 3 ) {
    fprintf( stderr, "Usage: %s IP PORT\n", argv[ 0 ] );
    exit( 1 );
  }

  ip = argv[ 1 ];
  port = myatoi( argv[ 2 ] );

  /* Read key from standard input */
  cout << "Key: ";
  string key;
  cin >> key;

  /* Adopt native locale */
  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
    exit( 1 );
  }

  STMClient client( ip, port, key.c_str() );

  client.init();

  client.main();

  client.shutdown();

  printf( "\n[mosh is exiting.]\n" );

  return 0;
}
