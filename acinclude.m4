dnl Macro to check for types
AC_DEFUN([LIBTC_CHECK_TYPE], [
AC_CHECK_TYPE($1, LIBTC_TYPE_$1="/* $1 is defined */",
	LIBTC_TYPE_$1="typedef $2 $1;")
])
