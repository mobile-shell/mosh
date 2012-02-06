#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT(mosh, 0.9, mosh-devel@mit.edu)
AM_INIT_AUTOMAKE(mosh, 0.9)
AC_CONFIG_SRCDIR([src/frontend/mosh-client.cc])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT(Makefile src/Makefile src/crypto/Makefile src/frontend/Makefile src/network/Makefile src/protobufs/Makefile src/statesync/Makefile src/terminal/Makefile src/util/Makefile scripts/Makefile)

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h langinfo.h limits.h locale.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h termios.h unistd.h wchar.h wctype.h])

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

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MBRTOWC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime gettimeofday inet_ntoa iswprint memchr memset nl_langinfo setenv setlocale socket strchr strdup strerror strtol wcwidth])

# Checks for protobuf
PKG_CHECK_MODULES([protobuf_lite], [protobuf-lite])

AC_OUTPUT
