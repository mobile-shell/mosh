#ifndef PARSE_HPP
#define PARSE_HPP

#include <wchar.h>

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
    Parser() : family(), state( NULL ) {}
    Parser( const Parser & );
    bool operator=( const Parser & );
    void input( wchar_t c ); /* should return list of actions */
    ~Parser() {}
  };
}

#endif
