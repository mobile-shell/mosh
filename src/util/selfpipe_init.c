/* ISC license. */

/* MT-unsafe */

#include "config.h"

#include <errno.h>
#include <signal.h>
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HAVE_SIGNALFD
# include <sys/signalfd.h>
#endif

int selfpipe_init (void)
{
  if (selfpipe_fd >= 0) return (errno = EBUSY, -1) ;
  sigemptyset(&selfpipe_caught) ;
#ifdef HAVE_SIGNALFD
  selfpipe_fd = signalfd(-1, &selfpipe_caught, SFD_NONBLOCK | SFD_CLOEXEC) ;
#else
  if (pipenbcoe(selfpipe) < 0) return -1 ;
#endif
  return selfpipe_fd ;
}
