#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for diag
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([hci-qcomm-init], [1.0.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([bthci_qcomm_linux.cpp])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_PROG_LIBTOOL
# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

# Library configs
AC_ARG_WITH([common_includes],
      AC_HELP_STRING([--with-common-includes=@<:@dir@:>@],
         [Specify the location of the common headers]),
      [common_incdir=$withval],
      with_common_includes=no)

if test "x$with_common_includes" != "xno"; then
   CFLAGS="${CFLAGS} -I${common_incdir}"
fi

AC_SUBST([CPPFLAGS])
AC_SUBST([CFLAGS])
AC_SUBST([CC])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/socket.h sys/time.h termios.h unistd.h])

AC_ARG_WITH([lib_path],
      AC_HELP_STRING([--with-lib-path=@<:@dir@:>@],
         [Specify the location of the libraries]),
      [std_libdir=$withval],
      with_lib_path=no)

if test "x$with_lib_path" != "xno"; then
   SYS_LIB=${std_libdir}
fi

AC_SUBST(SYS_LIB)

AC_CHECK_FUNC(signalfd, dummy=yes,
    AC_MSG_ERROR(signalfd support is required))

AC_CHECK_LIB(rt, clock_gettime, dummy=yes,
    AC_MSG_ERROR(realtime clock support is required))

AC_CHECK_LIB(pthread, pthread_create, dummy=yes,
    AC_MSG_ERROR(posix thread support is required))

AC_ARG_ENABLE(target,
  [AS_HELP_STRING([--enable-target=TARGET], [Specify the target product to build])],
  [TARGET=$enableval],
  [TARGET=none]
)

AC_ARG_ENABLE(rome,
  [AS_HELP_STRING([--enable-rome=ROME], [Specify the rome product to build])],
  [ROME=$enableval],
  [ROME=none]
)

AM_CONDITIONAL([MDM_ROME], [test "x$TARGET" = "xmdm9607" -o "x$TARGET" = "xmdm9635" -o "x$TARGET" = "xmdm9640" -o "x$TARGET" = "xmdm9650" -o "x$TARGET" = "xapq8096" -o "x$TARGET" = "xsdx20" -o "x$ROME" = "xqsap" -o "x$ROME" = "xrobot-rome"])
AM_CONDITIONAL([MDM_PRONTO], [test "x$TARGET" = "xapq8009" -o "x$TARGET" = "xapq8017" -o "x$TARGET" = "xapq8053" -o "x$TARGET" = "xapq8016"])

AM_CONDITIONAL([MDM_CHEROKEE], [test "x$TARGET" = "xapq8098"])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_ARG_WITH([glib],
     AC_HELP_STRING([--with-glib],
        [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
       PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.16, dummy=yes,
                               AC_MSG_ERROR(GThread >= 2.16 is required))
       PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
                               AC_MSG_ERROR(GLib >= 2.16 is required))
       GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
       GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"

       AC_SUBST(GLIB_CFLAGS)
       AC_SUBST(GLIB_LIBS)
fi

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")
# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday memset select socket strerror])

AC_CONFIG_FILES([ \
    Makefile \
    wcnss_filter/Makefile \
    ])

AC_OUTPUT
