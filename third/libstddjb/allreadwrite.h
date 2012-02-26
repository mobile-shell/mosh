/* ISC license. */

#ifndef ALLREADWRITE_H
#define ALLREADWRITE_H

typedef int iofunc_t (int, char *, unsigned int) ;
typedef iofunc_t *iofunc_t_ref ;

typedef unsigned int alliofunc_t (int, char *, unsigned int) ;
typedef alliofunc_t *alliofunc_t_ref ;

extern int sanitize_read (int) ;
extern unsigned int allreadwrite (iofunc_t_ref, int, char *, unsigned int) ;

extern int fd_read (int, char *, unsigned int) ;
extern int fd_write (int, char const *, unsigned int) ;

extern int fd_recv (int, char *, unsigned int, unsigned int) ;
extern int fd_send (int, char const *, unsigned int, unsigned int) ;

extern unsigned int allread (int, char *, unsigned int) ;
extern unsigned int allwrite (int, char const *, unsigned int) ;


#endif
