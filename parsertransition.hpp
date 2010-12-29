#ifndef PARSERTRANSITION_HPP
#define PARSERTRANSITION_HPP

#include <stdlib.h>

#include "parseraction.hpp"

namespace Parser {
  class State;

  class Transition
  {
  public:
    Action action;
    State *next_state;

    Transition( const Transition &x )
      : action( x.action ),
	next_state( x.next_state ) {}
    bool operator=( const Transition & );
    virtual ~Transition() {}

    Transition( Action s_action, State *s_next_state )
      : action( s_action ), next_state( s_next_state )
    {}
  };
}

#endif
