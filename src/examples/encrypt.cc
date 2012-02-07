#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "crypto.h"

using namespace Crypto;

int main( int argc, char *argv[] )
{
  if ( argc != 2 ) {
    fprintf( stderr, "Usage: %s NONCE\n", argv[ 0 ] );
    return 1;
  }

  try {
    Base64Key key;
    Session session( key );
    Nonce nonce( myatoi( argv[ 1 ] ) );

    /* Read input */
    char *input = NULL;
    int total_size = 0;

    while ( 1 ) {
      unsigned char buf[ 16384 ];
      ssize_t bytes_read = read( STDIN_FILENO, buf, 16384 );
      if ( bytes_read == 0 ) { /* EOF */
	break;
      } else if ( bytes_read < 0 ) {
	perror( "read" );
	exit( 1 );
      } else {
	input = (char *)realloc( input, total_size + bytes_read );
	assert( input );
	memcpy( input + total_size, buf, bytes_read );
	total_size += bytes_read;
      }
    }

    string plaintext( input, total_size );
    free( input );

    /* Encrypt message */

    string ciphertext = session.encrypt( Message( nonce, plaintext ) );

    cerr << "Key: " << key.printable_key() << endl;

    cout << ciphertext;
  } catch ( CryptoException e ) {
    cerr << e.text << endl;
    exit( 1 );
  }

  return 0;
}
