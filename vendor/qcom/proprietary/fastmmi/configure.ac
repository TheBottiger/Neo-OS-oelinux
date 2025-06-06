AC_PREREQ(2.61)
AC_INIT([fastmmi], 1.0.0)
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AM_PROG_AS
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

AM_CONDITIONAL(NO_UI, true)
AM_CONDITIONAL(ANDROID, false)
AM_CONDITIONAL(USE_LIBMINIUI, false)

AC_ARG_WITH([kernel],
      AC_HELP_STRING([--with-kernel=@<:@dir@:>@],
         [Specify the location of the Linux kernel headers]),
      [kerneldir=$withval],
      with_kernel=no)

if test "x$with_kernel" != "xno"; then
   CFLAGS="${CFLAGS} -I${kerneldir}/include -I${kerneldir}/arch/arm/include"
fi

PKG_CHECK_MODULES([XML2], [libxml-2.0])
AC_SUBST([XML2_CFLAGS])
AC_SUBST([XML2_LIBS])

PKG_CHECK_MODULES([DIAG], [diag])
AC_SUBST([DIAG_CFLAGS])
AC_SUBST([DIAG_LIBS])

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
	AC_DEFINE(ENABLE_USEGLIB, 1, [Define if HLOS systems uses glib])
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

AC_ARG_ENABLE(product,
        [AS_HELP_STRING([--enable-product=PRODUCT], [Specify the product])],
        [PRODUCT=$enableval],
        [PRODUCT=none]
)
AM_CONDITIONAL(DRONE, test "x${PRODUCT}" = "xdrone")
AM_CONDITIONAL(QSAP, test "x${PRODUCT}" = "xqsap")
AM_CONDITIONAL(ROBOT, test "x${PRODUCT}" = "xrobot")

AC_ARG_ENABLE(target,
        [AS_HELP_STRING([--enable-target=TARGET], [Specify the target])],
        [TARGET=$enableval],
        [TARGET=none]
)
AM_CONDITIONAL([APQ8009], [test "x$TARGET" = "xapq8009"])
AS_IF([test "x$TARGET" = "xapq8096" -o "x$TARGET" = "xapq8053" -o "x$TARGET" = "xapq8017"], [MACHINE_64BIT=TRUE])

AS_CASE([$host],
        [arm*], [ARM=yes],
        [ARM=no]
)
AM_CONDITIONAL(ARM, [test "x$ARM" = "xyes"])

AS_CASE([$host],
        [aarch64*], [ARM64=yes],
        [ARM64=no]
)
AM_CONDITIONAL(ARM64, [test "x$ARM64" = "xyes"])

AM_CONDITIONAL(AARCH64_32COMPILER, [test "x$MACHINE_64BIT" = "xTRUE" -a "x$ARM" = "xyes"])

AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC


AC_CONFIG_FILES([      \
      Makefile
      mmi/Makefile
      qti-system-daemon/Makefile
      libmmi/Makefile
      module/cpu/Makefile
      module/battery/Makefile
      module/memory/Makefile
      module/storage/Makefile
      module/wifi/Makefile
      module/sysinfo/Makefile
      module/sensor/Makefile
      module/usb/Makefile
      module/audio/Makefile
      module/bluetooth/Makefile
      module/camera/Makefile
      ])
AC_OUTPUT
