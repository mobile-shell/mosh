#ifndef PARSER_HPP
#define PARSER_HPP

/* Based on Paul Williams's parser,
   http://www.vt100.net/emu/dec_ansi_parser */

#include <wchar.h>
#include <vector>

#include "parsertransition.hpp"
#include "parseraction.hpp"
#include "parserstate.hpp"
#include "parserstatefamily.hpp"

namespace Parser {
  class Parser {
  private:
    StateFamily family;
    State *state;

  public:
    Parser() : family(), state( &family.s_Ground ) {}

    Parser( const Parser & );
    bool operator=( const Parser & );
    ~Parser() {}

    std::vector<Action> input( wchar_t c );
  };
}

#endif
