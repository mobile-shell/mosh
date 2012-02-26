/* ISC license. */

#ifndef SIG_H
#define SIG_H

#include <signal.h>
#include "gccattributes.h"

extern int sig_alarm gccattr_deprecated ;
extern int sig_child gccattr_deprecated ;
extern int sig_stop gccattr_deprecated ;
extern int sig_cont gccattr_deprecated ;
extern int sig_hangup gccattr_deprecated ;
extern int sig_int gccattr_deprecated ;
extern int sig_kill gccattr_deprecated ;
extern int sig_pipe gccattr_deprecated ;
extern int sig_term gccattr_deprecated ;
extern int sig_usr1 gccattr_deprecated ;
extern int sig_usr2 gccattr_deprecated ;
extern int sig_quit gccattr_deprecated ;
extern int sig_abort gccattr_deprecated ;


typedef void skasighandler_t (int) ;
typedef skasighandler_t *skasighandler_t_ref ;

struct skasigaction
{
  skasighandler_t_ref handler ;
  unsigned int flags : 2 ;
} ;

#define SKASA_MASKALL 0x01
#define SKASA_NOCLDSTOP 0x02

extern struct skasigaction const SKASIG_DFL ;
extern struct skasigaction const SKASIG_IGN ;
extern int skasigaction (int, struct skasigaction const *, struct skasigaction *) ;

#define sig_catcha(sig, ac) skasigaction(sig, (ac), 0)
#define sig_restore(sig) skasigaction((sig), &SKASIG_DFL, 0)

extern void sig_restoreto (sigset_t const *, unsigned int) ;
extern int sig_catch (int, skasighandler_t_ref) ;
#define sig_ignore(sig) sig_catcha((sig), &SKASIG_IGN)
#define sig_uncatch(sig) sig_restore(sig)

#define SIGSTACKSIZE 16
extern int sig_pusha (int, struct skasigaction const *) ;
extern int sig_pop (int) ;
extern int sig_push (int, skasighandler_t_ref) ;

extern void sig_block (int) ;
extern void sig_blockset (sigset_t const *) ;
extern void sig_unblock (int) ;
extern void sig_blocknone (void) ;
extern void sig_pause (void) ;
extern void sig_shield (void) ;
extern void sig_unshield (void) ;

#endif
