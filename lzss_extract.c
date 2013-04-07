/*
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "lzss_extract.h"

static inline int
LZSSBufferWrite(int fd, unsigned char *Buffer, int *BufferCount,
		unsigned char value)
{
	Buffer[*BufferCount] = value;
	*BufferCount += 1;

	if (*BufferCount == 0x1000) {
		if (write(fd, Buffer, 0x1000) != 0x1000) {
			fprintf(stderr, "Error writing to output file: %s",
				strerror(errno));
			return 1;
		}
		*BufferCount = 0;
	}

	return 0;
}

int LZSSExtract(unsigned char *Input, int InputSize, int fd)
{
	unsigned char Buffer[0x1000];
	unsigned short BitBuffer = 0;
	int i = 0, k, BitCount = 8, BufferCount = 0;

	while (i < InputSize) {

		if (BitCount == 8) {
			BitBuffer = Input[i];
			BitCount = -1;
		} else if ((BitBuffer >> BitCount) & 0x01) {
			if (LZSSBufferWrite(fd, Buffer, &BufferCount, Input[i]))
				return 1;
		} else if ((i + 1) < InputSize) {
			int offset =
			    ((Input[i] | ((Input[i + 1] & 0xF0) << 4)) -
			     0xFEE) & 0xFFF;
			int length = (Input[i + 1] & 0x0F) + 3;

			for (k = 0; k < length; k++) {
				if (LZSSBufferWrite
				    (fd, Buffer, &BufferCount,
				     Buffer[(offset + k) & 0xFFF]))
					return 1;
			}
			i++;
		} else {
			fprintf(stderr,
				"Error: requesting data beyond end of input file.\n");
			return 1;
		}

		i++;
		BitCount++;
	}

	if (BufferCount) {
		if (write(fd, Buffer, BufferCount) != BufferCount) {
			fprintf(stderr, "Error writing to output file: %s",
				strerror(errno));
			return 1;
		}
	}

	return 0;
}
