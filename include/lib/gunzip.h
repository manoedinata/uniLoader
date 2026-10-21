/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2026, Hendra Manudinata <hm@ndraa.xyz>
 */

#ifndef GUNZIP_H_
#define GUNZIP_H_

#define GUNZIP_ERR_HEADER	-1	/* not a gzip member we can handle */
#define GUNZIP_ERR_SPACE	-2	/* output does not fit in dst_size */
#define GUNZIP_ERR_DATA		-3	/* corrupt deflate stream */
#define GUNZIP_ERR_TRUNCATED	-4	/* input ended inside the stream */
#define GUNZIP_ERR_CRC		-5	/* trailer CRC32 or size mismatch */

/*
 * Inflate the single gzip member in src[0..src_size) into dst. Returns the
 * decompressed size, or a negative GUNZIP_ERR_* code. Memory is only accessed
 * byte-wise, so it is safe to run with the MMU off.
 */
long gunzip(void *dst, unsigned long dst_size, const void *src,
	    unsigned long src_size);

#endif // GUNZIP_H_
