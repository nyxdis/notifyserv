#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.59])
AC_INIT([NotifyServ], [notifyserv_version-notifyserv_rev], [angelos@unkreativ.org], [notifyserv])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([dist-bzip2 foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname select socket strdup strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
