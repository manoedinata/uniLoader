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
#ifdef CONFIG_BOOTLOADER_FDT_HOLD_SEC
#include <lib/delay.h>
#endif
#ifdef CONFIG_USE_BOOTLOADER_FDT
#include <lib/libfdt/libfdt.h>
#endif

extern struct board_data board_ops;

static void print_splash(void)
{
	printk(KERN_INFO, "             .__.____                     .___\n");
	printk(KERN_INFO, " __ __  ____ |__|    |    _________     __| _/___________\n");
	printk(KERN_INFO, "|  |  \\/    \\|  |    |   /  _ \\__  \\   / __ |/ __ \\_  __\\\n");
	printk(KERN_INFO, "|  |  /   |  \\  |    |__(  <_> ) __ \\_/ /_/ \\  ___/|  |\\/\n");
	printk(KERN_INFO, "|____/|___|  /__|_______ \\____(____  /\\____ |\\___  >__|\n");
	printk(KERN_INFO, "           \\/           \\/         \\/      \\/    \\/\n");

	printk(KERN_INFO, "passed board initialization\n");
	printk(KERN_INFO, "welcome to uniLoader %s on %s\n", VER_TAG, board_ops.name);
}

void main(void* dt, void* kernel, void* ramdisk, void* bl_dt)
{
	early_console_init();
	INITCALL(board_ops.ops.early_init);

	driver_probe_all(board_ops.devices, board_ops.num_devices);

	print_splash();

	INITCALL(board_ops.ops.late_init);

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
