pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ../../omx.pc
EXTRA_DIST = $(pkgconfig_DATA)

AM_CFLAGS = -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -g -O0 \
        -fno-inline \
        -fno-short-enums \
        -fpic \
        -I ./inc/ \
         $(MM_CORE_CFLAGS)

AM_CPPFLAGS = -I ./inc/ \
              $(MM_CORE_CFLAGS)

c_sources = src/adec_svr.c \
            src/omx_amr_adec.cpp

#LIBRARY for Android apps
lib_LTLIBRARIES = libOmxAmrDec.la
libOmxAmrDec_la_CC = @CC@
libOmxAmrDec_la_SOURCES = $(c_sources)
libOmxAmrDec_la_CFLAGS = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@ -DQC_MODIFIED -D_ENABLE_QC_MSG_LOG_ -DVERBOSE -D_DEBUG -Wconversion -D__unused=__attribute__\(\(__unused__\)\)
libOmxAmrDec_la_CPPFLAGS = $(AM_CPPFLAGS) -Dstrlcpy=g_strlcpy -DUSE_GLIB @GLIB_CFLAGS@  -DQC_MODIFIED -D_ENABLE_QC_MSG_LOG_ -DVERBOSE -D_DEBUG -Wconversion -include glib.h -D__unused=__attribute__\(\(__unused__\)\)
libOmxAmrDec_la_LDFLAGS = $(GLIB_LIBS) -shared -avoid-version
