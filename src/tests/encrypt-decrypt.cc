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

/* Tests the Mosh crypto layer by encrypting and decrypting a bunch of random
   messages, interspersed with some random bad ciphertexts which we need to
   reject. */

#include <stdio.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "crypto.h"
#include "prng.h"
#include "fatal_assert.h"
#include "test_utils.h"

using namespace Crypto;

PRNG prng;

const size_t MESSAGE_SIZE_MAX     = (2048 - 16);
const size_t MESSAGES_PER_SESSION = 256;
const size_t NUM_SESSIONS         = 64;

bool verbose = false;

#define NONCE_FMT "%016" PRIx64

static std::string random_payload( void ) {
  const size_t len = prng.uint32() % MESSAGE_SIZE_MAX;
  char *buf = new char[len];
  prng.fill( buf, len );

  std::string payload( buf, len );
  delete [] buf;
  return payload;
}

static void test_bad_decrypt( Session &decryption_session ) {
  std::string bad_ct = random_payload();

  bool got_exn = false;
  try {
    decryption_session.decrypt( bad_ct );
  } catch ( const CryptoException &e ) {
    got_exn = true;

    /* The "bad decrypt" exception needs to be non-fatal, otherwise we are
       vulnerable to an easy DoS. */
    fatal_assert( ! e.fatal );
  }

  if ( verbose ) {
    hexdump( bad_ct, "bad ct" );
  }
  fatal_assert( got_exn );
}

/* Generate a single key and initial nonce, then perform some encryptions. */
static void test_one_session( void ) {
  Base64Key key;
  Session encryption_session( key );
  Session decryption_session( key );

  uint64_t nonce_int = prng.uint64();

  if ( verbose ) {
    hexdump( key.data(), 16, "key" );
  }

  for ( size_t i=0; i<MESSAGES_PER_SESSION; i++ ) {
    Nonce nonce( nonce_int );
    fatal_assert( nonce.val() == nonce_int );

    std::string plaintext = random_payload();
    if ( verbose ) {
      printf( DUMP_NAME_FMT NONCE_FMT "\n", "nonce", nonce_int );
      hexdump( plaintext, "pt" );
    }

    std::string ciphertext = encryption_session.encrypt( Message( nonce, plaintext ) );
    if ( verbose ) {
      hexdump( ciphertext, "ct" );
    }

    Message decrypted = decryption_session.decrypt( ciphertext );
    if ( verbose ) {
      printf( DUMP_NAME_FMT NONCE_FMT "\n", "dec nonce", decrypted.nonce.val() );
      hexdump( decrypted.text, "dec pt" );
    }

    fatal_assert( decrypted.nonce.val() == nonce_int );
    fatal_assert( decrypted.text == plaintext );

    nonce_int++;

    if ( ! ( prng.uint8() % 16 ) ) {
      test_bad_decrypt( decryption_session );
    }

    if ( verbose ) {
      printf( "\n" );
    }
  }
}

int main( int argc, char *argv[] ) {
  if ( argc >= 2 && strcmp( argv[ 1 ], "-v" ) == 0 ) {
    verbose = true;
  }

  for ( size_t i=0; i<NUM_SESSIONS; i++ ) {
    try {
      test_one_session();
    } catch ( const CryptoException &e ) {
      fprintf( stderr, "Crypto exception: %s\r\n",
               e.what() );
      fatal_assert( false );
    }
  }

  return 0;
}
