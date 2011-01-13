#ifndef PARSERTRANSITION_HPP
#define PARSERTRANSITION_HPP

#include <stdlib.h>

#include "parseraction.hpp"

namespace Parser {
  class State;

  class Transition
  {
  public:
    Action *action;
    State *next_state;

    Transition( const Transition &x )
      : action( x.action ),
	next_state( x.next_state ) {}
    Transition & operator=( const Transition &t )
    {
      action = t.action;
      next_state = t.next_state;

      return *this;
    }
    virtual ~Transition() {}

    Transition( Action *s_action=new Ignore(), State *s_next_state=NULL )
      : action( s_action ), next_state( s_next_state )
    {}

    Transition( State *s_next_state )
      : action( new Ignore() ), next_state( s_next_state )
    {}
  };
}

#endif
