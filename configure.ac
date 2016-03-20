#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([json5], [0.1], [simon@monoxid.net], [json5], [https://github.com/detomon/json5])

AC_CONFIG_SRCDIR([src/json5-coder.c])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

m4_include([common.m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for libraries.
#AC_CHECK_LIB([unicodetable])

# Checks for header files.
AC_CHECK_HEADERS([float.h stdint.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset strndup])

AM_CFLAGS="$CFLAGS"

# Flags
AM_CFLAGS="$AM_CFLAGS -Wall \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wstrict-prototypes -Wpointer-arith \
	-Wwrite-strings -Wcast-qual -Wcast-align \
	-Wbad-function-cast \
	-Wformat-security \
	-Wmissing-format-attribute \
	-Winline"

CHECK_COMPILE_FLAG([-std=c11], [AM_CFLAGS])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	test/Makefile
])

# Exports to unicode-table
export UT_SYMBOL_PREFIX="json5_ut"
export UT_SNAKE_CASE=1
export UT_CATEGORIES="Lu,Ll,Lt,Lm,Lo,Nl,Nd,Mn,Mc,Zp,Zs"
export UT_INCLUDE_INFO="flags,categories"

AC_CONFIG_SUBDIRS([unicode-table])

AC_SUBST([AM_CFLAGS])

AC_OUTPUT
