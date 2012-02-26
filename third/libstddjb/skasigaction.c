/* ISC license. */

#include <signal.h>
#include "sysdeps.h"
#include "sig.h"

#ifdef HASSIGACTION

int skasigaction (int sig, struct skasigaction const *new, struct skasigaction *old)
{
  struct sigaction sanew, saold ;
  if (((new->flags & SKASA_MASKALL) ? sigfillset(&sanew.sa_mask) : sigemptyset(&sanew.sa_mask)) == -1) return -1 ;
  sanew.sa_handler = new->handler ;
  sanew.sa_flags = (new->flags & SKASA_NOCLDSTOP) ? SA_NOCLDSTOP : 0 ;
  if (sigaction(sig, &sanew, &saold) == -1) return -1 ;
  if (old)
  {
    register int r = sigismember(&saold.sa_mask, (sig == SIGTERM) ? SIGPIPE : SIGTERM) ;
    if (r == -1) return -1 ;
    old->flags = 0 ;
    if (r) old->flags |= SKASA_MASKALL ;
    if (saold.sa_flags & SA_NOCLDSTOP) old->flags |= SKASA_NOCLDSTOP ;
    old->handler = saold.sa_handler ;
  }
  return 0 ;
}

#else

int skasigaction (int sig, struct skasigaction const *new, struct skasigaction *old)
{
  skasighandler_t_ref haold = signal(sig, new->handler) ;
  if (haold == SIG_ERR) return -1 ;
  if (old)
  {
    old->handler = haold ;
    old->flags = 0 ;
  }
  return 0 ;
}

#endif
