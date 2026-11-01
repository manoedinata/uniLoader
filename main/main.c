// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */

#include <board.h>
#include <drivers/framework.h>
#include <lib/console.h>
#include <main/boot.h>
#include <main/main.h>
#include <lib/debug.h>
#include <lib/failsafe.h>
#include <lib/ramlog.h>
#ifdef CONFIG_BOOTLOADER_FDT_HOLD_SEC
#include <lib/delay.h>
#endif
#ifdef CONFIG_USE_BOOTLOADER_FDT
#include <lib/libfdt/libfdt.h>
#endif

extern struct board_data board_ops;

#ifdef CONFIG_USE_BOOTLOADER_FDT
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
#endif

static void print_splash(void)
{
#ifndef CONFIG_NO_SPLASH
	printk(KERN_INFO, "             .__.____                     .___\n");
	printk(KERN_INFO, " __ __  ____ |__|    |    _________     __| _/___________\n");
	printk(KERN_INFO, "|  |  \\/    \\|  |    |   /  _ \\__  \\   / __ |/ __ \\_  __\\\n");
	printk(KERN_INFO, "|  |  /   |  \\  |    |__(  <_> ) __ \\_/ /_/ \\  ___/|  |\\/\n");
	printk(KERN_INFO, "|____/|___|  /__|_______ \\____(____  /\\____ |\\___  >__|\n");
	printk(KERN_INFO, "           \\/           \\/         \\/      \\/    \\/\n");
#endif

	printk(KERN_INFO, "passed board initialization\n");
	printk(KERN_INFO, "welcome to uniLoader %s on %s\n", VER_TAG, board_ops.name);
}

void main(void* dt, void* kernel, void* ramdisk, void* bl_dt)
{
	early_console_init();
	ramlog_init();
	INITCALL(board_ops.ops.early_init);

	driver_probe_all(board_ops.devices, board_ops.num_devices);

	print_splash();

	INITCALL(board_ops.ops.late_init);

	failsafe_run(board_ops.failsafe);

	/* position independent: where the previous bootloader put this image */
	printk(KERN_INFO, "running at %p, bootloader x0=%p\n", (void *)main, bl_dt);

#ifdef CONFIG_USE_BOOTLOADER_FDT
	/* The payload wants what the bootloader built (dtbo applied, memory
	 * map, reserved regions, cmdline), not the embedded dtb. */
	if (bl_dt && fdt_check_header(bl_dt) == 0) {
		int chosen = fdt_path_offset(bl_dt, "/chosen");
		const char *args = chosen >= 0 ?
			fdt_getprop(bl_dt, chosen, "bootargs", NULL) : NULL;

		printk(KERN_INFO, "bootloader FDT at %p, %u bytes, model '%s'\n",
		       bl_dt, fdt_totalsize(bl_dt),
		       (const char *)fdt_getprop(bl_dt, 0, "model", NULL));
		printk(KERN_INFO, "bootloader bootargs: %s\n", args ? args : "(none)");
		if (chosen >= 0)
			printk(KERN_INFO, "bootloader initrd: %lx..%lx\n",
			       fdt_getprop_addr(bl_dt, chosen, "linux,initrd-start"),
			       fdt_getprop_addr(bl_dt, chosen, "linux,initrd-end"));
		dt = bl_dt;
#if CONFIG_BOOTLOADER_FDT_HOLD_SEC
		mdelay(CONFIG_BOOTLOADER_FDT_HOLD_SEC * 1000);
#endif
	} else {
		printk(KERN_ERR, "no valid bootloader FDT (x0=%p), using the embedded dtb\n",
		       bl_dt);
	}
#endif

	INITCALL(board_ops.ops.pre_boot);

	boot_kernel(dt, kernel, ramdisk);

	// todo: reset the board?
	printk(KERN_EMERG, "Something wrong happened, we shouldn't be here. Hanging....\n");
	HANG();
}
