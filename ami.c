/*
 * Decompression utility for AMI BIOSes.
 *
 * Copyright 2009      Luc Verhaegen <libv@skynet.be>
 * Copyright 2000-2006 Anthony Borisow
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
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "bios_extract.h"
#include "compat.h"
#include "lh5_extract.h"

struct AMI95ModuleName {
	uint8_t Id;
	char *Name;
};

static struct AMI95ModuleName AMI95ModuleNames[] = {
	{0x00, "POST"},
	{0x01, "Setup Server"},
	{0x02, "RunTime"},
	{0x03, "DIM"},
	{0x04, "Setup Client"},
	{0x05, "Remote Server"},
	{0x06, "DMI Data"},
	{0x07, "Green PC"},
	{0x08, "Interface"},
	{0x09, "MP"},
	{0x0A, "Notebook"},
	{0x0B, "Int-10"},
	{0x0C, "ROM-ID"},
	{0x0D, "Int-13"},
	{0x0E, "OEM Logo"},
	{0x0F, "ACPI Table"},
	{0x10, "ACPI AML"},
	{0x11, "P6 Microcode"},
	{0x12, "Configuration"},
	{0x13, "DMI Code"},
	{0x14, "System Health"},
	{0x15, "Memory Sizing"},
	{0x16, "Memory Test"},
	{0x17, "Debug"},
	{0x18, "ADM (Display MGR)"},
	{0x19, "ADM Font"},
	{0x1A, "Small Logo"},
	{0x1B, "SLAB"},
	{0x20, "PCI AddOn ROM"},
	{0x21, "Multilanguage"},
	{0x22, "UserDefined"},
	{0x23, "ASCII Font"},
	{0x24, "BIG5 Font"},
	{0x25, "OEM Logo"},
	{0x2A, "User ROM"},
	{0x2B, "PXE Code"},
	{0x2C, "AMI Font"},
	{0x2E, "User ROM"},
	{0x2D, "Battery Refresh"},
	{0x30, "Font Database"},
	{0x31, "OEM Logo Data"},
	{0x32, "Graphic Logo Code"},
	{0x33, "Graphic Logo Data"},
	{0x34, "Action Logo Code"},
	{0x35, "Action Logo Data"},
	{0x36, "Virus"},
	{0x37, "Online Menu"},
	{0x38, "Lang1 as ROM"},
	{0x39, "Lang2 as ROM"},
	{0x3A, "Lang3 as ROM"},
	{0x40, "AMD CIM-X NB binary"},
	{0x60, "AMD CIM-X SB binary"},
	{0x70, "OSD Bitmaps"},
	{0xf0, "Asrock Backup Util"},
	{0xf9, "Asrock AMD AHCI DLL"},
	{0xfa, "Asrock LOGO GIF"},
	{0xfb, "Asrock LOGO JPG"},
	{0xfc, "Asrock LOGO JPG"},
	{0xfd, "Asrock LOGO PCX - Instant boot"},
	{0, NULL}
};

static char *AMI95ModuleNameGet(uint8_t ID)
{
	int i;

	for (i = 0; AMI95ModuleNames[i].Name; i++)
		if (AMI95ModuleNames[i].Id == ID)
			return AMI95ModuleNames[i].Name;
	return NULL;
}

/*
 *
 */
Bool
AMI95Extract(unsigned char *BIOSImage, int BIOSLength, int BIOSOffset,
	     uint32_t AMIBOffset, uint32_t ABCOffset)
{
	Bool Compressed;
	uint32_t Offset;
	char Date[9];
	int i;

	struct abc {
		const char AMIBIOSC[8];
		const char Version[4];
		const uint16_t CRCLen;
		const uint32_t CRC32;
		const uint16_t BeginLo;
		const uint16_t BeginHi;
	} *abc;

	struct bigpart {
		const uint32_t CSize;
		const uint32_t Unknown;
	} *bigpart;

	struct part {
		/* When Previous Part Address is 0xFFFFFFFF, then this is the last part. */
		const uint16_t PrePartLo;	/* Previous part low word */
		const uint16_t PrePartHi;	/* Previous part high word */
		const uint16_t CSize;	/* Header length */
		const uint8_t PartID;	/* ID for this header */
		const uint8_t IsComprs;	/* 0x80 -> compressed */
		const uint32_t RealCS;	/* Old BIOSes:
					   Real Address in RAM where to expand to
					   Now:
					   Type 0x20 PCI ID of device for this ROM
					   Type 0x21 Language ID (ascii) */
		const uint32_t ROMSize;	/* Compressed Length */
		const uint32_t ExpSize;	/* Expanded Length */
	} *part;

	if (!ABCOffset) {
		if ((BIOSImage[8] == '1') && (BIOSImage[9] == '0') &&
		    (BIOSImage[11] == '1') && (BIOSImage[12] == '0'))
			fprintf(stderr,
				"Error: This is an AMI '94 (1010) BIOS Image.\n");
		else
			fprintf(stderr,
				"Error: This is an AMI '94 BIOS Image.\n");
		return FALSE;
	}

	/* now the individual modules */
	abc = (struct abc *)(BIOSImage + ABCOffset);

	/* Get Date */
	memcpy(Date, BIOSImage + BIOSLength - 11, 8);
	Date[8] = 0;

	printf("AMI95 Version\t: %.4s (%s)\n", abc->Version, Date);

	/* First, the boot rom */
	uint32_t BootOffset;
	int fd;

	BootOffset = AMIBOffset & 0xFFFF0000;

	printf("0x%05X (%6d bytes) -> amiboot.rom\n", BootOffset,
	       BIOSLength - BootOffset);

	fd = open("amiboot.rom", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Error: unable to open %s: %s\n\n",
			"amiboot.rom", strerror(errno));
		return FALSE;
	}

	write(fd, BIOSImage + BootOffset, BIOSLength - BootOffset);
	close(fd);

	/* now dump the individual modules */
	if (BIOSLength > 0x100000)
		Offset = (le16toh(abc->BeginHi) << 16) + le16toh(abc->BeginLo);
	else
		Offset = (le16toh(abc->BeginHi) << 4) + le16toh(abc->BeginLo);

	for (i = 0; i < 0x80; i++) {
		char filename[64], *ModuleName;
		unsigned char *Buffer;
		int BufferSize, ROMSize;

		part = (struct part *)(BIOSImage + (Offset - BIOSOffset));

		if (part->IsComprs & 0x80)
			Compressed = FALSE;
		else
			Compressed = TRUE;

		/* even they claim they are compressed they arent */
		if ((part->PartID == 0x40) || (part->PartID == 0x60))
			Compressed = FALSE;

		if (part->PartID == 0x20) {
			uint16_t vid = le32toh(part->RealCS) & 0xFFFF;
			uint16_t pid = le32toh(part->RealCS) >> 16;
			sprintf(filename, "amipci_%04X_%04X.rom", vid, pid);
		} else if (part->PartID == 0x21) {
			sprintf(filename, "amilang_%c%c.rom",
				(le32toh(part->RealCS) >> 8) & 0xFF,
				le32toh(part->RealCS) & 0xFF);
		} else
			sprintf(filename, "amibody_%02x.rom", part->PartID);

		if (Compressed) {
			ROMSize = le32toh(part->ROMSize);
			BufferSize = le32toh(part->ExpSize);
		} else {
			BufferSize = le16toh(part->CSize);
			if (!BufferSize || (BufferSize == 0xFFFF)) {
				bigpart =
				    (struct bigpart *)(BIOSImage +
						       (Offset - BIOSOffset) -
						       sizeof(struct bigpart));
				BufferSize = le32toh(bigpart->CSize);
			}
			ROMSize = BufferSize;
		}

		if (Compressed)
			printf("0x%05X (%6d bytes)", Offset - BIOSOffset + 0x14,
			       ROMSize);
		else
			printf("0x%05X (%6d bytes)", Offset - BIOSOffset + 0x0C,
			       ROMSize);

		printf(" -> %-20s", filename);

		if (Compressed)
			printf(" (%6d bytes)", BufferSize);
		else
			printf("               ");

		ModuleName = AMI95ModuleNameGet(part->PartID);
		if (ModuleName)
			printf("  \"%s\"\n", ModuleName);
		else
			printf("\n");

		Buffer = MMapOutputFile(filename, BufferSize);
		if (!Buffer)
			return FALSE;

		if (Compressed)
			LH5Decode(BIOSImage + (Offset - BIOSOffset) + 0x14,
				  ROMSize, Buffer, BufferSize);
		else
			memcpy(Buffer, BIOSImage + (Offset - BIOSOffset) + 0x0C,
			       BufferSize);

		munmap(Buffer, BufferSize);

		if ((le16toh(part->PrePartHi) == 0xFFFF)
		    || (le16toh(part->PrePartLo) == 0xFFFF))
			break;

		if (BIOSLength > 0x100000)
			Offset =
			    (le16toh(part->PrePartHi) << 16) +
			    le16toh(part->PrePartLo);
		else
			Offset =
			    (le16toh(part->PrePartHi) << 4) +
			    le16toh(part->PrePartLo);
	}

	return TRUE;
}
