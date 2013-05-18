#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([libsf], [0.1], [ebisawa@gmail.com])
AC_CONFIG_SRCDIR([sf.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign no-installinfo no-installman])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h syslog.h unistd.h])
AC_CHECK_HEADERS([bsd/string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday localtime_r memmove memset socket strerror])

# Checks for OS-specific features.
AC_CHECK_FUNC(kqueue, [AC_DEFINE([HAVE_KQUEUE], [1], [Define to 1 if you have kqueue features.])])
AC_CHECK_FUNC(epoll_create, [AC_DEFINE([HAVE_EPOLL], [1], [Define to 1 if you have epoll features.])])

# Generate files.
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
