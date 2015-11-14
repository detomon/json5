#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([Example], [0.1], [simon@monoxid.net], [json5], [https://github.com/detomon/json5])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset])

# Checks for libraries.
#AC_CHECK_LIB([iconv], [iconv], [], echo "libiconv not found" && exit 1)

AC_CONFIG_FILES([
 Makefile
])

# Flags
CFLAGS="$CFLAGS -std=c99 -Wall \
-Wmissing-prototypes -Wmissing-declarations \
-Wstrict-prototypes -Wpointer-arith \
-Wwrite-strings -Wcast-qual -Wcast-align \
-Wbad-function-cast \
-Wformat-security  -Wformat-nonliteral \
-Wmissing-format-attribute \
-Winline"

AC_OUTPUT
