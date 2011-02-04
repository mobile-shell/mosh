#ifndef TERMINAL_CPP
#define TERMINAL_CPP

#include <wchar.h>
#include <stdio.h>
#include <vector>
#include <deque>

#include "parseraction.hpp"
#include "terminalframebuffer.hpp"
#include "terminaldispatcher.hpp"
#include "terminaluserinput.hpp"

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
    friend void Parser::UserByte::act_on_terminal( Emulator * );
    friend void Parser::Resize::act_on_terminal( Emulator * );

  private:
    Framebuffer fb;
    Dispatcher dispatch;
    UserInput user;

    /* action methods */
    void print( Parser::Print *act );
    void execute( Parser::Execute *act );
    void CSI_dispatch( Parser::CSI_Dispatch *act );
    void Esc_dispatch( Parser::Esc_Dispatch *act );
    void OSC_end( Parser::OSC_End *act );
    void resize( size_t s_width, size_t s_height );

  public:
    Emulator( size_t s_width, size_t s_height );

    std::string read_octets_to_host( void );

    void debug_printout( int fd );

    std::string open( void ); /* put user cursor keys in application mode */
    std::string close( void ); /* restore user cursor keys */
  };
}

#endif
