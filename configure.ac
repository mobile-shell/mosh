#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([mosh], [1.3.2], [mosh-devel@mit.edu])
AM_INIT_AUTOMAKE([foreign std-options -Wall -Werror])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_SRCDIR([src/frontend/mosh-client.cc])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC([cc gcc clang])
AC_PROG_CXX([c++ g++ clang++])
AC_PROG_RANLIB
AC_PATH_PROG([PROTOC], [protoc], [])
AS_IF([test x"$PROTOC" = x],
  [AC_MSG_ERROR([cannot find protoc, the Protocol Buffers compiler])])

# automake 1.12 seems to require this, but automake 1.11 doesn't recognize it
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

WARNING_CXXFLAGS=""
PICKY_CXXFLAGS=""
DISTCHECK_CXXFLAGS=""
AC_ARG_ENABLE([compile-warnings],
  [AS_HELP_STRING([--enable-compile-warnings@<:@=no/yes/maximum/error/distcheck@:>@],
     [Turn on compiler warnings])],
  [case "$enableval" in
     no)
       ;;
     '' | yes)
       WARNING_CXXFLAGS="-Wall"
       ;;
     maximum)
       WARNING_CXXFLAGS="-Wall"
       PICKY_CXXFLAGS="-Wextra -pedantic -Wno-long-long -Weffc++ -Wmissing-declarations"
       ;;
     error)
       WARNING_CXXFLAGS="-Wall -Werror"
       PICKY_CXXFLAGS="-Wextra -pedantic -Wno-long-long -Weffc++ -Wmissing-declarations"
       ;;
     distcheck)
       WARNING_CXXFLAGS="-Wall -Werror"
       PICKY_CXXFLAGS="-Wextra -pedantic -Wno-long-long -Weffc++ -Wmissing-declarations"
       AC_LANG_PUSH(C++)
       AX_CHECK_COMPILE_FLAG([-Wno-error=unused-parameter],
         [PICKY_CXXFLAGS="$PICKY_CXXFLAGS -Wno-error=unused-parameter"], [], [-Werror])
       AX_CHECK_COMPILE_FLAG([-Wno-error=c++11-extensions],
         [PICKY_CXXFLAGS="$PICKY_CXXFLAGS -Wno-error=c++11-extensions"], [], [-Werror])
       AX_CHECK_COMPILE_FLAG([-Wno-error=deprecated-declarations],
         [PICKY_CXXFLAGS="$PICKY_CXXFLAGS -Wno-error=deprecated-declarations"], [], [-Werror])
       AC_LANG_POP(C++)
       ;;
     *)
       AC_MSG_ERROR([Unknown argument '$enableval' to --enable-compile-warnings])
       ;;
   esac],
  [WARNING_CXXFLAGS="-Wall"])
AC_SUBST([WARNING_CXXFLAGS])
AC_SUBST([PICKY_CXXFLAGS])

# We want to check for compiler flag support, but there is no way to make
# clang's "argument unused" warning fatal.  So we invoke the compiler through a
# wrapper script that greps for this message.

saved_CC="$CC"
saved_CXX="$CXX"
saved_LD="$LD"
flag_wrap="$srcdir/scripts/wrap-compiler-for-flag-check"
CC="$flag_wrap $CC"
CXX="$flag_wrap $CXX"
LD="$flag_wrap $LD"

# We use the same hardening flags for C and C++.  We must check that each flag
# is supported by both compilers.
AC_DEFUN([check_cc_cxx_flag],
 [AC_LANG_PUSH(C)
  AX_CHECK_COMPILE_FLAG([$1],
   [AC_LANG_PUSH(C++)
    AX_CHECK_COMPILE_FLAG([$1], [$2], [$3], [-Werror $4])
    AC_LANG_POP(C++)],
   [$3], [-Werror $4])
  AC_LANG_POP(C)])
AC_DEFUN([check_link_flag],
 [AX_CHECK_LINK_FLAG([$1], [$2], [$3], [-Werror $4])])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--enable-hardening],
    [Enable compiler and linker options to frustrate memory corruption exploits @<:@yes@:>@])],
  [hardening="$enableval"],
  [hardening="yes"])

HARDEN_CFLAGS=""
HARDEN_LDFLAGS=""
AS_IF([test x"$hardening" != x"no"], [
  check_cc_cxx_flag([-fno-strict-overflow], [HARDEN_CFLAGS="$HARDEN_CFLAGS -fno-strict-overflow"])

  # This one will likely succeed, even on platforms where it does nothing.
  check_cc_cxx_flag([-D_FORTIFY_SOURCE=2], [HARDEN_CFLAGS="$HARDEN_CFLAGS -D_FORTIFY_SOURCE=2"])

  check_link_flag([-fstack-protector-all],
   [HARDEN_CFLAGS="$HARDEN_CFLAGS -fstack-protector-all"
    check_cc_cxx_flag([-Wstack-protector], [HARDEN_CFLAGS="$HARDEN_CFLAGS -Wstack-protector"],
      [], [-fstack-protector-all])
    check_cc_cxx_flag([--param ssp-buffer-size=1], [HARDEN_CFLAGS="$HARDEN_CFLAGS --param ssp-buffer-size=1"],
      [], [-fstack-protector-all])])

  check_cc_cxx_flag([-fPIE],
   [check_link_flag([-fPIE -pie],
     [HARDEN_CFLAGS="$HARDEN_CFLAGS -fPIE"
      HARDEN_LDFLAGS="$HARDEN_LDFLAGS -pie"],
     [check_link_flag([-fPIE -Wl,-pie],
       [HARDEN_CFLAGS="$HARDEN_CFLAGS -fPIE"
        HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-pie"])])])

  check_link_flag([-Wl,-z,relro],
   [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,relro"
    check_link_flag([-Wl,-z,now], [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,now"])])])
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])

# Also check for a few non-hardening-related flags.
MISC_CXXFLAGS=""
AC_LANG_PUSH(C++)
AX_CHECK_COMPILE_FLAG([-fno-default-inline],
  [MISC_CXXFLAGS="$MISC_CXXFLAGS -fno-default-inline"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-pipe],
  [MISC_CXXFLAGS="$MISC_CXXFLAGS -pipe"], [], [-Werror])
AC_LANG_POP(C++)
AC_SUBST([MISC_CXXFLAGS])

# End of flag tests.
CC="$saved_CC"
CXX="$saved_CXX"
LD="$saved_LD"

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

AC_ARG_ENABLE([examples],
  [AS_HELP_STRING([--enable-examples], [Build the miscellaneous programs in src/examples @<:@no@:>@])],
  [build_examples="$enableval"],
  [build_examples="no"])
AM_CONDITIONAL([BUILD_EXAMPLES], [test x"$build_examples" != xno])

AC_ARG_ENABLE([ufw],
  [AS_HELP_STRING([--enable-ufw], [Install firewall profile for ufw (Uncomplicated Firewall) @<:@no@:>@])],
  [install_ufw="$enableval"],
  [install_ufw="no"])
AM_CONDITIONAL([INSTALL_UFW], [test x"$install_ufw" != xno])

AC_ARG_ENABLE([completion],
  [AS_HELP_STRING([--enable-completion], [Install bash_completion rule @<:@no@:>@])],
  [install_completion="$enableval"],
  [install_completion="no"])
AM_CONDITIONAL([INSTALL_COMPLETION], [test x"$install_completion" != xno])

# Checks for libraries.
AC_ARG_WITH([utempter],
  [AS_HELP_STRING([--with-utempter], [write utmp entries using libutempter @<:@check@:>@])],
  [with_utempter="$withval"],
  [with_utempter="check"])
AS_IF([test x"$with_utempter" != xno],
  [AC_SEARCH_LIBS([utempter_remove_record], [utempter],
    [AC_DEFINE([HAVE_UTEMPTER], [1], [Define if libutempter is available.])],
    [AS_IF([test x"$with_utempter" = xcheck],
      [AC_MSG_WARN([Unable to find libutempter; utmp entries will not be made.])],
      [AC_MSG_ERROR([--with-utempter was given but libutempter was not found.])])])])

AC_SEARCH_LIBS([compress], [z], , [AC_MSG_ERROR([Unable to find zlib.])])

AC_SEARCH_LIBS([socket], [socket network])
AC_SEARCH_LIBS([inet_addr], [nsl])

AC_SEARCH_LIBS([clock_gettime], [rt])

# Checks for header files.
AC_CHECK_HEADERS(m4_normalize([
  fcntl.h
  langinfo.h
  limits.h
  locale.h
  netdb.h
  netinet/in.h
  stddef.h
  stdint.h
  inttypes.h
  stdlib.h
  string.h
  sys/ioctl.h
  sys/resource.h
  sys/socket.h
  sys/stat.h
  sys/time.h
  termios.h
  unistd.h
  wchar.h
  wctype.h
  ]), [], [AC_MSG_ERROR([Missing required header file.])])

AC_CHECK_HEADERS([pty.h util.h libutil.h paths.h])
AC_CHECK_HEADERS([endian.h sys/endian.h])
AC_CHECK_HEADERS([utmpx.h])
AC_CHECK_HEADERS([termio.h])
AC_CHECK_HEADERS([sys/uio.h])
AC_LANG_PUSH(C++)
AC_CHECK_HEADERS([memory tr1/memory])
AC_LANG_POP(C++)

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
AC_CHECK_FUNCS(m4_normalize([
  gettimeofday
  setrlimit
  iswprint
  memchr
  memset
  nl_langinfo
  posix_memalign
  setenv
  setlocale
  sigaction
  socket
  strchr
  strdup
  strncasecmp
  strtok
  strerror
  strtol
  wcwidth
  cfmakeraw
  pselect
  getaddrinfo
  getnameinfo
  pledge
  ]))

# Start by trying to find the needed tinfo parts by pkg-config
PKG_CHECK_MODULES([TINFO], [tinfo],
  [AC_DEFINE([HAVE_CURSES_H], [1], [Define to 1 if <curses.h> is present])],
  [PKG_CHECK_MODULES([TINFO], [ncurses],
    [AC_DEFINE([HAVE_CURSES_H], [1], [Define to 1 if <curses.h> is present])],
    [AX_CHECK_LIBRARY([TINFO], [curses.h], [tinfo],
      [AC_DEFINE([HAVE_CURSES_H], [1], [Define to 1 if <curses.h> is present])
        AC_SUBST([TINFO_CFLAGS], ["$TINFO_CPPFLAGS"])
        AC_SUBST([TINFO_LIBS], ["$TINFO_LDFLAGS -ltinfo"])],)])])

# Then try to find it in a specific install dir
AC_ARG_WITH(curses, AC_HELP_STRING([--with-curses=DIR], [Where curses is installed]),
    [if test $withval != yes; then
        cv_curses=$withval
    fi
    if test x$cv_curses != x/usr; then
        CURSES_LDFLAGS="-L${cv_curses}/lib"
        CURSES_CPPFLAGS="-I${cv_curses}/include"
    fi])

if test "x$ax_cv_have_TINFO" = xno ; then
    # save away old LDFLAGS/CPPFLAGS and check for curses with cv_curses
    old_LDFLAGS="$LDFLAGS"
    old_CPPFLAGS="$CPPFLAGS"
    LDFLAGS="$LDFLAGS $CURSES_LDFLAGS"
    CPPFLAGS="$CPPFLAGS $CURSES_CPPFLAGS"

    AX_WITH_CURSES
    # restore old LDFLAGS/CPPFLAGS
    LDFLAGS="$old_LDFLAGS"
    CPPFLAGS="$old_CPPFLAGS"

    if test "x$ax_cv_curses_which" = xno ; then
        AC_MSG_ERROR([requires either tinfo, NcursesW or Ncurses library])
    else
        AC_SUBST([TINFO_LIBS], ["$CURSES_LDFLAGS $CURSES_LIB"])
        AC_SUBST([TINFO_CFLAGS], ["$CURSES_CPPFLAGS"])
    fi
fi

dnl Default to OpenSSL, or OS X crypto library if found
AC_CHECK_HEADERS([CommonCrypto/CommonCrypto.h],
  [default_crypto_library="apple-common-crypto"],
  [default_crypto_library="openssl"]
)

dnl Allow user to select over the default.
AC_ARG_WITH(
  [crypto-library],
  [AS_HELP_STRING([--with-crypto-library=library], [build with the given crypto library, TYPE=openssl|nettle|apple-common-crypto @<:@default=openssl@:>@])],
  [
    case "${withval}" in
      openssl|nettle|apple-common-crypto) ;;
      *) AC_MSG_ERROR([bad value ${withval} for --with-crypto-library]) ;;
    esac
  ],
  [with_crypto_library="$default_crypto_library"]
)

dnl Checks for chosen crypto library
case "${with_crypto_library}" in
  openssl)
    PKG_CHECK_MODULES([CRYPTO], [openssl],
      [AC_DEFINE([USE_OPENSSL_AES], [1], [Use OpenSSL library])],
      [AX_CHECK_LIBRARY([CRYPTO], [openssl/aes.h], [crypto],
        [AC_DEFINE([USE_OPENSSL_AES], [1], [Use OpenSSL library])
          AC_SUBST([CRYPTO_CFLAGS], ["$CRYPTO_CPPFLAGS"])
          AC_SUBST([CRYPTO_LIBS], ["$CRYPTO_LDFLAGS -lcrypto"])],
        [AC_MSG_ERROR([OpenSSL crypto library not found])])])
        ;;
  nettle)
    PKG_CHECK_MODULES([CRYPTO], [nettle],
      [],
      [AC_MSG_ERROR([Nettle crypto library not found])])
    AC_DEFINE([USE_NETTLE_AES], [1], [Use Nettle library])
    ;;
  apple-common-crypto)
    dnl Common Crypto is in Apple's standard paths and base libraries.
    dnl So just check for presence of the header.
    AC_CHECK_HEADERS([CommonCrypto/CommonCrypto.h],
      [],
      [AC_MSG_ERROR([Apple Common Crypto header not found])])
    AC_DEFINE([USE_APPLE_COMMON_CRYPTO_AES], [1], [Use Apple Common Crypto library])
    ;;
esac

AC_CHECK_DECL([forkpty],
  [AC_DEFINE([FORKPTY_IN_LIBUTIL], [1],
     [Define if libutil.h necessary for forkpty().])],
  , [[#include <sys/types.h>
  #include <libutil.h>]])

AC_SEARCH_LIBS([forkpty], [util bsd], [
  AC_DEFINE([HAVE_FORKPTY],, [Define if you have forkpty().])
])

AC_MSG_CHECKING([whether pipe2(..., O_CLOEXEC) is supported])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
int pipefd[2];
]], [[(void) pipe2(pipefd, O_CLOEXEC);]])],
  [AC_DEFINE([HAVE_PIPE2_CLOEXEC], [1],
     [Define if pipe2(..., O_CLOEXEC) is available.])
   AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([whether FD_ISSET() argument is const])
AC_LANG_PUSH(C++)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/select.h>
class T {
public:
  fd_set fds;
  bool Fun( void ) const { return FD_ISSET( 0, &fds ); } };]],
[[T x; return x.Fun();]])],
  [AC_DEFINE([FD_ISSET_IS_CONST], [1],
     [Define if FD_ISSET() fd_set argument is const.])
   AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])
AC_LANG_POP(C++)

AC_MSG_CHECKING([whether std::shared_ptr is available])
AC_LANG_PUSH(C++)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <memory>
class T {
public:
  std::shared_ptr<int> Fun( void ) { return std::shared_ptr<int>( new int ( 0 ) ); } };]],
[[T x; return !!x.Fun();]])],
  [AC_DEFINE([HAVE_STD_SHARED_PTR], [1],
     [Define if std::shared_ptr is available.])
   AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])
AC_LANG_POP(C++)

AC_MSG_CHECKING([whether std::tr1::shared_ptr is available])
AC_LANG_PUSH(C++)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <tr1/memory>
class T {
public:
  std::tr1::shared_ptr<int> Fun( void ) { return std::tr1::shared_ptr<int>( new int ( 0 ) ); } };]],
[[T x; return !!x.Fun();]])],
  [AC_DEFINE([HAVE_STD_TR1_SHARED_PTR], [1],
     [Define if std::tr1::shared_ptr is available.])
   AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])
AC_LANG_POP(C++)

AC_MSG_CHECKING([whether clock_gettime() is supported])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>
struct timespec ts;
]], [[return clock_gettime(CLOCK_MONOTONIC, &ts);]])],
  [AC_DEFINE([HAVE_CLOCK_GETTIME], [1],
     [Define if clock_gettime() is available.])
   AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])

AC_CHECK_DECL([mach_absolute_time],
  [AC_DEFINE([HAVE_MACH_ABSOLUTE_TIME], [1],
     [Define if mach_absolute_time is available.])],
  , [[#include <mach/mach_time.h>]])


AC_CHECK_DECLS([__builtin_ctz])

AC_CHECK_DECLS([ffs], [], [],
  [[#include <strings.h>]])

AC_CHECK_DECLS([be64toh, betoh64, bswap64, __builtin_bswap64], [], [],
  [[#if defined(HAVE_ENDIAN_H)
#include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/types.h>
#include <sys/endian.h>
#endif]])

AS_IF([test x"$ac_cv_have_decl_be64toh" != xyes &&
       test x"$ac_cv_have_decl_betoh64" != xyes],
  [AC_CHECK_DECL([OSSwapHostToBigInt64],
     [AC_DEFINE([HAVE_OSX_SWAP], [1],
        [Define if OSSwapHostToBigInt64 and friends exist.])],
     [AC_MSG_WARN([Unable to find byte swapping functions; using built-in routines.])],
     [[#include <libkern/OSByteOrder.h>]])])

AC_CHECK_DECL([IP_MTU_DISCOVER],
  [AC_DEFINE([HAVE_IP_MTU_DISCOVER], [1],
     [Define if IP_MTU_DISCOVER is a valid sockopt.])],
  , [[#include <netinet/in.h>]])

AC_CHECK_DECL([IP_RECVTOS],
  [AC_DEFINE([HAVE_IP_RECVTOS], [1],
     [Define if IP_RECVTOS is a valid sockopt.])],
  , [[#include <netinet/in.h>]])

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

# On Debian Sid 2016-03-16ish at least, "pkgconfig --libs protobuf"
# gives us the bogus "-lprotobuf -pthread -lpthread"; remove
# "-lpthread" which misconfigures compile, causing a segfault in
# mosh-server.
AS_IF([echo "$protobuf_LIBS" | grep -q -- -pthread],
  [protobuf_LIBS="`echo $protobuf_LIBS | sed 's/-lpthread//g'`"])

# Bash completion needs to ask where it goes if >= 2.0 is installed.
AS_IF([test "$install_completion" != no],
  [PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
     [if test "$prefix" = "NONE"; then
        completions="`pkg-config --variable=completionsdir bash-completion`"
      else
        completions="`pkg-config --define-variable=prefix=$prefix --variable=completionsdir bash-completion`"
      fi],
     [completions="${sysconfdir}/bash_completion.d"])
   AC_SUBST([completions])])

AC_CONFIG_FILES([
  Makefile
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
  conf/Makefile
])
AC_OUTPUT

AC_MSG_NOTICE([ === Configuration results ===])
AC_MSG_NOTICE([Version:             $PACKAGE_VERSION])
AC_MSG_NOTICE([c++ compiler:        $CXX])
AC_MSG_NOTICE([Warning CXXFLAGS:    $WARNING_CXXFLAGS])
AC_MSG_NOTICE([Picky CXXFLAGS:      $PICKY_CXXFLAGS])
AC_MSG_NOTICE([Harden CFLAGS:       $HARDEN_CFLAGS])
AC_MSG_NOTICE([ =============================])
