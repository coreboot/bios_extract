/*
 * This file is a severely cut down and cleaned up version of lha.
 *
 * All changes compared to lha-svn894 are:
 *
 * Copyright 2009      Luc Verhaegen <libv@skynet.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * LHA has a terrible history... It dates back to 1988, has had many different
 * authors and has been mostly Public Domain Software.
 *
 * Since 1999, Koji Arai <arai@users.sourceforge.jp> has been doing most of
 * the work at http://sourceforge.jp/projects/lha/.
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "compat.h"
#include "lh5_extract.h"

/*
 * LHA header parsing.
 */
static int calc_sum(unsigned char *p, int len)
{
	int sum = 0;

	while (len--)
		sum += *p++;

	return sum & 0xff;
}

/*
 * level 1 header
 *
 *
 * offset   size  field name
 * -----------------------------------
 *     0       1  header size   [*1]
 *     1       1  header sum
 *             -------------------------------------
 *     2       5  method ID                        ^
 *     7       4  skip size     [*2]               |
 *    11       4  original size                    |
 *    15       2  time                             |
 *    17       2  date                             |
 *    19       1  attribute (0x20 fixed)           | [*1] header size (X+Y+25)
 *    20       1  level (0x01 fixed)               |
 *    21       1  name length                      |
 *    22       X  filename                         |
 * X+ 22       2  file crc (CRC-16)                |
 * X+ 24       1  OS ID                            |
 * X +25       Y  ???                              |
 * X+Y+25      2  next-header size                 v
 * -------------------------------------------------
 * X+Y+27      Z  ext-header                       ^
 *                 :                               |
 * -----------------------------------             | [*2] skip size
 * X+Y+Z+27       data                             |
 *                 :                               v
 * -------------------------------------------------
 *
 */
unsigned int
LH5HeaderParse(unsigned char *Buffer, int BufferSize,
	       unsigned int *original_size, unsigned int *packed_size,
	       char **name, unsigned short *crc)
{
	unsigned int offset;
	unsigned char header_size, checksum, name_length;

	if (BufferSize < 27) {
		fprintf(stderr,
			"Error: Packed Buffer is too small to contain an lha header.\n");
		return 0;
	}

	/* check attribute */
	if (Buffer[19] != 0x20) {
		fprintf(stderr, "Error: Invalid lha header attribute byte.\n");
		return 0;
	}

	/* check method */
	if (memcmp(Buffer + 2, "-lh5-", 5) != 0) {
		fprintf(stderr, "Error: Compression method is not LZHUFF5.\n");
		return 0;
	}

	/* check header level */
	if (Buffer[20] != 1) {
		fprintf(stderr, "Error: Header level %d is not supported\n",
			Buffer[20]);
		return 0;
	}

	/* read in the full header */
	header_size = Buffer[0];
	if (BufferSize < (header_size + 2)) {
		fprintf(stderr,
			"Error: Packed Buffer is too small to contain the full header.\n");
		return 0;
	}

	/* verify checksum */
	checksum = Buffer[1];
	if (calc_sum(Buffer + 2, header_size) != checksum) {
		fprintf(stderr, "Error: Invalid lha header checksum.\n");
		return 0;
	}

	*packed_size = le32toh(*(unsigned int *)(Buffer + 7));
	*original_size = le32toh(*(unsigned int *)(Buffer + 11));

	name_length = Buffer[21];
	*name = strndup((char *)Buffer + 22, name_length);

	*crc = le16toh(*(unsigned short *)(Buffer + 22 + name_length));

	offset = header_size + 2;
	/* Skip extended headers */
	while (1) {
		unsigned short extend_size =
		    le16toh(*(unsigned short *)(Buffer + offset - 2));

		if (!extend_size)
			break;

		*packed_size -= extend_size;
		offset += extend_size;

		if (BufferSize < offset) {
			fprintf(stderr,
				"Error: Buffer to small to contain extended header.\n");
			return 0;
		}
	}

	return offset;
}

/*
 * CRC Calculation.
 */
#define CRCPOLY 0xA001		/* CRC-16 (x^16+x^15+x^2+1) */

unsigned short CRC16Calculate(unsigned char *Buffer, int BufferSize)
{
	unsigned short CRCTable[0x100];
	unsigned short crc;
	int i;

	/* First, initialise our CRCTable */
	for (i = 0; i < 0x100; i++) {
		unsigned short r = i;
		unsigned int j;

		for (j = 0; j < 8; j++) {
			if (r & 1)
				r = (r >> 1) ^ CRCPOLY;
			else
				r >>= 1;
		}
		CRCTable[i] = r;
	}

	/* now go over the entire Buffer */
	crc = 0;
	for (i = 0; i < BufferSize; i++)
		crc = CRCTable[(crc ^ Buffer[i]) & 0xFF] ^ (crc >> 8);

	return crc;
}

/*
 * Bit handling code.
 */
static unsigned char *CompressedBuffer;
static int CompressedSize;
static int CompressedOffset;

static unsigned short bitbuf;
static unsigned char subbitbuf, bitcount;

static void BitBufInit(unsigned char *Buffer, int BufferSize)
{
	CompressedBuffer = Buffer;
	CompressedOffset = 0;
	CompressedSize = BufferSize;

	bitbuf = 0;
	subbitbuf = 0;
	bitcount = 0;
}

static void fillbuf(unsigned char n)
{				/* Shift bitbuf n bits left, read n bits */
	while (n > bitcount) {
		n -= bitcount;
		bitbuf = (bitbuf << bitcount) + (subbitbuf >> (8 - bitcount));

		if (CompressedOffset < CompressedSize) {
			subbitbuf = CompressedBuffer[CompressedOffset];
			CompressedOffset++;
		} else
			subbitbuf = 0;

		bitcount = 8;
	}
	bitcount -= n;
	bitbuf = (bitbuf << n) + (subbitbuf >> (8 - n));
	subbitbuf <<= n;
}

static unsigned short getbits(unsigned char n)
{
	unsigned short x;

	x = bitbuf >> (16 - n);
	fillbuf(n);

	return x;
}

static unsigned short peekbits(unsigned char n)
{
	unsigned short x;

	x = bitbuf >> (16 - n);

	return x;
}

/*
 *
 * LHA extraction.
 *
 */
#define MIN(a,b) ((a) <= (b) ? (a) : (b))

#define LZHUFF5_DICBIT		13	/* 2^13 =  8KB sliding dictionary */
#define MAXMATCH			256	/* formerly F (not more than 255 + 1) */
#define THRESHOLD			3	/* choose optimal value */
#define NP					(LZHUFF5_DICBIT + 1)
#define NT					(16 + 3)	/* USHORT + THRESHOLD */
#define NC					(255 + MAXMATCH + 2 - THRESHOLD)

#define PBIT 4			/* smallest integer such that (1 << PBIT) > * NP */
#define TBIT 5			/* smallest integer such that (1 << TBIT) > * NT */
#define CBIT 9			/* smallest integer such that (1 << CBIT) > * NC */

/* #if NT > NP #define NPT NT #else #define NPT NP #endif  */
#define NPT         0x80

static unsigned short left[2 * NC - 1], right[2 * NC - 1];

static unsigned short c_table[4096];	/* decode */
static unsigned short pt_table[256];	/* decode */

static unsigned char c_len[NC];
static unsigned char pt_len[NPT];

static int
make_table(short nchar, unsigned char bitlen[], short tablebits,
	   unsigned short table[])
{
	unsigned short count[17];	/* count of bitlen */
	unsigned short weight[17];	/* 0x10000ul >> bitlen */
	unsigned short start[17];	/* first code of bitlen */
	unsigned short total;
	unsigned int i, l;
	int j, k, m, n, avail;
	unsigned short *p;

	avail = nchar;

	/* initialize */
	for (i = 1; i <= 16; i++) {
		count[i] = 0;
		weight[i] = 1 << (16 - i);
	}

	/* count */
	for (i = 0; i < nchar; i++) {
		if (bitlen[i] > 16) {
			/* CVE-2006-4335 */
			fprintf(stderr, "Error: Bad table (case a)\n");
			return -1;
		} else
			count[bitlen[i]]++;
	}

	/* calculate first code */
	total = 0;
	for (i = 1; i <= 16; i++) {
		start[i] = total;
		total += weight[i] * count[i];
	}
	if (((total & 0xffff) != 0) || (tablebits > 16)) {	/* 16 for weight below */
		fprintf(stderr, "Error: make_table(): Bad table (case b)\n");
		return -1;
	}

	/* shift data for make table. */
	m = 16 - tablebits;
	for (i = 1; i <= tablebits; i++) {
		start[i] >>= m;
		weight[i] >>= m;
	}

	/* initialize */
	j = start[tablebits + 1] >> m;
	k = MIN(1 << tablebits, 4096);
	if (j != 0)
		for (i = j; i < k; i++)
			table[i] = 0;

	/* create table and tree */
	for (j = 0; j < nchar; j++) {
		k = bitlen[j];
		if (k == 0)
			continue;
		l = start[k] + weight[k];
		if (k <= tablebits) {
			/* code in table */
			l = MIN(l, 4096);
			for (i = start[k]; i < l; i++)
				table[i] = j;
		} else {
			/* code not in table */
			i = start[k];
			if ((i >> m) > 4096) {
				/* CVE-2006-4337 */
				fprintf(stderr, "Error: Bad table (case c)");
				exit(1);
			}
			p = &table[i >> m];
			i <<= tablebits;
			n = k - tablebits;
			/* make tree (n length) */
			while (--n >= 0) {
				if (*p == 0) {
					right[avail] = left[avail] = 0;
					*p = avail++;
				}
				if (i & 0x8000)
					p = &right[*p];
				else
					p = &left[*p];
				i <<= 1;
			}
			*p = j;
		}
		start[k] = l;
	}
	return 0;
}

static int read_pt_len(short nn, short nbit, short i_special)
{
	int i, c, n;

	n = getbits(nbit);
	if (n == 0) {
		c = getbits(nbit);
		for (i = 0; i < nn; i++)
			pt_len[i] = 0;
		for (i = 0; i < 256; i++)
			pt_table[i] = c;
	} else {
		i = 0;
		while (i < MIN(n, NPT)) {
			c = peekbits(3);
			if (c != 7)
				fillbuf(3);
			else {
				unsigned short mask = 1 << (16 - 4);
				while (mask & bitbuf) {
					mask >>= 1;
					c++;
				}
				fillbuf(c - 3);
			}

			pt_len[i++] = c;
			if (i == i_special) {
				c = getbits(2);
				while (--c >= 0 && i < NPT)
					pt_len[i++] = 0;
			}
		}
		while (i < nn)
			pt_len[i++] = 0;

		if (make_table(nn, pt_len, 8, pt_table) == -1)
			return -1;
	}
	return 0;
}

static int read_c_len(void)
{
	short i, c, n;

	n = getbits(CBIT);
	if (n == 0) {
		c = getbits(CBIT);
		for (i = 0; i < NC; i++)
			c_len[i] = 0;
		for (i = 0; i < 4096; i++)
			c_table[i] = c;
	} else {
		i = 0;
		while (i < MIN(n, NC)) {
			c = pt_table[peekbits(8)];
			if (c >= NT) {
				unsigned short mask = 1 << (16 - 9);
				do {
					if (bitbuf & mask)
						c = right[c];
					else
						c = left[c];
					mask >>= 1;
				} while (c >= NT && (mask || c != left[c]));	/* CVE-2006-4338 */
			}
			fillbuf(pt_len[c]);
			if (c <= 2) {
				if (c == 0)
					c = 1;
				else if (c == 1)
					c = getbits(4) + 3;
				else
					c = getbits(CBIT) + 20;
				while (--c >= 0)
					c_len[i++] = 0;
			} else
				c_len[i++] = c - 2;
		}
		while (i < NC)
			c_len[i++] = 0;

		if (make_table(NC, c_len, 12, c_table) == -1)
			return -1;
	}
	return 0;
}

static unsigned short decode_c_st1(void)
{
	unsigned short j, mask;

	j = c_table[peekbits(12)];
	if (j < NC)
		fillbuf(c_len[j]);
	else {
		fillbuf(12);
		mask = 1 << (16 - 1);
		do {
			if (bitbuf & mask)
				j = right[j];
			else
				j = left[j];
			mask >>= 1;
		} while (j >= NC && (mask || j != left[j]));	/* CVE-2006-4338 */
		fillbuf(c_len[j] - 12);
	}
	return j;
}

static unsigned short decode_p_st1(void)
{
	unsigned short j, mask;

	j = pt_table[peekbits(8)];
	if (j < NP)
		fillbuf(pt_len[j]);
	else {
		fillbuf(8);
		mask = 1 << (16 - 1);
		do {
			if (bitbuf & mask)
				j = right[j];
			else
				j = left[j];
			mask >>= 1;
		} while (j >= NP && (mask || j != left[j]));	/* CVE-2006-4338 */
		fillbuf(pt_len[j] - 8);
	}
	if (j != 0)
		j = (1 << (j - 1)) + getbits(j - 1);
	return j;
}

int
LH5Decode(unsigned char *PackedBuffer, int PackedBufferSize,
	  unsigned char *OutputBuffer, int OutputBufferSize)
{
	unsigned short blocksize = 0;
	unsigned int i, c;
	int n = 0;

	BitBufInit(PackedBuffer, PackedBufferSize);
	fillbuf(2 * 8);

	while (n < OutputBufferSize) {
		if (blocksize == 0) {
			blocksize = getbits(16);

			if (read_pt_len(NT, TBIT, 3) == -1)
				return -1;
			if (read_c_len() == -1)
				return -1;
			if (read_pt_len(NP, PBIT, -1) == -1)
				return -1;
		}
		blocksize--;
		c = decode_c_st1();

		if (c < 256)
			OutputBuffer[n++] = c;
		else {
			int length = c - 256 + THRESHOLD;
			int offset = 1 + decode_p_st1();

			if (offset > n)
				return -1;

			for (i = 0; i < length; i++) {
				OutputBuffer[n] = OutputBuffer[n - offset];
				n++;
			}
		}
	}
	return 0;
}
