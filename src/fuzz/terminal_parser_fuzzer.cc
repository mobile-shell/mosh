#include <cstddef>
#include <cstdint>

#include "src/terminal/parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Parser::UTF8Parser parser;
  Parser::Actions result;

  for (size_t i = 0; i < size; i++) {
    parser.input(data[i], result);
  }
  
  return 0;
}
