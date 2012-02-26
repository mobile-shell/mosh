/* ISC license. */

/* MT-unsafe */

#ifndef SELFPIPE_INTERNAL_H
#define SELFPIPE_INTERNAL_H

#include <signal.h>
#include "sysdeps.h"

extern sigset_t selfpipe_caught ;

#ifdef HASSIGNALFD

extern int selfpipe_fd ;

#else

#include "sig.h"

extern int selfpipe[2] ;
#define selfpipe_fd selfpipe[0]

extern struct skasigaction const selfpipe_ssa ;

#endif

#endif
