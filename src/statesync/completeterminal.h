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

#ifndef COMPLETE_TERMINAL_HPP
#define COMPLETE_TERMINAL_HPP

#include <list>
#include <stdint.h>

#include "parser.h"
#include "terminal.h"

/* This class represents the complete terminal -- a UTF8Parser feeding Actions to an Emulator. */

namespace Terminal {
  class Complete {
  private:
    Parser::UTF8Parser parser;
    Terminal::Emulator terminal;
    Terminal::Display display;

    std::list< std::pair<uint64_t, uint64_t> > input_history;
    uint64_t echo_ack;

    static const int ECHO_TIMEOUT = 50; /* for late ack */

  public:
    Complete( size_t width, size_t height ) : parser(), terminal( width, height ), display( false ),
					      input_history(), echo_ack( 0 ) {}
    
    std::string act( const std::string &str );
    std::string act( const Parser::Action *act );

    const Framebuffer & get_fb( void ) const { return terminal.get_fb(); }
    bool parser_grounded( void ) const { return parser.is_grounded(); }

    uint64_t get_echo_ack( void ) const { return echo_ack; }
    bool set_echo_ack( uint64_t now );
    void register_input_frame( uint64_t n, uint64_t now );
    int wait_time( uint64_t now ) const;

    /* interface for Network::Transport */
    void subtract( const Complete * ) {}
    std::string diff_from( const Complete &existing ) const;
    void apply_string( std::string diff );
    bool operator==( const Complete &x ) const;
  };
}

#endif
