#ifndef PARSERSTATE_HPP
#define PARSERSTATE_HPP

#include "parsertransition.hpp"

namespace Parser {
  class StateFamily;

  class State
  {
  protected:
    virtual Transition input_state_rule( wchar_t ch ) = 0;
    StateFamily *family;

  private:
    Transition anywhere_rule( wchar_t ch );

  public:
    void setfamily( StateFamily *s_family ) { family = s_family; }
    Transition input( wchar_t ch );
    virtual Action *enter( void ) { return NULL; }
    virtual Action *exit( void ) { return NULL; }

    State() : family( NULL ) {};
    virtual ~State() {};

    State( const State & );
    State & operator=( const State & );
  };

  class Ground : public State {
    Transition input_state_rule( wchar_t ch );
  };

  class Escape : public State {
    Action *enter( void );
    Transition input_state_rule( wchar_t ch );
  };

  class Escape_Intermediate : public State {
    Transition input_state_rule( wchar_t ch );
  };

  class CSI_Entry : public State {
    Action *enter( void );
    Transition input_state_rule( wchar_t ch );
  };
  class CSI_Param : public State {
    Transition input_state_rule( wchar_t ch );
  };
  class CSI_Intermediate : public State {
    Transition input_state_rule( wchar_t ch );
  };
  class CSI_Ignore : public State {
    Transition input_state_rule( wchar_t ch );
  };
  
  class DCS_Entry : public State {
    Action *enter( void );
    Transition input_state_rule( wchar_t ch );
  };
  class DCS_Param : public State {
    Transition input_state_rule( wchar_t ch );
  };
  class DCS_Intermediate : public State {
    Transition input_state_rule( wchar_t ch );
  };
  class DCS_Passthrough : public State {
    Action *enter( void );
    Transition input_state_rule( wchar_t ch );
    Action *exit( void );
  };
  class DCS_Ignore : public State {
    Transition input_state_rule( wchar_t ch );
  };

  class OSC_String : public State {
    Action *enter( void );
    Transition input_state_rule( wchar_t ch );
    Action *exit( void );
  };
  class SOS_PM_APC_String : public State {
    Transition input_state_rule( wchar_t ch );
  };
}

#endif
