// SPDX-License-Identifier: GPL-2.0-only
#ifndef MAIN_MMU_H_
#define MAIN_MMU_H_

#ifdef CONFIG_MMU_IDENTITY
/* Identity map, cacheable only in [a_start, a_end) and [b_start, b_end) */
void mmu_enable_identity(unsigned long a_start, unsigned long a_end,
			 unsigned long b_start, unsigned long b_end);
/* Clean the cacheable ranges to the PoC, MMU and D-cache off */
void mmu_disable(void);
#else
static inline void mmu_enable_identity(unsigned long a_start, unsigned long a_end,
				       unsigned long b_start, unsigned long b_end) {}
static inline void mmu_disable(void) {}
#endif

#endif
