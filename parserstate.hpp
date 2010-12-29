#ifndef PARSERSTATE_HPP
#define PARSERSTATE_HPP

#include "parsertransition.hpp"

namespace Parser {
  class State
  {
  protected:
    const virtual Transition input_state_rule( wchar_t ch ) { ch = ch; return Transition( Ignore(), NULL ); }

  private:
    const virtual Action enter( void ) { return Ignore(); };
    const virtual Action leave( void ) { return Ignore(); };

    const Transition input( wchar_t ch );
    const static Transition anywhere_rule( wchar_t ch );

  public:
    virtual ~State() {};
  };

  class Ground : public State {};
  class Escape : public State {};
  class Escape_Intermediate : public State {};

  class CSI_Entry : public State {};
  class CSI_Param : public State {};
  class CSI_Intermediate : public State {};
  class CSI_Ignore : public State {};
  
  class DCS_Entry : public State {};
  class DCS_Param : public State {};
  class DCS_Intermediate : public State {};
  class DCS_Passthrough : public State {};
  class DCS_Ignore : public State {};

  class OSC_String : public State {};
  class SOS_PM_APC_String : public State {};
}

#endif
