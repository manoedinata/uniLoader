// SPDX-License-Identifier: GPL-2.0-only
/*
 * Identity-mapped MMU for the payload copy / inflate.
 *
 * With the MMU off every data access is Device-nGnRnE: uncached, one beat at
 * a time, which makes inflating a gzip'd kernel take a long time. Around the
 * payload load, turn on an identity map in which only uniLoader's own image
 * and the payload window are Normal write-back cacheable; everything else
 * the CPU can address stays Device-nGnRnE and execute-never, so MMIO and
 * framebuffers behave as before and nothing is speculatively read from
 * secure or reserved DRAM.
 *
 * Works at EL1 and at EL2 without VHE (HCR_EL2.E2H = 0); anywhere else it
 * stays a no-op and the payload is loaded uncached as before.
 *
 * Layout: 4K granule, 39-bit VA (T0SZ = 25, walks start at level 1), 1 GiB
 * Device blocks, and a level 2 table of 2 MiB blocks for each GiB that holds
 * a cacheable range.
 */

#include <lib/debug.h>
#include <main/mmu.h>

#define SZ_2M			0x200000UL
#define SZ_1G			0x40000000UL
#define VA_BITS			39
#define MAX_L2			4	/* two ranges, each may straddle a GiB */

/* MAIR: attr0 = Device-nGnRnE, attr1 = Normal inner/outer WB RA/WA */
#define MAIR_VALUE		(0x00UL | (0xffUL << 8))
#define ATTR_DEVICE		0
#define ATTR_NORMAL		1

/* Stage 1 descriptor bits */
#define DESC_BLOCK		0x1UL
#define DESC_TABLE		0x3UL
#define DESC_ATTRIDX(i)		((unsigned long)(i) << 2)
#define DESC_SH_INNER		(0x3UL << 8)
#define DESC_AF			(1UL << 10)
#define DESC_PXN		(1UL << 53)	/* EL1&0 only; RES0 at EL2 */
#define DESC_XN			(1UL << 54)	/* UXN at EL1, XN at EL2 */

/* TCR fields shared by TCR_EL1 and TCR_EL2: T0SZ, IRGN0/ORGN0 WB RA/WA,
 * SH0 inner shareable, TG0 4K */
#define TCR_COMMON		((64UL - VA_BITS) | (1UL << 8) | (1UL << 10) | \
				 (3UL << 12))
#define TCR_EL1_EPD1		(1UL << 23)
#define TCR_EL1_IPS_SHIFT	32
#define TCR_EL2_RES1		((1UL << 31) | (1UL << 23))
#define TCR_EL2_PS_SHIFT	16

#define SCTLR_M			(1UL << 0)
#define SCTLR_C			(1UL << 2)
#define SCTLR_I			(1UL << 12)

#define HCR_EL2_E2H		(1UL << 34)

static unsigned long l1_table[512] __attribute__((aligned(4096)));
static unsigned long l2_tables[MAX_L2][512] __attribute__((aligned(4096)));
static unsigned long cached_lo[2], cached_hi[2];
static unsigned long mmu_el;	/* EL the map was turned on at, 0 = off */

extern void __mmu_clean_disable(unsigned long a_lo, unsigned long a_hi,
				unsigned long b_lo, unsigned long b_hi,
				unsigned long line, unsigned long el);

static unsigned long current_el(void)
{
	unsigned long v;

	asm volatile("mrs %0, CurrentEL" : "=r"(v));
	return (v >> 2) & 3;
}

static unsigned long dcache_line(void)
{
	unsigned long ctr;

	asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
	return 4UL << ((ctr >> 16) & 0xf);
}

/* ID_AA64MMFR0_EL1.PARange: field value, and the number of PA bits */
static unsigned long pa_range(unsigned long *bits)
{
	static const unsigned char pa_bits[] = { 32, 36, 40, 42, 44, 48 };
	unsigned long mmfr0, r;

	asm volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
	r = mmfr0 & 0xf;
	if (r > 5)		/* 52 bits needs LPA2 with a 4K granule */
		r = 5;
	*bits = pa_bits[r];
	return r;
}

/* invalidate by VA; with the MMU off VA == PA */
static void dcache_invalidate(unsigned long start, unsigned long end)
{
	unsigned long line = dcache_line();

	for (start &= ~(line - 1); start < end; start += line)
		asm volatile("dc ivac, %0" : : "r"(start) : "memory");
	asm volatile("dsb sy" : : : "memory");
}

static unsigned long *l2_for(unsigned long gb, unsigned long dev_attrs,
			     unsigned long *used)
{
	unsigned long i, *l2;

	if ((l1_table[gb] & 3) == DESC_TABLE)
		return (unsigned long *)(l1_table[gb] & ~0xfffUL);
	if (*used == MAX_L2)
		return 0;

	l2 = l2_tables[(*used)++];
	for (i = 0; i < 512; i++)
		l2[i] = (gb * SZ_1G + i * SZ_2M) | dev_attrs;
	l1_table[gb] = (unsigned long)l2 | DESC_TABLE;
	return l2;
}

static int map_cached(int slot, unsigned long start, unsigned long end,
		      unsigned long limit, unsigned long dev_attrs,
		      unsigned long *used)
{
	unsigned long a, *l2;

	cached_lo[slot] = cached_hi[slot] = 0;
	if (end <= start)
		return 0;

	start &= ~(SZ_2M - 1);
	end = (end + SZ_2M - 1) & ~(SZ_2M - 1);
	if (end > limit)
		return -1;

	for (a = start; a < end; a += SZ_2M) {
		l2 = l2_for(a / SZ_1G, dev_attrs, used);
		if (!l2)
			return -1;
		l2[(a % SZ_1G) / SZ_2M] = a | DESC_BLOCK | DESC_AF |
			DESC_SH_INNER | DESC_ATTRIDX(ATTR_NORMAL);
	}
	cached_lo[slot] = start;
	cached_hi[slot] = end;
	return 0;
}

void mmu_enable_identity(unsigned long a_start, unsigned long a_end,
			 unsigned long b_start, unsigned long b_end)
{
	unsigned long el = current_el(), bits, ps, gb, limit, used = 0;
	unsigned long dev_attrs, sctlr, i;

	if (el == 2) {
		unsigned long hcr;

		asm volatile("mrs %0, hcr_el2" : "=r"(hcr));
		if (hcr & HCR_EL2_E2H)
			el = 0;		/* VHE: EL1 register layout, not handled */
	}
	if (el != 1 && el != 2) {
		printk(KERN_INFO, "mmu: EL%lu not handled, staying uncached\n",
		       current_el());
		return;
	}

	ps = pa_range(&bits);
	limit = bits >= VA_BITS ? 1UL << VA_BITS : 1UL << bits;
	dev_attrs = DESC_BLOCK | DESC_AF | DESC_ATTRIDX(ATTR_DEVICE) | DESC_XN |
		(el == 1 ? DESC_PXN : 0);

	/* everything the CPU can address: 1 GiB Device blocks... */
	for (gb = 0; gb < 512; gb++)
		l1_table[gb] = gb * SZ_1G < limit ? (gb * SZ_1G) | dev_attrs : 0;

	/* ...except uniLoader itself and the payload window */
	if (map_cached(0, a_start, a_end, limit, dev_attrs, &used) ||
	    map_cached(1, b_start, b_end, limit, dev_attrs, &used)) {
		printk(KERN_ERR, "mmu: cannot map %lx-%lx %lx-%lx, staying uncached\n",
		       a_start, a_end, b_start, b_end);
		return;
	}

	/*
	 * The previous bootloader ran cached: drop any lines it left for the
	 * ranges about to become cacheable, or they would shadow what was
	 * written with the MMU off. The tables themselves were written uncached.
	 */
	for (i = 0; i < 2; i++)
		if (cached_hi[i] > cached_lo[i])
			dcache_invalidate(cached_lo[i], cached_hi[i]);

	if (el == 1)
		asm volatile(
			"msr mair_el1, %1\n\t"
			"msr tcr_el1, %2\n\t"
			"msr ttbr0_el1, %3\n\t"
			"isb\n\t"
			"tlbi vmalle1\n\t"
			"dsb sy\n\t"
			"ic iallu\n\t"
			"dsb sy\n\t"
			"isb\n\t"
			"mrs %0, sctlr_el1\n\t"
			"orr %0, %0, %4\n\t"
			"msr sctlr_el1, %0\n\t"
			"isb"
			: "=&r"(sctlr)
			: "r"(MAIR_VALUE),
			  "r"(TCR_COMMON | TCR_EL1_EPD1 | (ps << TCR_EL1_IPS_SHIFT)),
			  "r"(l1_table), "r"(SCTLR_M | SCTLR_C | SCTLR_I)
			: "memory");
	else
		asm volatile(
			"msr mair_el2, %1\n\t"
			"msr tcr_el2, %2\n\t"
			"msr ttbr0_el2, %3\n\t"
			"isb\n\t"
			"tlbi alle2\n\t"
			"dsb sy\n\t"
			"ic iallu\n\t"
			"dsb sy\n\t"
			"isb\n\t"
			"mrs %0, sctlr_el2\n\t"
			"orr %0, %0, %4\n\t"
			"msr sctlr_el2, %0\n\t"
			"isb"
			: "=&r"(sctlr)
			: "r"(MAIR_VALUE),
			  "r"(TCR_COMMON | TCR_EL2_RES1 | (ps << TCR_EL2_PS_SHIFT)),
			  "r"(l1_table), "r"(SCTLR_M | SCTLR_C | SCTLR_I)
			: "memory");

	mmu_el = el;
	printk(KERN_INFO, "mmu: on at EL%lu (%lu-bit PA), cached %lx-%lx %lx-%lx\n",
	       el, bits, cached_lo[0], cached_hi[0], cached_lo[1], cached_hi[1]);
}

void mmu_disable(void)
{
	if (!mmu_el)
		return;

	/* the kernel image has to reach the PoC (arm64 boot protocol) */
	__mmu_clean_disable(cached_lo[0], cached_hi[0], cached_lo[1],
			    cached_hi[1], dcache_line(), mmu_el);
	mmu_el = 0;
}
