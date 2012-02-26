/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include "allreadwrite.h"

int fd_write (int fd, char const *buf, unsigned int len)
{
  register int r ;
  do r = write(fd, buf, len) ;
  while ((r == -1) && (errno == EINTR)) ;
  return r ;
}
