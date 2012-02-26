/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include "allreadwrite.h"

int fd_read (int fd, char *buf, unsigned int len)
{
  register int r ;
  do r = read(fd, buf, len) ;
  while ((r == -1) && (errno == EINTR)) ;
  return r ;
}
