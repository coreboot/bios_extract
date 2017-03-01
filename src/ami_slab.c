/*
 * Copyright 2010      Michael Karcher <flashrom@mkarcher.dialup.fu-berlin.de>
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>

#include "compat.h"

#if !defined(le32toh) || !defined(le16toh)
#if BYTE_ORDER == LITTLE_ENDIAN
#define le32toh(x) (x)
#define le16toh(x) (x)
#else
#include <byteswap.h>
#define le32toh(x) bswap_32(x)
#define le16toh(x) bswap_16(x)
#endif
#endif

struct slabentry {
	uint32_t destaddr;
	uint32_t length_flag;
};

struct slabheader {
	uint16_t entries;
	uint16_t headersize;
	struct slabentry blocks[0];
};

struct nameentry {
	uint8_t segtype;
	uint16_t dtor_offset;
	char name[0];
};

int slabextract(const unsigned char *buffer, int bufferlen)
{
	const struct slabheader *h = (const void *)buffer;
	const unsigned char *listpointer;
	const unsigned char *datapointer;
	int i, count, headersize;

	headersize = le16toh(h->headersize);
	count = le16toh(h->entries);
	if ((headersize < ((count * 8) + 4)) || (bufferlen < headersize)) {
		fprintf(stderr,
			"Invalid file header - probably not a SLAB file\n");
		return 1;
	}
	printf("%d entries\n", count);

	/* FIXME: Is the 37 really constant? */
	if (((8 * count) + 37) < headersize) {
		listpointer = buffer + 8 * count + 37;
		printf("Name            Tp ");
	} else {
		listpointer = NULL;	/* No names present */
		printf("Name    ");
	}

	datapointer = buffer + le32toh(h->headersize);

	printf("LoadAddr     size initialized\n");

	for (i = 0; i < count; i++) {
		const struct slabentry *block;
		char filename[25];
		uint32_t len;
		int has_data;

		if (listpointer) {
			const struct nameentry *entry =
			    (const void *)listpointer;
			block =
			    (const void *)(buffer +
					   le16toh(entry->dtor_offset));
			sprintf(filename, "%.20s.bin", entry->name);
			listpointer += strlen(entry->name) + 4;
			printf("%-15s %02x ", entry->name, entry->segtype);
		} else {
			block = (const void *)(buffer + 4 + 8 * i);
			sprintf(filename, "block%02d.bin", i);
			printf("block%02d ", i);
		}

		len = le32toh(block->length_flag);
		if (len & 0x80000000)
			has_data = 1;
		else
			has_data = 0;
		len &= 0x7fffffff;

		printf("%08x %8d\t %s\n", le32toh(block->destaddr), len,
		       has_data ? "yes" : "no");

		if (has_data) {
			int outfd;

			if (datapointer + len > buffer + bufferlen) {
				fprintf(stderr,
					"Not enough data. File truncated?\n");
				return 1;
			}
			outfd =
			    open(filename, O_WRONLY | O_CREAT | O_TRUNC,
				 S_IRUSR | S_IWUSR);
			if (outfd != -1) {
				int ret = write(outfd, datapointer, len);
				if (ret == -1)
					fprintf(stderr, "Can't write %s: %s\n",
						filename, strerror(errno));
				else if (ret < len)
					fprintf(stderr,
						"Can't write %s completely: Disk full?\n",
						filename);
				close(outfd);
			} else
				fprintf(stderr,
					"Can't create output file %s: %s\n",
					filename, strerror(errno));
			datapointer += len;
		}
	}

	if (datapointer != buffer + bufferlen)
		fprintf(stderr, "Warning: Unexpected %d trailing bytes",
			(int)(buffer + bufferlen - datapointer));

	return 0;
}

int main(int argc, char *argv[])
{
	int infd;
	unsigned char *InputBuffer;
	int InputBufferSize;

	if (argc != 2) {
		printf("usage: %s <input file>\n", argv[0]);
		return 1;
	}

	infd = open(argv[1], O_RDONLY);
	if (infd < 0) {
		fprintf(stderr, "Error: Failed to open %s: %s\n", argv[1],
			strerror(errno));
		return 1;
	}

	InputBufferSize = lseek(infd, 0, SEEK_END);
	if (InputBufferSize < 0) {
		fprintf(stderr, "Error: Failed to lseek \"%s\": %s\n", argv[1],
			strerror(errno));
		return 1;
	}

	InputBuffer =
	    mmap(NULL, InputBufferSize, PROT_READ, MAP_PRIVATE, infd, 0);
	if (InputBuffer < 0) {
		fprintf(stderr, "Error: Failed to mmap %s: %s\n", argv[1],
			strerror(errno));
		return 1;
	}

	/* fixed header size - everything else is checked dynamically in slabextract */
	if (InputBufferSize < 4) {
		fprintf(stderr,
			"Error: \"%s\" is too small to be a SLAB file.\n",
			argv[1]);
		return 1;
	}

	return slabextract(InputBuffer, InputBufferSize);
}
