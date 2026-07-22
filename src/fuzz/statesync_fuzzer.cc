/* Fuzzes the post-decryption state-sync input path: the serialized protobuf
   "diff" a counterparty sends, applied by the client (Complete::apply_string,
   host state) and the server (UserStream::apply_string, user state). This is
   the exact attacker-controlled surface reached after a datagram is OCB-
   authenticated, reassembled and decompressed. Malformed input is expected to
   be dropped via a Crypto::CryptoException; anything else (an abort or another
   uncaught exception) is a bug. */

#include <cstddef>
#include <cstdint>
#include <string>

#include "src/crypto/crypto.h"
#include "src/statesync/completeterminal.h"
#include "src/statesync/user.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  const std::string diff( reinterpret_cast<const char*>( data ), size );

  /* client applying host state from the server */
  try {
    Terminal::Complete complete( 80, 24 );
    complete.apply_string( diff );
  } catch ( const Crypto::CryptoException& ) {
    /* malformed counterparty input, dropped */
  }

  /* server applying user state from the client */
  try {
    Network::UserStream user;
    user.apply_string( diff );
  } catch ( const Crypto::CryptoException& ) {
    /* malformed counterparty input, dropped */
  }

  return 0;
}
