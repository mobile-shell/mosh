#ifndef PARSERTRANSITION_HPP
#define PARSERTRANSITION_HPP

#include <stdlib.h>

#include "parseraction.hpp"

class State;

namespace Parser {
  class Transition
  {
  public:
    Action action;
    State *next_state;

    Transition();
    Transition( const Transition & );
    bool operator=( const Transition & );
    virtual ~Transition();
  };

  class IgnoreTransition : public Transition
  {
  public:
    IgnoreTransition()
    {
      action = Ignore();
      next_state = NULL;
    }
  };
}

#endif
