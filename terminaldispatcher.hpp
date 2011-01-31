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
}

namespace Terminal {
  class Framebuffer;
  class Dispatcher;

  enum Function_Type { ESCAPE, CSI, CONTROL };

  class Function {
  public:
    Function() : function( NULL ) {}
    Function( Function_Type type, std::string dispatch_chars,
	      void (*s_function)( Framebuffer *, Dispatcher * ) );
    void (*function)( Framebuffer *, Dispatcher * );
  };

  typedef std::map<std::string, Function> dispatch_map_t;

  class DispatchRegistry {
  public:
    dispatch_map_t escape;
    dispatch_map_t CSI;
    dispatch_map_t control;

    DispatchRegistry() : escape(), CSI(), control() {}
  };

  static DispatchRegistry global_dispatch_registry;

  class Dispatcher {
  private:
    std::string params;
    std::vector<int> parsed_params;
    bool parsed;

    std::string dispatch_chars;

    void parse_params( void );

  public:
    std::string terminal_to_host; /* this is the reply string */

    Dispatcher();
    int getparam( size_t N, int defaultval );
    int param_count( void );

    void newparamchar( Parser::Param *act );
    void collect( Parser::Collect *act );
    void clear( Parser::Clear *act );
    
    std::string str( void );

    void dispatch( Function_Type type, Parser::Action *act, Framebuffer *fb );
    const std::string get_dispatch_chars( void ) { return dispatch_chars; }
  };
}

#endif
