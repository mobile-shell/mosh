/* ISC license. */

/* MT-unsafe */

#include <signal.h>
#include "sig.h"

void sig_restoreto (sigset_t const *set, unsigned int n)
{
  register unsigned int i = 1 ;
  for (; i <= n ; i++)
  {
    register int h = sigismember(set, i) ;
    if (h < 0) continue ;
    if (h) sig_restore(i) ;
  }
}
