#ifndef PTY_COMPAT_HPP
#define PTY_COMPAT_HPP

#include "config.h"

#ifndef HAVE_FORKPTY
#  define forkpty my_forkpty
#endif
#ifndef HAVE_CFMAKERAW
#  define cfmakeraw my_cfmakeraw
#endif

pid_t my_forkpty( int *amaster, char *name,
		  const struct termios *termp,
		  const struct winsize *winp );

void my_cfmakeraw( struct termios *termios_p );

#endif
