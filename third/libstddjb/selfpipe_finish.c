/* ISC license. */

/* MT-unsafe */

#include <signal.h>
#include "sysdeps.h"
#include "djbunix.h"
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HASSIGNALFD

void selfpipe_finish (void)
{
  sigprocmask(SIG_UNBLOCK, &selfpipe_caught, 0) ;
  sigemptyset(&selfpipe_caught) ;
  fd_close(selfpipe_fd) ;
  selfpipe_fd = -1 ;
}

#else

#include "sig.h"
#include "nsig.h"

void selfpipe_finish (void)
{
  sig_restoreto(&selfpipe_caught, NSIG) ;
  sigemptyset(&selfpipe_caught) ;
  fd_close(selfpipe[1]) ;
  fd_close(selfpipe[0]) ;
  selfpipe[0] = selfpipe[1] = -1 ;
}

#endif
