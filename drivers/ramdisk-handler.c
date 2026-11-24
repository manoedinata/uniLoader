// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Victor Paul <vipollmail@gmail.com>
 */

#include <drivers/ramdisk-handler.h>
#include <lib/debug.h>
#include <lib/libfdt/libfdt.h>

#ifdef CONFIG_RAMDISK_FROM_BOOTLOADER
static unsigned long fdt_getprop_addr(const void *fdt, int node, const char *name)
{
	int len;
	const fdt32_t *p = fdt_getprop(fdt, node, name, &len);

	if (!p)
		return 0;
	if (len == 8)
		return ((unsigned long)fdt32_to_cpu(p[0]) << 32) | fdt32_to_cpu(p[1]);
	return fdt32_to_cpu(p[0]);
}

/* The initrd range from /chosen of the previous bootloader's FDT */
static int bootloader_initrd(unsigned long *start, unsigned long *end)
{
	const void *bl_fdt = (const void *)CONFIG_BOOTLOADER_FDT_ADDR;
	int chosen;

	/* the fixed address first: x0 may not be an FDT (or even RAM) */
	if (!bl_fdt || fdt_check_header(bl_fdt) != 0)
		bl_fdt = bootloader_fdt;
	if (!bl_fdt || fdt_check_header(bl_fdt) != 0) {
		printk(KERN_ERR, "no bootloader FDT (x0=%p), booting without initrd\n",
		       bootloader_fdt);
		return -1;
	}

	chosen = fdt_path_offset(bl_fdt, "/chosen");
	if (chosen < 0) {
		printk(KERN_ERR, "bootloader FDT at %p has no /chosen\n", bl_fdt);
		return -1;
	}

	*start = fdt_getprop_addr(bl_fdt, chosen, "linux,initrd-start");
	*end = fdt_getprop_addr(bl_fdt, chosen, "linux,initrd-end");
	printk(KERN_INFO, "bootloader FDT at %p: initrd %lx..%lx\n", bl_fdt, *start, *end);
	if (!*start || *end <= *start) {
		printk(KERN_ERR, "no usable initrd range, booting without initrd\n");
		return -1;
	}
	return 0;
}
#endif

int ramdisk_handler_patch_dtb(const void *fdt, void *buf, int bufsize)
{
	int ret, dtb_chosen_offset;
#ifdef CONFIG_RAMDISK_FROM_BOOTLOADER
	unsigned long ramdisk_start, ramdisk_end;
#elif defined(__aarch64__)
	unsigned long ramdisk_start = CONFIG_RAMDISK_ENTRY;
	void *ramdisk_end = (void*)CONFIG_RAMDISK_ENTRY + (unsigned long)&ramdisk_size;
#elif __arm__
	unsigned long ramdisk_start = CONFIG_RAMDISK_ENTRY;
	void *ramdisk_end = (void*)CONFIG_RAMDISK_ENTRY + ramdisk_size;
#endif

	printk(KERN_INFO, "trying to open fdt...\n");
	ret = fdt_open_into(fdt, buf, bufsize);
	if (ret < 0) {
		printk(KERN_ERR, "fdt_open_into() failed: %s\n", fdt_strerror(ret));
		return ret;
	}

	dtb_chosen_offset = fdt_path_offset(buf, "/chosen");
	if (dtb_chosen_offset < 0) {
		printk(KERN_ERR, "Could not find /chosen node: %s\n", fdt_strerror(dtb_chosen_offset));
		return dtb_chosen_offset;
	}

#ifdef CONFIG_RAMDISK_FROM_BOOTLOADER
	/* still hand on the opened copy: the kernel then runs without initrd */
	if (bootloader_initrd(&ramdisk_start, &ramdisk_end))
		return 0;
#endif

	printk(KERN_INFO, "adding linux,initrd-start...\n");
#ifdef __aarch64__
	ret = fdt_setprop_u64(buf, dtb_chosen_offset, "linux,initrd-start", (uint64_t)ramdisk_start);
#elif __arm__
	ret = fdt_setprop_u32(buf, dtb_chosen_offset, "linux,initrd-start", (uint32_t)ramdisk_start);
#endif
	if (ret < 0) {
		printk(KERN_ERR, "failed to add linux,initrd-start: %s\n", fdt_strerror(ret));
		return ret;
	}

	printk(KERN_INFO, "adding linux,initrd-end...\n");
#ifdef __aarch64__
	ret = fdt_setprop_u64(buf, dtb_chosen_offset, "linux,initrd-end", (uint64_t)ramdisk_end);
#elif __arm__
	ret = fdt_setprop_u32(buf, dtb_chosen_offset, "linux,initrd-end", (uint32_t)ramdisk_end);
#endif
	if (ret < 0) {
		printk(KERN_ERR, "failed to add linux,initrd-end: %s\n", fdt_strerror(ret));
		return ret;
	}

	return 0;
}
