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

#ifndef BIOS_EXTRACT_H
#define BIOS_EXTRACT_H

#define Bool int
#define FALSE 0
#define TRUE  1

/* bios_extract.c */
unsigned char *MMapOutputFile(char *filename, int size);

/* ami.c */
Bool AMI95Extract(unsigned char *BIOSImage, int BIOSLength, int BIOSOffset,
		  Bool Extract, uint32_t AMIBOffset, uint32_t ABCOffset);

#endif /* BIOS_EXTRACT_H */
