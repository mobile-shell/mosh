#ifndef TERMINAL_CPP
#define TERMINAL_CPP

#include <wchar.h>
#include <stdio.h>
#include <vector>
#include <deque>

#include "parser.hpp"
#include "terminalframebuffer.hpp"
#include "terminalactionstate.hpp"

namespace Terminal {
  class Emulator {
    friend void Parser::Print::act_on_terminal( Emulator * );
    friend void Parser::Execute::act_on_terminal( Emulator * );
    friend void Parser::Clear::act_on_terminal( Emulator * );
    friend void Parser::Param::act_on_terminal( Emulator * );
    friend void Parser::Collect::act_on_terminal( Emulator * );
    friend void Parser::CSI_Dispatch::act_on_terminal( Emulator * );
    friend void Parser::Esc_Dispatch::act_on_terminal( Emulator * );

  private:
    Parser::UTF8Parser parser;
    Framebuffer fb;
    ActionState as;

    std::string terminal_to_host;

    /* action methods */
    void print( Parser::Print *act );
    void execute( Parser::Execute *act );
    void param( Parser::Param *act );
    void collect( Parser::Collect *act );
    void clear( Parser::Clear *act );
    void CSI_dispatch( Parser::CSI_Dispatch *act );
    void Esc_dispatch( Parser::Esc_Dispatch *act );

    /* CSI and Escape methods */
    void CSI_EL( void );
    void CSI_ED( void );
    void CSI_cursormove( void );
    void CSI_DA( void );
    void Esc_DECALN( void );

  public:
    Emulator( size_t s_width, size_t s_height );

    std::string input( char c, int debug_fd );

    void debug_printout( int fd );
  };
}

#endif
