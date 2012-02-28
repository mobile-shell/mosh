/* ISC license. */

#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include "gccattributes.h"

extern char const *error_str (int) gccattr_const ;
extern int error_temp (int) gccattr_const ;

#define error_isagain(e) (((e) == EAGAIN) || ((e) == EWOULDBLOCK))

#endif
