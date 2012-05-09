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

#ifndef DOS_ASSERT_HPP
#define DOS_ASSERT_HPP

#include <stdio.h>
#include <stdlib.h>

#include "crypto.h"

static void dos_detected( const char *expression, const char *file, int line, const char *function )
{
  char buffer[ 2048 ];
  snprintf( buffer, 2048, "Illegal counterparty input (possible denial of service) in function %s at %s:%d, failed test: %s\n",
	    function, file, line, expression );
  throw Crypto::CryptoException( buffer );
}

#define dos_assert(expr)						\
  ((expr)								\
   ? (void)0								\
   : dos_detected (#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__ ))

#endif
