/* ISC license. */

/* MT-unsafe */

#ifndef SELFPIPE_INTERNAL_H
#define SELFPIPE_INTERNAL_H

#include "config.h"

#include <signal.h>

#define DJBUNIX_FLAG_NB  0x01U
#define DJBUNIX_FLAG_COE 0x02U

extern int pipe_internal (int *, unsigned int) ;
#define pipenb(p) pipe_internal(p, DJBUNIX_FLAG_NB)
#define pipecoe(p) pipe_internal(p, DJBUNIX_FLAG_COE)
#define pipenbcoe(p) pipe_internal(p, DJBUNIX_FLAG_NB|DJBUNIX_FLAG_COE)
extern int coe (int) ;
extern int ndelay_on (int) ;

extern int fd_close (int) ;
extern int fd_write (int, char const *, unsigned int) ;
extern int fd_read (int, char *, unsigned int) ;
extern int sanitize_read (int) ;

typedef void skasighandler_t (int) ;
typedef skasighandler_t *skasighandler_t_ref ;

struct skasigaction
{
  skasighandler_t_ref handler ;
  unsigned int flags : 2 ;
} ;

extern struct skasigaction const SKASIG_DFL ;

#define SKASA_MASKALL 0x01
#define SKASA_NOCLDSTOP 0x02

extern int skasigaction (int, struct skasigaction const *, struct skasigaction *) ;
extern void sig_restoreto (sigset_t const *, unsigned int) ;
#define sig_restore(sig) skasigaction((sig), &SKASIG_DFL, 0)

extern sigset_t selfpipe_caught ;

#define sig_catcha(sig, ac) skasigaction(sig, (ac), 0)

#ifdef HAVE_SIGNALFD

extern int selfpipe_fd ;

#else

extern int selfpipe[2] ;
#define selfpipe_fd selfpipe[0]

extern struct skasigaction const selfpipe_ssa ;

#endif

#endif
