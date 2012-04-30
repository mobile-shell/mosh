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

#ifndef FATAL_ASSERT_HPP
#define FATAL_ASSERT_HPP

#include <stdio.h>
#include <stdlib.h>

static void fatal_error( const char *expression, const char *file, int line, const char *function )
{
  fprintf( stderr, "Fatal assertion failure in function %s at %s:%d\nFailed test: %s\n",
           function, file, line, expression );
  exit( 2 );
}

#define fatal_assert(expr)						\
  ((expr)								\
   ? (void)0								\
   : fatal_error (#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__ ))

#endif
