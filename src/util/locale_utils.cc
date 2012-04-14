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

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

#if HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include "locale_utils.h"

const char *locale_charset( void )
{
  static const char ASCII_name[] = "US-ASCII (ANSI_X3.4-1968)";

  /* Produce more pleasant name of US-ASCII */
  const char *ret = nl_langinfo( CODESET );

  if ( strcmp( ret, "ANSI_X3.4-1968" ) == 0 ) {
    ret = ASCII_name;
  }

  return ret;
}

bool is_utf8_locale( void ) {
  /* Verify locale calls for UTF-8 */
  if ( strcmp( locale_charset(), "UTF-8" ) != 0 &&
       strcmp( locale_charset(), "utf-8" ) != 0 ) {
    return 0;
  }
  return 1;
}

void set_native_locale( void ) {
  /* Adopt native locale */
  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
  }
}

void clear_locale_variables( void ) {
  unsetenv( "LANG" );
  unsetenv( "LANGUAGE" );
  unsetenv( "LC_CTYPE" );
  unsetenv( "LC_NUMERIC" );
  unsetenv( "LC_TIME" );
  unsetenv( "LC_COLLATE" );
  unsetenv( "LC_MONETARY" );
  unsetenv( "LC_MESSAGES" );
  unsetenv( "LC_PAPER" );
  unsetenv( "LC_NAME" );
  unsetenv( "LC_ADDRESS" );
  unsetenv( "LC_TELEPHONE" );
  unsetenv( "LC_MEASUREMENT" );
  unsetenv( "LC_IDENTIFICATION" );
  unsetenv( "LC_ALL" );
}
