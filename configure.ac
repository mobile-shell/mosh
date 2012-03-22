#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([mosh], [1.1.3], [mosh-devel@mit.edu])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_SRCDIR([src/frontend/mosh-client.cc])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PATH_PROG([PROTOC], [protoc], [])
AS_IF([test x"$PROTOC" = x],
  [AC_MSG_ERROR([cannot find protoc, the Protocol Buffers compiler])])

WARNING_CXXFLAGS=""
PICKY_CXXFLAGS=""
AC_ARG_ENABLE([compile-warnings],
  [AS_HELP_STRING([--enable-compile-warnings@<:@=no/yes/maximum/error@:>@],
     [Turn on compiler warnings])],
  [case "$enableval" in
     no)
       ;;
     '' | yes)
       WARNING_CXXFLAGS="-Wall"
       ;;
     maximum)
       WARNING_CXXFLAGS="-Wall"
       PICKY_CXXFLAGS="-Wextra -pedantic -Wno-long-long -Weffc++"
       ;;
     error)
       WARNING_CXXFLAGS="-Wall -Werror"
       PICKY_CXXFLAGS="-Wextra -pedantic -Wno-long-long -Weffc++"
       ;;
     *)
       AC_MSG_ERROR([Unknown argument '$enableval' to --enable-compile-warnings])
       ;;
   esac],
  [WARNING_CXXFLAGS="-Wall"])
AC_SUBST([WARNING_CXXFLAGS])
AC_SUBST([PICKY_CXXFLAGS])

AC_ARG_ENABLE([client],
  [AS_HELP_STRING([--enable-client], [Build the mosh-client program @<:@yes@:>@])],
  [build_client="$enableval"],
  [build_client="yes"])
AM_CONDITIONAL([BUILD_CLIENT], [test x"$build_client" != xno])

AC_ARG_ENABLE([server],
  [AS_HELP_STRING([--enable-server], [Build the mosh-server program @<:@yes@:>@])],
  [build_server="$enableval"],
  [build_server="yes"])
AM_CONDITIONAL([BUILD_SERVER], [test x"$build_server" != xno])

AC_ARG_ENABLE([tests],
  [AS_HELP_STRING([--enable-tests], [Build tests])],
  [build_tests="$enableval"],
  [build_tests="no"])
AM_CONDITIONAL([BUILD_TESTS], [test x"$build_tests" != xno])

AC_ARG_ENABLE([examples],
  [AS_HELP_STRING([--enable-examples], [Build the miscellaneous programs in src/examples @<:@no@:>@])],
  [build_examples="$enableval"],
  [build_examples="no"])
AM_CONDITIONAL([BUILD_EXAMPLES], [test x"$build_examples" != xno])

# Checks for libraries.
AC_ARG_WITH([utempter],
  [AS_HELP_STRING([--with-utempter], [write utmp entries using libutempter @<:@check@:>@])],
  [with_utempter="$withval"],
  [with_utempter="check"])
AS_IF([test x"$with_utempter" != xno],
  [AC_SEARCH_LIBS([utempter_remove_added_record], [utempter],
    [AC_DEFINE([HAVE_UTEMPTER], [1], [Define if libutempter is available.])],
    [AS_IF([test x"$with_utempter" = xcheck],
      [AC_MSG_WARN([Unable to find libutempter; utmp entries will not be made.])],
      [AC_MSG_ERROR([--with-utempter was given but libutempter was not found.])])])])

AC_SEARCH_LIBS([compress], [z], , [AC_MSG_ERROR([Unable to find zlib.])])

AX_BOOST_BASE(, , [AC_MSG_ERROR([Unable to find boost libraries.])])

AC_ARG_WITH([skalibs],
  [AS_HELP_STRING([--with-skalibs[=DIR]],
     [root directory of skalibs installation])],
  [with_skalibs=$withval
   AS_IF([test x"$withval" != xyes],
     [SKALIBS_CPPFLAGS="-I$withval/include"
      SKALIBS_LDFLAGS="-L$withval/lib"])],
  [with_skalibs=no])
AC_ARG_WITH([skalibs-include],
  [AS_HELP_STRING([--with-skalibs-include=DIR],
     [specify exact include dir for skalibs headers])],
  [SKALIBS_CPPFLAGS="-I$withval"])
AC_ARG_WITH([skalibs-libdir],
  [AS_HELP_STRING([--with-skalibs-libdir=DIR],
     [specify exact library dir for skalibs libraries])],
  [SKALIBS_LDFLAGS="-L$withval"])

AM_CONDITIONAL([COND_THIRD_LIBSTDDJB], [test x"$with_skalibs" = xno])
AS_IF([test x"$with_skalibs" != xno],
  [AX_CHECK_LIBRARY([SKALIBS], [selfpipe.h], [stddjb], [],
     [AC_MSG_ERROR([Unable to find skalibs.])])
   AC_SUBST([STDDJB_CPPFLAGS], ["$SKALIBS_CPPFLAGS"])
   AC_SUBST([STDDJB_LDFLAGS], ["$SKALIBS_LDFLAGS -lstddjb"])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h langinfo.h limits.h locale.h netinet/in.h stddef.h stdint.h inttypes.h stdlib.h string.h sys/ioctl.h sys/resource.h sys/socket.h sys/time.h term.h termios.h unistd.h wchar.h wctype.h], [], [AC_MSG_ERROR([Missing required header file.])])

AC_CHECK_HEADERS([pty.h util.h libutil.h])
AC_CHECK_HEADERS([endian.h sys/endian.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINTPTR_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MBRTOWC
AC_CHECK_FUNCS([gettimeofday setrlimit inet_ntoa iswprint memchr memset nl_langinfo posix_memalign setenv setlocale sigaction socket strchr strdup strerror strtol wcwidth])

AC_SEARCH_LIBS([clock_gettime], [rt], [AC_DEFINE([HAVE_CLOCK_GETTIME], [1], [Define if clock_gettime is available.])])

PKG_CHECK_MODULES([TINFO], [tinfo], ,
  [PKG_CHECK_MODULES([TINFO], [ncurses], ,
     [AX_CHECK_LIBRARY([TINFO], [curses.h], [tinfo],
        [AC_SUBST([TINFO_CFLAGS], ["$TINFO_CPPFLAGS"])
         AC_SUBST([TINFO_LIBS], ["$TINFO_LDFLAGS -ltinfo"])],
        [AX_CHECK_LIBRARY([TINFO], [curses.h], [ncurses], ,
           [AC_SUBST([TINFO_CFLAGS], ["$TINFO_CPPFLAGS"])
            AC_SUBST([TINFO_LIBS], ["$TINFO_LDFLAGS -lncurses"])],
           [AC_MSG_ERROR([Unable to find libtinfo or libncurses])])])])])

AC_ARG_VAR([poll_CFLAGS], [C compiler flags for poll])
AC_ARG_VAR([poll_LIBS], [linker flags for poll])
if test -z "$poll_LIBS"; then
   AC_CHECK_LIB([poll], [poll], [poll_LIBS="-lpoll"])
fi

AC_CHECK_DECLS([__builtin_bswap64, __builtin_ctz])

AC_CHECK_DECL([mach_absolute_time],
  [AC_DEFINE([HAVE_MACH_ABSOLUTE_TIME], [1],
     [Define if mach_absolute_time is available.])],
  , [[#include <mach/mach_time.h>]])

AC_CHECK_DECL([pipe2],
  [AC_DEFINE([HAVE_PIPE2], [1],
     [Define if pipe2 is available.])],
  , [[#define _GNU_SOURCE
      #include <unistd.h>]])

AC_CHECK_DECL([signalfd],
  [AC_DEFINE([HAVE_SIGNALFD], [1],
     [Define if signalfd is available.])],
  , [[#include <sys/signalfd.h>]])

AC_CHECK_DECL([forkpty],
  [AC_DEFINE([FORKPTY_IN_LIBUTIL], [1],
     [Define if libutil.h necessary for forkpty().])],
  , [[#include <libutil.h>]])

AC_CHECK_DECL([htobe64],
  [AC_DEFINE([HAVE_HTOBE64], [1],
  [Define if htobe64 and friends exist.])],
  [AC_CHECK_DECL([OSSwapHostToBigInt64],
     [AC_DEFINE([HAVE_OSX_SWAP], [1],
        [Define if OSSwapHostToBigInt64 and friends exist.])],
     [AC_MSG_ERROR([Unable to find byte swapping functions])],
     [[#include <libkern/OSByteOrder.h>]])],
  [[#if defined(HAVE_ENDIAN_H)
#include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>
#endif]])

AC_CHECK_DECL([IP_MTU_DISCOVER],
  [AC_DEFINE([HAVE_IP_MTU_DISCOVER], [1],
     [Define if IP_MTU_DISCOVER is a valid sockopt.])],
  , [[#include <netinet/ip.h>]])

AC_CHECK_DECL([__STDC_ISO_10646__],
  [],
  [AC_MSG_WARN([C library doesn't advertise wchar_t is Unicode (OS X works anyway with workaround).])],
  [[#include <wchar.h>]])

AC_CHECK_DECL([IUTF8],
  [AC_DEFINE([HAVE_IUTF8], [1],
     [Define if IUTF8 is a defined termios mode.])],
  [AC_MSG_WARN([No IUTF8 termios mode; character-erase of multibyte character sequence probably does not work properly in canonical mode on this platform.])],
  [[#include <termios.h>]])

# Checks for protobuf
PKG_CHECK_MODULES([protobuf], [protobuf])

AC_CONFIG_FILES([
  Makefile
  third/Makefile
  third/libstddjb/Makefile
  src/Makefile
  src/crypto/Makefile
  src/frontend/Makefile
  src/network/Makefile
  src/protobufs/Makefile
  src/statesync/Makefile
  src/terminal/Makefile
  src/util/Makefile
  scripts/Makefile
  src/examples/Makefile
  src/tests/Makefile
  man/Makefile
])
AC_OUTPUT
