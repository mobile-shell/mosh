/* ISC license. */

#ifndef DJBUNIX_H
#define DJBUNIX_H

#define DJBUNIX_FLAG_NB  0x01U
#define DJBUNIX_FLAG_COE 0x02U

extern int coe (int) ;
extern int ndelay_on (int) ;
extern int pipe_internal (int *, unsigned int) ;
#define pipenb(p) pipe_internal(p, DJBUNIX_FLAG_NB)
#define pipecoe(p) pipe_internal(p, DJBUNIX_FLAG_COE)
#define pipenbcoe(p) pipe_internal(p, DJBUNIX_FLAG_NB|DJBUNIX_FLAG_COE)
extern int fd_close (int) ;

#endif
