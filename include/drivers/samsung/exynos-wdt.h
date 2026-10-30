/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos (s3c2410-style) watchdog
 */
#ifndef EXYNOS_WDT_H_
#define EXYNOS_WDT_H_

struct exynos_wdt {
	unsigned long base;		/* WTCON @0, WTDAT @4, WTCNT @8 */
	unsigned long clk_hz;		/* input clock (often OSCCLK) */
	/* PMU unmask of this watchdog's reset: bit set = disabled / masked */
	unsigned long pmu_disable;	/* absolute address, 0 = none */
	unsigned long pmu_mask_reset;	/* absolute address, 0 = none */
	unsigned int pmu_bit;
};

/* Arms the watchdog for about @sec seconds (0 or too long = the maximum).
 * Returns the timeout set, in seconds. */
unsigned int exynos_wdt_arm(const struct exynos_wdt *wdt, unsigned int sec);

#endif /* EXYNOS_WDT_H_ */
