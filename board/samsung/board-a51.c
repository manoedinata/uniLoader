// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Galaxy A51 (SM-A515F, Exynos 9611)
 *
 * The A51 this is written for has a dead power key, so a hang cannot be
 * cleared with the forced-reset combo. With CONFIG_FAILSAFE the loader arms
 * the CPUCL0 watchdog before handing over and escapes into the other slot
 * (BOOT/RECOVERY, through the S-BOOT INFORM3 target) after a watchdog reset
 * or a boot loop. RST_STAT bits per downstream sec_debug_reset_reason.c.
 */
#include <board.h>
#include <util.h>
#include <drivers/framework.h>
#include <drivers/samsung/exynos-wdt.h>
#include <drivers/samsung/sboot.h>
#include <lib/debug.h>
#include <lib/failsafe.h>
#include <lib/io.h>
#include <lib/simplefb.h>

#define DECON_F_BASE		0x148b0000
#define HW_SW_TRIG_CONTROL	0x70

#define PMU_BASE		0x11860000
#define PMU_SWRESET		0x0400
#define PMU_RST_STAT		0x0404
/* bit 23 CLUSTER1_WDTRESET, bit 24 CLUSTER0_WDTRESET */
#define RST_STAT_WDT		((1u << 23) | (1u << 24))
#define PMU_WDT_DISABLE		0x0408
#define PMU_WDT_MASK_RESET	0x040c
#define PMU_INFORM2		0x0808
#define PMU_INFORM3		0x080c

#ifdef CONFIG_FAILSAFE
static const struct exynos_wdt a51_wdt = {
	.base = 0x10050000,		/* WDT_CPUCL0 */
	.clk_hz = 26000000,		/* OSCCLK */
	.pmu_disable = PMU_BASE + PMU_WDT_DISABLE,
	.pmu_mask_reset = PMU_BASE + PMU_WDT_MASK_RESET,
	.pmu_bit = 1u << 23,
};

static int a51_watchdog_reset(void)
{
	unsigned int rst = readl(PMU_BASE + PMU_RST_STAT);

	printk(KERN_INFO, "a51: RST_STAT=%x INFORM2=%x INFORM3=%x\n", rst,
	       readl(PMU_BASE + PMU_INFORM2), readl(PMU_BASE + PMU_INFORM3));
	return !!(rst & RST_STAT_WDT);
}

static void a51_reset_to(unsigned int slot)
{
	sboot_reset(PMU_BASE + PMU_INFORM3, PMU_BASE + PMU_SWRESET,
		    slot == FAILSAFE_SLOT_PRIMARY ? SBOOT_NORMAL : SBOOT_RECOVERY);
}

static unsigned int a51_arm_watchdog(unsigned int sec)
{
	/* S-BOOT honours INFORM3 after anything but a watchdog reset: point it
	 * at the other slot already, for a panic that resets */
	writel(CONFIG_FAILSAFE_SLOT == FAILSAFE_SLOT_PRIMARY ? SBOOT_RECOVERY : SBOOT_NORMAL,
	       PMU_BASE + PMU_INFORM3);
	return exynos_wdt_arm(&a51_wdt, sec);
}

static const struct failsafe_ops a51_failsafe = {
	.watchdog_reset = a51_watchdog_reset,
	.reset_to = a51_reset_to,
	.arm_watchdog = a51_arm_watchdog,
};
#endif

static unsigned int decon_trig_orig;

static int a51_init(void)
{
	decon_trig_orig = readl(DECON_F_BASE + HW_SW_TRIG_CONTROL);
	/* Allow framebuffer to be written to */
	writel(0x1281, DECON_F_BASE + HW_SW_TRIG_CONTROL);
	return 0;
}

#ifdef CONFIG_USE_BOOTLOADER_FDT
/*
 * The downstream kernel enables the DPU SysMMU while DECON is still fetching
 * the bootloader framebuffer; with the trigger left as above that is a READ
 * PAGE FAULT -> "Unrecoverable System MMU Fault" panic at ~2.3 s. Hand DECON
 * over in the state S-BOOT left it.
 */
static int a51_pre_boot(void)
{
	writel(decon_trig_orig, DECON_F_BASE + HW_SW_TRIG_CONTROL);
	return 0;
}
#endif

static struct video_info a51_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = 1080,
	.height = 2400,
	.stride = 4,
	/* auto scale picks 3 at 1080x2400: big and slow to draw with the MMU off */
	.scale = 2,
	.address = (void *)0xca000000
};

static const struct device a51_devices[] = {
	{ "simplefb", &a51_fb, "fb" },
};

struct board_data board_ops = {
	.name = "samsung-a51",
	.ops = {
		.early_init = a51_init,
#ifdef CONFIG_USE_BOOTLOADER_FDT
		.pre_boot = a51_pre_boot,
#endif
	},
	.devices = a51_devices,
	.num_devices = ARRAY_SIZE(a51_devices),
	.quirks = 0,
#ifdef CONFIG_FAILSAFE
	.failsafe = &a51_failsafe,
#endif
};
