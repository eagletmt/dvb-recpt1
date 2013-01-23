#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([recpt1], [0.1.0], [eagletmt@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([tuner.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.

# Checks for boost
AC_ARG_WITH(boost,
  AS_HELP_STRING([--with-boost=PATH], [specify boost install directory]),
  [ if test "x$withval" != "x"; then
      CXXFLAGS="$CXXFLAGS -I$withval/include"
      LDFLAGS="-L$withval/lib $LDFLAGS"
    fi ], [])
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS(
  [boost/tokenizer.hpp boost/algorithm/string/trim.hpp boost/lexical_cast.hpp boost/program_options.hpp],
  [],
  [AC_MSG_ERROR(boost headers are not found.)],
  [])
AC_CHECK_LIB([boost_program_options], [main], [], [AC_MSG_ERROR(boost_program_options library is not found.)])
AC_LANG_POP([C++])

AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime socket strtol])

CXXFLAGS="$CXXFLAGS -std=c++11 -Wall -Wextra"
LIBS="$LIBS -lrt -lboost_program_options"

AC_OUTPUT
