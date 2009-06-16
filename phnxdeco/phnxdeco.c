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
#define	XtractM		0x20
#define	ListM  		0x21

static unsigned char HelpSystem(int argc, char *argv[])
{
    unsigned char x = 0, retcode = 0;

    for (x = 1; x < argc; x++) {
	if (strcmp(argv[x], "-xs") == 0)
	    retcode = 0x20;
	if (strcmp(argv[x], "-ls") == 0)
	    retcode = 0x21;
	if (strcmp(argv[x], "-x") == 0)
	    retcode = 0x10;
	if (strcmp(argv[x], "-l") == 0)
	    retcode = 0x11;
    }
    return (retcode);

}

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

static char *GetModuleName(char ID)
{
    switch (ID) {
    case 'A':
	return ("ACPI");
    case 'B':
	return ("BIOSCODE");
    case 'C':
	return ("UPDATE");
    case 'D':
	return ("DISPLAY");
    case 'E':
	return ("SETUP");
    case 'F':
	return ("FONT");
    case 'G':
	return ("DECOMPCODE");
    case 'I':
	return ("BOOTBLOCK");
    case 'L':
	return ("LOGO");
    case 'M':
	return ("MISER");
    case 'N':
	return ("ROMPILOTLOAD");
    case 'O':
	return ("NETWORK");
    case 'P':
	return ("ROMPILOTINIT");
    case 'R':
	return ("OPROM");
    case 'S':
	return ("STRINGS");
    case 'T':
	return ("TEMPLATE");
    case 'U':
	return ("USER");
    case 'X':
	return ("ROMEXEC");
    case 'W':
	return ("WAV");

    case 'H':
	return ("TCPA_H");	/* TCPA (Trust Computing), USBKCLIB? */
    case 'K':
	return ("TCPA_K");	/* TCPA (Trust Computing), "AUTH"? */
    case 'Q':
	return ("TCPA_Q");	/* TCPA (Trust Computing), "SROM"? */
    case '<':
	return ("TCPA_<");
    case '*':
	return ("TCPA_*");
    case '?':
	return ("TCPA_?");

    case 'J':
	return ("SmartCardPAS");

    default:
	return ("User-Defined");
    }
}

static char *GetCompressionName(unsigned char ID)
{
    switch (ID) {
    case 0x0:
	return ("NONE");
    case 0x2:
	return ("LZARI");
    case 0x3:
	return ("LZSS");
    case 0x4:
	return ("LZHUF");
    case 0x5:
	return ("LZINT");
    default:
	return ("RSRV!");
    }
}

static void decodeM3(interfacing interface)
{
    FILE *ptx, *bmx;
    unsigned short Index, Index2, DX, Loop, XorOp, i;
    unsigned char *Buffer, tmp;
    unsigned int RealLen, Now;

    ptx = interface.infile;
    bmx = interface.outfile;
    RealLen = interface.original;

    Buffer = (unsigned char *) calloc(4096, 1);
    if (!Buffer)
	return;

    DX = 0;
    Index = 0xFEE;
    Now = 0;

    for (;;) {
	DX >>= 1;
	if ((DX & 0x100) == 0) {
	    if (Now >= RealLen) {
		free(Buffer);
		return;
	    }
	    fread(&tmp, 1, 1, ptx);
	    DX = (unsigned short) (0xFF) * 0x100 + tmp;
	};

	if ((DX & 0x1) != 0) {
	    if (Now++ >= RealLen) {
		free(Buffer);
		return;
	    }
	    fread(&tmp, 1, 1, ptx);
	    fwrite(&tmp, 1, 1, bmx);
	    Buffer[Index++] = tmp;
	    Index &= 0xFFF;
	    continue;
	};

	Index2 = Index;
	if (Now >= RealLen) {
	    free(Buffer);
	    return;
	}
	fread(&tmp, 1, 1, ptx);
	Index = (unsigned short) tmp;
	if (Now >= RealLen) {
	    free(Buffer);
	    return;
	}
	fread(&tmp, 1, 1, ptx);
	Loop = (unsigned short) tmp & 0xf;
	Loop += 3;
	XorOp = (unsigned short) tmp & 0xf0;
	XorOp <<= 4;
	Index |= XorOp;
	for (i = 0; i < Loop; i++) {
	    tmp = Buffer[Index++];
	    Index &= 0xFFF;
	    fwrite(&tmp, 1, 1, bmx);
	    Buffer[Index2++] = tmp;
	    Index2 &= 0xFFF;
	}

	Now += Loop;
	Index = Index2;
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

static unsigned char TotalSec(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int BankSize)
{
    FILE *pto;
    interfacing interface;
    char Buffer[12];

    unsigned int CurPos = 0;
    unsigned int RealRead = 0, i, TotalRead = 0, Tmp;
    char Head[] = { '\x31', '\x31', '\x0' };
    PHOENIXHEAD phhead;
    unsigned int End = 0xFFFFFFFF;
    unsigned char TotalSec = 0;


    switch (Action) {
    case List:

	printf("+------------------------------------------------------------------------------+\n"
	       "| Class.Instance (Name)        Packed --->  Expanded      Compression   Offset |\n"
	       "+------------------------------------------------------------------------------+\n");
    };

    fseek(ptx, 0, 2);
    End = ftell(ptx);
    rewind(ptx);
    while ((!feof(ptx)) && (CurPos < BankSize)) {
	fseek(ptx, CurPos, 0);
	RealRead = fread(Buf, 1, BLOCK, ptx);
	TotalRead += RealRead;
	if (RealRead != BLOCK) {
	    for (i = RealRead; i < BLOCK; i++)
		Buf[i] = 0xFF;
	}

	if (RealRead < 0x80)
	    break;
	if ((CurPos = FoundAt(ptx, Buf, Head, RealRead)) != 0) {
	    fseek(ptx, CurPos - 1 - 4, 0);
	    TotalSec++;
	    fread(&phhead, 1, sizeof(phhead), ptx);
	    if ((phhead.ID_HI >= 0x0 && phhead.ID_HI <= 0x20) && (phhead.ID_LO <= 0x60) && phhead.Sig[0] == '\x0') {
		switch (Action) {
		case List:


		    printf("\n   %c.%.2X (%12.12s)   %5.5X (%5.5u) => %5.5X (%6.3u)  %5.5s (%3.1i%%)  %5.2Xh", phhead.ID_LO, phhead.ID_HI,
			   GetModuleName(phhead.ID_LO), phhead.Packed1, phhead.Packed1, phhead.ExpLen1, phhead.ExpLen1, GetCompressionName(phhead.isPacked),
			   (unsigned int) 100 * (unsigned int) phhead.Packed1 / (unsigned int) phhead.ExpLen1, CurPos);
		    break;

		case Xtract:
		    if (phhead.isPacked == 0)
			sprintf(Buffer, "phoenix0.%C%.1X", phhead.ID_LO, phhead.ID_HI);
		    else
			sprintf(Buffer, "phoenix_.%C%.1X", phhead.ID_LO, phhead.ID_HI);

		    printf("%C.%.2X :: Saving %s ...", phhead.ID_LO, phhead.ID_HI, Buffer);
		    if ((pto = fopen(Buffer, "wb")) == NULL) {
			printf("\nFile %s I/O error..Exit", Buf);
			exit(1);
		    }

		    interface.infile = ptx;
		    interface.outfile = pto;
		    interface.original = phhead.ExpLen1;
		    interface.packed = phhead.Packed1;

		    interface.dicbit = 13;
		    interface.method = 5;

		    /* isPacked == PackedLevel == CompressionName */
		    if (phhead.isPacked == 0x5)
			decode(interface);
		    else if (phhead.isPacked == 0x3) {
			fseek(ptx, -4L, 1);
			decodeM3(interface);
		    } else {
			fseek(ptx, -4L, 1);
			for (i = 0; i < phhead.ExpLen1; i++) {
			    fread(&Buffer[0], 1, 1, ptx);
			    fwrite(&Buffer[0], 1, 1, pto);
			};
		    }
		    fclose(pto);

		    printf("Done\n");

		    break;
		}

	    } else {
		TotalSec--;
		fseek(ptx, CurPos + 1, 0);
		RealRead = fread(Buf, 1, BLOCK, ptx);
		if ((Tmp = FoundAt(ptx, Buf, Head, RealRead)) == 0)
		    CurPos = ftell(ptx) - 80;
		else
		    CurPos = Tmp;
		continue;
	    }
	    CurPos += phhead.Packed1;

	} else
	    CurPos = ftell(ptx);
    }

    return (TotalSec);
}


/*
 * Modified Module Detection & Manipulating
 * According to BCPSYS block
 */
static unsigned char TotalSecM(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int Start, unsigned int ConstOff, unsigned int SYSOff)
{
    FILE *pto;
    interfacing interface;
    char Buffer[12];

    unsigned int i;
    unsigned int blkend = 0, blkstart = 0;
    PHOENIXHEAD phhead;
    unsigned char TotalSec = 0;
    unsigned int Offset = Start;

    phhead.Prev = 0xFFFF0000;
    fseek(ptx, 0, 2);

    switch (Action) {
    case ListM:

	printf("\n================================== MODULE MAP =================================" "\nClass Code" "\n. Instance" "\n. ."
	       "\nC I    LEVEL      START       END     LENGTH  RATIO   LINK TO   FILEOFFSET"
	       "\n----   -----    ---------  ---------  ------  -----  ---------  ----------");
    };


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
	case ListM:

	    printf("\n%c  %.1X   %5.5s    %4.4X %4.4X  %4.4X %4.4X  %5.X    %3.1i%%  %4.4X %4.4X    %5.2Xh", phhead.ID_LO, phhead.ID_HI,
		   GetCompressionName(phhead.isPacked), blkstart >> 0x10, blkstart & 0xFFFF, (blkend + phhead.Packed1) >> 0x10,
		   (blkend + phhead.Packed1) & 0xFFFF, phhead.Packed1, (unsigned int) 100 * (unsigned int) phhead.Packed1 / (unsigned int) phhead.ExpLen1, phhead.Prev >> 0x10,
		   phhead.Prev & 0xFFFF, Start);


	    break;

	case XtractM:

	    printf("\n%c.%1.1X", phhead.ID_LO, phhead.ID_HI);
	    sprintf(Buffer, "%s", GetModuleName(phhead.ID_LO));

	    if (strlen(Buffer) > 7)
		sprintf(Buffer, "%7.7s%1.1X.rom", GetModuleName(phhead.ID_LO), phhead.ID_HI);
	    else {
		sprintf(Buffer, "%s%1.1X.rom", GetModuleName(phhead.ID_LO), phhead.ID_HI);
	    }

	    printf(" %-12.12s ... O'k", Buffer);


	    if ((pto = fopen(Buffer, "wb")) == NULL) {
		printf("\nFile %s I/O error..Exit", Buf);
		exit(1);
	    }


	    interface.infile = ptx;
	    interface.outfile = pto;
	    interface.original = phhead.ExpLen1;
	    interface.packed = phhead.Packed1;

	    interface.dicbit = 13;
	    interface.method = 5;

	    /* Evidently, isPacked == PackedLevel */
	    if (phhead.isPacked == 0x5)
		decode(interface);
	    else if (phhead.isPacked == 0x3) {
		fseek(ptx, -4L, 1);
		decodeM3(interface);
	    } else {
		fseek(ptx, -4L, 1);
		for (i = 0; i < phhead.ExpLen1; i++) {
		    fread(&Buffer[0], 1, 1, ptx);
		    fwrite(&Buffer[0], 1, 1, pto);
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
    unsigned char *Buf;
    unsigned int CurPos, fLen, Start, Offset;
    unsigned short FirstBLK, BBsz, BANKsz;
    unsigned int POSTOff, SYSOff, FCPOff, FirstBLKf;
    unsigned char phtime[8];
    char BCPSEGMENT[] = "BCPSEGMENT";
    char BCPFCP[] = "BCPFCP";
    char phdate[9]; /* XX/XX/XX\0 */
    unsigned char TotalSections = 0, Action, Mods = 0;
    PHNXID IDMod;

    switch (HelpSystem(argc, argv)) {
    case 0x10:
	Action = Xtract;
	break;
    case 0x11:
	Action = List;
	break;
    case 0x20:
	Action = XtractM;
	break;
    case 0x21:
	Action = ListM;
	break;
    default:
	printf("\n");
	return 0;
    }

    if ((ptx = fopen(argv[1], "rb")) == NULL) {
	printf("\nFATAL ERROR: File %s opening error...\n", argv[1]);
	return 0;
    };

    Buf = (unsigned char *) calloc(BLOCK, 1);
    if (!Buf) {
	printf("Memory Error..\n");
	return 0;
    }

    CurPos = 0;
    IDMod.Name[0] = 0xff;
    IDMod.Len = 0xff;
    POSTOff = 0;
    SYSOff = 0;

    fseek(ptx, 0, 2);
    fLen = ftell(ptx);
    rewind(ptx);
    printf("Filelength\t: %X (%u bytes)\n", fLen, fLen);
    printf("Filename\t: %s\n", argv[1]);


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

	if (memcmp(IDMod.Name, "BCPOST", 6) == 0)
	    POSTOff = CurPos;
	if (memcmp(IDMod.Name, "BCPSYS", 6) == 0)
	    SYSOff = CurPos;
	CurPos += IDMod.Len;
	fseek(ptx, (unsigned int) (CurPos), 0);
	if (IDMod.Name[0] == 0x0 || IDMod.Name[0] < 0x41)
	    break;
	else
	    Mods++;
    }

    /* Looking for BCPFCP control structure */
    rewind(ptx);
    while (!feof(ptx)) {
	fread(Buf, 1, BLOCK, ptx);
	if ((CurPos = FoundAt(ptx, Buf, BCPFCP, BLOCK)) != 0)
	    break;
	/* O'K, we got this hook */
	CurPos = ftell(ptx) - 0x100;

    }
    FCPOff = CurPos;

    printf("System Information at\t\t: %X\n", SYSOff);

    fseek(ptx, SYSOff + 0x7E, 0);
    fread(&BBsz, 1, sizeof(BBsz), ptx);

    fseek(ptx, SYSOff + 0x7B, 0);
    fread(&BANKsz, 1, sizeof(BANKsz), ptx);

    fseek(ptx, SYSOff + 15, 0);
    fread(&phdate, 1, 8, ptx);
    phdate[8] = 0;
    fread(Buf, 1, 1, ptx);
    fread(&phtime, 1, 8, ptx);

    fseek(ptx, SYSOff + 0x77, 0);
    /* Move to the pointer of 1st module */
    fread(&Start, 1, sizeof(Start), ptx);
    Offset = (0xFFFE0000 - (ftell(ptx) & 0xFFFE0000));
    Start -= Offset;

    /* Move to the DEVEL string */
    fseek(ptx, SYSOff + 0x37, 0);
    fread(Buf, 1, 8, ptx);

    printf("BootBlock\t: %X bytes\n", (BBsz == 0x0) ? (0x10000) : (BBsz));
    printf("BankSize\t: %i KB\n", BANKsz);
    printf("Version\t\t: %8.8s\n", Buf);
    printf("Start\t\t: %X\n", Start);
    printf("Offset\t\t: %X\n", 0xFFFF0000 - Offset);

    printf("BCP Modules\t: %i\n", Mods);
    printf("BCPFCP\t\t: %X\n", FCPOff);
    fseek(ptx, FCPOff + 0x18, 0);
    fread(&FirstBLK, 1, sizeof(FirstBLK), ptx);
    FirstBLKf = (ftell(ptx) & 0xF0000) + FirstBLK;
    printf("FCP 1st module\t: %X (%X)\n", FirstBLK, FirstBLKf);

    printf("Released\t: %s at %8.8s\n", phdate, phtime);

    printf("/* Copyrighted Information */\n");

    CurPos = IsPhoenixBIOS(ptx, Buf);
    fseek(ptx, (unsigned int) (CurPos), 0);

    fread(Buf, 1, 0x100, ptx);

    printf("\t%.64s\n", Buf);

    switch (Action) {
    case ListM:
    case XtractM:
	TotalSections = TotalSecM(ptx, Buf, Action, Start, Offset, SYSOff);
	break;
    case List:
    case Xtract:
	TotalSections = TotalSec(ptx, Buf, Action, (BANKsz) << 10);
	break;
    }

    printf("\n");
    printf("Total Sections: %u\n", TotalSections);

    free(Buf);
    fclose(ptx);

    printf("\n");
    return 0;
}
