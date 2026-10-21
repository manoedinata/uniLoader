// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal gzip (RFC 1952) / DEFLATE (RFC 1951) decoder for compressed kernels.
 * The Huffman decoding follows the canonical-code approach of zlib's puff.c,
 * with a lookup table in front of it for the short codes.
 *
 * Copyright (c) 2026, Hendra Manudinata <hm@ndraa.xyz>
 */

#include <lib/gunzip.h>
#include <string.h>

#define MAXBITS		15	/* longest DEFLATE code */
#define MAXLCODES	286	/* literal/length codes */
#define MAXDCODES	30	/* distance codes */
#define FIXLCODES	288	/* literal/length codes in the fixed table */
#define FAST_BITS	9	/* codes up to this long decode in one lookup */

#define GZ_FHCRC	0x02
#define GZ_FEXTRA	0x04
#define GZ_FNAME	0x08
#define GZ_FCOMMENT	0x10

struct huffman {
	unsigned short count[MAXBITS + 1];	/* codes of each length */
	unsigned short symbol[FIXLCODES];	/* symbols in canonical order */
	unsigned short fast[1 << FAST_BITS];	/* len << 9 | symbol, 0: slow path */
};

struct state {
	const unsigned char *in, *in_end;
	unsigned char *out, *out_start, *out_end;
	unsigned long bitbuf;
	unsigned int bitcnt;
	unsigned int crc;
};

static struct huffman lencode, distcode;
static unsigned short lengths[MAXLCODES + MAXDCODES];

static void refill(struct state *s)
{
	while (s->bitcnt <= sizeof(s->bitbuf) * 8 - 8 && s->in < s->in_end) {
		s->bitbuf |= (unsigned long)*s->in++ << s->bitcnt;
		s->bitcnt += 8;
	}
}

/* n <= 16; returns the bits or GUNZIP_ERR_TRUNCATED */
static long bits(struct state *s, unsigned int n)
{
	long val;

	if (s->bitcnt < n) {
		refill(s);
		if (s->bitcnt < n)
			return GUNZIP_ERR_TRUNCATED;
	}
	val = s->bitbuf & ((1UL << n) - 1);
	s->bitbuf >>= n;
	s->bitcnt -= n;
	return val;
}

static inline void put(struct state *s, unsigned char c)
{
	unsigned int crc = s->crc ^ c;
	int k;

	for (k = 0; k < 8; k++)
		crc = (crc >> 1) ^ (0xedb88320 & -(crc & 1));
	s->crc = crc;
	*s->out++ = c;
}

static int decode(struct state *s, const struct huffman *h)
{
	int code, first, count, index, len;
	unsigned int entry;

	if (s->bitcnt < MAXBITS)
		refill(s);

	entry = h->fast[s->bitbuf & ((1 << FAST_BITS) - 1)];
	len = entry >> 9;
	if (len) {
		if ((unsigned int)len > s->bitcnt)
			return GUNZIP_ERR_TRUNCATED;
		s->bitbuf >>= len;
		s->bitcnt -= len;
		return entry & 0x1ff;
	}

	/* longer than FAST_BITS (or invalid): walk the canonical code bit by bit */
	code = first = index = 0;
	for (len = 1; len <= MAXBITS; len++) {
		if (!s->bitcnt)
			return GUNZIP_ERR_TRUNCATED;
		code |= s->bitbuf & 1;
		s->bitbuf >>= 1;
		s->bitcnt--;
		count = h->count[len];
		if (code - count < first)
			return h->symbol[index + (code - first)];
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return GUNZIP_ERR_DATA;
}

/* returns 0 for a complete code, > 0 for an incomplete one, < 0 if oversubscribed */
static int construct(struct huffman *h, const unsigned short *length, int n)
{
	unsigned short offs[MAXBITS + 1];
	int sym, len, left, code, index, i, j, rev;

	for (len = 0; len <= MAXBITS; len++)
		h->count[len] = 0;
	for (sym = 0; sym < n; sym++)
		h->count[length[sym]]++;
	memset(h->fast, 0, sizeof(h->fast));
	if (h->count[0] == n)
		return 0;

	left = 1;
	for (len = 1; len <= MAXBITS; len++) {
		left <<= 1;
		left -= h->count[len];
		if (left < 0)
			return left;
	}

	offs[1] = 0;
	for (len = 1; len < MAXBITS; len++)
		offs[len + 1] = offs[len] + h->count[len];
	for (sym = 0; sym < n; sym++)
		if (length[sym])
			h->symbol[offs[length[sym]]++] = sym;

	/* DEFLATE sends codes MSB first into an LSB-first stream: index by reversed code */
	code = index = 0;
	for (len = 1; len <= FAST_BITS; len++) {
		for (i = 0; i < h->count[len]; i++, code++, index++) {
			rev = 0;
			for (j = 0; j < len; j++)
				rev |= ((code >> j) & 1) << (len - 1 - j);
			for (j = rev; j < (1 << FAST_BITS); j += 1 << len)
				h->fast[j] = (len << 9) | h->symbol[index];
		}
		code <<= 1;
	}

	return left;
}

static int codes(struct state *s)
{
	static const unsigned short lbase[29] = {
		3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
		35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
	static const unsigned char lext[29] = {
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
		3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
	static const unsigned short dbase[30] = {
		1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
		257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
		8193, 12289, 16385, 24577 };
	static const unsigned char dext[30] = {
		0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
		7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
	const unsigned char *from;
	long len, dist, extra;
	int sym;

	for (;;) {
		sym = decode(s, &lencode);
		if (sym < 0)
			return sym;
		if (sym < 256) {
			if (s->out == s->out_end)
				return GUNZIP_ERR_SPACE;
			put(s, sym);
			continue;
		}
		if (sym == 256)
			return 0;

		sym -= 257;
		if (sym >= 29)
			return GUNZIP_ERR_DATA;
		extra = bits(s, lext[sym]);
		if (extra < 0)
			return extra;
		len = lbase[sym] + extra;

		sym = decode(s, &distcode);
		if (sym < 0)
			return sym;
		if (sym >= 30)
			return GUNZIP_ERR_DATA;
		extra = bits(s, dext[sym]);
		if (extra < 0)
			return extra;
		dist = dbase[sym] + extra;

		if (dist > s->out - s->out_start)
			return GUNZIP_ERR_DATA;
		if (len > s->out_end - s->out)
			return GUNZIP_ERR_SPACE;
		from = s->out - dist;
		while (len--)
			put(s, *from++);
	}
}

static int stored(struct state *s)
{
	long len, nlen;

	/* discard the rest of the current byte; the buffer then holds whole bytes */
	s->bitbuf >>= s->bitcnt & 7;
	s->bitcnt -= s->bitcnt & 7;

	len = bits(s, 16);
	nlen = bits(s, 16);
	if (len < 0 || nlen < 0)
		return GUNZIP_ERR_TRUNCATED;
	if (len != (~nlen & 0xffff))
		return GUNZIP_ERR_DATA;
	if (len > s->out_end - s->out)
		return GUNZIP_ERR_SPACE;

	while (len--) {
		if (s->bitcnt) {
			put(s, s->bitbuf & 0xff);
			s->bitbuf >>= 8;
			s->bitcnt -= 8;
		} else if (s->in < s->in_end) {
			put(s, *s->in++);
		} else {
			return GUNZIP_ERR_TRUNCATED;
		}
	}
	return 0;
}

static int fixed(struct state *s)
{
	int sym;

	for (sym = 0; sym < 144; sym++)
		lengths[sym] = 8;
	for (; sym < 256; sym++)
		lengths[sym] = 9;
	for (; sym < 280; sym++)
		lengths[sym] = 7;
	for (; sym < FIXLCODES; sym++)
		lengths[sym] = 8;
	construct(&lencode, lengths, FIXLCODES);

	for (sym = 0; sym < MAXDCODES; sym++)
		lengths[sym] = 5;
	construct(&distcode, lengths, MAXDCODES);

	return codes(s);
}

static int dynamic(struct state *s)
{
	static const unsigned char order[19] = {
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	long nlen, ndist, ncode, val;
	int index, sym, len, err;

	nlen = bits(s, 5);
	ndist = bits(s, 5);
	ncode = bits(s, 4);
	if (nlen < 0 || ndist < 0 || ncode < 0)
		return GUNZIP_ERR_TRUNCATED;
	nlen += 257;
	ndist += 1;
	ncode += 4;
	if (nlen > MAXLCODES || ndist > MAXDCODES)
		return GUNZIP_ERR_DATA;

	for (index = 0; index < ncode; index++) {
		val = bits(s, 3);
		if (val < 0)
			return val;
		lengths[order[index]] = val;
	}
	for (; index < 19; index++)
		lengths[order[index]] = 0;
	if (construct(&lencode, lengths, 19) != 0)
		return GUNZIP_ERR_DATA;

	index = 0;
	while (index < nlen + ndist) {
		sym = decode(s, &lencode);
		if (sym < 0)
			return sym;
		if (sym < 16) {
			lengths[index++] = sym;
			continue;
		}

		len = 0;
		if (sym == 16) {
			if (index == 0)
				return GUNZIP_ERR_DATA;
			len = lengths[index - 1];
			val = bits(s, 2);
			sym = 3;
		} else if (sym == 17) {
			val = bits(s, 3);
			sym = 3;
		} else {
			val = bits(s, 7);
			sym = 11;
		}
		if (val < 0)
			return val;
		sym += val;
		if (index + sym > nlen + ndist)
			return GUNZIP_ERR_DATA;
		while (sym--)
			lengths[index++] = len;
	}

	/* the end-of-block code must exist */
	if (lengths[256] == 0)
		return GUNZIP_ERR_DATA;

	/* incomplete codes are only allowed when they hold a single symbol */
	err = construct(&lencode, lengths, nlen);
	if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1]))
		return GUNZIP_ERR_DATA;
	err = construct(&distcode, lengths + nlen, ndist);
	if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1]))
		return GUNZIP_ERR_DATA;

	return codes(s);
}

static unsigned int get_le32(const unsigned char *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24;
}

long gunzip(void *dst, unsigned long dst_size, const void *src,
	    unsigned long src_size)
{
	const unsigned char *p = src, *end = p + src_size;
	struct state s;
	unsigned int flags, isize;
	long last, type;
	int err;

	if (src_size < 18 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8)
		return GUNZIP_ERR_HEADER;
	flags = p[3];
	if (flags & 0xe0)
		return GUNZIP_ERR_HEADER;
	p += 10;
	if (flags & GZ_FEXTRA) {
		unsigned long xlen = p[0] | p[1] << 8;

		p += 2;
		if (xlen > (unsigned long)(end - p))
			return GUNZIP_ERR_HEADER;
		p += xlen;
	}
	if (flags & GZ_FNAME)
		while (p < end && *p++)
			;
	if (flags & GZ_FCOMMENT)
		while (p < end && *p++)
			;
	if (flags & GZ_FHCRC)
		p += 2;
	if (p > end || end - p < 8)
		return GUNZIP_ERR_HEADER;

	/* ISIZE is the size mod 2^32, exact for anything that fits in RAM here */
	isize = get_le32(end - 4);
	if (isize > dst_size)
		return GUNZIP_ERR_SPACE;

	s.in = p;
	s.in_end = end - 8;
	s.out = s.out_start = dst;
	s.out_end = s.out + isize;
	s.bitbuf = 0;
	s.bitcnt = 0;
	s.crc = 0xffffffff;

	do {
		last = bits(&s, 1);
		type = bits(&s, 2);
		if (last < 0 || type < 0)
			return GUNZIP_ERR_TRUNCATED;
		if (type == 0)
			err = stored(&s);
		else if (type == 1)
			err = fixed(&s);
		else if (type == 2)
			err = dynamic(&s);
		else
			err = GUNZIP_ERR_DATA;
		if (err)
			return err;
	} while (!last);

	if ((unsigned int)(s.out - s.out_start) != isize ||
	    ~s.crc != get_le32(end - 8))
		return GUNZIP_ERR_CRC;

	return isize;
}
