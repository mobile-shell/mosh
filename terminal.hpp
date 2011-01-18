#ifndef TERMINAL_CPP
#define TERMINAL_CPP

#include <wchar.h>
#include "parser.hpp"

namespace Terminal {
  class Terminal {
  private:
    Parser::UTF8Parser parser;

    wchar_t *framebuffer;
    size_t width, height;

    size_t cursor_col, cursor_row;

  public:
    Terminal();
    ~Terminal();

    
    
    Terminal( const Terminal & );
    Terminal &operator=( const Terminal & );
  };
}

#endif
