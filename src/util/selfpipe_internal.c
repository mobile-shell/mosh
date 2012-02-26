/* ISC license. */

/* MT-unsafe */

#include <signal.h>
#include "sysdeps.h"
#include "selfpipe-internal.h"

sigset_t selfpipe_caught ;

#ifdef HASSIGNALFD

int selfpipe_fd = -1 ;

#else

#include <errno.h>
#include "allreadwrite.h"
#include "djbunix.h"

int selfpipe[2] = { -1, -1 } ;

static void selfpipe_trigger (int s)
{
  char c = (char)s ;
  fd_write(selfpipe[1], &c, 1) ;
}

struct skasigaction const selfpipe_ssa = { &selfpipe_trigger, SKASA_NOCLDSTOP | SKASA_MASKALL } ;

#endif
