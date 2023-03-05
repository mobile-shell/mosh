#include <cstddef>
#include <cstdint>

#include "src/terminal/parser.h"
#include "src/statesync/completeterminal.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Terminal::Display display(false);
  Terminal::Complete complete(80, 24);
  Terminal::Framebuffer state(80, 24);
  for (size_t i = 0; i < size; i++) {
    complete.act(Parser::UserByte(data[i]));
  }
  display.new_frame(true, state, complete.get_fb());
  
  return 0;
}
