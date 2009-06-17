/*
 *      Phoenix BIOS Decompress
 *
 *      Copyright (C) 2000, 2002, 2003 Anthony Borisow
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

#include	<stdio.h>
#include	<stdlib.h>
#include	<memory.h>

#include	"phnxdeco.h"

#define	BLOCK		0x8000

#define	Xtract		0x10
#define	List	   	0x11

static unsigned int FoundAt(FILE * ptx, unsigned char *Buf, char *Pattern, unsigned int BLOCK_LEN)
{

    unsigned int i, Ret;
    unsigned short Len;
    Len = strlen(Pattern);
    for (i = 0; i < BLOCK_LEN - 0x80; i++) {
	if (memcmp(Buf + i, Pattern, Len) == 0) {
	    Ret = ftell(ptx) - (BLOCK_LEN - i);
	    return (Ret);
	}
    }
    return 0;
}
struct PhoenixModuleName
{
    char Id;
    char *Name;
};

static struct PhoenixModuleName
PhoenixModuleNames[] = {
    {'A', "ACPI"},
    {'B', "BIOSCODE"},
    {'C', "UPDATE"},
    {'D', "DISPLAY"},
    {'E', "SETUP"},
    {'F', "FONT"},
    {'G', "DECOMPCODE"},
    {'I', "BOOTBLOCK"},
    {'L', "LOGO"},
    {'M', "MISER"},
    {'N', "ROMPILOTLOAD"},
    {'O', "NETWORK"},
    {'P', "ROMPILOTINIT"},
    {'R', "OPROM"},
    {'S', "STRINGS"},
    {'T', "TEMPLATE"},
    {'U', "USER"},
    {'X', "ROMEXEC"},
    {'W', "WAV"},
    {'H', "TCPA_H"}, /* TCPA (Trusted Computing), USBKCLIB? */
    {'K', "TCPA_K"}, /* TCPA (Trusted Computing), "AUTH"? */
    {'Q', "TCPA_Q"}, /* TCPA (Trusted Computing), "SROM"? */
    {'<', "TCPA_<"},
    {'*', "TCPA_*"},
    {'?', "TCPA_?"},
    {'J', "SmartCardPAS"},
};

static char *
PhoenixModuleNameGet(char Id)
{
    int i;

    for (i = 0; PhoenixModuleNames[i].Name; i++)
	if (PhoenixModuleNames[i].Id == Id)
	    return PhoenixModuleNames[i].Name;

    return "";
}

#define COMPRESSION_NONE    0
#define COMPRESSION_UNKNOWN 1
#define COMPRESSION_LZARI   2
#define COMPRESSION_LZSS    3
#define COMPRESSION_LZHUF   4
#define COMPRESSION_LZINT   5

/*
 * This is nothing but the lzss algo, done rather horribly wrong.
 */
static void decodeM3(FILE *ptx, FILE *bmx, unsigned int RealLen)
{
    unsigned short Index, Index2, DX, Loop, i;
    unsigned char Buffer[0x1000], tmp;
    unsigned int Now;

    memset(Buffer, 0, 0x1000);

    DX = 0;
    Index = 0xFEE;
    Now = 0;

    for (;;) {
	DX >>= 1;
	if ((DX & 0x100) == 0) {
	    if (Now >= RealLen)
		return;
	    fread(&tmp, 1, 1, ptx);
	    DX = (unsigned short) (0xFF) * 0x100 + tmp;
	};

	if ((DX & 0x1) != 0) {
	    if (Now++ >= RealLen)
		return;

	    fread(&tmp, 1, 1, ptx);
	    fwrite(&tmp, 1, 1, bmx);
	    Buffer[Index++] = tmp;
	    Index &= 0xFFF;
	    continue;
	};

	fread(&Index2, 1, 1, ptx);
	Index2 &= 0xFF;

	fread(&tmp, 1, 1, ptx);

	Index2 += (tmp & 0xF0) << 4;
	Loop = (tmp & 0xf) + 3;

	for (i = 0; i < Loop; i++) {
	    tmp = Buffer[Index2++];
	    Index2 &= 0xFFF;
	    fwrite(&tmp, 1, 1, bmx);
	    Buffer[Index++] = tmp;
	    Index &= 0xFFF;
	}

	Now += Loop;
    }

}

typedef struct {
    unsigned int Prev;
    unsigned char Sig[3];
    unsigned char ID_HI;
    unsigned char ID_LO;
    unsigned char HeadLen;
    unsigned char isPacked;
    unsigned short Offset;
    unsigned short Segment;
    unsigned int ExpLen1;
    unsigned int Packed1;
    unsigned int Packed2;
    unsigned int ExpLen2;
} PHOENIXHEAD;

typedef struct {
    unsigned char Name[6];
    unsigned short Flags;
    unsigned short Len;
} PHNXID;

/*
 * Modified Module Detection & Manipulating
 * According to BCPSYS block
 */
static unsigned char TotalSecM(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int Start, unsigned int ConstOff, unsigned int SYSOff)
{
    FILE *pto;
    unsigned int i;
    unsigned int blkend = 0, blkstart = 0;
    PHOENIXHEAD phhead;
    unsigned char TotalSec = 0;
    unsigned int Offset = Start;

    phhead.Prev = 0xFFFF0000;
    fseek(ptx, 0, 2);

    if (Action == List)
	printf("\nC I  COMP      START     END    LENGTH   LINK TO   FILEOFFSET");


    while (phhead.Prev != 0x0 && !feof(ptx)) {
	fseek(ptx, Offset, 0);
	TotalSec++;
	Start = Offset;
	if (Start > 0x100000)
	    break;
	fread(&phhead, 1, sizeof(phhead), ptx);
	Offset = phhead.Prev - ConstOff;

	blkend = Start + ConstOff + sizeof(PHOENIXHEAD) - 5;
	blkstart = Start + 0xFFF00000;
	switch (Action) {
	case List:

	    printf("\n%c  %.1X   %1d    %8X  %8X  %5.X    %8X   %5.2Xh", phhead.ID_LO, phhead.ID_HI,
		   phhead.isPacked, blkstart, blkend + phhead.Packed1, phhead.Packed1, phhead.Prev, Start);


	    break;

	case Xtract:
	    {
		char *modulename = PhoenixModuleNameGet(phhead.ID_LO);
		char filename[64];

		snprintf(filename, 64, "%s%1.1X.rom", modulename, phhead.ID_HI);
		printf("\n%c %1.1X -> %s", phhead.ID_LO, phhead.ID_HI, filename);


		if ((pto = fopen(filename, "wb")) == NULL) {
		    printf("\nFile %s I/O error..Exit", filename);
		    exit(1);
		}
	    }

	    /* Evidently, isPacked == PackedLevel */
	    if (phhead.isPacked == 0x5) {
		interfacing interface;

		interface.infile = ptx;
		interface.outfile = pto;
		interface.original = phhead.ExpLen1;
		interface.packed = phhead.Packed1;

		decode(interface);
	    } else if (phhead.isPacked == 0x3) {
		fseek(ptx, -4L, 1);
		decodeM3(ptx, pto, phhead.ExpLen1);
	    } else {
		unsigned char tmp[1];

		fseek(ptx, -4L, 1);
		for (i = 0; i < phhead.ExpLen1; i++) {
		    fread(tmp, 1, 1, ptx);
		    fwrite(tmp, 1, 1, pto);
		};
	    }
	    fclose(pto);

	    break;
	}
    }

    return (TotalSec);
}

static unsigned int IsPhoenixBIOS(FILE * ptx, unsigned char * Buf)
{

    unsigned int CurPos = 0;
    char FirstPattern[] = "Phoenix FirstBIOS";
    char PhoenixPattern[] = "PhoenixBIOS 4.0";

    rewind(ptx);
    while (!feof(ptx)) {
	fread(Buf, 1, BLOCK, ptx);
	if ((CurPos = FoundAt(ptx, Buf, PhoenixPattern, BLOCK)) != 0)
	    break;
	/* O'K, we got PhoenixBIOS 4.0 hook */
	CurPos = ftell(ptx) - 0x100;
    }

    if (feof(ptx)) {
	rewind(ptx);
	while (!feof(ptx)) {
	    fread(Buf, 1, BLOCK, ptx);
	    if ((CurPos = FoundAt(ptx, Buf, FirstPattern, BLOCK)) != 0)
		break;
	    /* O'K, we got Phoenix FirstBIOS hook */
	    CurPos = ftell(ptx) - 0x100;
	}

	/* Neither PhoenixBIOS 4.0 nor FirstBIOS */
	if (feof(ptx))
	    return (0);

    }

    return (CurPos);
}

int main(int argc, char *argv[])
{
    FILE *ptx;
    unsigned char Buf[BLOCK];
    unsigned int CurPos, fLen, Start, Offset;
    unsigned int SYSOff;
    char BCPSEGMENT[] = "BCPSEGMENT";
    char phdate[18]; /* "XX/XX/XX\20XX:XX:XX\0" */
    unsigned char TotalSections = 0, Action, Mods = 0;
    PHNXID IDMod;

    memset(Buf, 0, BLOCK);

    if (argc != 3) {
	printf("wrong usage.\n");
	return 1;
    }

    if (!strcmp(argv[1], "x"))
	Action = Xtract;
    else if (!strcmp(argv[1], "l"))
	Action = List;
    else {
	printf("wrong usage.\n");
	return 1;
    }

    if ((ptx = fopen(argv[2], "rb")) == NULL) {
	printf("\nFATAL ERROR: File %s opening error...\n", argv[2]);
	return 0;
    };

    CurPos = 0;
    IDMod.Name[0] = 0xff;
    IDMod.Len = 0xff;
    SYSOff = 0;

    fseek(ptx, 0, 2);
    fLen = ftell(ptx);
    rewind(ptx);
    printf("Filelength\t: %X (%u bytes)\n", fLen, fLen);
    printf("Filename\t: %s\n", argv[2]);

    while (!feof(ptx)) {
	fread(Buf, 1, BLOCK, ptx);
	if ((CurPos = FoundAt(ptx, Buf, BCPSEGMENT, BLOCK)) != 0)
	    break;
	/* O'K, we got PhoenixBIOS BCPSEGMENT hook */
	CurPos = ftell(ptx) - 0x100;

    }

    if (feof(ptx)) {
	printf("Searched through file. Not a PhoenixBIOS\n");
	exit(1);
    }

    printf("PhoenixBIOS hook found at\t: %X\n", CurPos);
    CurPos += 10;
    fseek(ptx, (unsigned int) (CurPos), 0);

    while (IDMod.Name[0] != 0x0 && IDMod.Len != 0x0) {
	fread(&IDMod, 1, sizeof(IDMod), ptx);

	/* Wrong Count w/ ALR and some S/N internal errors ? */
	if (IDMod.Name[0] < 0x41 && IDMod.Name[0] != 0x0) {
	    do {
		fread(Buf, 1, 1, ptx);
	    } while (Buf[0] != 0x42);
	    fseek(ptx, -1L, SEEK_CUR);
	    CurPos = ftell(ptx);
	    fread(&IDMod, 1, sizeof(IDMod), ptx);
	};

	if (memcmp(IDMod.Name, "BCPSYS", 6) == 0)
	    SYSOff = CurPos;
	CurPos += IDMod.Len;
	fseek(ptx, (unsigned int) (CurPos), 0);
	if (IDMod.Name[0] == 0x0 || IDMod.Name[0] < 0x41)
	    break;
	else
	    Mods++;
    }

    printf("System Information at\t\t: %X\n", SYSOff);

    fseek(ptx, SYSOff + 15, 0);
    fread(&phdate, 1, 17, ptx);
    phdate[8] = ' ';
    phdate[17] = 0;

    fseek(ptx, SYSOff + 0x77, 0);
    /* Move to the pointer of 1st module */
    fread(&Start, 1, sizeof(Start), ptx);
    Offset = (0xFFFE0000 - (ftell(ptx) & 0xFFFE0000));
    Start -= Offset;

    /* Move to the DEVEL string */
    fseek(ptx, SYSOff + 0x37, 0);
    fread(Buf, 1, 8, ptx);

    printf("Version\t\t: %8.8s\n", Buf);
    printf("Start\t\t: %X\n", Start);
    printf("Offset\t\t: %X\n", 0xFFFF0000 - Offset);

    printf("BCP Modules\t: %i\n", Mods);

    printf("Released\t: %s\n", phdate);

    CurPos = IsPhoenixBIOS(ptx, Buf);
    fseek(ptx, (unsigned int) (CurPos), 0);

    fread(Buf, 1, 0x100, ptx);

    printf("\t%.64s\n", Buf);

    TotalSections = TotalSecM(ptx, Buf, Action, Start, Offset, SYSOff);

    printf("\n");
    printf("Total Sections: %u\n", TotalSections);

    fclose(ptx);

    printf("\n");
    return 0;
}
