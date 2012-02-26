/* ISC license. */

/* MT-unsafe */

#include <errno.h>
#include <signal.h>
#include "sysdeps.h"
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HASSIGNALFD
# include <sys/signalfd.h>
#else
# include "djbunix.h"
#endif

int selfpipe_init (void)
{
  if (selfpipe_fd >= 0) return (errno = EBUSY, -1) ;
  sigemptyset(&selfpipe_caught) ;
#ifdef HASSIGNALFD
  selfpipe_fd = signalfd(-1, &selfpipe_caught, SFD_NONBLOCK | SFD_CLOEXEC) ;
#else
  if (pipenbcoe(selfpipe) < 0) return -1 ;
#endif
  return selfpipe_fd ;
}
