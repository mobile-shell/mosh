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

/* Tests that malformed fragments from a counterparty are rejected as dropped
   packets (a caught CryptoException) rather than aborting the process, while
   well-formed fragments still reassemble correctly. */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/crypto/crypto.h"
#include "src/network/transportfragment.h"

using namespace Network;
using namespace TransportBuffers;

static int failures = 0;

/* Run body(); it must throw Crypto::CryptoException (a dropped packet). */
#define EXPECT_DROP( what, body )                                                                                       \
  do {                                                                                                                  \
    bool threw = false;                                                                                                 \
    try {                                                                                                               \
      body;                                                                                                            \
    } catch ( const Crypto::CryptoException& ) {                                                                        \
      threw = true;                                                                                                     \
    }                                                                                                                   \
    if ( !threw ) {                                                                                                     \
      fprintf( stderr, "FAIL: %s: expected a dropped packet (CryptoException), none thrown\n", what );                 \
      failures++;                                                                                                       \
    }                                                                                                                   \
  } while ( 0 )

int main( void )
{
  /* 1. A datagram shorter than the fragment header must be dropped, not abort. */
  EXPECT_DROP( "short datagram", { Fragment f( std::string( "abc" ) ); (void)f; } );

  /* 2. Two different fragments claiming the same id and fragment_num must be
        dropped, not abort. */
  {
    FragmentAssembly fa;
    Fragment a( 42, 0, false, "hello" );
    Fragment b( 42, 0, false, "world" );
    fa.add_fragment( a );
    EXPECT_DROP( "conflicting duplicate fragment", fa.add_fragment( b ) );
  }

  /* 3. A complete packet whose reassembled bytes are not valid
        compressed protobuf must be dropped, not abort. */
  {
    FragmentAssembly fa;
    Fragment junk( 7, 0, true, "this is not zlib-compressed protobuf" );
    fa.add_fragment( junk );
    EXPECT_DROP( "garbage reassembly", { Instruction i = fa.get_assembly(); (void)i; } );
  }

  /* 4. Positive control: well-formed fragments still round-trip. */
  {
    Instruction inst;
    inst.set_protocol_version( 0 );
    inst.set_old_num( 3 );
    inst.set_new_num( 4 );
    inst.set_ack_num( 2 );
    inst.set_throwaway_num( 1 );
    inst.set_diff( std::string( 400, 'x' ) + std::string( 400, 'y' ) );

    Fragmenter fragmenter;
    std::vector<Fragment> frags = fragmenter.make_fragments( inst, 80 );

    FragmentAssembly fa;
    bool done = false;
    for ( size_t i = 0; i < frags.size(); i++ ) {
      done = fa.add_fragment( frags[i] );
    }
    if ( !done ) {
      fprintf( stderr, "FAIL: round-trip: reassembly did not complete\n" );
      failures++;
    } else {
      Instruction out = fa.get_assembly();
      if ( out.new_num() != 4 || out.old_num() != 3 || out.ack_num() != 2 || out.throwaway_num() != 1
           || out.diff() != inst.diff() ) {
        fprintf( stderr, "FAIL: round-trip: reassembled instruction does not match\n" );
        failures++;
      }
    }
  }

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
