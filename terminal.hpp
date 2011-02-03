#ifndef TERMINAL_CPP
#define TERMINAL_CPP

#include <wchar.h>
#include <stdio.h>
#include <vector>
#include <deque>

#include "parser.hpp"
#include "terminalframebuffer.hpp"
#include "terminaldispatcher.hpp"

namespace Terminal {
  class Emulator {
    friend void Parser::Print::act_on_terminal( Emulator * );
    friend void Parser::Execute::act_on_terminal( Emulator * );
    friend void Parser::Clear::act_on_terminal( Emulator * );
    friend void Parser::Param::act_on_terminal( Emulator * );
    friend void Parser::Collect::act_on_terminal( Emulator * );
    friend void Parser::CSI_Dispatch::act_on_terminal( Emulator * );
    friend void Parser::Esc_Dispatch::act_on_terminal( Emulator * );
    friend void Parser::OSC_Start::act_on_terminal( Emulator * );
    friend void Parser::OSC_Put::act_on_terminal( Emulator * );
    friend void Parser::OSC_End::act_on_terminal( Emulator * );

  private:
    Framebuffer fb;
    Dispatcher dispatch;

    /* action methods */
    void print( Parser::Print *act );
    void execute( Parser::Execute *act );
    void CSI_dispatch( Parser::CSI_Dispatch *act );
    void Esc_dispatch( Parser::Esc_Dispatch *act );
    void OSC_end( Parser::OSC_End *act );

  public:
    Emulator( size_t s_width, size_t s_height );

    std::string read_octets_to_host( void );

    void debug_printout( int fd );
  };
}

#endif
