AC_PREREQ(2.65)
AC_INIT(guile-zmq, 2.1.0, [guile-user@gnu.org])
AC_CONFIG_SRCDIR(zmq.scm)
AC_CONFIG_AUX_DIR(build-aux)

LT_INIT

AC_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([color-tests silent-rules])

AC_DISABLE_STATIC

AC_PROG_CC

WARN_CFLAGS=-Wall
AC_ARG_ENABLE([Werror], AC_HELP_STRING([--disable-Werror],[Don't stop the build on errors]),
        [], WARN_CFLAGS="-Wall -Werror")
AC_SUBST(WARN_CFLAGS)

PKG_CHECK_MODULES(GUILE, [guile-2.0 >= 2.0.0])

AC_CHECK_LIB([zmq], [zmq_init])

AC_CONFIG_FILES(
Makefile
guile-zmq/Makefile
zmq/Makefile
tests/Makefile
doc/Makefile
)

AC_OUTPUT
