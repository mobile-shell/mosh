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

#ifndef TERMINALDISPATCHER_HPP
#define TERMINALDISPATCHER_HPP

#include <vector>
#include <string>
#include <map>

namespace Parser {
  class Action;
  class Param;
  class Collect;
  class Clear;
  class Esc_Dispatch;
  class CSI_Dispatch;
  class Execute;
  class OSC_Start;
  class OSC_Put;
  class OSC_End;
}

namespace Terminal {
  class Framebuffer;
  class Dispatcher;

  enum Function_Type { ESCAPE, CSI, CONTROL };

  class Function {
  public:
    Function() : function( NULL ), clears_wrap_state( true ) {}
    Function( Function_Type type, std::string dispatch_chars,
	      void (*s_function)( Framebuffer *, Dispatcher * ),
	      bool s_clears_wrap_state = true );
    void (*function)( Framebuffer *, Dispatcher * );
    bool clears_wrap_state;
  };

  typedef std::map<std::string, Function> dispatch_map_t;

  class DispatchRegistry {
  public:
    dispatch_map_t escape;
    dispatch_map_t CSI;
    dispatch_map_t control;

    DispatchRegistry() : escape(), CSI(), control() {}
  };

  DispatchRegistry & get_global_dispatch_registry( void );

  class Dispatcher {
  private:
    std::string params;
    std::vector<int> parsed_params;
    bool parsed;

    std::string dispatch_chars;
    std::vector<wchar_t> OSC_string; /* only used to set the window title */

    void parse_params( void );

  public:
    std::string terminal_to_host; /* this is the reply string */

    Dispatcher();
    int getparam( size_t N, int defaultval );
    int param_count( void );

    void newparamchar( const Parser::Param *act );
    void collect( const Parser::Collect *act );
    void clear( const Parser::Clear *act );
    
    std::string str( void );

    void dispatch( Function_Type type, const Parser::Action *act, Framebuffer *fb );
    std::string get_dispatch_chars( void ) { return dispatch_chars; }
    std::vector<wchar_t> get_OSC_string( void ) { return OSC_string; }

    void OSC_put( const Parser::OSC_Put *act );
    void OSC_start( const Parser::OSC_Start *act );
    void OSC_dispatch( const Parser::OSC_End *act, Framebuffer *fb );

    bool operator==( const Dispatcher &x ) const;
  };
}

#endif
