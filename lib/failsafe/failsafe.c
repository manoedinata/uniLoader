// SPDX-License-Identifier: GPL-2.0
/*
 * Failsafe for devices without a working reset combo (dead power key, sealed
 * battery): whatever the payload does - hang, panic, crash-loop, or boot
 * fine but unreachable - the device must end up in a known-good image by
 * itself. Runs after the board's late_init:
 *
 * 1. Watchdog reset: many bootloaders ignore the reboot target after a
 *    watchdog reset (Samsung S-BOOT boots the same slot again). If the
 *    payload that hung was this loader's own, reset into the other slot. If
 *    it was the other slot's (this image is its escape target; a bootloader
 *    that falls back to the primary slot), boot on.
 * 2. Boot-loop breaker: count boots in a scratch word that survives resets;
 *    the payload's init clears it (writes 0 to FAILSAFE_SCRATCH_ADDR + 4)
 *    once it gets there. FAILSAFE_MAX_BOOTS boots without that: reset into
 *    the other slot.
 * 3. Mark this slot as the one running, then arm the watchdog: the payload
 *    has to take it over (Linux: s3c2410_wdt.tmr_atboot=1 + a userspace
 *    daemon / systemd RuntimeWatchdogSec) or the device resets.
 *
 * The scratch area (3 words) must be reserved in the payload's DT.
 */
#include <lib/debug.h>
#include <lib/delay.h>
#include <lib/failsafe.h>
#include <lib/ramlog.h>

#define FAILSAFE_MAGIC		0x5afeb007
#define FAILSAFE_SLOT_MARK	0x5afe5100	/* | slot */

#define SELF			CONFIG_FAILSAFE_SLOT
#define OTHER			(1 - CONFIG_FAILSAFE_SLOT)

static volatile unsigned int *const scratch =
	(volatile unsigned int *)CONFIG_FAILSAFE_SCRATCH_ADDR;

static void escape(const struct failsafe_ops *ops, const char *why)
{
	printk(KERN_ERR, "failsafe: %s, reset into slot %d in 3 s\n", why, OTHER);
	ramlog_dump_tail();
	udelay(3000000);
	ops->reset_to(OTHER);
	while (1)
		;
}

static void check_watchdog_reset(const struct failsafe_ops *ops)
{
	if (!ops->watchdog_reset || !ops->watchdog_reset())
		return;

	scratch[1] = 0;
	if (scratch[0] == FAILSAFE_MAGIC && scratch[2] == (FAILSAFE_SLOT_MARK | OTHER)) {
		printk(KERN_ERR, "failsafe: watchdog reset of slot %d, booting this one\n", OTHER);
		ramlog_dump_tail();
		return;
	}
	escape(ops, "last reset was the watchdog");
}

static void check_boot_loop(const struct failsafe_ops *ops)
{
	unsigned int count = scratch[0] == FAILSAFE_MAGIC ? scratch[1] + 1 : 1;

	scratch[0] = FAILSAFE_MAGIC;
	scratch[1] = count;
	printk(KERN_INFO, "failsafe: slot %d, boot counter %u (escape slot %d)\n",
	       SELF, count, OTHER);
	if (count < 2)
		return;

	printk(KERN_ERR, "failsafe: boot %u without reaching init\n", count);
	ramlog_dump_tail();
	udelay(3000000);
	if (count < CONFIG_FAILSAFE_MAX_BOOTS)
		return;

	scratch[1] = 0;
	escape(ops, "boot loop");
}

#ifdef CONFIG_FAILSAFE_WDT_TEST
/* Never hand over: the watchdog must reset the device. If it does not
 * within three timeouts, reset into the other slot anyway. */
static void watchdog_test(const struct failsafe_ops *ops, unsigned int sec)
{
	unsigned long start = timer_ticks();

	printk(KERN_INFO, "failsafe: test, waiting for the watchdog reset\n");
	while (timer_ticks() - start < 3 * sec * timer_hz())
		;
	escape(ops, "no watchdog reset");
}
#endif

void failsafe_run(const struct failsafe_ops *ops)
{
	unsigned int sec;

	if (!ops || !ops->reset_to || !ops->arm_watchdog) {
		printk(KERN_ERR, "failsafe: board has no failsafe ops\n");
		return;
	}

	check_watchdog_reset(ops);
	check_boot_loop(ops);
	scratch[2] = FAILSAFE_SLOT_MARK | SELF;

#ifdef CONFIG_FAILSAFE_WDT_TEST
	sec = ops->arm_watchdog(CONFIG_FAILSAFE_WDT_TEST_SEC);
#else
	sec = ops->arm_watchdog(CONFIG_FAILSAFE_WDT_SEC);
#endif
	printk(KERN_INFO, "failsafe: watchdog armed, %u s\n", sec);
#ifdef CONFIG_FAILSAFE_WDT_TEST
	watchdog_test(ops, sec);
#endif
}
