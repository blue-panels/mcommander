dnl
dnl Embedded PTY terminal widget (mcterm) support.
dnl
dnl mcterm is the shell mc runs its command line on; there is no longer a
dnl subshell to fall back to, so it is required, not optional.
dnl
AC_DEFUN([mc_MCTERM], [

    AC_CHECK_HEADERS([pty.h libutil.h util.h])
    AC_CHECK_FUNCS([closefrom])
    have_openpty=no
    AC_CHECK_FUNCS([openpty], [have_openpty=yes],
        [AC_CHECK_LIB([util], [openpty],
            [AC_DEFINE([HAVE_OPENPTY], [1], [Define if openpty() is available])
             LIBS="$LIBS -lutil"
             have_openpty=yes])])

    AC_MSG_CHECKING([for mcterm terminal support])
    if test "x$have_openpty" = xno; then
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([openpty() is required but was not found.
Install the appropriate development package (e.g. libutil-dev, libbsd-dev).])
    fi
    AC_MSG_RESULT([yes])

    AC_DEFINE([ENABLE_MCTERM], [1], [Define to enable embedded PTY terminal widget])
    AM_CONDITIONAL([ENABLE_MCTERM], [true])
    mcterm=yes
])
