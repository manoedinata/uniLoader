/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MMIO accessors (the MMU is off: plain volatile accesses)
 */
#ifndef IO_H_
#define IO_H_

static inline unsigned int readl(unsigned long addr)
{
	return *(volatile unsigned int *)addr;
}

static inline void writel(unsigned int val, unsigned long addr)
{
	*(volatile unsigned int *)addr = val;
}

#endif /* IO_H_ */
