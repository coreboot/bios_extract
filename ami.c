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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "bios_extract.h"
#include "lh5_extract.h"

struct AMI95ModuleName
{
    uint8_t Id;
    char *Name;
};

static struct AMI95ModuleName
AMI95ModuleNames[] = {
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
    {0x70, "OSD Bitmaps"},
    {0, NULL}
};

static char *
AMI95ModuleNameGet(uint8_t ID, int V95)
{
    int i;

    for (i = 0; AMI95ModuleNames[i].Name; i++)
	if (AMI95ModuleNames[i].Id == ID)
	    return AMI95ModuleNames[i].Name;

    return "";
}

/*
 *
 */
Bool
AMI95Extract(unsigned char *BIOSImage, int BIOSLength, int BIOSOffset, Bool Extract,
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

    struct part {
	/* When Previous Part Address is 0xFFFFFFFF, then this is the last part. */
	const uint16_t PrePartLo; /* Previous part low word */
	const uint16_t PrePartHi; /* Previous part high word */
	const uint16_t CSize; /* Header length */
	const uint8_t PartID; /* ID for this header */
	const uint8_t IsComprs; /* 0x80 -> compressed */
	const uint32_t RealCS; /* Real Address in RAM where to expand to */
	const uint32_t ROMSize; /* Compressed Length */
	const uint32_t ExpSize; /* Expanded Length */
    } *part;

    if (!ABCOffset) {
	if ((BIOSImage[8] == '1') && (BIOSImage[9] == '0') &&
	    (BIOSImage[11] == '1') && (BIOSImage[12] == '0'))
	    fprintf(stderr, "Error: This is an AMI '94 (1010) BIOS Image.\n");
	else
	    fprintf(stderr, "Error: This is an AMI '94 BIOS Image.\n");
	return FALSE;
    }

    /* First, the boot rom */
    if (!Extract)
	printf("AMIBOOT ROM at 0x%05X (0x%05X)\n",
	       AMIBOffset, BIOSLength - AMIBOffset);
    else {
	uint32_t RealOffset;
	int fd;

	RealOffset = AMIBOffset & 0xFFFF0000;

	fd = open("amiboot.rom", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
	    fprintf(stderr, "Error: unable to open %s: %s\n\n",
		    "amiboot.rom", strerror(errno));
	    return FALSE;
	}

	printf("Dumping amiboot.rom.\n");

	write(fd, BIOSImage + RealOffset, BIOSLength - RealOffset);
	close(fd);
    }

    /* now the individual modules */
    printf("AMIBIOS 95 header at 0x%05X\n", ABCOffset);
    abc = (struct abc *) (BIOSImage + ABCOffset);

    /* Get Date */
    memcpy(Date, BIOSImage + BIOSLength - 11, 8);
    Date[8] = 0;

    printf("AMI95 Version\t: %.4s (%s)\n", abc->Version, Date);
    printf("Packed Data\t: %X (%u bytes)\n",
	   (uint32_t) abc->CRCLen * 8, (uint32_t) abc->CRCLen * 8);

    Offset = (abc->BeginHi << 4) + abc->BeginLo;
    printf("Modules offset\t: 0x%05X\n", Offset);

    printf("\nListing Modules:\n");

    for (i = 0; i < 0x80; i++) {
	part = (struct part *) (BIOSImage + (Offset - BIOSOffset));

	if (part->IsComprs == 0x80)
	    Compressed = FALSE;
	else
	    Compressed = TRUE;

	if (!Extract) {
	    if (Compressed)
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X -> 0x%05X)\n",
		       i, part->PartID, AMI95ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 0x14, part->ROMSize, part->ExpSize);
	    else
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X)\n",
		       i, part->PartID, AMI95ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 0x0C, part->CSize);
	} else {
	    char filename[64];
	    static uint8_t Multiple = 0; /* For the case of multiple 0x20 modules */
	    unsigned char *Buffer;
	    int BufferSize;

	    if (part->PartID == 0x20)
		sprintf(filename, "amipci_%.2X.%.2X", Multiple++, part->PartID);
	    else
		sprintf(filename, "amibody.%.2x", part->PartID);

	    printf("Dumping part %d to %s\n", i, filename);

	    if (Compressed)
		BufferSize = part->ExpSize;
	    else
		BufferSize = part->CSize;

	    Buffer = MMapOutputFile(filename, BufferSize);
	    if (!Buffer)
		return FALSE;

	    if (Compressed)
		LH5Decode(BIOSImage + (Offset - BIOSOffset) + 0x14,
			  part->ROMSize, Buffer, BufferSize);
	    else
		memcpy(Buffer, BIOSImage + (Offset - BIOSOffset) + 0x0C,
		       BufferSize);

	    munmap(Buffer, BufferSize);
	}

	if ((part->PrePartHi == 0xFFFF) || (part->PrePartLo == 0xFFFF))
	    break;
	Offset = (part->PrePartHi << 4) + part->PrePartLo;
    }

    return TRUE;
}
