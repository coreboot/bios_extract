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
 * Test/Example code for LH5 extraction.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "lh5_extract.h"

int main(int argc, char *argv[])
{
	char *filename;
	unsigned short header_crc;
	unsigned int header_size, original_size, packed_size;
	int infd, outfd;
	int LHABufferSize = 0;
	unsigned char *LHABuffer, *OutBuffer;

	if (argc != 2) {
		fprintf(stderr, "Error: archive file not specified\n");
		return 1;
	}

	/* open archive file */
	infd = open(argv[1], O_RDONLY);
	if (infd == -1) {
		fprintf(stderr, "Error: Failed to open \"%s\": %s\n", argv[1],
			strerror(errno));
		return 1;
	}

	LHABufferSize = lseek(infd, 0, SEEK_END);
	if (LHABufferSize < 0) {
		fprintf(stderr, "Error: Failed to lseek \"%s\": %s\n", argv[1],
			strerror(errno));
		return 1;
	}

	LHABuffer = mmap(NULL, LHABufferSize, PROT_READ, MAP_PRIVATE, infd, 0);
	if (LHABuffer == ((void *)-1)) {
		fprintf(stderr, "Error: Failed to mmap %s: %s\n", argv[1],
			strerror(errno));
		return 1;
	}

	header_size = LH5HeaderParse(LHABuffer, LHABufferSize, &original_size,
				     &packed_size, &filename, &header_crc);
	if (!header_size)
		return 1;

	if ((header_size + packed_size) < LHABufferSize) {
		fprintf(stderr, "Error: LHA archive is bigger than \"%s\".\n",
			argv[1]);
		return 1;
	}

	outfd = open(filename, O_RDWR | O_TRUNC | O_CREAT, S_IRWXU);
	if (outfd == -1) {
		fprintf(stderr, "Error: Failed to open \"%s\": %s\n", filename,
			strerror(errno));
		return 1;
	}

	/* grow file */
	if (lseek(outfd, original_size - 1, SEEK_SET) == -1) {
		fprintf(stderr, "Error: Failed to grow \"%s\": %s\n", filename,
			strerror(errno));
		return 1;
	}

	if (write(outfd, "", 1) != 1) {
		fprintf(stderr, "Error: Failed to write to \"%s\": %s\n",
			filename, strerror(errno));
		return 1;
	}

	OutBuffer =
	    mmap(NULL, original_size, PROT_READ | PROT_WRITE, MAP_SHARED, outfd,
		 0);
	if (OutBuffer == ((void *)-1)) {
		fprintf(stderr, "Error: Failed to mmap %s: %s\n", filename,
			strerror(errno));
		return 1;
	}

	LH5Decode(LHABuffer + header_size, packed_size, OutBuffer,
		  original_size);

	if (CRC16Calculate(OutBuffer, original_size) != header_crc) {
		fprintf(stderr, "Warning: invalid CRC on \"%s\"\n", filename);
		return 1;
	}

	if (munmap(OutBuffer, original_size))
		fprintf(stderr, "Warning: Failed to munmap \"%s\": %s\n",
			filename, strerror(errno));

	if (close(outfd))
		fprintf(stderr, "Warning: Failed to close \"%s\": %s\n",
			filename, strerror(errno));

	free(filename);

	/* get rid of our input file */
	if (munmap(LHABuffer, LHABufferSize))
		fprintf(stderr, "Warning: Failed to munmap \"%s\": %s\n",
			argv[1], strerror(errno));

	if (close(infd))
		fprintf(stderr, "Warning: Failed to close \"%s\": %s\n",
			argv[1], strerror(errno));

	return 0;
}
