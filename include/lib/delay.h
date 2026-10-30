/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Busy-wait delays on the ARMv8 generic timer (aarch64 only)
 */
#ifndef DELAY_H_
#define DELAY_H_

unsigned long timer_ticks(void);
unsigned long timer_hz(void);
void udelay(unsigned long us);
void mdelay(unsigned long ms);

#endif /* DELAY_H_ */
