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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "crypto.h"

using namespace Crypto;
using namespace std;

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
