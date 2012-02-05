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
    mutable bool handled;

    std::string str( void );

    virtual std::string name( void ) = 0;

    virtual void act_on_terminal( Terminal::Emulator * ) const {};

    Action() : char_present( false ), ch( -1 ), handled( false ) {};
    virtual ~Action() {};

    virtual bool operator==( const Action &other ) const;
  };

  class Ignore : public Action {
  public: std::string name( void ) { return std::string( "Ignore" ); }
  };
  class Print : public Action {
  public:
    std::string name( void ) { return std::string( "Print" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class Execute : public Action {
  public:
    std::string name( void ) { return std::string( "Execute" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class Clear : public Action {
  public:
    std::string name( void ) { return std::string( "Clear" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class Collect : public Action {
  public:
    std::string name( void ) { return std::string( "Collect" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class Param : public Action {
  public:
    std::string name( void ) { return std::string( "Param" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class Esc_Dispatch : public Action {
  public:
    std::string name( void ) { return std::string( "Esc_Dispatch" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class CSI_Dispatch : public Action {
  public:
    std::string name( void ) { return std::string( "CSI_Dispatch" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
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
  public:
    std::string name( void ) { return std::string( "OSC_Start" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class OSC_Put : public Action {
  public:
    std::string name( void ) { return std::string( "OSC_Put" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };
  class OSC_End : public Action {
  public:
    std::string name( void ) { return std::string( "OSC_End" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;
  };

  class UserByte : public Action {
    /* user keystroke -- not part of the host-source state machine*/
  public:
    char c; /* The user-source byte. We don't try to interpret the charset */

    std::string name( void ) { return std::string( "UserByte" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;

    UserByte( int s_c ) : c( s_c ) {}

    bool operator==( const UserByte &other ) const
    {
      return c == other.c;
    }
  };

  class Resize : public Action {
    /* resize event -- not part of the host-source state machine*/
  public:
    size_t width, height;

    std::string name( void ) { return std::string( "Resize" ); }
    void act_on_terminal( Terminal::Emulator *emu ) const;

    Resize( size_t s_width, size_t s_height )
      : width( s_width ),
	height( s_height )
    {}

    bool operator==( const Resize &other ) const
    {
      return ( width == other.width ) && ( height == other.height );
    }
  };
}

#endif
