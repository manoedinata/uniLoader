// SPDX-License-Identifier: GPL-2.0
/*
 * Loader log that survives resets: every printk is appended to a ramoops
 * console zone (Linux persistent_ram_buffer: "DBGC" signature, start, size,
 * then the ring; no ECC), right after the log of the kernel that ran before.
 * The next kernel with the same ramoops geometry saves the zone as
 * /sys/fs/pstore/console-ramoops-0, i.e. "previous kernel + loader lines of
 * the boots since". Kernels wipe a zone after saving it, so use the console
 * zone (pmsg/ftrace may be re-initialized with other signatures).
 *
 * RAMLOG_ADDR/RAMLOG_SIZE must match the kernel's ramoops console zone and
 * the memory must survive a reset (it is DRAM in self-refresh on most SoCs).
 * Byte stores only: the MMU is off.
 */
#include <lib/console.h>
#include <lib/debug.h>
#include <lib/ramlog.h>

#define RAMOOPS_SIG		0x43474244	/* "DBGC" */
#define RAMLOG_HDR		12
#define RAMLOG_CAP		(CONFIG_RAMLOG_SIZE - RAMLOG_HDR)

static volatile unsigned int *const hdr = (volatile unsigned int *)CONFIG_RAMLOG_ADDR;
static volatile char *const data = (volatile char *)(CONFIG_RAMLOG_ADDR + RAMLOG_HDR);
/* ring state at init: the previous log ends there */
static unsigned int prev_start, prev_size;

static void ramlog_putc(char c)
{
	unsigned int start = hdr[1], size = hdr[2];

	data[start] = c;
	start = (start + 1) % RAMLOG_CAP;
	if (size < RAMLOG_CAP)
		size++;
	hdr[1] = start;
	hdr[2] = size;
}

static void ramlog_write(int level, const char *prefix, const char *msg)
{
	(void)level;
	while (prefix && *prefix)
		ramlog_putc(*prefix++);
	while (*msg)
		ramlog_putc(*msg++);
}

static const struct console ramlog = {
	.write = ramlog_write,
};

/* Print the end of the previous log (what was in the zone at init). */
void ramlog_dump_tail(void)
{
	static char tail[CONFIG_RAMLOG_TAIL + 1];
	unsigned int start = prev_start, size = prev_size;
	unsigned int n, i, pos;
	char *line;

	if (size == 0) {
		printk(KERN_INFO, "ramlog: previous log is empty\n");
		return;
	}

	n = size < CONFIG_RAMLOG_TAIL ? size : CONFIG_RAMLOG_TAIL;
	/* the newest byte is just before start; walk back n bytes */
	pos = (size < RAMLOG_CAP ? size : start) + RAMLOG_CAP - n;
	for (i = 0; i < n; i++) {
		char c = data[(pos + i) % RAMLOG_CAP];

		tail[i] = (c == '\n' || (c >= 0x20 && c < 0x7f)) ? c : '.';
	}
	tail[n] = 0;

	/* printk appends to the zone itself: print from the copy */
	printk(KERN_INFO, "ramlog: ---- previous log (last %u of %u bytes) ----\n", n, size);
	line = tail;
	for (i = 0; i < n; i++) {
		if (tail[i] == '\n') {
			tail[i] = 0;
			printk(KERN_INFO, "%s\n", line);
			line = &tail[i + 1];
		}
	}
	if (*line)
		printk(KERN_INFO, "%s\n", line);
	printk(KERN_INFO, "ramlog: ---- end of previous log ----\n");
}

void ramlog_init(void)
{
	if (hdr[0] != RAMOOPS_SIG || hdr[1] >= RAMLOG_CAP || hdr[2] > RAMLOG_CAP) {
		hdr[0] = RAMOOPS_SIG;
		hdr[1] = 0;
		hdr[2] = 0;
	}
	prev_start = hdr[1];
	prev_size = hdr[2];
	console_register(&ramlog);
	/* marks where this boot starts, after the previous kernel's log */
	printk(KERN_INFO, "ramlog: ==== uniLoader boot ====\n");
}
