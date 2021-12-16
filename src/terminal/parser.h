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

#ifndef PARSER_HPP
#define PARSER_HPP

/* Based on Paul Williams's parser,
   http://www.vt100.net/emu/dec_ansi_parser */

#include <wchar.h>
#include <string.h>

#include "parsertransition.h"
#include "parseraction.h"
#include "parserstate.h"
#include "parserstatefamily.h"

namespace Parser {
  extern const StateFamily family;

  class Parser {
  private:
    State const *state;

  public:
    Parser() : state( &family.s_Ground ) {}

    Parser( const Parser &other );
    Parser & operator=( const Parser & );
    ~Parser() {}

    void input( wchar_t ch, Actions &actions );

    void reset_input( void )
    {
      state = &family.s_Ground;
    }

  };

  static const size_t BUF_SIZE = 8;

  class UTF8Parser {
  private:
    Parser parser;

    char buf[ BUF_SIZE ];
    size_t buf_len;

  public:
    UTF8Parser();

    void input( char c, Actions &actions );

    void reset_input( void )
    {
      parser.reset_input();
      buf[0] = '\0';
      buf_len = 0;
    }
  };
}

#endif
