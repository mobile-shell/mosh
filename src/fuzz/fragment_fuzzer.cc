/* Fuzzes fragment reassembly: the wire-format Fragment header parse and the
   FragmentAssembly state machine that reassembles an Instruction from a
   counterparty's datagrams (parse header -> collect fragments -> uncompress
   -> parse protobuf). This is reached right after OCB authentication.

   The input is used two ways: (1) parsed directly as a single wire Fragment,
   and (2) chopped into a stream of (fragment_num, final, contents) records fed
   to one FragmentAssembly to exercise the dedup / out-of-order-final /
   completion logic. Malformed input must be dropped via a
   Crypto::CryptoException, not abort. */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "src/crypto/crypto.h"
#include "src/network/transportfragment.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  const char* p = reinterpret_cast<const char*>( data );

  /* 1. parse the raw bytes as a single wire Fragment */
  try {
    Network::Fragment frag( std::string( p, size ) );
    Network::FragmentAssembly single;
    if ( single.add_fragment( frag ) ) {
      single.get_assembly();
    }
  } catch ( const Crypto::CryptoException& ) {
  }

  /* 2. drive one FragmentAssembly with a stream of structured fragments that
        all share an id (i.e. one logical packet) */
  try {
    Network::FragmentAssembly assembly;
    size_t pos = 0;
    while ( pos + 4 <= size ) {
      const uint16_t num = ( static_cast<uint8_t>( data[pos] ) | ( static_cast<uint8_t>( data[pos + 1] ) << 8 ) )
                           & 0x7fff;
      const bool final = data[pos + 2] & 1;
      const size_t want = data[pos + 3];
      pos += 4;
      const size_t take = std::min( want, size - pos );
      std::string contents( p + pos, take );
      pos += take;

      Network::Fragment frag( 0 /* shared id */, num, final, contents );
      if ( assembly.add_fragment( frag ) ) {
        assembly.get_assembly();
      }
    }
  } catch ( const Crypto::CryptoException& ) {
  }

  return 0;
}
