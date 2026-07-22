/* Fuzzes the host-output terminal emulator: the escape/control-sequence byte
   stream a server sends to a client, driven through Complete::act(std::string)
   -- the path that actually reaches terminaldispatcher, terminalfunctions
   (CUP/DCH/ICH/scroll/ECH/...), OSC dispatch (including OSC 8 hyperlinks) and
   the framebuffer mutators, and then the new_frame() diff/render.

   The existing terminal_fuzzer feeds bytes as Parser::UserByte, which exercises
   the keystroke-translation path instead; this harness covers the emulator and
   renderer that a malicious server's output actually drives. Any crash is a
   bug. */

#include <cstddef>
#include <cstdint>
#include <string>

#include "src/statesync/completeterminal.h"
#include "src/terminal/terminaldisplay.h"
#include "src/terminal/terminalframebuffer.h"

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
  Terminal::Complete complete( 80, 24 );

  /* drive the emulator with the fuzzed host byte stream */
  complete.act( std::string( reinterpret_cast<const char*>( data ), size ) );

  /* exercise the frame diff/render against a blank reference framebuffer */
  Terminal::Display display( false );
  Terminal::Framebuffer blank( 80, 24 );
  display.new_frame( true, blank, complete.get_fb() );

  return 0;
}
