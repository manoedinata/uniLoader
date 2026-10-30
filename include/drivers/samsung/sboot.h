/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung S-BOOT boot target (sec_reboot.c SEC_RESET_REASON_* in a PMU
 * INFORM register) and PMU software reset
 */
#ifndef SBOOT_H_
#define SBOOT_H_

#define SBOOT_NORMAL		0x12345670	/* BOOT */
#define SBOOT_DOWNLOAD		0x12345671
#define SBOOT_UPLOAD		0x12345672
#define SBOOT_RECOVERY		0x12345674	/* RECOVERY */

/* Sets the boot target in @inform (absolute address) and writes 1 to the
 * PMU SWRESET register @swreset. Does not return. */
void sboot_reset(unsigned long inform, unsigned long swreset, unsigned int target);

#endif /* SBOOT_H_ */
