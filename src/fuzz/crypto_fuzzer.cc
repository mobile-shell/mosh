/* Fuzzes the KEYLESS, pre-authentication surface: a raw UDP datagram fed
   straight into Session::decrypt() with a fixed key the "attacker" does not
   know. This is exactly what an internet attacker who cannot forge the OCB tag
   can reach. In practice no fuzz input will pass the AES-OCB tag (~2^-128), so
   this exercises the reject path and every bit of buffer handling that runs
   before and around the tag check: the length arithmetic, the nonce parse, the
   memcpy into the fixed receive buffers, and ae_decrypt() over the bundled
   AES-OCB implementation. Each such datagram must be rejected with a
   Crypto::CryptoException and must never read or write out of bounds -- any
   AddressSanitizer report here is a remote, keyless memory-safety bug. */

#include <cstddef>
#include <cstdint>
#include <string>

#include "src/crypto/crypto.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  /* A fixed all-zero key (canonical 22-char base64). The point is not the key's
     secrecy but that decrypt() handles arbitrary ciphertext safely. */
  static const std::string key_b64( 22, 'A' );
  static Crypto::Session session( ( Crypto::Base64Key( key_b64 ) ) );

  try {
    session.decrypt( reinterpret_cast<const char*>( data ), size );
  } catch ( const Crypto::CryptoException& ) {
    /* forged / short / oversized ciphertext: correctly rejected */
  }

  return 0;
}
