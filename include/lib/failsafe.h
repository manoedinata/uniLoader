/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Failsafe for devices that cannot be reset by hand: watchdog, boot-loop
 * breaker and an escape into the other boot slot
 */
#ifndef FAILSAFE_H_
#define FAILSAFE_H_

/* Boot slots: 0 = primary (Samsung: BOOT), 1 = secondary (RECOVERY) */
#define FAILSAFE_SLOT_PRIMARY	0
#define FAILSAFE_SLOT_SECONDARY	1

struct failsafe_ops {
	/* nonzero if the last reset was caused by the watchdog */
	int (*watchdog_reset)(void);
	/* reset into the image in @slot; must not return */
	void (*reset_to)(unsigned int slot);
	/* arm a watchdog that resets the SoC after about @sec seconds (0 =
	 * the longest it can do); returns the timeout actually set */
	unsigned int (*arm_watchdog)(unsigned int sec);
};

#ifdef CONFIG_FAILSAFE
void failsafe_run(const struct failsafe_ops *ops);
#else
static inline void failsafe_run(const struct failsafe_ops *ops) { (void)ops; }
#endif

#endif /* FAILSAFE_H_ */
