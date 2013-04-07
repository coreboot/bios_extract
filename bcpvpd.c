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

#include "compat.h"
#include "lzss_extract.h"

int main(int argc, char *argv[])
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

	if (InputBufferSize < 0x52) {
		fprintf(stderr,
			"Error: \"%s\" is too small tp be a BCPVPD file.\n",
			argv[1]);
		return 1;
	}

	if (strncmp((char *)InputBuffer, "BCPVPD", 7)) {
		fprintf(stderr,
			"Error: unable to find BCPVPD header in \"%s\".\n",
			argv[1]);
		return 1;
	}

	outfd = open(argv[2], O_RDWR | O_TRUNC | O_CREAT, S_IRWXU);
	if (outfd == -1) {
		fprintf(stderr, "Error: Failed to open \"%s\": %s\n", argv[2],
			strerror(errno));
		return 1;
	}

	return LZSSExtract(InputBuffer + 0x52, InputBufferSize - 0x52, outfd);
}
