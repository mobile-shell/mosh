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

#ifndef PARSER_HPP
#define PARSER_HPP

/* Based on Paul Williams's parser,
   http://www.vt100.net/emu/dec_ansi_parser */

#include <wchar.h>
#include <list>
#include <string.h>

#include "parsertransition.h"
#include "parseraction.h"
#include "parserstate.h"
#include "parserstatefamily.h"

#ifndef __STDC_ISO_10646__
#error "Must have __STDC_ISO_10646__"
#endif

namespace Parser {
  static const StateFamily family;

  class Parser {
  private:
    State const *state;

  public:
    Parser() : state( &family.s_Ground ) {}

    Parser( const Parser &other );
    Parser & operator=( const Parser & );
    ~Parser() {}

    std::list<Action *> input( wchar_t ch );

    bool operator==( const Parser &x ) const
    {
      return state == x.state;
    }

    bool is_grounded( void ) const { return state == &family.s_Ground; }
  };

  static const size_t BUF_SIZE = 8;

  class UTF8Parser {
  private:
    Parser parser;

    char buf[ BUF_SIZE ];
    size_t buf_len;

  public:
    UTF8Parser();

    std::list<Action *> input( char c );

    bool operator==( const UTF8Parser &x ) const
    {
      return parser == x.parser;
    }

    bool is_grounded( void ) const { return parser.is_grounded(); }
  };
}

#endif
