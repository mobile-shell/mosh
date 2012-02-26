/* ISC license. */

/* MT-unsafe */

#include <errno.h>
#include <signal.h>
#include "sysdeps.h"
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HASSIGNALFD

#include <sys/signalfd.h>

int selfpipe_untrap (int sig)
{
  sigset_t ss = selfpipe_caught ;
  sigset_t blah ;
  register int r = sigismember(&selfpipe_caught, sig) ;
  if (selfpipe_fd < 0) return (errno = EBADF, -1) ;
  if (r < 0) return -1 ;
  if (!r) return (errno = EINVAL, -1) ;
  if ((sigdelset(&ss, sig) < 0)
   || (signalfd(selfpipe_fd, &ss, SFD_NONBLOCK | SFD_CLOEXEC) < 0))
    return -1 ;
  sigemptyset(&blah) ;
  sigaddset(&blah, sig) ;
  if (sigprocmask(SIG_UNBLOCK, &blah, 0) < 0)
  {
    int e = errno ;
    signalfd(selfpipe_fd, &selfpipe_caught, SFD_NONBLOCK | SFD_CLOEXEC) ;
    errno = e ;
    return -1 ;
  }
  selfpipe_caught = ss ;
  return 0 ;
}

#else

#include "sig.h"

int selfpipe_untrap (int sig)
{
  register int r = sigismember(&selfpipe_caught, sig) ;
  if (selfpipe_fd < 0) return (errno = EBADF, -1) ;
  if (r < 0) return -1 ;
  if (!r) return (errno = EINVAL, -1) ;
  if (sig_restore(sig) < 0) return -1 ;
  sigdelset(&selfpipe_caught, sig) ;
  return 0 ;
}

#endif
