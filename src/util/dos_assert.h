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
