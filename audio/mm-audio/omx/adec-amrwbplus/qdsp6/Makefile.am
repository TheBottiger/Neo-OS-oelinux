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
        -I ./src/common/ \
         $(MM_CORE_CFLAGS) \
         $(AUDIOALSA_CFLAGS)

AM_CPPFLAGS = -I ./inc/ \
              $(MM_CORE_CFLAGS)

c_sources = src/adec_svr.c \
            src/omx_amrwbplus_adec.cpp

#LIBRARY for Android apps
lib_LTLIBRARIES = libOmxAmrwbplusDec.la
libOmxAmrwbplusDec_la_CC = @CC@
libOmxAmrwbplusDec_la_SOURCES = $(c_sources)
libOmxAmrwbplusDec_la_CFLAGS = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@ -DQC_MODIFIED -D_ENABLE_QC_MSG_LOG_ -DVERBOSE -D_DEBUG -DAUDIOV2 -Wconversion -DQCOM_AUDIO_USE_SYSTEM_HEAP_ID
libOmxAmrwbplusDec_la_CPPFLAGS = $(AM_CPPFLAGS) -Dstrlcpy=g_strlcpy -DUSE_GLIB @GLIB_CFLAGS@ -DQC_MODIFIED -D_ENABLE_QC_MSG_LOG_ -DVERBOSE -D_DEBUG -DAUDIOV2 -Wconversion -DQCOM_AUDIO_USE_SYSTEM_HEAP_ID -include glib.h
libOmxAmrwbplusDec_la_LDFLAGS = $(GLIB_LIBS) -llog -shared -avoid-version
