#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([audiosum], [0.2], [alvarezp@alvarezp.ods.org])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/audiosum.c])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([-Wall foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_CHECK_LIB([mhash], [mhash_init], , [ERROR=1; ERROR_MSG=`printf "%s\n%s\n" "$ERROR_MSG" "Requirement missing: mhash library."`])
AC_CHECK_FUNCS([pow strstr])

AS_IF([test "$ERROR" = "1"], [echo; echo $ERROR_MSG; echo; exit])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
