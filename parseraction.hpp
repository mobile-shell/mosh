#ifndef PARSERACTION_HPP
#define PARSERACTION_HPP

#include <string>

namespace Parser {
  class Action
  {
  public:
    bool char_present;
    wchar_t ch;

    std::string name;

    Action() : char_present( false ), ch( -1 ), name( "" ) {}
    virtual ~Action() {};
  };

  class Ignore : public Action {
  public: Ignore() { name = "Ignore"; }
  };
  class Print : public Action {
  public: Print() { name = "Print"; }
  };
  class Execute : public Action {
  public: Execute() { name = "Execute"; }
  };
  class Clear : public Action {
  public: Clear() { name = "Clear"; }
  };
  class Collect : public Action {
  public: Collect() { name = "Collect"; }
  };
  class Param : public Action {
  public: Param() { name = "Param"; }
  };
  class Esc_Dispatch : public Action {
  public: Esc_Dispatch() { name = "Esc_Dispatch"; }
  };
  class CSI_Dispatch : public Action {
  public: CSI_Dispatch() { name = "CSI_Dispatch"; }
  };
  class Hook : public Action {
  public: Hook() { name = "Hook"; }
  };
  class Put : public Action {
  public: Put() { name = "Put"; }
  };
  class Unhook : public Action {
  public: Unhook() { name = "Unhook"; }
  };
  class OSC_Start : public Action {
  public: OSC_Start() { name = "OSC_Start"; }
  };
  class OSC_Put : public Action {
  public: OSC_Put() { name = "OSC_Put"; }
  };
  class OSC_End : public Action {
  public: OSC_End() { name = "OSC_End"; }
  };
}

#endif
