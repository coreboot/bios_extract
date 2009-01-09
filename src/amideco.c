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

#include "kernel.h"

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
    int V95;
    char *Name;
};

static struct ModuleName
ModuleNames[] = {
    {0x00, TRUE, "POST"},
    {0x01, FALSE, "Setup Server"},
    {0x02, FALSE, "RunTime"},
    {0x03, FALSE, "DIM"},
    {0x04, FALSE, "Setup Client"},
    {0x05, FALSE, "Remote Server"},
    {0x06, FALSE, "DMI Data"},
    {0x07, FALSE, "Green PC"},
    {0x08, FALSE, "Interface"},
    {0x09, FALSE, "MP"},
    {0x0A, FALSE, "Notebook"},
    {0x0B, FALSE, "Int-10"},
    {0x0C, FALSE, "ROM-ID"},
    {0x0D, FALSE, "Int-13"},
    {0x0E, FALSE, "OEM Logo"},
    {0x0F, FALSE, "ACPI Table"},
    {0x10, FALSE, "ACPI AML"},
    {0x11, FALSE, "P6 Microcode"},
    {0x12, FALSE, "Configuration"},
    {0x13, FALSE, "DMI Code"},
    {0x14, FALSE, "System Health"},
    {0x15, FALSE, "Memory Sizing"},
    {0x16, TRUE,  "Memory Test"},
    {0x17, TRUE,  "Debug"},
    {0x18, TRUE,  "ADM (Display MGR)"},
    {0x19, TRUE,  "ADM Font"},
    {0x1A, TRUE,  "Small Logo"},
    {0x1B, TRUE,  "SLAB"},
    {0x20, FALSE, "PCI AddOn ROM"},
    {0x21, FALSE, "Multilanguage"},
    {0x22, FALSE, "UserDefined"},
    {0x23, TRUE,  "ASCII Font"},
    {0x24, TRUE,  "BIG5 Font"},
    {0x25, TRUE,  "OEM Logo"},
    {0x2A, TRUE,  "User ROM"},
    {0x2B, TRUE,  "PXE Code"},
    {0x2C, TRUE,  "AMI Font"},
    {0x2E, TRUE,  "User ROM"},
    {0x2D, TRUE,  "Battery Refresh"},
    {0x30, FALSE, "Font Database"},
    {0x31, FALSE, "OEM Logo Data"},
    {0x32, FALSE, "Graphic Logo Code"},
    {0x33, FALSE, "Graphic Logo Data"},
    {0x34, FALSE, "Action Logo Code"},
    {0x35, FALSE, "Action Logo Data"},
    {0x36, FALSE, "Virus"},
    {0x37, FALSE, "Online Menu"},
    {0x38, TRUE,  "Lang1 as ROM"},
    {0x39, TRUE,  "Lang2 as ROM"},
    {0x3A, TRUE,  "Lang3 as ROM"},
    {0x70, TRUE,  "OSD Bitmaps"},
    {0, FALSE, NULL}
};

static char *
ModuleNameGet(uint8_t ID, int V95)
{
    int i;

    for (i = 0; ModuleNames[i].Name; i++)
	if (ModuleNames[i].Id == ID) {
	    if (!V95 && ModuleNames[i].V95)
		return "";
	    else
		return ModuleNames[i].Name;
	}

    return "";
}

/*
 *
 */
static void
Xtract95(FILE *ptx, uint32_t ABCOffset)
{
    FILE *pto;
    char Buf[64];
    /* For the case of multiple 0x20 modules */
    uint8_t Multiple = 0;
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
		       Offset - BIOSOffset + 20, part->ROMSize, part->ExpSize);
	    else
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X)\n",
		       i, part->PartID, ModuleNameGet(part->PartID, TRUE),
		       Offset - BIOSOffset + 20, part->CSize);

	    break;
	case ACT_EXTRACT:
	    if (part->PartID == 0x20)
		sprintf(Buf, "amipci_%.2X.%.2X", Multiple++, part->PartID);
	    else
		sprintf(Buf, "amibody.%.2x", part->PartID);

	    pto = fopen(Buf, "wb");
	    if (!pto) {
		fprintf(stderr, "Error: unable to open %s: %s\n\n",
			Buf, strerror(errno));
		exit(1);
	    }

	    if (part->IsComprs != 0x80) {
		/* we should move to dumping .lha files */
		fseek(ptx, (Offset - BIOSOffset) + 0x14, SEEK_SET);
		printf("Decoding part %d to %s\n", i, Buf);
                decode(ptx, part->ROMSize, pto, part->ExpSize);
	    } else {
		printf("dumping part %d to %s\n", i, Buf);
		fwrite(BIOSImage + (Offset - BIOSOffset) + 0x0C, part->CSize, 1, pto);
	    }

	    fclose(pto);
	    break;
	}

	if ((part->PrePartHi == 0xFFFF) || (part->PrePartLo == 0xFFFF))
	    break;
	Offset = (part->PrePartHi << 4) + part->PrePartLo;
    }
}

struct BIOS94
{
    uint16_t PackLenLo;
    uint16_t PackLenHi;
    uint16_t RealLenLo;
    uint16_t RealLenHi;
};

/*
 *
 */
static void
Xtract0725(FILE *ptx, uint32_t Offset)
{
    struct BIOS94 *b94;
    FILE *pto;
    char Buf[12];
    int i;
    uint8_t Module = 0;

    printf("\n AMI94.");
    printf("\nStart\t\t: %X", Offset);

    for (i = 0; i < 0x80; i++) {
	b94 = (struct BIOS94 *) (BIOSImage + Offset);

	if (!b94->PackLenLo && !b94->RealLenLo)
	    break;

        switch (Action) {
	case ACT_LIST:
	    break;
	case ACT_EXTRACT: /* Xtracting Part */
	    sprintf(Buf, "amibody.%.2x", Module++);
	    pto = fopen(Buf, "wb");
	    /* check? */

	    fseek(ptx, Offset + 8, SEEK_SET); /* urgh */
	    decode(ptx, b94->PackLenLo, pto, b94->RealLenLo);
	    fclose(pto);
	    break;
	}

	Offset = Offset + b94->PackLenLo;
    }
    printf("\n\nThis Scheme Usually Contains: \n\t%s\n\t%s\n\t%s",
	   ModuleNames[0].Name, ModuleNames[1].Name, ModuleNames[2].Name);

    printf("\nTotal Sections\t: %i\n", i);
}

/*
 *
 */
static void
Xtract1010(FILE *ptx, uint32_t Offset)
{
    FILE *pto;
    uint16_t ModsInHead = 0;
    uint16_t GlobalMods = 0;
    struct BIOS94 ModHead;
    uint16_t Tmp;
    uint32_t i, ii;
    char Buf[12];
    uint8_t Module = 0;

    struct Mods94 {
	uint8_t PartID; /* ID for this header */
	uint8_t IsCompressed; /* 0x80 -> compressed */
	uint32_t RealCS; /* Real Address in RAM where to expand to */
    };

    struct Mods94 *Mods94;

    printf("\n AMI 10.");
    printf("\nStart\t\t: %X", Offset);

    fseek(ptx, 0x10, SEEK_SET);
    fread(&ModsInHead, 1, sizeof(uint16_t), ptx);
    GlobalMods = ModsInHead - 1;

    Mods94 = (struct Mods94 *) calloc(ModsInHead, sizeof(struct Mods94));

    for (i = 0; i < ModsInHead; i++) {
	fseek(ptx, 0x14 + i*4, SEEK_SET);
	fread(&Tmp, 1, sizeof(Tmp), ptx);

	if (i == 0)
	    Mods94[i].RealCS = 0x10000 + Tmp;
	else
	    Mods94[i].RealCS = Tmp;

	fread(&Tmp, 1, sizeof(Tmp), ptx);
	Mods94[i].PartID = Tmp & 0xFF;
	if (Tmp & 0x8000)
	    Mods94[i].IsCompressed = TRUE;
	else
	    Mods94[i].IsCompressed = FALSE;
    }


    switch (Action) {
    case ACT_LIST:
	printf("\n\nModules According to HeaderInfo: %i\n", ModsInHead);
	for (i = 1; i < ModsInHead; i++) {
	    fseek(ptx, Mods94[i].RealCS, SEEK_SET);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    if (Mods94[i].IsCompressed)
		printf("%.2X (%17.17s) %5.5X => %5.5X, %5.5Xh\n",
		       Mods94[i].PartID, ModuleNameGet(Mods94[i].PartID, FALSE),
		       ModHead.PackLenLo, ModHead.RealLenLo, Mods94[i].RealCS);
	    else
		printf("%.2X (%17.17s) %5.5X @ %5.5Xh\n",
		   Mods94[i].PartID, ModuleNameGet(Mods94[i].PartID, FALSE),
		   0x10000 - Mods94[i].RealCS, Mods94[i].RealCS);
	}

	Offset = 0x10000;
	while(Offset <= Mods94[0].RealCS) {
	    fseek(ptx, Offset, SEEK_SET);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    if(ModHead.RealLenHi && ModHead.PackLenHi) {
		Offset += 0x1000;
		if ((ModHead.RealLenHi == 0xFFFF) && (ModHead.PackLenHi == 0xFFFF) && ((Offset % 0x1000) != 0))
		    Offset = 0x1000 * ( Offset / 0x1000 );
	    } else {
		printf("\nNext Module (%i): %5.5X (%5.5u) => %5.5X (%5.5u),  %5.5Xh",
		       GlobalMods,
		       ModHead.PackLenLo,
		       ModHead.PackLenLo,
		       ModHead.RealLenLo,
		       ModHead.RealLenLo,
		       Offset);
		GlobalMods++;
		Offset += ModHead.PackLenLo;
	    }
	}

	break;
    case ACT_EXTRACT:
	for(i = 0; i < ModsInHead; i++) {
	    fseek(ptx, Mods94[i].RealCS, SEEK_SET);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    sprintf(Buf, "amibody.%.2x", Module++);
	    pto = fopen(Buf, "wb");
	    if (!pto) {
		printf("\nFile %s I/O Error..Exit", Buf);
		exit(1);
	    }

	    if (!Mods94[i].IsCompressed) {
		fseek(ptx, -8L, SEEK_CUR);
		for(ii = 0; ii < (0x10000 - (uint32_t) Mods94[i].RealCS); ii++) {
		    fread(&Buf[0], 1, 1, ptx);
		    fwrite(&Buf[0], 1, 1, pto);
		};
	    } else
		decode(ptx, ModHead.PackLenLo, pto, ModHead.RealLenLo);
	    fclose(pto);
	}

	/*------- Hidden Parts -----------*/

	Offset = 0x10000;
	while(Offset < Mods94[0].RealCS) {
	    fseek(ptx, Offset, SEEK_SET);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    if (ModHead.RealLenHi && ModHead.PackLenHi) {
		Offset += 0x1000;
		if ((ModHead.RealLenHi == 0xFFFF) && (ModHead.PackLenHi == 0xFFFF) && ((Offset % 0x1000) != 0)) {
		    Offset = 0x1000 * (Offset/0x1000);
		    continue;
		}
		continue;
	    } else {
		GlobalMods++;
		sprintf(Buf, "amibody.%.2x", Module++);

		pto = fopen(Buf,"wb");
		if(!pto) {
		    printf("\nFile %s I/O Error..Exit", Buf);
		    exit(1);
		}

		decode(ptx, ModHead.PackLenLo, pto, ModHead.RealLenLo);
		fclose(pto);

		Offset += ModHead.PackLenLo;
	    }
	}

	break;
    }

    free(Mods94);
    printf("\n\nThis Scheme Usually Doesn't Contain Modules Identification");

    printf("\nTotal Sections\t: %i\n", GlobalMods);
}

int
main(int argc, char *argv[])
{
    int fd;
    FILE *ptx;
    char Date[9];
    char *ABC;

    ArgumentsParse(argc, argv);

    ptx = fopen(FileName, "rb");
    if (!ptx) {
	fprintf(stderr, "Error: Failed to open %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

    /* get filesize, awkwardly */
    fseek(ptx, 0, SEEK_END);
    FileLength = ftell(ptx);
    BIOSOffset = 0x100000 - FileLength;
    rewind(ptx);

    fd = fileno(ptx);
    if (fd < 0) {
	fprintf(stderr, "Error: Failed to get the fileno for %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

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
	    Xtract1010(ptx, 0x30);
	else
	    Xtract0725(ptx, 0x10);
    } else
	Xtract95(ptx, ABC - BIOSImage);

    return 0;
}
