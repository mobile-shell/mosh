/* ISC license. */

#include <errno.h>

#define error_isagain(e) (((e) == EAGAIN) || ((e) == EWOULDBLOCK))

int sanitize_read (int r)
{
  switch (r)
  {
    case -1 : return error_isagain(errno) ? (errno = 0, 0) : -1 ;
    case 0  : return (errno = EPIPE, -1) ;
    default : return r ;
  }
}
