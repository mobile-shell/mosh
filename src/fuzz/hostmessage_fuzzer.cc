#include <clocale>
#include <cstddef>
#include <cstdint>
#include <string>

#include "src/protobufs/hostinput.pb.h"
#include "src/statesync/completeterminal.h"

/*
 * Fuzzes the client side of the mosh host-display protocol. HostMessage
 * protobufs sent by the mosh server are deserialized and applied by
 * Terminal::Complete::apply_string() (hostbytes / resize / echoack
 * instructions) -- the boundary where server-sent bytes turn into client
 * terminal-emulator state.
 *
 * terminal_fuzzer only exercises the *encode* side of the protocol
 * (UserMessage serialization of user keystrokes); the decode-and-apply path
 * was previously unreached by every harness.
 *
 * apply_string() fatal_asserts that the message parses: a real mosh server
 * always sends a serialized (hence parseable) HostMessage, and mosh
 * deliberately aborts on corrupt protocol data. The harness therefore parses
 * the input once up front and discards it when it is not a well-formed
 * message, so fuzzing budget is spent on the semantic layer (instruction
 * handling) rather than dying at the parse check. All behaviour downstream
 * of a *parseable* message -- including how apply_string() reacts to hostile
 * but well-formed instructions -- stays reachable.
 */

extern "C" int LLVMFuzzerInitialize( int*, char*** )
{
  /* wcwidth(3) is locale-dependent; see hostbytes_fuzzer.cc. */
  if ( setlocale( LC_ALL, "C.UTF-8" ) == NULL ) {
    setlocale( LC_ALL, "en_US.UTF-8" );
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  const std::string bytes( reinterpret_cast<const char*>( data ), size );

  HostBuffers::HostMessage message;
  if ( !message.ParseFromString( bytes ) ) {
    return 0;
  }

  Terminal::Complete complete( 80, 24 );
  complete.apply_string( bytes );

  return 0;
}
