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

#define _GNU_SOURCE /* memmem is useful */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "lh5_extract.h"

#define Bool int
#define FALSE 0
#define TRUE  1

#define VERSION "0.32"

#define ACT_NONE    0
#define ACT_EXTRACT 1
#define ACT_LIST    2
static int Action = 0;
static char *FileName = NULL;
static int FileLength = 0;
static uint32_t BIOSOffset = 0;
static unsigned char *BIOSImage = NULL;

static void
HelpPrint(char *name)
{
    printf("%s version %s \n\n", name, VERSION);
    printf("Program to extract AMI Bios images (AMIBIOS '94 and '95).\n\n");
    printf("Usage: %s <action> <filename>\n", name);
    printf("Actions:\n");
    printf("\"l\"\tList Bios Structure.\n");
    printf("\"x\"\tExtract Bios Modules.\n");
    printf("\"h\"\tPrint usage information.\n");
}

static void
ArgumentsParse(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "h")) {
	    HelpPrint(argv[0]);
	    exit(0);
	} else if (!strcmp(argv[i], "x")) {
	    if (!Action)
		Action = ACT_EXTRACT;
	    else {
		fprintf(stderr, "Error: wrong argument (%s)."
			" Please provide only one action.\n", argv[i]);
		HelpPrint(argv[0]);
		exit(1);
	    }
	} else if (!strcmp(argv[i], "l")) {
	    if (!Action)
		Action = ACT_LIST;
	    else {
		fprintf(stderr, "Error: wrong argument (%s)."
			" Please provide only one action.\n", argv[i]);
		HelpPrint(argv[0]);
		exit(1);
	    }
	} else {
	    if (!FileName)
		FileName = argv[i];
	    else {
		fprintf(stderr, "Error: wrong argument (%s)."
			" Please provide only one filename.\n", argv[i]);
		HelpPrint(argv[0]);
		exit(1);
	    }
	}
    }

    if (!FileName) {
	fprintf(stderr, "Error: Please provide a filename.\n");
	HelpPrint(argv[0]);
	exit(1);
    }

    if (!Action) {
	HelpPrint(argv[0]);
	exit(0);
    }
}

struct ModuleName
{
    uint8_t Id;
    char *Name;
};

static struct ModuleName
ModuleNames[] = {
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
ModuleNameGet(uint8_t ID, int V95)
{
    int i;

    for (i = 0; ModuleNames[i].Name; i++)
	if (ModuleNames[i].Id == ID)
	    return ModuleNames[i].Name;

    return "";
}

/*
 *
 */
static Bool
AMIBIOS95(uint32_t AMIBOffset, uint32_t ABCOffset)
{
    int Compressed;
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
    if (Action == ACT_LIST)
	printf("AMIBOOT ROM at 0x%05X (0x%05X)\n",
	       AMIBOffset, FileLength - AMIBOffset);
    else {
	uint32_t RealOffset;
	int fd;

	RealOffset = AMIBOffset & 0xFFFF0000;

	fd = open("amiboot.rom", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
	    fprintf(stderr, "Error: unable to open %s: %s\n\n",
		    "amiboot.rom", strerror(errno));
	    exit(1);
	}

	printf("Dumping amiboot.rom.\n");

	write(fd, BIOSImage + RealOffset, FileLength - RealOffset);
	close(fd);
    }

    /* now the individual modules */
    printf("AMIBIOS 95 header at 0x%05X\n", ABCOffset);
    abc = (struct abc *) (BIOSImage + ABCOffset);

    /* Get Date */
    memcpy(Date, BIOSImage + FileLength - 11, 8);
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

	if (Action == ACT_LIST) {
	    if (Compressed)
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X -> 0x%05X)\n",
		       i, part->PartID, ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 0x14, part->ROMSize, part->ExpSize);
	    else
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X)\n",
		       i, part->PartID, ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 0x0C, part->CSize);
	} else {
	    char filename[64];
	    static uint8_t Multiple = 0; /* For the case of multiple 0x20 modules */
	    unsigned char *Buffer;
	    int BufferSize;
	    int fd;

	    if (part->PartID == 0x20)
		sprintf(filename, "amipci_%.2X.%.2X", Multiple++, part->PartID);
	    else
		sprintf(filename, "amibody.%.2x", part->PartID);

	    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	    if (fd < 0) {
		fprintf(stderr, "Error: unable to open %s: %s\n\n",
			filename, strerror(errno));
		exit(1);
	    }

	    printf("Dumping part %d to %s\n", i, filename);

	    if (Compressed)
		BufferSize = part->ExpSize;
	    else
		BufferSize = part->CSize;

	     /* grow file */
	    if (lseek(fd, BufferSize - 1, SEEK_SET) == -1) {
		fprintf(stderr, "Error: Failed to grow \"%s\": %s\n",
			filename, strerror(errno));
		return FALSE;
	    }

	    if (write(fd, "", 1) != 1) {
		fprintf(stderr, "Error: Failed to write to \"%s\": %s\n",
			filename, strerror(errno));
		return FALSE;
	    }

	    Buffer = mmap(NULL, BufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	    if (Buffer == ((void *) -1)) {
		fprintf(stderr, "Error: Failed to mmap %s: %s\n",
			filename, strerror(errno));
		return FALSE;
	    }

	    if (Compressed)
		LH5Decode(BIOSImage + (Offset - BIOSOffset) + 0x14,
			  part->ROMSize, Buffer, BufferSize);
	    else
		memcpy(Buffer, BIOSImage + (Offset - BIOSOffset) + 0x0C,
		       BufferSize);

	    munmap(Buffer, BufferSize);
	    close(fd);
	}

	if ((part->PrePartHi == 0xFFFF) || (part->PrePartLo == 0xFFFF))
	    break;
	Offset = (part->PrePartHi << 4) + part->PrePartLo;
    }

    return TRUE;
}

struct {
    char *String1;
    char *String2;
    Bool (*Handler) (uint32_t Offset1, uint32_t Offset2);
} BIOSIdentification[] = {
    {"AMIBOOT ROM", "AMIBIOSC", AMIBIOS95},
    {NULL, NULL, NULL},
};

/*
 *
 */
int
main(int argc, char *argv[])
{
    int fd;
    uint32_t Offset1, Offset2;
    int i, len;
    unsigned char *tmp;

    ArgumentsParse(argc, argv);

    fd = open(FileName, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "Error: Failed to open %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

    FileLength = lseek(fd, 0, SEEK_END);
    if (FileLength < 0) {
	fprintf(stderr, "Error: Failed to lseek \"%s\": %s\n",
		FileName, strerror(errno));
	return 1;
    }
    BIOSOffset = 0x100000 - FileLength;

    BIOSImage = mmap(NULL, FileLength, PROT_READ, MAP_PRIVATE, fd, 0);
    if (BIOSImage < 0) {
	fprintf(stderr, "Error: Failed to mmap %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

    printf("Using file \"%s\" (%ukB)\n", FileName, FileLength >> 10);

    for (i = 0; BIOSIdentification[i].Handler; i++) {
	len = strlen(BIOSIdentification[i].String1);
	tmp = memmem(BIOSImage, FileLength - len, BIOSIdentification[i].String1, len);
	if (!tmp)
	    continue;
	Offset1 = tmp - BIOSImage;

	len = strlen(BIOSIdentification[i].String2);
	tmp = memmem(BIOSImage, FileLength - len, BIOSIdentification[i].String2, len);
	if (!tmp)
	    continue;
	Offset2 = tmp - BIOSImage;

	if (BIOSIdentification[i].Handler(Offset1, Offset2))
	    return 0;
	else
	    return 1;
    }

    fprintf(stderr, "Error: Unable to detect BIOS Image type.\n");
    return 1;
}
