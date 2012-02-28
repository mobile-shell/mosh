#ifndef SYSDEPS_H
#define SYSDEPS_H

#include "config.h"

#ifdef HAVE_PIPE2
#define HASPIPE2
#else
#undef HASPIPE2
#endif

#ifdef HAVE_SIGNALFD
#define HASSIGNALFD
#else
#undef HASSIGNALFD
#endif

#ifdef HAVE_SIGACTION
#define HASSIGACTION
#else
#undef HASSIGACTION
#endif

#endif
