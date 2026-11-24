/* SPDX-License-Identifier: GPL-2.0 */
/*
 * __mmu_clean_disable(a_lo, a_hi, b_lo, b_hi, line, el)
 *
 * Clean+invalidate [a_lo, a_hi) and [b_lo, b_hi) to the PoC, then turn the
 * MMU and D-cache of EL1 or EL2 (el = 1 or 2) off. A leaf without stores:
 * nothing can be dirtied between the last clean and the switch, and once
 * the MMU is off the stack (inside one of the ranges) is accessed uncached
 * with no stale lines left.
 */
#include "asmdefs.h"

ENTRY(__mmu_clean_disable)
	sub	x6, x4, #1
	bic	x0, x0, x6
1:	cmp	x0, x1
	b.hs	2f
	dc	civac, x0
	add	x0, x0, x4
	b	1b
2:	bic	x2, x2, x6
3:	cmp	x2, x3
	b.hs	4f
	dc	civac, x2
	add	x2, x2, x4
	b	3b
4:	dsb	sy
	cmp	x5, #2
	b.eq	5f
	mrs	x7, sctlr_el1
	bic	x7, x7, #(1 << 0)	/* M */
	bic	x7, x7, #(1 << 2)	/* C */
	msr	sctlr_el1, x7
	isb
	tlbi	vmalle1
	b	6f
5:	mrs	x7, sctlr_el2
	bic	x7, x7, #(1 << 0)
	bic	x7, x7, #(1 << 2)
	msr	sctlr_el2, x7
	isb
	tlbi	alle2
6:	ic	iallu
	dsb	sy
	isb
	ret
END(__mmu_clean_disable)
