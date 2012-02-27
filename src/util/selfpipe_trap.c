/* ISC license. */

/* MT-unsafe */

#include "config.h"

#include <errno.h>
#include <signal.h>
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HAVE_SIGNALFD

#include <sys/signalfd.h>

int selfpipe_trap (int sig)
{
  sigset_t ss = selfpipe_caught ;
  sigset_t old ;
  if (selfpipe_fd < 0) return (errno = EBADF, -1) ;
  if ((sigaddset(&ss, sig) < 0) || (sigprocmask(SIG_BLOCK, &ss, &old) < 0))
    return -1 ;
  if (signalfd(selfpipe_fd, &ss, SFD_NONBLOCK | SFD_CLOEXEC) < 0)
  {
    int e = errno ;
    sigprocmask(SIG_SETMASK, &old, 0) ;
    errno = e ;
    return -1 ;
  }
  selfpipe_caught = ss ;
  return 0 ;
}

#else

int selfpipe_trap (int sig)
{
  if (selfpipe_fd < 0) return (errno = EBADF, -1) ;
  if (sig_catcha(sig, &selfpipe_ssa) < 0) return -1 ;
  if (sigaddset(&selfpipe_caught, sig) < 0)
  {
    int e = errno ;
    sig_restore(sig) ;
    errno = e ;
    return -1 ;
  }
  return 0 ;
}

#endif
