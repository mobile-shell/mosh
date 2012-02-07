/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PARSERTRANSITION_HPP
#define PARSERTRANSITION_HPP

#include <stdlib.h>

#include "parseraction.h"

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

    Transition( Action *s_action=new Ignore, State *s_next_state=NULL )
      : action( s_action ), next_state( s_next_state )
    {}

    Transition( State *s_next_state )
      : action( new Ignore ), next_state( s_next_state )
    {}
  };
}

#endif
