/* ISC license. */

/* MT-unsafe */

#include <errno.h>
#include <signal.h>
#include "sysdeps.h"
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HASSIGNALFD

#include <sys/signalfd.h>

int selfpipe_trapset (sigset_t const *set)
{
  sigset_t old ;
  if (selfpipe_fd < 0) return (errno = EBADF, -1) ;
  if (sigprocmask(SIG_SETMASK, set, &old) < 0) return -1 ;
  if (signalfd(selfpipe_fd, set, SFD_NONBLOCK | SFD_CLOEXEC) < 0)
  {
    int e = errno ;
    sigprocmask(SIG_SETMASK, &old, 0) ;
    errno = e ;
    return -1 ;
  }
  selfpipe_caught = *set ;
  return 0 ;
}

#else

#include "sig.h"
#include "nsig.h"

int selfpipe_trapset (sigset_t const *set)
{
  unsigned int i = 1 ;
  if (selfpipe_fd < 0) return (errno = EBADF, -1) ;
  for (; i <= NSIG ; i++)
  {
    register int h = sigismember(set, i) ;
    if (h < 0) continue ;
    if (h)
    {
      if (sig_catcha(i, &selfpipe_ssa) < 0) break ;
    }
    else if (sigismember(&selfpipe_caught, i))
    {
      if (sig_restore(i) < 0) break ;
    }
  }
  if (i <= NSIG)
  {
    int e = errno ;
    sig_restoreto(set, i) ;
    errno = e ;
    return -1 ;
  }
  selfpipe_caught = *set ;
  return 0 ;
}

#endif
