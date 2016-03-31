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

    // Only used locally by act(), but kept here as a performance optimization,
    // to avoid construction/destruction.  It must always be empty
    // outside calls to act() to keep horrible things from happening.
    Parser::Actions actions;

    typedef std::list< std::pair<uint64_t, uint64_t> > input_history_type;
    input_history_type input_history;
    uint64_t echo_ack;

    static const int ECHO_TIMEOUT = 50; /* for late ack */

  public:
    Complete( size_t width, size_t height ) : parser(), terminal( width, height ), display( false ),
					      actions(), input_history(), echo_ack( 0 ) {}
    
    std::string act( const std::string &str );
    std::string act( const Parser::Action *act );

    const Framebuffer & get_fb( void ) const { return terminal.get_fb(); }
    void reset_input( void ) { parser.reset_input(); }
    uint64_t get_echo_ack( void ) const { return echo_ack; }
    bool set_echo_ack( uint64_t now );
    void register_input_frame( uint64_t n, uint64_t now );
    int wait_time( uint64_t now ) const;

    /* interface for Network::Transport */
    void subtract( const Complete * ) const {}
    std::string diff_from( const Complete &existing ) const;
    std::string init_diff( void ) const;
    void apply_string( const std::string & diff );
    bool operator==( const Complete &x ) const;

    bool compare( const Complete &other ) const;
  };
}

#endif
