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

#ifndef PARSERSTATE_HPP
#define PARSERSTATE_HPP

#include "parsertransition.h"

namespace Parser {
  class StateFamily;

  class State
  {
  protected:
    virtual Transition input_state_rule( wchar_t ch ) const = 0;
    StateFamily *family;

  private:
    Transition anywhere_rule( wchar_t ch ) const;

  public:
    void setfamily( StateFamily *s_family ) { family = s_family; }
    Transition input( wchar_t ch ) const;
    virtual Action *enter( void ) const { return new Ignore; }
    virtual Action *exit( void ) const { return new Ignore; }

    State() : family( NULL ) {};
    virtual ~State() {};

    State( const State & );
    State & operator=( const State & );
  };

  class Ground : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };

  class Escape : public State {
    Action *enter( void ) const;
    Transition input_state_rule( wchar_t ch ) const;
  };

  class Escape_Intermediate : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };

  class CSI_Entry : public State {
    Action *enter( void ) const;
    Transition input_state_rule( wchar_t ch ) const;
  };
  class CSI_Param : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };
  class CSI_Intermediate : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };
  class CSI_Ignore : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };
  
  class DCS_Entry : public State {
    Action *enter( void ) const;
    Transition input_state_rule( wchar_t ch ) const;
  };
  class DCS_Param : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };
  class DCS_Intermediate : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };
  class DCS_Passthrough : public State {
    Action *enter( void ) const;
    Transition input_state_rule( wchar_t ch ) const;
    Action *exit( void ) const;
  };
  class DCS_Ignore : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };

  class OSC_String : public State {
    Action *enter( void ) const;
    Transition input_state_rule( wchar_t ch ) const;
    Action *exit( void ) const;
  };
  class SOS_PM_APC_String : public State {
    Transition input_state_rule( wchar_t ch ) const;
  };
}

#endif
