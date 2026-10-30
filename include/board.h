/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Board files registration framework
 * Copyright (c) 2024, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */
#ifndef BOARD_H_
#define BOARD_H_

#include <stddef.h>
#include <drivers/framework.h>
#include <lib/failsafe.h>

struct board_ops {
	int (*early_init)(void);
	int (*late_init)(void);
	/* optional: last thing before the payload is started */
	int (*pre_boot)(void);
};

struct board_data {
	const char *name;
	struct board_ops ops;
	const struct device *devices;
	size_t num_devices;
	unsigned int quirks;
	/* optional, used with CONFIG_FAILSAFE */
	const struct failsafe_ops *failsafe;
};

#endif /* BOARD_H_ */
