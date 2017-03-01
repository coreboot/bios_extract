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

#ifndef LH5_EXTRACT_H
#define LH5_EXTRACT_H

unsigned int LH5HeaderParse(unsigned char *Buffer, int BufferSize,
			    unsigned int *original_size,
			    unsigned int *packed_size,
			    char **name, unsigned short *crc);

unsigned short CRC16Calculate(unsigned char *Buffer, int BufferSize);

int LH5Decode(unsigned char *PackedBuffer, int PackedBufferSize,
	      unsigned char *OutputBuffer, int OutputBufferSize);

#endif				/* LH5_EXTRACT_H */
