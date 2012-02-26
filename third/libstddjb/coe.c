/* ISC license. */

#include <fcntl.h>
#include "djbunix.h"

int coe (int fd)
{
   register int flags = fcntl(fd, F_GETFD, 0) ;
   if (flags < 0) return -1 ;
   return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) ;
}
