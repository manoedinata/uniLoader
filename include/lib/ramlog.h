/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Loader log in a Linux ramoops (pstore) console zone
 */
#ifndef RAMLOG_H_
#define RAMLOG_H_

#ifdef CONFIG_RAMLOG
void ramlog_init(void);
void ramlog_dump_tail(void);
#else
static inline void ramlog_init(void) {}
static inline void ramlog_dump_tail(void) {}
#endif

#endif /* RAMLOG_H_ */
