// SPDX-License-Identifier: GPL-2.0
/*
 * Busy-wait delays on the ARMv8 generic timer. CNTFRQ_EL0 is set up by the
 * firmware before any bootloader runs, so this needs no driver.
 */
#include <lib/delay.h>

unsigned long timer_ticks(void)
{
	unsigned long v;

	__asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(v));
	return v;
}

unsigned long timer_hz(void)
{
	unsigned long v;

	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

void udelay(unsigned long us)
{
	unsigned long start = timer_ticks();
	unsigned long ticks = timer_hz() / 1000000 * us;

	while (timer_ticks() - start < ticks)
		;
}

void mdelay(unsigned long ms)
{
	while (ms--)
		udelay(1000);
}
