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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#ifndef PARSERACTION_HPP
#define PARSERACTION_HPP

#include <string>
#include <vector>

namespace Terminal {
  class Emulator;
}

namespace Parser {
  class Action
  {
  public:
    wchar_t ch;
    bool char_present;

    virtual std::string name( void ) = 0;

    virtual void act_on_terminal( Terminal::Emulator * ) const {};

    virtual bool ignore() const { return false; }

    Action() : ch( -1 ), char_present( false ) {};
    virtual ~Action() {};

    virtual bool operator==( const Action &other ) const;
  };

  typedef std::vector<Action *> Actions;

  class Ignore : public Action {
  public:
    std::string name( void ) { return std::string( "Ignore" ); }
    bool ignore() const { return true; }
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
