/* ISC license. */

/* MT-unsafe */

#include "config.h"

#include <signal.h>
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HAVE_SIGNALFD

void selfpipe_finish (void)
{
  sigprocmask(SIG_UNBLOCK, &selfpipe_caught, 0) ;
  sigemptyset(&selfpipe_caught) ;
  fd_close(selfpipe_fd) ;
  selfpipe_fd = -1 ;
}

#else

void selfpipe_finish (void)
{
  sig_restoreto(&selfpipe_caught, NSIG) ;
  sigemptyset(&selfpipe_caught) ;
  fd_close(selfpipe[1]) ;
  fd_close(selfpipe[0]) ;
  selfpipe[0] = selfpipe[1] = -1 ;
}

#endif
