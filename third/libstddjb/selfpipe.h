/* ISC license. */

/* MT-unsafe */

#ifndef SELFPIPE_H
#define SELFPIPE_H

#include <signal.h>

extern int selfpipe_init (void) ;
extern int selfpipe_trap (int) ;
extern int selfpipe_untrap (int) ;
extern int selfpipe_trapset (sigset_t const *) ;
extern int selfpipe_read (void) ;
extern void selfpipe_finish (void) ;

#endif
