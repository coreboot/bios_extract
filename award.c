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

#define _GNU_SOURCE 1		/* for memmem */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>

#include "compat.h"
#include "bios_extract.h"
#include "lh5_extract.h"

/*
 *
 */
Bool
AwardExtract(unsigned char *BIOSImage, int BIOSLength, int BIOSOffset,
	     uint32_t Offset1, uint32_t BCPSegmentOffset)
{
	unsigned char *p, *Buffer;
	int HeaderSize;
	unsigned int BufferSize, PackedSize;
	char *filename;
	unsigned short crc;

	printf("Found Award BIOS.\n");

	p = BIOSImage;
	while (p) {
		p = memmem(p, BIOSLength - (p - BIOSImage), "-lh5-", 5);
		if (!p)
			break;
		p -= 2;
		HeaderSize = LH5HeaderParse(p, BIOSLength - (p - BIOSImage),
					    &BufferSize, &PackedSize, &filename,
					    &crc);
		if (!HeaderSize)
			return FALSE;

		printf("0x%05X (%6d bytes)    ->    %s  \t(%6d bytes)\n",
		       (unsigned int)(p - BIOSImage), HeaderSize + PackedSize,
		       filename, BufferSize);

		Buffer = MMapOutputFile(filename, BufferSize);
		if (!Buffer)
			return FALSE;

		LH5Decode(p + HeaderSize, PackedSize, Buffer, BufferSize);

		munmap(Buffer, BufferSize);

		p += HeaderSize + PackedSize;
	}

	return TRUE;
}
