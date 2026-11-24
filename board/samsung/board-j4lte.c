/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023, predefine <predefine@yandex.ru>
 * Copyright (c) 2024, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */

#include <board.h>
#include <util.h>
#include <drivers/framework.h>
#include <lib/simplefb.h>

#define DECON_F_BASE		0x14830000
#define HW_SW_TRIG_CONTROL	0x70

/* PMU INFORM3: S-BOOT boot target, not cleared by S-BOOT itself */
#define PMU_INFORM3		0x11c8080c
#define SBOOT_TARGET_NORMAL	0x12345670

// Early initialization
int j4lte_init(void)
{
	/* Allow framebuffer to be written to */
	*(int*) (DECON_F_BASE + HW_SW_TRIG_CONTROL) = 0x1281;

	/*
	 * After "reboot recovery" INFORM3 still says RECOVERY: if this boot
	 * hangs, a forced reset would start the same image again. Point it
	 * back at BOOT now that we run.
	 */
	*(volatile unsigned int *)PMU_INFORM3 = SBOOT_TARGET_NORMAL;
	return 0;
}

static struct video_info j4lte_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = 720,
	.height = 1280,
	.stride = 4,
	.address = (void *)0x067000000
};

static const struct device j4lte_devices[] = {
	{ "simplefb", &j4lte_fb, "fb" },
};

struct board_data board_ops = {
	.name = "samsung-j4lte",
	.ops = {
		.early_init = j4lte_init,
	},
	.devices = j4lte_devices,
	.num_devices = ARRAY_SIZE(j4lte_devices),
	.quirks = 0
};
