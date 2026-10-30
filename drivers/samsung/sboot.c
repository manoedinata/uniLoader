// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung S-BOOT reboot: the target goes into a PMU INFORM register (INFORM3
 * on most Exynos), then a PMU software reset. This is what "reboot recovery"
 * does on stock. Note: S-BOOT ignores the target after a watchdog reset.
 */
#include <drivers/samsung/sboot.h>
#include <lib/io.h>

void sboot_reset(unsigned long inform, unsigned long swreset, unsigned int target)
{
	writel(target, inform);
	writel(1, swreset);
	while (1)
		;
}
