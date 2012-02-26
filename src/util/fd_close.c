/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include "djbunix.h"

int fd_close (int fd)
{
  register unsigned int i = 0 ;
doit:
  if (!close(fd)) return 0 ;
  i++ ;
  if (errno == EINTR) goto doit ;
  return ((errno == EBADF) && (i > 1)) ? 0 : -1 ;
}
