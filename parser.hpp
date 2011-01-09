#ifndef PARSE_HPP
#define PARSE_HPP

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
