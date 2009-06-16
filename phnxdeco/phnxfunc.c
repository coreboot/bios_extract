/**********************************************
**
**
**	Defines & Functions for PhnxDeco
**
**
***********************************************/

/* ------ 26 Jan 2003 ------- 
**	New Name Conventions
* ------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "phnxdeco.h"

unsigned int FoundAt(FILE * ptx, unsigned char *Buf, char *Pattern, unsigned int BLOCK_LEN)
{

    unsigned int i, Ret;
    word Len;
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
    word Index, Index2, DX, Loop, XorOp, i;
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
	    DX = (word) (0xFF) * 0x100 + tmp;
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
	Index = (word) tmp;
	if (Now >= RealLen) {
	    free(Buffer);
	    return;
	}
	fread(&tmp, 1, 1, ptx);
	Loop = (word) tmp & 0xf;
	Loop += 3;
	XorOp = (word) tmp & 0xf0;
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

unsigned char TotalSec(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int BankSize)
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
unsigned char TotalSecM(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int Start, unsigned int ConstOff, unsigned int SYSOff)
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


unsigned int IsPhoenixBIOS(FILE * ptx, unsigned char * Buf)
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
