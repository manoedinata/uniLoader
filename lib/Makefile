lib-y += compiler-rt.o

# simplefb
lib-$(CONFIG_SIMPLE_FB) += simplefb/simplefb.o

# compressed kernel
lib-$(CONFIG_KERNEL_GZIP) += gunzip/gunzip.o
# hot loop over the whole kernel: optimize it (strict alignment comes from the
# top-level Makefile), and keep the vectorizer away from FP/SIMD registers the
# previous stage may not have enabled
CFLAGS_gunzip.o := -O2
ifeq ($(ARCH),aarch64)
CFLAGS_gunzip.o += -mgeneral-regs-only
endif

# debug
lib-y += console/console.o
lib-$(CONFIG_RAMLOG) += ramlog/ramlog.o

# busy-wait delays on the ARMv8 generic timer
ifeq ($(ARCH),aarch64)
lib-y += delay/delay.o
endif

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
