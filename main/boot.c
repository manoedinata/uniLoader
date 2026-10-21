// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <lib/debug.h>
#include <main/boot.h>
#include <main/boot-fdt.h>
#include <string.h>

#ifdef CONFIG_KERNEL_GZIP
#include <lib/gunzip.h>

extern char __image_start[], __image_end[];

int load_kernel_payload(const void* kernel, unsigned long size)
{
	char *dst = (char *)CONFIG_PAYLOAD_ENTRY;
	unsigned long room = -1UL - (unsigned long)dst;
	long ret;

	/* the output must stop short of uniLoader, which holds the input */
	if (dst < __image_start)
		room = __image_start - dst;
	else if (dst < __image_end)
		room = 0;

	printk(KERN_INFO, "Decompressing kernel...\n");
	ret = gunzip(dst, room, kernel, size);
	if (ret < 0) {
		printk(KERN_ERR, "kernel gunzip failed: %ld\n", ret);
		return -1;
	}
	printk(KERN_INFO, "kernel: %lu -> %ld bytes\n", size, ret);
	return 0;
}
#else
int load_kernel_payload(const void* kernel, unsigned long size)
{
	memcpy((void*)CONFIG_PAYLOAD_ENTRY, kernel, size);
	return 0;
}
#endif

void boot_kernel(void* dt, void* kernel, void* ramdisk)
{
#ifdef CONFIG_LIBFDT
	patch_dtb(&dt);
#endif

	printk(KERN_INFO, "Booting kernel...\n");
	arch_load_kernel(kernel, dt, ramdisk);
}
