#ifndef PARSERACTION_HPP
#define PARSERACTION_HPP

#include <string>

namespace Terminal {
  class Emulator;
}

namespace Parser {
  class Action
  {
  public:
    bool char_present;
    wchar_t ch;
    bool handled;

    virtual std::string name( void ) = 0;

    virtual void act_on_terminal( Terminal::Emulator * ) {};

    Action() : char_present( false ), ch( -1 ), handled( false ) {};
    virtual ~Action() {};
  };

  class Ignore : public Action {
  public: std::string name( void ) { return std::string( "Ignore" ); }
  };
  class Print : public Action {
  public:
    std::string name( void ) { return std::string( "Print" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class Execute : public Action {
  public:
    std::string name( void ) { return std::string( "Execute" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class Clear : public Action {
  public:
    std::string name( void ) { return std::string( "Clear" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class Collect : public Action {
  public:
    std::string name( void ) { return std::string( "Collect" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class Param : public Action {
  public:
    std::string name( void ) { return std::string( "Param" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class Esc_Dispatch : public Action {
  public:
    std::string name( void ) { return std::string( "Esc_Dispatch" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class CSI_Dispatch : public Action {
  public:
    std::string name( void ) { return std::string( "CSI_Dispatch" ); }
    void act_on_terminal( Terminal::Emulator *emu );
  };
  class Hook : public Action {
  public: std::string name( void ) { return std::string( "Hook" ); }
  };
  class Put : public Action {
  public: std::string name( void ) { return std::string( "Put" ); }
  };
  class Unhook : public Action {
  public: std::string name( void ) { return std::string( "Unhook" ); }
  };
  class OSC_Start : public Action {
  public: std::string name( void ) { return std::string( "OSC_Start" ); }
  };
  class OSC_Put : public Action {
  public: std::string name( void ) { return std::string( "OSC_Put" ); }
  };
  class OSC_End : public Action {
  public: std::string name( void ) { return std::string( "OSC_End" ); }
  };
}

#endif
