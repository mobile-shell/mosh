#include "stmclient.hpp"

int main( int argc, char *argv[] )
{
  /* Get arguments */
  char *ip, *key;
  int port;

  if ( argc != 4 ) {
    fprintf( stderr, "Usage: %s IP PORT KEY\n", argv[ 0 ] );
    exit( 1 );
  }

  ip = argv[ 1 ];
  port = atoi( argv[ 2 ] );
  key = argv[ 3 ];

  STMClient client( ip, port, key );

  /* Adopt implementation locale */
  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
    exit( 1 );
  }

  client.init();

  client.main();

  client.shutdown();
  printf( "\033[!p\n[stm is exiting.]\n" );

  return 0;
}
