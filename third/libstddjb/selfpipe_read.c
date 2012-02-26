/* ISC license. */

/* MT-unsafe */

#include "sysdeps.h"
#include "allreadwrite.h"
#include "selfpipe-internal.h"
#include "selfpipe.h"

#ifdef HASSIGNALFD

#include <sys/signalfd.h>

int selfpipe_read (void)
{
  struct signalfd_siginfo buf ;
  register int r = sanitize_read(fd_read(selfpipe_fd, (char *)&buf, sizeof(struct signalfd_siginfo))) ;
  return (r <= 0) ? r : (int)buf.ssi_signo ;
}
      
#else

int selfpipe_read (void)
{
  char c ;
  register int r = sanitize_read((fd_read(selfpipe_fd, &c, 1))) ;
  return (r <= 0) ? r : (int)c ;
}

#endif

