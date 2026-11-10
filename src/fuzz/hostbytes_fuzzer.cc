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
 * Each input starts with a two-byte header selecting the terminal geometry:
 * the first byte is the initial number of columns and the second byte the
 * initial number of rows, offset by one and taken modulo a cap (1-128
 * columns, 1-64 rows). This keeps the framebuffer size fuzzed while staying
 * within the range a real terminal can have and bounding per-input time and
 * memory. The remaining bytes are the host byte stream.
 *
 * Window resizes reach the terminal as a Parser::Resize action rather than
 * as an escape sequence (the real client injects one on SIGWINCH), so after
 * the byte stream is applied the terminal is resized to the transposed
 * geometry. This drives Emulator::resize() -> Framebuffer::resize()/
 * DrawState::resize() (including the scroll-region reset) in both the
 * growing and shrinking directions, and leaves the final frame a different
 * size than it started, exercising the resize handling in
 * Display::new_frame().
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
  if ( size < 2 ) {
    return 0;
  }

  /* Initial terminal geometry from the two header bytes, offset by one so
     the framebuffer is always at least one cell (the Framebuffer constructor
     asserts width and height are positive). The caps bound the cost of
     rendering and diffing full frames per input. */
  const size_t width  = 1 + static_cast<size_t>( data[0] % 128 );
  const size_t height = 1 + static_cast<size_t>( data[1] % 64 );

  Terminal::Complete complete( width, height );

  (void)complete.act( std::string( reinterpret_cast<const char*>( data ) + 2, size - 2 ) );

  /* Resize to the transposed geometry, the way a SIGWINCH on the client
     would; see the comment at the top of the file. */
  (void)complete.act( Parser::Resize( height, width ) );

  Terminal::Display display( false );
  Terminal::Framebuffer state( static_cast<int>( width ), static_cast<int>( height ) );
  (void)display.new_frame( true, state, complete.get_fb() );

  return 0;
}
