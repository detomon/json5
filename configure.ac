#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([json5], [0.1], [simon@monoxid.net], [json5], [https://github.com/detomon/json5])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for libraries.
AC_CHECK_LIB([unicodetable])

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

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	test/Makefile
])

AC_CONFIG_SUBDIRS([unicode-table])

# Flags
CFLAGS="$CFLAGS -std=c99 \-Wall \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wstrict-prototypes -Wpointer-arith \
	-Wwrite-strings -Wcast-qual -Wcast-align \
	-Wbad-function-cast \
	-Wformat-security \
	-Wmissing-format-attribute \
	-Winline"

AC_OUTPUT
