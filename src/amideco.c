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

#define VERSION "0.32"

#define ACT_NONE    0
#define ACT_EXTRACT 1
#define ACT_LIST    2
static int Action = 0;
static char *FileName = NULL;
static int FileLength = 0;
static uint32_t BIOSOffset = 0;
static char *BIOSImage = NULL;

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
}

#define FALSE 0
#define TRUE  1

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
 * LHA level1 header.
 */
struct lhl1_header_pt1 {
    uint8_t header_size;
    uint8_t header_checksum;
    /* !!! string "-lh5-" is 6 bytes long, but lha header has only 5 places */
    char method_id[5];
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t timestamp;
    uint8_t nullx20;
    uint8_t level;
    uint8_t filename_length;
};
/* then the filename without '\0' */
struct lhl1_header_pt2 {
    uint16_t file_crc16;
    char os_id;
    uint16_t next_header_size;
};

static void
lhl1_header_write(int fd, char *name,
		  uint32_t compressed_size, uint32_t uncompressed_size)
{
    struct lhl1_header_pt1 head_pt1;
    struct lhl1_header_pt2 head_pt2;
    int checksum = 0, i;

    head_pt1.header_size = sizeof(struct lhl1_header_pt1) +
	sizeof(struct lhl1_header_pt2) + strlen(name) - 2;
    /* checksum is for later. */
    memcpy(head_pt1.method_id, "-lh5-", 5);
    head_pt1.compressed_size = compressed_size;
    head_pt1.uncompressed_size = uncompressed_size;
    head_pt1.timestamp = 0;
    head_pt1.nullx20 = 0x20;
    head_pt1.level = 1;
    head_pt1.filename_length = strlen(name);

    head_pt2.file_crc16 = 0;
    head_pt2.os_id = ' ';
    head_pt2.next_header_size = 0;

    for (i = 2; i < sizeof(struct lhl1_header_pt1); i++)
	checksum += ((uint8_t *) &head_pt1)[i];
    for (i = 0; i < strlen(name); i++)
	checksum += ((uint8_t *) name)[i];
    for (i = 0; i < sizeof(struct lhl1_header_pt2); i++)
	checksum += ((uint8_t *) &head_pt2)[i];

    head_pt1.header_checksum = checksum;

    write(fd, &head_pt1, sizeof(struct lhl1_header_pt1));
    write(fd, name, strlen(name));
    write(fd, &head_pt2, sizeof(struct lhl1_header_pt2));
}

/*
 *
 */
static void
Xtract95(uint32_t ABCOffset)
{
    int Compressed;
    uint32_t Offset;
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

    printf("AMIBIOS 95 header at 0x%05X\n", ABCOffset);
    abc = (struct abc *) (BIOSImage + ABCOffset);

    printf("AMI95 Version\t: %.4s\n", abc->Version);
    printf("Packed Data\t: %X (%u bytes)\n", (uint32_t) abc->CRCLen * 8, (uint32_t) abc->CRCLen * 8);

    Offset = (abc->BeginHi << 4) + abc->BeginLo;
    printf("Modules offset\t: 0x%05X\n", Offset);

    printf("\nListing Modules:\n");

    for (i = 0; i < 0x80; i++) {
	part = (struct part *) (BIOSImage + (Offset - BIOSOffset));

	if (part->IsComprs == 0x80)
	    Compressed = FALSE;
	else
	    Compressed = TRUE;

	switch (Action){
	case ACT_LIST:
	    if (Compressed)
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X -> 0x%05X)\n",
		       i, part->PartID, ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 0x14, part->ROMSize, part->ExpSize);
	    else
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X)\n",
		       i, part->PartID, ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 0x0C, part->CSize);

	    break;
	case ACT_EXTRACT:
	    {
		char *filename, Buf[64], Buflzh[64];
		static uint8_t Multiple = 0; /* For the case of multiple 0x20 modules */
		int fd;

		if (part->PartID == 0x20)
		    sprintf(Buf, "amipci_%.2X.%.2X", Multiple++, part->PartID);
		else
		    sprintf(Buf, "amibody.%.2x", part->PartID);

		if (Compressed) {
		    filename = Buflzh;
		    sprintf(filename, "%s.lzh", Buf);
		} else
		    filename = Buf;

		fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if (fd < 0) {
		    fprintf(stderr, "Error: unable to open %s: %s\n\n",
			    filename, strerror(errno));
		    exit(1);
		}

		printf("Dumping part %d to %s\n", i, filename);

		if (Compressed) {
		    lhl1_header_write(fd, Buf, part->ROMSize, part->ExpSize);
		    write(fd, BIOSImage + (Offset - BIOSOffset) + 0x14, part->ROMSize);
		} else
		    write(fd, BIOSImage + (Offset - BIOSOffset) + 0x0C, part->CSize);

		close(fd);
		break;
	    }
	}

	if ((part->PrePartHi == 0xFFFF) || (part->PrePartLo == 0xFFFF))
	    break;
	Offset = (part->PrePartHi << 4) + part->PrePartLo;
    }
}

int
main(int argc, char *argv[])
{
    int fd;
    char Date[9];
    char *ABC;

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
		argv[1], strerror(errno));
	return 1;
    }
    BIOSOffset = 0x100000 - FileLength;

    BIOSImage = mmap(NULL, FileLength, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!BIOSImage) {
	fprintf(stderr, "Error: Failed to mmap %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

    /* Get Date */
    memcpy(Date, BIOSImage + FileLength - 11, 8);
    Date[8] = 0;

    printf("File \"%s\" (%s) at 0x%08X (%ukB)\n",
	   FileName, Date, 0xFFF00000 + BIOSOffset, FileLength >> 10);

    /* Look for AMIBIOSC Header */
    ABC = memmem(BIOSImage, FileLength, "AMIBIOSC", 8);
    if (!ABC) {
	fprintf(stderr, "Error: Unable to find AMIBIOSC string.\n");
	return 1;
    }

    if (ABC == BIOSImage) {
	if ((BIOSImage[8] == '1') && (BIOSImage[9] == '0') &&
	    (BIOSImage[11] == '1') && (BIOSImage[12] == '0'))
	    fprintf(stderr, "Error: This is an AMI '94 (1010) BIOS Image.\n");
	else
	    fprintf(stderr, "Error: This is an AMI '94 BIOS Image.\n");
	return 1;
    }

    Xtract95(ABC - BIOSImage);

    return 0;
}
