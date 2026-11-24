// SPDX-License-Identifier: GPL-2.0-only
/*
 * Identity-mapped MMU for the payload copy / inflate.
 *
 * With the MMU off every data access is Device-nGnRnE: uncached, one beat at
 * a time, which makes inflating a gzip'd kernel take a long time. Around the
 * payload load, turn on an identity map in which only uniLoader's own image
 * and the payload window are Normal write-back cacheable; everything else in
 * the 4 GiB space stays Device-nGnRnE, so MMIO and the framebuffer behave as
 * before and nothing is speculatively read from secure or reserved DRAM.
 *
 * EL1 only (where S-BOOT hands over); at any other EL this is a no-op.
 */

#include <lib/debug.h>
#include <main/mmu.h>

#define SZ_2M			0x200000UL
#define SZ_1G			0x40000000UL

/* MAIR_EL1: attr0 = Device-nGnRnE, attr1 = Normal inner/outer WB RA/WA */
#define MAIR_VALUE		(0x00UL | (0xffUL << 8))
#define ATTR_DEVICE		0
#define ATTR_NORMAL		1

/* Block descriptor bits (stage 1, 4K granule) */
#define DESC_BLOCK		0x1UL
#define DESC_TABLE		0x3UL
#define DESC_ATTRIDX(i)		((unsigned long)(i) << 2)
#define DESC_SH_INNER		(0x3UL << 8)
#define DESC_AF			(1UL << 10)
#define DESC_PXN		(1UL << 53)
#define DESC_UXN		(1UL << 54)

/*
 * TCR_EL1: T0SZ = 32 (4 GiB, lookup starts at level 1), 4K granule,
 * inner/outer WB RA/WA walks, inner shareable, TTBR1 walks disabled,
 * IPS = 32 bits
 */
#define TCR_VALUE		(32UL | (1UL << 8) | (1UL << 10) | (3UL << 12) | \
				 (1UL << 23))

#define SCTLR_M			(1UL << 0)
#define SCTLR_C			(1UL << 2)
#define SCTLR_I			(1UL << 12)

static unsigned long l1_table[512] __attribute__((aligned(4096)));
static unsigned long l2_tables[4][512] __attribute__((aligned(4096)));
static unsigned long cached_lo[2], cached_hi[2];
static int mmu_on;

static unsigned long read_sctlr(void)
{
	unsigned long v;

	asm volatile("mrs %0, sctlr_el1" : "=r"(v));
	return v;
}

static void write_sctlr(unsigned long v)
{
	asm volatile("msr sctlr_el1, %0\n\tisb" : : "r"(v) : "memory");
}

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

/* invalidate by VA; with the MMU off VA == PA */
static void dcache_invalidate(unsigned long start, unsigned long end)
{
	unsigned long line = dcache_line();

	for (start &= ~(line - 1); start < end; start += line)
		asm volatile("dc ivac, %0" : : "r"(start) : "memory");
	asm volatile("dsb sy" : : : "memory");
}

static void map_cached(int slot, unsigned long start, unsigned long end)
{
	unsigned long a;

	start &= ~(SZ_2M - 1);
	end = (end + SZ_2M - 1) & ~(SZ_2M - 1);
	cached_lo[slot] = start;
	cached_hi[slot] = end;

	for (a = start; a < end && a < 4 * SZ_1G; a += SZ_2M)
		l2_tables[a / SZ_1G][(a % SZ_1G) / SZ_2M] =
			a | DESC_BLOCK | DESC_AF | DESC_SH_INNER |
			DESC_ATTRIDX(ATTR_NORMAL);
}

static void dcache_invalidate_cached_ranges(void)
{
	int i;

	for (i = 0; i < 2; i++)
		if (cached_hi[i] > cached_lo[i])
			dcache_invalidate(cached_lo[i], cached_hi[i]);
}

void mmu_enable_identity(unsigned long a_start, unsigned long a_end,
			 unsigned long b_start, unsigned long b_end)
{
	unsigned long gb, i;

	if (current_el() != 1) {
		printk(KERN_INFO, "mmu: EL%lu, staying uncached\n", current_el());
		return;
	}

	/* everything Device-nGnRnE and never executable... */
	for (gb = 0; gb < 4; gb++) {
		for (i = 0; i < 512; i++)
			l2_tables[gb][i] = (gb * SZ_1G + i * SZ_2M) | DESC_BLOCK |
				DESC_AF | DESC_ATTRIDX(ATTR_DEVICE) |
				DESC_PXN | DESC_UXN;
		l1_table[gb] = (unsigned long)l2_tables[gb] | DESC_TABLE;
	}
	/* ...except uniLoader itself and the payload window */
	map_cached(0, a_start, a_end);
	map_cached(1, b_start, b_end);

	/*
	 * The previous bootloader ran cached; drop any lines it left for the
	 * ranges about to become cacheable, or they would shadow what was
	 * written with the MMU off. The tables were written uncached.
	 */
	dcache_invalidate_cached_ranges();

	asm volatile(
		"msr mair_el1, %0\n\t"
		"msr tcr_el1, %1\n\t"
		"msr ttbr0_el1, %2\n\t"
		"isb\n\t"
		"tlbi vmalle1\n\t"
		"dsb sy\n\t"
		"ic iallu\n\t"
		"dsb sy\n\t"
		"isb"
		: : "r"(MAIR_VALUE), "r"(TCR_VALUE), "r"(l1_table) : "memory");

	write_sctlr(read_sctlr() | SCTLR_M | SCTLR_C | SCTLR_I);
	mmu_on = 1;
	printk(KERN_INFO, "mmu: on, cached %lx-%lx %lx-%lx\n",
	       cached_lo[0], cached_hi[0], cached_lo[1], cached_hi[1]);
}

/* mmu-off.S: clean+invalidate both ranges, then M and C off; leaf, no
 * stack use, so no dirty line can appear between the clean and the switch */
extern void __mmu_clean_disable(unsigned long a_lo, unsigned long a_hi,
				unsigned long b_lo, unsigned long b_hi,
				unsigned long line);

void mmu_disable(void)
{
	if (!mmu_on)
		return;

	/* the kernel image has to reach the PoC (boot protocol) */
	__mmu_clean_disable(cached_lo[0], cached_hi[0],
			    cached_lo[1], cached_hi[1], dcache_line());
	mmu_on = 0;
}
