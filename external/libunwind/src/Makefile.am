SOVERSION=8:1:0		# See comments at end of file.
SETJMP_SO_VERSION=0:0:0
COREDUMP_SO_VERSION=0:0:0
#
# Don't link with start-files since we don't use any constructors/destructors:
#
COMMON_SO_LDFLAGS = $(LDFLAGS_NOSTARTFILES)

lib_LIBRARIES =
lib_LTLIBRARIES =
if !REMOTE_ONLY
lib_LTLIBRARIES += libunwindandroidandroid.la
if BUILD_PTRACE
lib_LTLIBRARIES += libunwindandroid-ptrace.la
endif
if BUILD_COREDUMP
lib_LTLIBRARIES += libunwindandroid-coredump.la
endif
endif

noinst_HEADERS =
noinst_LTLIBRARIES =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libunwindandroidandroid-generic.pc

if !REMOTE_ONLY
pkgconfig_DATA += unwind/libunwindandroidandroid.pc
endif

if BUILD_PTRACE
pkgconfig_DATA += ptrace/libunwindandroidandroid-ptrace.pc
endif

if BUILD_SETJMP
pkgconfig_DATA += setjmp/libunwindandroidandroid-setjmp.pc
endif

if BUILD_COREDUMP
pkgconfig_DATA += coredump/libunwindandroidandroid-coredump.pc
endif

### libunwindandroid-ptrace:
libunwindandroidandroid_ptrace_la_SOURCES =						  \
	ptrace/_UPT_elf.c						  \
	ptrace/_UPT_accessors.c ptrace/_UPT_access_fpreg.c		  \
	ptrace/_UPT_access_mem.c ptrace/_UPT_access_reg.c		  \
	ptrace/_UPT_create.c ptrace/_UPT_destroy.c			  \
	ptrace/_UPT_find_proc_info.c ptrace/_UPT_get_dyn_info_list_addr.c \
	ptrace/_UPT_put_unwind_info.c ptrace/_UPT_get_proc_name.c	  \
	ptrace/_UPT_reg_offset.c ptrace/_UPT_resume.c
noinst_HEADERS += ptrace/_UPT_internal.h

### libunwindandroidandroid-coredump:
libunwindandroidandroid_coredump_la_SOURCES = \
	coredump/_UCD_accessors.c \
	coredump/_UCD_create.c \
	coredump/_UCD_destroy.c \
	coredump/_UCD_access_mem.c \
	coredump/_UCD_elf_map_image.c \
	coredump/_UCD_find_proc_info.c \
	coredump/_UCD_get_proc_name.c \
	\
	coredump/_UPT_elf.c \
	coredump/_UPT_access_fpreg.c \
	coredump/_UPT_get_dyn_info_list_addr.c \
	coredump/_UPT_put_unwind_info.c \
	coredump/_UPT_resume.c
libunwindandroidandroid_coredump_la_LDFLAGS = $(COMMON_SO_LDFLAGS) \
				-version-info $(COREDUMP_SO_VERSION)
noinst_HEADERS += coredump/_UCD_internal.h

### libunwindandroid-setjmp:
libunwindandroidandroid_setjmp_la_LDFLAGS		= $(COMMON_SO_LDFLAGS)		     \
					  -version-info $(SETJMP_SO_VERSION)

if USE_ELF32
libunwindandroid_ELF = libunwindandroidandroid-elf32.la
endif
if USE_ELF64
libunwindandroid_ELF = libunwindandroidandroid-elf64.la
endif
if USE_ELFXX
libunwindandroid_ELF = libunwindandroidandroid-elfxx.la
endif

libunwindandroidandroid_setjmp_la_LIBADD		= $(libunwindandroid_ELF)	\
					  libunwindandroidandroid-$(arch).la	\
					  libunwindandroidandroid.la -lc
libunwindandroid_setjmp_la_SOURCES		= setjmp/longjmp.c	\
					  setjmp/siglongjmp.c
noinst_HEADERS				+= setjmp/setjmp_i.h

### libunwindandroid:
libunwindandroidandroid_la_LIBADD =

# List of arch-independent files needed by both local-only and generic
# libraries:
libunwindandroidandroid_la_SOURCES_common =					\
	$(libunwindandroid_la_SOURCES_os)				\
	mi/init.c mi/flush_cache.c mi/mempool.c mi/strerror.c

# List of arch-independent files needed by generic library (libunwindandroid-$ARCH):
libunwindandroidandroid_la_SOURCES_generic =						\
	mi/Gdyn-extract.c mi/Gdyn-remote.c mi/Gfind_dynamic_proc_info.c	\
	mi/Gget_accessors.c						\
	mi/Gget_proc_info_by_ip.c mi/Gget_proc_name.c			\
	mi/Gput_dynamic_unwind_info.c mi/Gdestroy_addr_space.c		\
	mi/Gget_reg.c mi/Gset_reg.c					\
	mi/Gget_fpreg.c mi/Gset_fpreg.c					\
	mi/Gset_caching_policy.c

if SUPPORT_CXX_EXCEPTIONS
libunwindandroidandroid_la_SOURCES_local_unwind =					\
	unwind/Backtrace.c unwind/DeleteException.c			\
	unwind/FindEnclosingFunction.c unwind/ForcedUnwind.c		\
	unwind/GetBSP.c unwind/GetCFA.c unwind/GetDataRelBase.c		\
	unwind/GetGR.c unwind/GetIP.c unwind/GetLanguageSpecificData.c	\
	unwind/GetRegionStart.c unwind/GetTextRelBase.c			\
	unwind/RaiseException.c unwind/Resume.c				\
	unwind/Resume_or_Rethrow.c unwind/SetGR.c unwind/SetIP.c	\
	unwind/GetIPInfo.c

#  _ReadULEB()/_ReadSLEB() are needed for Intel C++ 8.0 compatibility
libunwindandroidandroid_la_SOURCES_os_linux_local = mi/_ReadULEB.c mi/_ReadSLEB.c
endif

# List of arch-independent files needed by local-only library (libunwindandroid):
libunwindandroidandroid_la_SOURCES_local_nounwind =					\
	$(libunwindandroid_la_SOURCES_os_local)				\
	mi/backtrace.c							\
	mi/dyn-cancel.c mi/dyn-info-list.c mi/dyn-register.c		\
	mi/Ldyn-extract.c mi/Lfind_dynamic_proc_info.c			\
	mi/Lget_accessors.c						\
	mi/Lget_proc_info_by_ip.c mi/Lget_proc_name.c			\
	mi/Lput_dynamic_unwind_info.c mi/Ldestroy_addr_space.c		\
	mi/Lget_reg.c   mi/Lset_reg.c					\
	mi/Lget_fpreg.c mi/Lset_fpreg.c					\
	mi/Lset_caching_policy.c

libunwindandroid_la_SOURCES_local =						\
	$(libunwindandroid_la_SOURCES_local_nounwind)				\
	$(libunwindandroid_la_SOURCES_local_unwind)

noinst_HEADERS += os-linux.h
libunwindandroid_la_SOURCES_os_linux = os-linux.c

libunwindandroid_la_SOURCES_os_hpux = os-hpux.c

libunwindandroid_la_SOURCES_os_freebsd = os-freebsd.c

libunwindandroid_la_SOURCES_os_qnx = os-qnx.c

libunwindandroid_dwarf_common_la_SOURCES = dwarf/global.c

libunwindandroid_dwarf_local_la_SOURCES = \
	dwarf/Lexpr.c dwarf/Lfde.c dwarf/Lparser.c dwarf/Lpe.c dwarf/Lstep.c \
	dwarf/Lfind_proc_info-lsb.c \
	dwarf/Lfind_unwind_table.c
libunwindandroid_dwarf_local_la_LIBADD = libunwindandroid-dwarf-common.la

libunwindandroid_dwarf_generic_la_SOURCES = \
	dwarf/Gexpr.c dwarf/Gfde.c dwarf/Gparser.c dwarf/Gpe.c dwarf/Gstep.c \
	dwarf/Gfind_proc_info-lsb.c \
	dwarf/Gfind_unwind_table.c
libunwindandroid_dwarf_generic_la_LIBADD = libunwindandroid-dwarf-common.la

if USE_DWARF
 noinst_LTLIBRARIES += libunwindandroid-dwarf-common.la libunwindandroid-dwarf-generic.la
if !REMOTE_ONLY
 noinst_LTLIBRARIES += libunwindandroid-dwarf-local.la
endif
 libunwindandroid_la_LIBADD += libunwindandroid-dwarf-local.la
endif

noinst_HEADERS += elf32.h elf64.h elfxx.h

libunwindandroid_elf32_la_SOURCES = elf32.c
libunwindandroid_elf64_la_SOURCES = elf64.c
libunwindandroid_elfxx_la_SOURCES = elfxx.c

noinst_LTLIBRARIES += $(LIBUNWIND_ELF)
libunwindandroid_la_LIBADD += $(LIBUNWIND_ELF)

# The list of files that go into libunwind and libunwind-aarch64:
noinst_HEADERS += aarch64/init.h aarch64/offsets.h aarch64/unwind_i.h
libunwindandroid_la_SOURCES_aarch64_common = $(libunwindandroid_la_SOURCES_common)	    \
	aarch64/is_fpreg.c aarch64/regname.c

# The list of files that go into libunwind:
libunwindandroid_la_SOURCES_aarch64 = $(libunwindandroid_la_SOURCES_aarch64_common)	    \
	$(libunwindandroid_la_SOURCES_local)					    \
	aarch64/Lcreate_addr_space.c aarch64/Lget_proc_info.c 		    \
	aarch64/Lget_save_loc.c aarch64/Lglobal.c aarch64/Linit.c	    \
	aarch64/Linit_local.c aarch64/Linit_remote.c 			    \
	aarch64/Lis_signal_frame.c aarch64/Lregs.c aarch64/Lresume.c 	    \
	aarch64/Lstep.c

libunwindandroid_aarch64_la_SOURCES_aarch64 = $(libunwindandroid_la_SOURCES_aarch64_common) \
	$(libunwindandroid_la_SOURCES_generic)					      \
	aarch64/Gcreate_addr_space.c aarch64/Gget_proc_info.c 		      \
	aarch64/Gget_save_loc.c aarch64/Gglobal.c aarch64/Ginit.c 	      \
	aarch64/Ginit_local.c aarch64/Ginit_remote.c			      \
	aarch64/Gis_signal_frame.c aarch64/Gregs.c aarch64/Gresume.c	      \
	aarch64/Gstep.c

# The list of files that go into libunwind and libunwind-arm:
noinst_HEADERS += arm/init.h arm/offsets.h arm/unwind_i.h
libunwindandroid_la_SOURCES_arm_common = $(libunwindandroid_la_SOURCES_common)	    \
	arm/is_fpreg.c arm/regname.c

# The list of files that go into libunwind:
libunwindandroid_la_SOURCES_arm = $(libunwindandroid_la_SOURCES_arm_common)		    \
	$(libunwindandroid_la_SOURCES_local)					    \
	arm/getcontext.S						    \
	arm/Lcreate_addr_space.c arm/Lget_proc_info.c arm/Lget_save_loc.c   \
	arm/Lglobal.c arm/Linit.c arm/Linit_local.c arm/Linit_remote.c	    \
	arm/Lis_signal_frame.c arm/Lregs.c arm/Lresume.c arm/Lstep.c	    \
	arm/Lex_tables.c

libunwindandroid_arm_la_SOURCES_arm = $(libunwindandroid_la_SOURCES_arm_common)	    \
	$(libunwindandroid_la_SOURCES_generic)					    \
	arm/Gcreate_addr_space.c arm/Gget_proc_info.c arm/Gget_save_loc.c   \
	arm/Gglobal.c arm/Ginit.c arm/Ginit_local.c arm/Ginit_remote.c	    \
	arm/Gis_signal_frame.c arm/Gregs.c arm/Gresume.c arm/Gstep.c	    \
	arm/Gex_tables.c

# The list of files that go both into libunwind and libunwind-ia64:
noinst_HEADERS += ia64/init.h ia64/offsets.h ia64/regs.h		    \
	ia64/ucontext_i.h ia64/unwind_decoder.h ia64/unwind_i.h
libunwindandroid_la_SOURCES_ia64_common = $(libunwindandroid_la_SOURCES_common)	    \
	ia64/regname.c

# The list of files that go into libunwind:
libunwindandroid_la_SOURCES_ia64 = $(libunwindandroid_la_SOURCES_ia64_common)		     \
	$(libunwindandroid_la_SOURCES_local)					     \
									     \
	ia64/dyn_info_list.S ia64/getcontext.S				     \
									     \
	ia64/Lcreate_addr_space.c ia64/Lget_proc_info.c ia64/Lget_save_loc.c \
	ia64/Lglobal.c ia64/Linit.c ia64/Linit_local.c ia64/Linit_remote.c   \
	ia64/Linstall_cursor.S ia64/Lis_signal_frame.c ia64/Lparser.c	     \
	ia64/Lrbs.c ia64/Lregs.c ia64/Lresume.c ia64/Lscript.c ia64/Lstep.c  \
	ia64/Ltables.c ia64/Lfind_unwind_table.c

# The list of files that go into libunwind-ia64:
libunwindandroid_ia64_la_SOURCES_ia64 = $(libunwindandroid_la_SOURCES_ia64_common)	     \
	$(libunwindandroid_la_SOURCES_generic)					     \
	ia64/Gcreate_addr_space.c ia64/Gget_proc_info.c ia64/Gget_save_loc.c \
	ia64/Gglobal.c ia64/Ginit.c ia64/Ginit_local.c ia64/Ginit_remote.c   \
	ia64/Ginstall_cursor.S ia64/Gis_signal_frame.c ia64/Gparser.c	     \
	ia64/Grbs.c ia64/Gregs.c ia64/Gresume.c ia64/Gscript.c ia64/Gstep.c  \
	ia64/Gtables.c ia64/Gfind_unwind_table.c

# The list of files that go both into libunwind and libunwind-hppa:
noinst_HEADERS += hppa/init.h hppa/offsets.h hppa/unwind_i.h
libunwindandroid_la_SOURCES_hppa_common = $(libunwindandroid_la_SOURCES_common)	\
	hppa/regname.c

# The list of files that go into libunwind:
libunwindandroid_la_SOURCES_hppa = $(libunwindandroid_la_SOURCES_hppa_common)		\
	$(libunwindandroid_la_SOURCES_local)					\
	hppa/getcontext.S hppa/setcontext.S				\
	hppa/Lcreate_addr_space.c hppa/Lget_save_loc.c hppa/Lglobal.c	\
	hppa/Linit.c hppa/Linit_local.c hppa/Linit_remote.c		\
	hppa/Lis_signal_frame.c hppa/Lget_proc_info.c hppa/Lregs.c	\
	hppa/Lresume.c hppa/Lstep.c

# The list of files that go into libunwind-hppa:
libunwindandroid_hppa_la_SOURCES_hppa = $(libunwindandroid_la_SOURCES_hppa_common)	\
	$(libunwindandroid_la_SOURCES_generic)					\
	hppa/Gcreate_addr_space.c hppa/Gget_save_loc.c hppa/Gglobal.c	\
	hppa/Ginit.c hppa/Ginit_local.c hppa/Ginit_remote.c		\
	hppa/Gis_signal_frame.c hppa/Gget_proc_info.c hppa/Gregs.c	\
	hppa/Gresume.c hppa/Gstep.c

# The list of files that go info libunwind and libunwindandroid-mips:
noinst_HEADERS += mips/init.h mips/offsets.h
libunwindandroid_la_SOURCES_mips_common = $(libunwindandroid_la_SOURCES_common)	    \
	mips/is_fpreg.c mips/regname.c

# The list of files that go into libunwind:
libunwindandroid_la_SOURCES_mips = $(libunwindandroid_la_SOURCES_mips_common)		    \
	$(libunwindandroid_la_SOURCES_local)					    \
	mips/getcontext.S						    \
	mips/Lcreate_addr_space.c mips/Lget_proc_info.c mips/Lget_save_loc.c   \
	mips/Lglobal.c mips/Linit.c mips/Linit_local.c mips/Linit_remote.c  \
	mips/Lis_signal_frame.c mips/Lregs.c mips/Lresume.c mips/Lstep.c

libunwind_mips_la_SOURCES_mips = $(libunwindandroid_la_SOURCES_mips_common)	    \
	$(libunwindandroid_la_SOURCES_generic)					    \
	mips/Gcreate_addr_space.c mips/Gget_proc_info.c mips/Gget_save_loc.c   \
	mips/Gglobal.c mips/Ginit.c mips/Ginit_local.c mips/Ginit_remote.c	    \
	mips/Gis_signal_frame.c mips/Gregs.c mips/Gresume.c mips/Gstep.c

# The list of files that go both into libunwindandroid and libunwindandroid-x86:
noinst_HEADERS += x86/init.h x86/offsets.h x86/unwind_i.h
libunwindandroid_la_SOURCES_x86_common = $(libunwindandroid_la_SOURCES_common)	\
	x86/is_fpreg.c x86/regname.c

# The list of files that go into libunwindandroid:
libunwindandroid_la_SOURCES_x86 = $(libunwindandroid_la_SOURCES_x86_common)		\
        $(libunwindandroid_la_SOURCES_x86_os_local)				\
	$(libunwindandroid_la_SOURCES_local)					\
	x86/Lcreate_addr_space.c x86/Lget_save_loc.c x86/Lglobal.c	\
	x86/Linit.c x86/Linit_local.c x86/Linit_remote.c		\
	x86/Lget_proc_info.c x86/Lregs.c				\
	x86/Lresume.c x86/Lstep.c

# The list of files that go into libunwindandroid-x86:
libunwindandroid_x86_la_SOURCES_x86 = $(libunwindandroid_la_SOURCES_x86_common)	\
        $(libunwindandroid_la_SOURCES_x86_os)					\
	$(libunwindandroid_la_SOURCES_generic)					\
	x86/Gcreate_addr_space.c x86/Gget_save_loc.c x86/Gglobal.c	\
	x86/Ginit.c x86/Ginit_local.c x86/Ginit_remote.c		\
	x86/Gget_proc_info.c x86/Gregs.c				\
	x86/Gresume.c x86/Gstep.c

# The list of files that go both into libunwindandroid and libunwindandroid-x86_64:
noinst_HEADERS += x86_64/offsets.h					\
	x86_64/init.h x86_64/unwind_i.h x86_64/ucontext_i.h
libunwindandroid_la_SOURCES_x86_64_common = $(libunwindandroid_la_SOURCES_common)	\
	x86_64/is_fpreg.c x86_64/regname.c

# The list of files that go into libunwindandroid:
libunwindandroid_la_SOURCES_x86_64 = $(libunwindandroid_la_SOURCES_x86_64_common)	    \
        $(libunwindandroid_la_SOURCES_x86_64_os_local)			    	    \
	$(libunwindandroid_la_SOURCES_local)					    \
	x86_64/setcontext.S						    \
	x86_64/Lcreate_addr_space.c x86_64/Lget_save_loc.c x86_64/Lglobal.c \
	x86_64/Linit.c x86_64/Linit_local.c x86_64/Linit_remote.c	    \
	x86_64/Lget_proc_info.c x86_64/Lregs.c x86_64/Lresume.c		    \
	x86_64/Lstash_frame.c x86_64/Lstep.c x86_64/Ltrace.c x86_64/getcontext.S

# The list of files that go into libunwindandroid-x86_64:
libunwindandroid_x86_64_la_SOURCES_x86_64 = $(libunwindandroid_la_SOURCES_x86_64_common)  \
        $(libunwindandroid_la_SOURCES_x86_64_os)			    	    \
	$(libunwindandroid_la_SOURCES_generic)					    \
	x86_64/Gcreate_addr_space.c x86_64/Gget_save_loc.c x86_64/Gglobal.c \
	x86_64/Ginit.c x86_64/Ginit_local.c x86_64/Ginit_remote.c	    \
	x86_64/Gget_proc_info.c x86_64/Gregs.c x86_64/Gresume.c		    \
	x86_64/Gstash_frame.c x86_64/Gstep.c x86_64/Gtrace.c

# The list of local files that go to Power 64 and 32:
libunwindandroid_la_SOURCES_ppc = ppc/Lcreate_addr_space.c 			\
	ppc/Lget_proc_info.c ppc/Lget_save_loc.c ppc/Linit_local.c	\
	ppc/Linit_remote.c ppc/Lis_signal_frame.c

# The list of generic files that go to Power 64 and 32:
libunwindandroid_ppc_la_SOURCES_ppc_generic = ppc/Gcreate_addr_space.c		\
	ppc/Gget_proc_info.c ppc/Gget_save_loc.c ppc/Ginit_local.c	\
	ppc/Ginit_remote.c ppc/Gis_signal_frame.c

# The list of files that go both into libunwindandroid and libunwindandroid-ppc32:
noinst_HEADERS += ppc32/init.h ppc32/unwind_i.h ppc32/ucontext_i.h
libunwindandroid_la_SOURCES_ppc32_common = $(libunwindandroid_la_SOURCES_common)      \
	ppc32/is_fpreg.c ppc32/regname.c ppc32/get_func_addr.c

# The list of files that go into libunwindandroid:
libunwindandroid_la_SOURCES_ppc32 = $(libunwindandroid_la_SOURCES_ppc32_common)       \
	$(libunwindandroid_la_SOURCES_local)                                   \
	$(libunwindandroid_la_SOURCES_ppc)					\
	ppc32/Lglobal.c ppc32/Linit.c					\
	ppc32/Lregs.c ppc32/Lresume.c ppc32/Lstep.c

# The list of files that go into libunwindandroid-ppc32:
libunwindandroid_ppc32_la_SOURCES_ppc32 = $(libunwindandroid_la_SOURCES_ppc32_common) \
	$(libunwindandroid_la_SOURCES_generic)                                 \
	$(libunwindandroid_ppc_la_SOURCES_ppc_generic)				\
	ppc32/Gglobal.c ppc32/Ginit.c					\
	ppc32/Gregs.c ppc32/Gresume.c ppc32/Gstep.c

# The list of files that go both into libunwindandroid and libunwindandroid-ppc64:
noinst_HEADERS += ppc64/init.h ppc64/unwind_i.h ppc64/ucontext_i.h
libunwindandroid_la_SOURCES_ppc64_common = $(libunwindandroid_la_SOURCES_common)      \
        ppc64/is_fpreg.c ppc64/regname.c ppc64/get_func_addr.c

# The list of files that go into libunwindandroid:
libunwindandroid_la_SOURCES_ppc64 = $(libunwindandroid_la_SOURCES_ppc64_common)       \
        $(libunwindandroid_la_SOURCES_local)                                   \
	$(libunwindandroid_la_SOURCES_ppc)					\
	ppc64/Lglobal.c ppc64/Linit.c					\
	ppc64/Lregs.c ppc64/Lresume.c ppc64/Lstep.c

# The list of files that go into libunwindandroid-ppc64:
libunwindandroid_ppc64_la_SOURCES_ppc64 = $(libunwindandroid_la_SOURCES_ppc64_common) \
        $(libunwindandroid_la_SOURCES_generic)                                 \
	$(libunwindandroid_ppc_la_SOURCES_ppc_generic)				\
	ppc64/Gglobal.c ppc64/Ginit.c					\
	ppc64/Gregs.c ppc64/Gresume.c ppc64/Gstep.c

# The list of files that go into libunwindandroid and libunwindandroid-sh:
noinst_HEADERS += sh/init.h sh/offsets.h sh/unwind_i.h
libunwindandroid_la_SOURCES_sh_common = $(libunwindandroid_la_SOURCES_common)		\
	sh/is_fpreg.c sh/regname.c

# The list of files that go into libunwindandroid:
libunwindandroid_la_SOURCES_sh = $(libunwindandroid_la_SOURCES_sh_common)		\
	$(libunwindandroid_la_SOURCES_local)					\
	sh/Lcreate_addr_space.c sh/Lget_proc_info.c sh/Lget_save_loc.c	\
	sh/Lglobal.c sh/Linit.c sh/Linit_local.c sh/Linit_remote.c	\
	sh/Lis_signal_frame.c sh/Lregs.c sh/Lresume.c sh/Lstep.c

libunwindandroid_sh_la_SOURCES_sh = $(libunwindandroid_la_SOURCES_sh_common)		\
	$(libunwindandroid_la_SOURCES_generic)					\
	sh/Gcreate_addr_space.c sh/Gget_proc_info.c sh/Gget_save_loc.c	\
	sh/Gglobal.c sh/Ginit.c sh/Ginit_local.c sh/Ginit_remote.c	\
	sh/Gis_signal_frame.c sh/Gregs.c sh/Gresume.c sh/Gstep.c

if REMOTE_ONLY
install-exec-hook:
#	Nothing to do here....
else
#
# This is not ideal, but I know of no other way to install an
# alias for a library.  For the shared version, we have to do
# a file check before creating the link, because it isn't going
# to be there if the user configured with --disable-shared.
#
install-exec-hook:
	cd $(DESTDIR)$(libdir) && $(LN_S) -f libunwindandroid-$(arch).a libunwindandroid-generic.a
	if test -f $(DESTDIR)$(libdir)/libunwindandroid-$(arch).so; then \
		cd $(DESTDIR)$(libdir) && $(LN_S) -f libunwindandroid-$(arch).so \
		libunwindandroid-generic.so; \
	fi
endif

if OS_LINUX
 libunwindandroid_la_SOURCES_os	      = $(libunwindandroid_la_SOURCES_os_linux)
 libunwindandroid_la_SOURCES_os_local	      = $(libunwindandroid_la_SOURCES_os_linux_local)
 libunwindandroid_la_SOURCES_x86_os          = x86/Gos-linux.c
 libunwindandroid_x86_la_SOURCES_os	      = x86/getcontext-linux.S
 libunwindandroid_la_SOURCES_x86_os_local    = x86/Los-linux.c
 libunwindandroid_la_SOURCES_x86_64_os       = x86_64/Gos-linux.c
 libunwindandroid_la_SOURCES_x86_64_os_local = x86_64/Los-linux.c
 libunwindandroid_coredump_la_SOURCES += coredump/_UCD_access_reg_linux.c
endif

if OS_HPUX
 libunwindandroid_la_SOURCES_os	= $(libunwindandroid_la_SOURCES_os_hpux)
 libunwindandroid_la_SOURCES_os_local	= $(libunwindandroid_la_SOURCES_os_hpux_local)
endif

if OS_FREEBSD
 libunwindandroid_la_SOURCES_os	= $(libunwindandroid_la_SOURCES_os_freebsd)
 libunwindandroid_la_SOURCES_os_local	= $(libunwindandroid_la_SOURCES_os_freebsd_local)
 libunwindandroid_la_SOURCES_x86_os          = x86/Gos-freebsd.c
 libunwindandroid_x86_la_SOURCES_os	      = x86/getcontext-freebsd.S
 libunwindandroid_la_SOURCES_x86_os_local    = x86/Los-freebsd.c
 libunwindandroid_la_SOURCES_x86_64_os       = x86_64/Gos-freebsd.c
 libunwindandroid_la_SOURCES_x86_64_os_local = x86_64/Los-freebsd.c
 libunwindandroid_coredump_la_SOURCES += coredump/_UCD_access_reg_freebsd.c
endif

if OS_QNX
 libunwindandroid_la_SOURCES_os	= $(libunwindandroid_la_SOURCES_os_qnx)
 libunwindandroid_la_SOURCES_os_local	= $(libunwindandroid_la_SOURCES_os_qnx_local)
endif

if ARCH_AARCH64
 lib_LTLIBRARIES += libunwindandroid-aarch64.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_aarch64)
 libunwindandroid_aarch64_la_SOURCES = $(libunwindandroid_aarch64_la_SOURCES_aarch64)
 libunwindandroid_aarch64_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_aarch64_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_aarch64_la_LIBADD += libunwindandroid-elf64.la
if !REMOTE_ONLY
 libunwindandroid_aarch64_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += aarch64/siglongjmp.S
else
if ARCH_ARM
 lib_LTLIBRARIES += libunwindandroid-arm.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_arm)
 libunwindandroid_arm_la_SOURCES = $(libunwindandroid_arm_la_SOURCES_arm)
 libunwindandroid_arm_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_arm_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_arm_la_LIBADD += libunwindandroid-elf32.la
if !REMOTE_ONLY
 libunwindandroid_arm_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += arm/siglongjmp.S
else
if ARCH_IA64
 BUILT_SOURCES = Gcursor_i.h Lcursor_i.h
mk_Gcursor_i.s: $(srcdir)/ia64/mk_Gcursor_i.c
	$(COMPILE) -S "$(srcdir)/ia64/mk_Gcursor_i.c" -o mk_Gcursor_i.s
mk_Lcursor_i.s: $(srcdir)/ia64/mk_Lcursor_i.c
	$(COMPILE) -S "$(srcdir)/ia64/mk_Lcursor_i.c" -o mk_Lcursor_i.s
Gcursor_i.h: mk_Gcursor_i.s
	"$(srcdir)/ia64/mk_cursor_i" mk_Gcursor_i.s > Gcursor_i.h
Lcursor_i.h: mk_Lcursor_i.s
	"$(srcdir)/ia64/mk_cursor_i" mk_Lcursor_i.s > Lcursor_i.h

 lib_LTLIBRARIES += libunwindandroid-ia64.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_ia64)
 libunwindandroid_ia64_la_SOURCES = $(libunwindandroid_ia64_la_SOURCES_ia64)
 libunwindandroid_ia64_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_ia64_la_LIBADD = libunwindandroid-elf64.la
if !REMOTE_ONLY
 libunwindandroid_ia64_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += ia64/setjmp.S  ia64/sigsetjmp.S  \
				ia64/longjmp.S ia64/siglongjmp.S
else
if ARCH_HPPA
 lib_LTLIBRARIES += libunwindandroid-hppa.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_hppa)
 libunwindandroid_hppa_la_SOURCES = $(libunwindandroid_hppa_la_SOURCES_hppa)
 libunwindandroid_hppa_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_hppa_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_hppa_la_LIBADD += libunwindandroid-elf32.la
if !REMOTE_ONLY
 libunwindandroid_hppa_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += hppa/siglongjmp.S
else
if ARCH_MIPS
 lib_LTLIBRARIES += libunwindandroid-mips.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_mips)
 libunwindandroid_mips_la_SOURCES = $(libunwindandroid_mips_la_SOURCES_mips)
 libunwindandroid_mips_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_mips_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_mips_la_LIBADD += libunwindandroid-elfxx.la
if !REMOTE_ONLY
 libunwindandroid_mips_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += mips/siglongjmp.S
else
if ARCH_X86
 lib_LTLIBRARIES += libunwindandroid-x86.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_x86) $(libunwindandroid_x86_la_SOURCES_os)
 libunwindandroid_x86_la_SOURCES = $(libunwindandroid_x86_la_SOURCES_x86)
 libunwindandroid_x86_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_x86_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_x86_la_LIBADD += libunwindandroid-elf32.la
if !REMOTE_ONLY
 libunwindandroid_x86_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += x86/longjmp.S x86/siglongjmp.S
else
if ARCH_X86_64
 lib_LTLIBRARIES += libunwindandroid-x86_64.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_x86_64)
 libunwindandroid_x86_64_la_SOURCES = $(libunwindandroid_x86_64_la_SOURCES_x86_64)
 libunwindandroid_x86_64_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_x86_64_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_x86_64_la_LIBADD += libunwindandroid-elf64.la
if !REMOTE_ONLY
 libunwindandroid_x86_64_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += x86_64/longjmp.S x86_64/siglongjmp.S
else
if ARCH_PPC32
 lib_LTLIBRARIES += libunwindandroid-ppc32.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_ppc32)
 libunwindandroid_ppc32_la_SOURCES = $(libunwindandroid_ppc32_la_SOURCES_ppc32)
 libunwindandroid_ppc32_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_ppc32_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_ppc32_la_LIBADD += libunwindandroid-elf32.la
if !REMOTE_ONLY
 libunwindandroid_ppc32_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += ppc/longjmp.S ppc/siglongjmp.S
else
if ARCH_PPC64
 lib_LTLIBRARIES += libunwindandroid-ppc64.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_ppc64)
 libunwindandroid_ppc64_la_SOURCES = $(libunwindandroid_ppc64_la_SOURCES_ppc64)
 libunwindandroid_ppc64_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_ppc64_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_ppc64_la_LIBADD += libunwindandroid-elf64.la
if !REMOTE_ONLY
 libunwindandroid_ppc64_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += ppc/longjmp.S ppc/siglongjmp.S
else
if ARCH_SH
 lib_LTLIBRARIES += libunwindandroid-sh.la
 libunwindandroid_la_SOURCES = $(libunwindandroid_la_SOURCES_sh)
 libunwindandroid_sh_la_SOURCES = $(libunwindandroid_sh_la_SOURCES_sh)
 libunwindandroid_sh_la_LDFLAGS = $(COMMON_SO_LDFLAGS) -version-info $(SOVERSION)
 libunwindandroid_sh_la_LIBADD = libunwindandroid-dwarf-generic.la
 libunwindandroid_sh_la_LIBADD += libunwindandroid-elf32.la
if !REMOTE_ONLY
 libunwindandroid_sh_la_LIBADD += libunwindandroid.la -lc
endif
 libunwindandroid_setjmp_la_SOURCES += sh/siglongjmp.S

endif # ARCH_SH
endif # ARCH_PPC64
endif # ARCH_PPC32
endif # ARCH_X86_64
endif # ARCH_X86
endif # ARCH_MIPS
endif # ARCH_HPPA
endif # ARCH_IA64
endif # ARCH_ARM
endif # ARCH_AARCH64

# libunwindandroid-setjmp depends on libunwindandroid-$(arch). Therefore must be added
# at the end.
if BUILD_SETJMP
lib_LTLIBRARIES += libunwindandroid-setjmp.la
endif

#
# Don't link with standard libraries, because those may mention
# libunwindandroid already.
#
libunwindandroid_la_LDFLAGS =	$(COMMON_SO_LDFLAGS) -XCClinker -nostdlib \
			$(LDFLAGS_STATIC_LIBCXA) -version-info $(SOVERSION)
libunwindandroid_la_LIBADD  += -lc $(LIBCRTS)
libunwindandroid_la_LIBADD += $(LIBLZMA)

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/tdep-$(arch) -I.
AM_CCASFLAGS = $(AM_CPPFLAGS)
noinst_HEADERS += unwind/unwind-internal.h

EXTRA_DIST =	$(libunwindandroid_la_SOURCES_aarch64)			\
		$(libunwindandroid_la_SOURCES_arm)			\
		$(libunwindandroid_la_SOURCES_hppa)			\
		$(libunwindandroid_la_SOURCES_ia64)			\
		$(libunwindandroid_la_SOURCES_mips)			\
		$(libunwindandroid_la_SOURCES_sh)			\
		$(libunwindandroid_la_SOURCES_x86)			\
		$(libunwindandroid_la_SOURCES_os_freebsd)		\
		$(libunwindandroid_la_SOURCES_os_linux)		\
		$(libunwindandroid_la_SOURCES_os_hpux)			\
		$(libunwindandroid_la_SOURCES_os_qnx)			\
		$(libunwindandroid_la_SOURCES_common)			\
		$(libunwindandroid_la_SOURCES_local)			\
		$(libunwindandroid_la_SOURCES_generic)			\
		$(libunwindandroid_aarch64_la_SOURCES_aarch64)		\
		$(libunwindandroid_arm_la_SOURCES_arm)			\
		$(libunwindandroid_hppa_la_SOURCES_hppa)		\
		$(libunwindandroid_ia64_la_SOURCES_ia64)		\
		$(libunwindandroid_mips_la_SOURCES_mips)		\
		$(libunwindandroid_sh_la_SOURCES_sh)			\
		$(libunwindandroid_x86_la_SOURCES_x86)			\
		$(libunwindandroid_x86_64_la_SOURCES_x86_64)

MAINTAINERCLEANFILES = Makefile.in

# The -version-info flag accepts an argument of the form
# `current[:revision[:age]]'. So, passing `-version-info 3:12:1' sets
# current to 3, revision to 12, and age to 1.

# If either revision or age are omitted, they default to 0. Also note
# that age must be less than or equal to the current interface number.

# Here are a set of rules to help you update your library version
# information:

#    1. Start with version information of `0:0:0' for each libtool
#       library.

#    2. Update the version information only immediately before a public
#       release of your software. More frequent updates are unnecessary,
#       and only guarantee that the current interface number gets larger
#       faster.

#    3. If the library source code has changed at all since the last
#       update, then increment revision (`c:r:a' becomes `c:r+1:a').

#    4. If any interfaces have been added, removed, or changed since the
#       last update, increment current, and set revision to 0.

#    5. If any interfaces have been added since the last public release,
#       then increment age.

#    6. If any interfaces have been removed since the last public
#       release, then set age to 0.
