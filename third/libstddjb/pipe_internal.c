/* ISC license. */

#include "sysdeps.h"
#ifdef HASPIPE2
# define _GNU_SOURCE
# include <fcntl.h>
#else
# include <errno.h>
#endif

#include <unistd.h>
#include "djbunix.h"

int pipe_internal (int *p, unsigned int flags)
{
#ifdef HASPIPE2
  return pipe2(p, ((flags & DJBUNIX_FLAG_COE) ? O_CLOEXEC : 0) | ((flags & DJBUNIX_FLAG_NB) ? O_NONBLOCK : 0)) ;
#else
  int pi[2] ;
  if (pipe(pi) < 0) return -1 ;
  if (flags & DJBUNIX_FLAG_COE)
    if ((coe(pi[0]) < 0) || (coe(pi[1]) < 0)) goto err ;
  if (flags & DJBUNIX_FLAG_NB)
    if ((ndelay_on(pi[0]) < 0) || (ndelay_on(pi[1]) < 0)) goto err ;
  p[0] = pi[0] ; p[1] = pi[1] ;
  return 0 ;
 err:
  {
    register int e = errno ;
    fd_close(pi[1]) ;
    fd_close(pi[0]) ;
    errno = e ;
  }
  return -1 ;
#endif
}
