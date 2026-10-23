#include <clocale>
#include <cstddef>
#include <cstdint>
#include <string>

#include "src/statesync/completeterminal.h"
#include "src/terminal/parser.h"

/*
 * Fuzzes the host->client display-instruction path. Bytes that a program
 * running on the host writes (relayed by the mosh server) are fed through
 * Terminal::Complete::act(), which runs the UTF-8 parser, applies every
 * parsed action to the terminal emulator (Dispatcher -> the CSI/ESC/OSC
 * handler functions -> the Framebuffer), and then the resulting framebuffer
 * is rendered with a headless Display::new_frame(), the same encoding step
 * the mosh server performs when it diffs frames for the client.
 *
 * terminal_parser_fuzzer parses but discards parser actions, and
 * terminal_fuzzer drives the user-input (keystroke) direction only, so the
 * terminal emulation functions themselves (src/terminal/terminalfunctions.cc
 * and friends) are otherwise unreachable.
 */

extern "C" int LLVMFuzzerInitialize( int*, char*** )
{
  /* wcwidth(3) is locale-dependent and mosh's real main() calls
     setlocale(); without a UTF-8 locale the wide- and combining-character
     paths in Emulator::print() cannot be reached. */
  if ( setlocale( LC_ALL, "C.UTF-8" ) == NULL ) {
    setlocale( LC_ALL, "en_US.UTF-8" );
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  Terminal::Complete complete( 80, 24 );

  (void)complete.act( std::string( reinterpret_cast<const char*>( data ), size ) );

  Terminal::Display display( false );
  Terminal::Framebuffer state( 80, 24 );
  (void)display.new_frame( true, state, complete.get_fb() );

  return 0;
}
