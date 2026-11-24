// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <main/boot.h>
#include <string.h>
#include <lib/debug.h>
#include <main/mmu.h>

extern char __image_start[], __image_end[];

static unsigned long cntpct(void)
{
	unsigned long v;

	asm volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v));
	return v;
}

static unsigned long cntfrq(void)
{
	unsigned long v;

	asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

void arch_load_kernel(void* kernel, void* dt, void* ramdisk)
{
	unsigned long t0 = cntpct(), frq = cntfrq();
	int ret;

#ifdef CONFIG_MMU_IDENTITY
	mmu_enable_identity((unsigned long)__image_start, (unsigned long)__image_end,
			    CONFIG_PAYLOAD_ENTRY,
			    CONFIG_PAYLOAD_ENTRY + CONFIG_MMU_PAYLOAD_WINDOW);
#endif
	ret = load_kernel_payload(kernel, (unsigned long) &kernel_size);
#ifdef CONFIG_MMU_IDENTITY
	mmu_disable();
#endif
	if (frq)
		printk(KERN_INFO, "payload loaded in %lu ms\n",
		       (cntpct() - t0) * 1000 / frq);
	if (ret)
		return;
#ifndef CONFIG_RAMDISK_NO_COPY
	__optimized_memcpy((void*)CONFIG_RAMDISK_ENTRY, ramdisk, (unsigned long) &ramdisk_size);
#endif
	load_kernel_and_jump(dt, 0, 0, 0, (void*)CONFIG_PAYLOAD_ENTRY);
}
