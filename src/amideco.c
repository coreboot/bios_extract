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
#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>
#include <errno.h>

/* for mkdir */
#include <sys/stat.h>
#include <sys/types.h>

#include "kernel.h"

#define BLOCK   0x8000

typedef struct {
    char AMIBIOSC[8];
    char Version[4];
    uint16_t CRCLen;
    uint32_t CRC32;
    uint16_t BeginLo;
    uint16_t BeginHi;
} ABCTag;

typedef struct
{
    /* When Previous Part Address is 0xFFFFFFFF, then this is the last part. */
    uint16_t PrePartLo; /* Previous part low word */
    uint16_t PrePartHi; /* Previous part high word */
    uint16_t CSize; /* Header length */
    uint8_t PartID; /* ID for this header */
    uint8_t IsComprs; /* 0x80 -> compressed */
    uint32_t RealCS; /* Real Address in RAM where to expand to */
    uint32_t ROMSize; /* Compressed Length */
    uint32_t ExpSize; /* Expanded Length */
} PARTTag;

typedef struct
{
    uint16_t    PackLenLo;
    uint16_t    PackLenHi;
    uint16_t    RealLenLo;
    uint16_t    RealLenHi;
} BIOS94;

#define VERSION "0.32"

#define ACT_NONE    0
#define ACT_EXTRACT 1
#define ACT_LIST    2
static int Action = 0;
static char *FileName = NULL;
static int FileLength = 0;
static uint32_t BIOSOffset = 0;

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

/*---------------------------------
        FindHook
----------------------------------*/

static uint32_t
FoundAt(FILE *ptx, char *Buf, char *Pattern, uint32_t BLOCK_LEN)
{
    uint32_t i, Ret;
    uint16_t Len;
    Len = strlen(Pattern);

    for (i = 0; i < BLOCK_LEN - 0x80; i++) {
	if(memcmp(Buf + i, Pattern, Len) == 0) {
	    Ret = ftell(ptx) - (BLOCK_LEN - i);
	    return(Ret);
	}
    }
    return 0;
}

/*
 *
 */
static void
Xtract95(FILE *ptx, uint32_t ABCOffset)
{
    FILE *pto;
    uint8_t PartTotal = 0;
    PARTTag part;
    char Buf[64];
    uint32_t i;
    /* For the case of multiple 0x20 modules */
    uint8_t Multiple = 0;
    int Compressed;
    uint32_t Offset;
    ABCTag abc;

    fseek(ptx, ABCOffset, SEEK_SET);
    fread(&abc, 1, sizeof(abc), ptx);

    printf("AMI95 Version\t: %.4s\n", abc.Version);
    printf("Packed Data\t: %X (%u bytes)\n", (uint32_t) abc.CRCLen * 8, (uint32_t) abc.CRCLen * 8);

    Offset = (abc.BeginHi << 4) + abc.BeginLo;
    printf("Modules offset\t: 0x%05X\n", Offset);

    printf("\nListing Modules:\n");

    while((part.PrePartLo != 0xFFFF || part.PrePartHi != 0xFFFF) && PartTotal < 0x80) {
	fseek(ptx, Offset - BIOSOffset, SEEK_SET);
	fread(&part, 1, sizeof(part), ptx);

	if (part.IsComprs == 0x80)
	    Compressed = FALSE;
	else
	    Compressed = TRUE;

	PartTotal++;

	switch (Action){
	case ACT_LIST:
	    if (Compressed)
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X -> 0x%05X)\n",
		       PartTotal, part.PartID, ModuleNameGet(part.PartID, TRUE),
		       Offset - BIOSOffset + 20, part.ROMSize, part.ExpSize);
	    else
		printf("  %02i: %02X (%17.17s) 0x%05X (0x%05X)\n",
		       PartTotal, part.PartID, ModuleNameGet(part.PartID, TRUE),
		       Offset - BIOSOffset + 20, part.CSize);

	    break;
	case ACT_EXTRACT:
	    if (part.PartID == 0x20)
		sprintf(Buf, "amipci_%.2X.%.2X", Multiple++, part.PartID);
	    else
		sprintf(Buf,"amibody.%.2x", part.PartID);

	    pto = fopen(Buf, "wb");
	    if (!pto) {
		fprintf(stderr, "File %s I/O error... Exit\n", Buf);
		exit(1);
	    }

	    if (part.IsComprs != 0x80)
                decode(ptx, part.ROMSize, pto, part.ExpSize);
	    else {
		fseek(ptx, -8L, SEEK_CUR);
		for (i = 0; i < part.CSize; i++) {
		    fread(&Buf[0], 1, 1, ptx);
		    fwrite(&Buf[0], 1, 1, pto);
		}
	    }
	    fclose(pto);
	    break;
	}

	Offset = ((uint32_t)part.PrePartHi << 4) + (uint32_t)part.PrePartLo;
    }
}

/*
 *
 */
static void
Xtract0725(FILE *ptx, uint32_t Offset)
{
    BIOS94 b94;
    FILE *pto;
    char Buf[12];
    uint8_t PartTotal = 0;
    uint8_t Module = 0;

    printf("\n AMI94.");
    printf("\nStart\t\t: %X", Offset);

    while((b94.PackLenLo != 0x0000 || b94.RealLenLo != 0x0000) && PartTotal < 0x80) {
	fseek(ptx, Offset, SEEK_SET);
	fread(&b94, 1, sizeof(b94), ptx);

	if(b94.PackLenLo==0x0000 && b94.RealLenLo==0x0000)
	    break;

	PartTotal++;

        switch (Action) {
	case ACT_LIST:
	    break;
	case ACT_EXTRACT: /* Xtracting Part */
	    sprintf(Buf,"amibody.%.2x", Module++);
	    pto = fopen(Buf, "wb");
	    decode(ptx, b94.PackLenLo, pto, b94.RealLenLo);
	    fclose(pto);
	    break;
	}

	Offset = Offset + b94.PackLenLo;

    }
    printf("\n\nThis Scheme Usually Contains: \n\t%s\n\t%s\n\t%s",
	   ModuleNames[0].Name, ModuleNames[1].Name, ModuleNames[2].Name);

    printf("\nTotal Sections\t: %i\n", PartTotal);
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
    PARTTag *Mods94;
    BIOS94 ModHead;
    uint16_t Tmp;
    uint32_t i, ii;
    char Buf[12];
    uint8_t Module = 0;

    printf("\n AMI 10.");
    printf("\nStart\t\t: %X", Offset);

    fseek(ptx, 0x10, SEEK_SET);
    fread(&ModsInHead, 1, sizeof(uint16_t), ptx);
    GlobalMods = ModsInHead - 1;

    Mods94 = (PARTTag*) calloc(ModsInHead, sizeof(PARTTag));

    for (i = 0; i < ModsInHead; i++) {
	fseek(ptx, 0x14 + i*4, SEEK_SET);
	fread(&Tmp, 1, sizeof(Tmp), ptx);

	if (i == 0)
	    Mods94[i].RealCS = 0x10000 + Tmp;
	else
	    Mods94[i].RealCS = Tmp;

	fread(&Tmp, 1, sizeof(Tmp), ptx);
	Mods94[i].PartID = Tmp & 0xFF;
	Mods94[i].IsComprs = (Tmp & 0x8000) >> 15;
    }


    switch (Action) {
    case ACT_LIST:
	printf("\n\nModules According to HeaderInfo: %i\n", ModsInHead);
	for (i = 1; i < ModsInHead; i++) {
	    fseek(ptx, Mods94[i].RealCS, SEEK_SET);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);
	    printf("\n%.2s %.2X (%17.17s) %5.5X (%5.5u) => %5.5X (%5.5u), %5.5Xh",
		   (Mods94[i].IsComprs == 0) ? ("+") : (" "),
		   Mods94[i].PartID, ModuleNameGet(Mods94[i].PartID, FALSE),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.PackLenLo),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.PackLenLo),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.RealLenLo),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.RealLenLo),
		   Mods94[i].RealCS);
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

	    if(Mods94[i].IsComprs == 1) {
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
    FILE *ptx;
    char Date[9];

    ArgumentsParse(argc, argv);

    ptx = fopen(FileName, "rb");
    if (!ptx) {
	fprintf(stderr, "Error: Failed to open %s: %s\n", FileName, strerror(errno));
	return 1;
    }

    /* get filesize, awkwardly */
    fseek(ptx, 0, SEEK_END);
    FileLength = ftell(ptx);
    BIOSOffset = 0x100000 - FileLength;
    rewind(ptx);

    /* Get Date */
    fseek(ptx, -11L, SEEK_END);
    fread(Date, 1, 8, ptx);
    Date[8] = 0;

    printf("File \"%s\" (%s) at 0x%08X (%ukB)\n",
	   FileName, Date, 0xFFF00000 + BIOSOffset, FileLength >> 10);

    /*
     * Look for AMI bios header.
     */
    {
	char *BufBlk = (char *) calloc(BLOCK, 1);
	int i = 0;

	if (!BufBlk)
	    exit(1);

	while (!feof(ptx)) {
	    uint32_t RealRead;
	    uint32_t Offset;

	    fseek(ptx, i, SEEK_SET);

	    RealRead = fread(BufBlk, 1, BLOCK, ptx);

	    Offset = FoundAt(ptx, BufBlk, "AMIBIOSC", RealRead);
	    if (Offset != 0) {
		printf("AMIBIOS 95 header at 0x%05X\n", Offset);
		Xtract95(ptx, Offset);
		return 0;
	    }

	    i = ftell(ptx) - 0x100;
	}

	free(BufBlk);

	{
	    char Buf[12];

	    printf("AMI'95 hook not found..Turning to AMI'94\n");

	    fseek(ptx, 0, SEEK_SET);
	    fread(&Buf, 1, 8, ptx);

	    if (memcmp(Buf, "AMIBIOSC", 8) != 0) {
		printf("Obviously not even AMIBIOS standard..Exit\n");
		return 0;
	    } else {
		struct {
		    char Month[2];
		    char rsrv1;
		    char Day[2];
		    char rsrv2;
		    char Year[2];
		} amidate;

		fread(&amidate, 1, sizeof(amidate), ptx);
		if (atoi(amidate.Day) == 10 && atoi(amidate.Month) == 10)
		    Xtract1010(ptx, 0x30);
		else
		    Xtract0725(ptx, 0x10);
		return 0;
	    }
	}
    }

    return 0;
}
