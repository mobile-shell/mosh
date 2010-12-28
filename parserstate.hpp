#ifndef PARSERSTATE_HPP
#define PARSERSTATE_HPP

#include "parsertransition.hpp"

namespace Parser {
  class State
  {
  private:
    virtual Action enter( void ) { return Ignore(); };
    virtual Action leave( void ) { return Ignore(); };

    virtual Transition input( wchar_t ch __attribute__ ((unused)) ) {
      return IgnoreTransition();
    }

    static Transition anywhere( wchar_t ch );

  public:
    virtual ~State();
  };

  class Ground : public State {};
  class Esc : public State {};
  class Esc_Intermediate : public State {};

  class CSI_Entry : public State {};
  class CSI_Param : public State {};
  class CSI_Intermediate : public State {};
  class CSI_Ignore : public State {};
  
  class DCS_Entry : public State {};
  class DCS_Param : public State {};
  class DCS_Intermediate : public State {};
  class DCS_Passthrough : public State {};
  class DCS_Ignore : public State {};

  class OCS_String : public State {};
  class SOS_PM_APC_String : public State {};
}

#endif
