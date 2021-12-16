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

/* Test suite for the OCB-AES reference implementation included with Mosh.

   This tests cryptographic primitives implemented by others.  It uses the
   same interfaces and indeed the same compiled object code as the Mosh
   client and server.  It does not particularly test any code written for
   the Mosh project. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "base64.h"
#include "base64_vector.h"
#include "crypto.h"
#include "prng.h"
#include "fatal_assert.h"
// #include "test_utils.h"

#define KEY_LEN   16
#define NONCE_LEN 12
#define TAG_LEN   16

bool verbose = false;

static void test_base64( void ) {
  /* run through a test vector */
  char encoded[25];
  uint8_t decoded[16];
  size_t b64_len = 24;
  size_t raw_len = 16;
  for ( base64_test_row *row = static_base64_vector; *row->native != '\0'; row++ ) {
    memset(encoded, '\0', sizeof encoded);
    memset(decoded, '\0', sizeof decoded);

    base64_encode(static_cast<const uint8_t *>(row->native), raw_len, encoded, b64_len);
    fatal_assert( b64_len == 24 );
    fatal_assert( !memcmp(row->encoded, encoded, sizeof encoded));

    fatal_assert( base64_decode(row->encoded, b64_len, decoded, &raw_len ));
    fatal_assert( raw_len == 16 );
    fatal_assert( !memcmp(row->native, decoded, sizeof decoded));
  }
  if ( verbose ) {
    printf( "validation PASSED\n" );
  }
  /* try 0..255 in the last byte; make sure the final two characters are output properly */
  uint8_t source[16];
  memset(source, '\0', sizeof source);
  for ( int i = 0; i < 256; i++ ) {
    source[15] = i;
    base64_encode(source, raw_len, encoded, b64_len);
    fatal_assert( b64_len == 24 );

    fatal_assert( base64_decode(encoded, b64_len, decoded, &raw_len ));
    fatal_assert( raw_len == 16 );
    fatal_assert( !memcmp(source, decoded, sizeof decoded));
  }
  if ( verbose ) {
    printf( "last-byte PASSED\n" );
  }

  /* randomly try keys */
  PRNG prng;
  for ( int i = 0; i < ( 1<<17 ); i++ ) {
    Base64Key key1(prng);
    Base64Key key2(key1.printable_key());
    fatal_assert( key1.printable_key() == key2.printable_key() && !memcmp(key1.data(), key2.data(), 16 ));
  }
  if ( verbose ) {
    printf( "random PASSED\n" );
  }

  /* test bad keys */
  const char *bad_keys[] = {
    "",
    "AAAAAAAAAAAAAAAAAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAAA=",
    "AAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAA~=",
    "AAAAAAAAAAAAAAAAAAAAAA=~",
    "~AAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAA~A==",
    "AAAAAAAAAAAAAAAAAAAAA~==",
    "AAAAAAAAAA~AAAAAAAAAAA==",
    "AAAAAAAAAA==",
    NULL,
  };
  for ( const char **key = bad_keys; *key != NULL; key++ ) {
    b64_len = 24;
    raw_len = 16;
    fatal_assert( !base64_decode(*key, b64_len, decoded, &raw_len ));
  }
  if ( verbose ) {
    printf( "bad-keys PASSED\n" );
  }
}

int main( int argc, char *argv[] )
{
  if ( argc >= 2 && strcmp( argv[ 1 ], "-v" ) == 0 ) {
    verbose = true;
  }

  try {
    test_base64();
  } catch ( const std::exception &e ) {
    fprintf( stderr, "Error: %s\r\n", e.what() );
    return 1;
  }
  return 0;
}
