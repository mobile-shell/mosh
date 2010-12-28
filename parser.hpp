#ifndef PARSE_HPP
#define PARSE_HPP

#include <wchar.h>

#include "parsertransition.hpp"
#include "parseraction.hpp"
#include "parserstate.hpp"

namespace Parser {
  class Parser {
  private:
    State *state;

  public:
    Parser() : state( NULL ) {}
    Parser( const Parser & );
    bool operator=( const Parser & );
    void input( wchar_t c ); /* should return list of actions */
    ~Parser() {}
  };
}

#endif
