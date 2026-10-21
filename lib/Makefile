lib-y += compiler-rt.o

# simplefb
lib-$(CONFIG_SIMPLE_FB) += simplefb/simplefb.o

# compressed kernel
lib-$(CONFIG_KERNEL_GZIP) += gunzip/gunzip.o
# hot loop over the whole kernel: optimize it, but keep every access aligned
# since memory is Device-type when this runs with the MMU off, and keep the
# vectorizer away from FP/SIMD registers the previous stage may not have enabled
CFLAGS_gunzip.o := -O2
ifeq ($(ARCH),aarch64)
CFLAGS_gunzip.o += -mstrict-align -mgeneral-regs-only
else
CFLAGS_gunzip.o += -mno-unaligned-access
endif

# debug
lib-y += console/console.o

# unic (neatlibc still temporarily resides)
lib-y += unic/string.o

lib-$(CONFIG_LIBFDT) += \
	libfdt/fdt.o \
	libfdt/fdt_ro.o \
	libfdt/fdt_rw.o \
	libfdt/fdt_wip.o \
	libfdt/fdt_sw.o \
	libfdt/fdt_strerror.o

ccflags-y += -I$(srctree)/include/lib/libfdt
