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

/* Tests that Session::decrypt() fails closed on a ciphertext that is longer
   than its receive buffer -- throwing CryptoException rather than relying on
   an assert() (which is compiled out under -DNDEBUG) to catch the overflow --
   while normal encrypt/decrypt round-trips still work. */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/crypto/crypto.h"

using namespace Crypto;

static int failures = 0;

int main( void )
{
  Base64Key key;
  Session session( key );

  /* Length must be rejected with an exception (never an abort/exit) both below
     the 24-byte minimum and above RECEIVE_MTU. */
  {
    const size_t bad_lengths[] = { 0, 1, 23, Session::RECEIVE_MTU + 9, Session::RECEIVE_MTU + 100 };
    for ( size_t i = 0; i < sizeof( bad_lengths ) / sizeof( bad_lengths[0] ); i++ ) {
      std::string blob( bad_lengths[i], '\0' );
      bool threw = false;
      try {
        session.decrypt( blob );
      } catch ( const CryptoException& ) {
        threw = true;
      }
      if ( !threw ) {
        fprintf( stderr, "FAIL: length %zu was not rejected\n", bad_lengths[i] );
        failures++;
      }
    }
  }

  /* A tampered ciphertext must fail the integrity check. This is the property
     the pt_len >= 0 invariant protects: a forgery must never be accepted. */
  {
    Message message( Nonce( uint64_t( 7 ) ), "the mobile shell" );
    std::string ciphertext = session.encrypt( message );
    ciphertext[ciphertext.size() - 1] ^= 0x01; /* flip a bit in the tag */
    bool threw = false;
    try {
      session.decrypt( ciphertext );
    } catch ( const CryptoException& ) {
      threw = true;
    }
    if ( !threw ) {
      fprintf( stderr, "FAIL: tampered ciphertext was accepted\n" );
      failures++;
    }
  }

  /* Positive control: a minimum-length (empty-payload, 24-byte) round-trip and
     a normal round-trip both succeed. */
  {
    Message empty( Nonce( uint64_t( 2 ) ), "" );
    std::string ct_empty = session.encrypt( empty );
    if ( ct_empty.size() != 24 ) {
      fprintf( stderr, "FAIL: empty message did not produce a 24-byte packet (%zu)\n", ct_empty.size() );
      failures++;
    }
    if ( !session.decrypt( ct_empty ).text.empty() ) {
      fprintf( stderr, "FAIL: empty-payload round-trip did not recover empty plaintext\n" );
      failures++;
    }

    const std::string plaintext = "the mobile shell";
    Message message( Nonce( uint64_t( 1 ) ), plaintext );
    std::string ciphertext = session.encrypt( message );
    if ( session.decrypt( ciphertext ).text != plaintext ) {
      fprintf( stderr, "FAIL: round-trip did not recover the plaintext\n" );
      failures++;
    }
  }

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
