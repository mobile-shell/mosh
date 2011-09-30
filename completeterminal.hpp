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

    const Framebuffer & get_fb( void ) const { return terminal.get_fb(); }
    bool parser_grounded( void ) const { return parser.is_grounded(); }

    /* interface for Network::Transport */
    void subtract( const Complete * ) {}
    std::string diff_from( const Complete &existing );
    void apply_string( std::string diff );
    bool operator==( const Complete &x ) const;
  };
}

#endif
