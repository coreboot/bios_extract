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

/*
 * This code is heavily based on Veit Kannegiesers <veit@kannegieser.net>
 * e_bcpvpd.pas program. This software comes with no license, but is freely
 * downloadable at http://kannegieser.net/veit/quelle/phoedeco_src.arj
 */
/*
 * It should be very straightfoward to add support for the $COMPIBM compressed
 * bios images as well. According to Veits code, the id-string is "$COMPIBM",
 * and the data starts straight after the (not null-terminated) id-string.
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

static inline int
BufferWrite(int fd, unsigned char *Buffer, int *BufferCount, unsigned char value)
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

static int
BCPVPDExtract(unsigned char *Input, int InputSize, int fd)
{
    unsigned char Buffer[0x1000];
    unsigned short BitBuffer = 0;
    int i = 0, k, BitCount = 8, BufferCount = 0;

    while (i < InputSize) {

	if (BitCount == 8) {
	    BitBuffer = Input[i];
	    BitCount = -1;
	} else if ((BitBuffer >> BitCount) & 0x01) {
	    if (BufferWrite(fd, Buffer, &BufferCount, Input[i]))
		return 1;
	} else if ((i + 1) < InputSize) {
	    int offset = ((Input[i] | ((Input[i + 1] & 0xF0) << 4)) - 0xFEE) & 0xFFF;
	    int length = (Input[i + 1] & 0x0F) + 3;

	    for (k = 0; k < length; k++) {
		if (BufferWrite(fd, Buffer, &BufferCount, Buffer[(offset + k) & 0xFFF]))
		    return 1;
	    }
	    i++;
	} else {
	    fprintf(stderr, "Error: requesting data beyond end of input file.\n");
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

int
main(int argc, char *argv[])
{
    int infd, outfd;
    unsigned char *InputBuffer;
    int InputBufferSize;

    if (argc != 3) {
	printf("usage: %s <input file> <output file>\n", argv[0]);
	return 1;
    }

    infd = open(argv[1], O_RDONLY);
    if (infd < 0) {
	fprintf(stderr, "Error: Failed to open %s: %s\n",
		argv[1], strerror(errno));
	return 1;
    }

    InputBufferSize = lseek(infd, 0, SEEK_END);
    if (InputBufferSize < 0) {
	fprintf(stderr, "Error: Failed to lseek \"%s\": %s\n",
		argv[1], strerror(errno));
	return 1;
    }

    InputBuffer = mmap(NULL, InputBufferSize, PROT_READ, MAP_PRIVATE, infd, 0);
    if (InputBuffer < 0) {
	fprintf(stderr, "Error: Failed to mmap %s: %s\n",
		argv[1], strerror(errno));
	return 1;
    }

    if (InputBufferSize < 0x52) {
	fprintf(stderr, "Error: \"%s\" is too small tp be a BCPVPD file.\n",
		argv[1]);
	return 1;
    }

    if (strncmp((char *) InputBuffer, "BCPVPD", 7)) {
	fprintf(stderr, "Error: unable to find BCPVPD header in \"%s\".\n",
		argv[1]);
	return 1;
    }

    outfd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, S_IRWXU);
    if (outfd  == -1) {
        fprintf(stderr, "Error: Failed to open \"%s\": %s\n",
                argv[2], strerror(errno));
        return 1;
    }

    return BCPVPDExtract(InputBuffer + 0x52, InputBufferSize - 0x52, outfd);
}
