#ifndef DOS_ASSERT_HPP
#define DOS_ASSERT_HPP

#include <stdio.h>
#include <stdlib.h>

static void dos_detected( const char *expression, const char *file, int line, const char *function )
{
  fprintf( stderr, "Illegal counterparty input (possible denial of service) in function %s at %s:%d, failed test: %s\n",
	   function, file, line, expression );
  exit( 1 );
}

#define dos_assert(expr)						\
  ((expr)								\
   ? (void)0								\
   : dos_detected (__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__ ))

#endif
