/* ISC license. */

/* MT-unsafe */

#include "config.h"

#include <signal.h>
#include "selfpipe-internal.h"

sigset_t selfpipe_caught ;

#ifdef HAVE_SIGNALFD

int selfpipe_fd = -1 ;

#else

#include <errno.h>

int selfpipe[2] = { -1, -1 } ;

static void selfpipe_trigger (int s)
{
  char c = (char)s ;
  fd_write(selfpipe[1], &c, 1) ;
}

struct skasigaction const selfpipe_ssa = { &selfpipe_trigger, SKASA_NOCLDSTOP | SKASA_MASKALL } ;

#endif
