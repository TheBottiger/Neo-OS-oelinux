inherit autotools pkgconfig sdllvm

DESCRIPTION = "Android IPC utilities"
HOMEPAGE = "http://developer.android.com/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/\
${LICENSE};md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r1"

DEPENDS = "liblog libcutils libhardware libselinux system-core glib-2.0"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI   = "file://frameworks/native"
SRC_URI += "file://servicemanager.service"

S = "${WORKDIR}/native"

EXTRA_OECONF += " --with-core-includes=${WORKSPACE}/system/core/include --with-glib"

# Following machines compile kernel in 32bit. So enable binder IPC also in 32bit mode.
EXTRA_OECONF:append    += " --enable-32bit-binder-ipc"

CFLAGS += "-I${STAGING_INCDIR}/libselinux"

FILES:${PN}-servicemanager-dbg = "${bindir}/.debug/servicemanager"
FILES:${PN}-servicemanager     = "${bindir}/servicemanager"

FILES:${PN}-libbinder-dbg    = "${libdir}/.debug/libbinder.*"
FILES:${PN}-libbinder        = "${libdir}/libbinder.so.*"
FILES:${PN}-libbinder-dev    = "${libdir}/libbinder.so ${libdir}/libbinder.la ${includedir}"
FILES:${PN}-libbinder-static = "${libdir}/libbinder.a"

FILES:${PN}-libui-dbg    = "${libdir}/.debug/libui.*"
FILES:${PN}-libui        = "${libdir}/libui.so.*"
FILES:${PN}-libbui-dev    = "${libdir}/libui.so ${libdir}/libui.la ${includedir}"

do_install:append() {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${WORKDIR}/servicemanager.service -D ${D}${systemd_unitdir}/system/servicemanager.service
       install -d ${D}${systemd_unitdir}/system/multi-user.target.wants/
       # enable the service for multi-user.target
       ln -sf ${systemd_unitdir}/system/servicemanager.service \
            ${D}${systemd_unitdir}/system/multi-user.target.wants/servicemanager.service
   fi
}

FILES:${PN} += "${systemd_unitdir}/system/"
