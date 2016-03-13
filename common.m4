# CHECK_COMPILE_FLAG(flags)
# -------------------------------------
# Checks if the compiler supports the given `flags`.
# Append them to `CFLAGS` on success.
m4_define([CHECK_COMPILE_FLAG], [dnl
_check_flags=$1
_saved_flags="$CFLAGS"
CFLAGS="$_check_flags"
AC_MSG_CHECKING([whether C compiler supports $_check_flags])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
	[AC_MSG_RESULT([yes])]
	[$2="${$2} $_check_flags"],
	[AC_MSG_FAILURE([C compiler seem not to support $_check_flags])]
)
CFLAGS="$_saved_flags"
])
