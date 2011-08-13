#ifndef COMPLETE_TERMINAL_HPP
#define COMPLETE_TERMINAL_HPP

#include "parser.hpp"
#include "terminal.hpp"

/* This class represents the complete terminal -- a UTF8Parser feeding Actions to an Emulator. */

namespace Terminal {
  class Complete {
  private:
    Parser::UTF8Parser parser;
    Terminal::Emulator terminal;

  public:
    Complete( size_t width, size_t height ) : parser(), terminal( width, height ) {}
    
    std::string act( const std::string &str );
    std::string act( const Parser::Action *act );

    std::string open( void ) { return terminal.open(); }
    std::string close( void ) { return terminal.close(); }

    const Framebuffer & get_fb( void ) { return terminal.get_fb(); }
  };
}

#endif
