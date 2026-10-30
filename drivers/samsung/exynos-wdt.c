// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos (s3c2410-style) watchdog: prescaler 256, divider 128, reset
 * on expiry. The 16-bit counter limits the timeout (26 MHz OSCCLK: ~82 s).
 * Many Samsung bootloaders leave the PMU auto-disable/mask bits for the
 * watchdog reset set, so clear them too.
 */
#include <drivers/samsung/exynos-wdt.h>
#include <lib/io.h>

#define WTCON			0x00
#define WTDAT			0x04
#define WTCNT			0x08
#define WTCON_PRESCALE(x)	((x) << 8)
#define WTCON_DIV128		(3 << 3)
#define WTCON_ENABLE		(1 << 5)
#define WTCON_RSTEN		(1 << 0)

unsigned int exynos_wdt_arm(const struct exynos_wdt *wdt, unsigned int sec)
{
	unsigned long hz = wdt->clk_hz / 256 / 128;
	unsigned long count = sec ? sec * hz : 0xffff;

	if (count > 0xffff)
		count = 0xffff;

	writel(0, wdt->base + WTCON);
	if (wdt->pmu_disable)
		writel(readl(wdt->pmu_disable) & ~wdt->pmu_bit, wdt->pmu_disable);
	if (wdt->pmu_mask_reset)
		writel(readl(wdt->pmu_mask_reset) & ~wdt->pmu_bit, wdt->pmu_mask_reset);

	writel(count, wdt->base + WTDAT);
	writel(count, wdt->base + WTCNT);
	writel(WTCON_PRESCALE(255) | WTCON_DIV128 | WTCON_ENABLE | WTCON_RSTEN,
	       wdt->base + WTCON);

	return count / hz;
}
