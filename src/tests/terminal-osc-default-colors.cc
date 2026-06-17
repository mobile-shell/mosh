/*
    Mosh: the mobile shell
    Copyright 2026

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

/* Tests terminal replies for OSC default foreground/background color queries. */

#include <cstdlib>
#include <string>

#include "src/statesync/completeterminal.h"

static bool expect_response( const std::string& input, const std::string& expected )
{
  Terminal::Complete terminal( 80, 24 );
  return terminal.act( input ) == expected;
}

int main()
{
  setenv( "MOSH_DEFAULT_FG", "eeee/eeee/eeee", 1 );
  setenv( "MOSH_DEFAULT_BG", "1111/1111/1111", 1 );

  if ( !expect_response( "\033]10;?\033\\\033]11;?\033\\",
                         "\033]10;rgb:eeee/eeee/eeee\033\\\033]11;rgb:1111/1111/1111\033\\" ) ) {
    return EXIT_FAILURE;
  }

  unsetenv( "MOSH_DEFAULT_FG" );
  if ( !expect_response( "\033]10;?\033\\", "" ) ) {
    return EXIT_FAILURE;
  }

  setenv( "MOSH_DEFAULT_FG", "eeee/eeee/eeee\033]0;bad", 1 );
  if ( !expect_response( "\033]10;?\033\\", "" ) ) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
