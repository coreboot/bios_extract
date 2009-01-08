/*
 *	AMIBIOS	Decompress
 *
 *      Copyright (C) 2000, 2002 Anthony Borisow
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

/********************************************
**
**	Last update for 0.31 - Sep 29, 2000
**
********************************************/

/****************************************/
/*	0.31 Added Buffered Search	*/
/****************************************/
/*	WARNING!	Under DJGPP	*/
/****************************************/

/*		Version 0.31		*/


#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>

/* for mkdir */
#include <sys/stat.h>
#include <sys/types.h>

#include "kernel.h"

#define Xtract  0x10
#define List    0x01
#define Delete  0x12
#define XtractD 0x13

#define BLOCK   0x8000

typedef struct {
    char AMIBIOSC[8];
    uint8_t    Version[4];
    uint16_t    CRCLen;
    uint32_t   CRC32;
    uint16_t    BeginLo;
    uint16_t    BeginHi;
} ABCTag;

typedef struct
{
    uint16_t    PrePartLo;      /*      Previous part LO uint16_t   */
    uint16_t    PrePartHi;      /*      Previous part HI uint16_t   */
    /*      value 0xFFFFFFFF = last */
    uint16_t    CSize;          /*      This module header Len  */
    uint8_t    PartID;         /*      ID for this header      */
    uint8_t    IsComprs;       /*      Value 0x80 means that
				    this module not
				    compressed, 0x00 - is   */
    uint32_t   RealCS;         /*      Address in RAM where
				    expand to               */
    uint32_t   ROMSize;        /*      Compressed Len          */
    uint32_t   ExpSize;        /*      Expanded Len            */
} PARTTag;

typedef struct
{
    uint16_t    PackLenLo;
    uint16_t    PackLenHi;
    uint16_t    RealLenLo;
    uint16_t    RealLenHi;
} BIOS94;

typedef struct
{
    char Month[2];
    char rsrv1;
    char Day[2];
    char rsrv2;
    char Year[2];
} AMIDATE;

#define SftName "AmiBIOSDeco"
#define SftEMail "Anton Borisov, anton.borisov@gmail.com"

/********SoftWare*************/
uint8_t CopyRights[] = "\n(C) Anton Borisov, 2000, 2002-2003, 2006. Portions (C) 1999-2000";
uint8_t Url[] = "Bug-reports direct to "SftEMail;

#define SftVersion "0.31e"

static uint8_t
HelpSystem(int argc, char *argv[])
{
    uint8_t x;
    uint8_t ID = 0;

    for (x = 1; x < argc; x++) {
	if (strcmp(argv[x], "-h") == 0) {
	    printf("\n"SftName" HelpSystem Starting Now!\n");
	    printf("\nThis Program Version Number %s",SftVersion);
	    printf(
		   "\n"SftName" - Decompressor for AmiBIOSes only.\n"
		   "\tSupported formats: AMIBIOS'94, AMIBIOS'95 or later\n\n"
		   ""SftName" performs on 386 or better CPU systems\n"
		   "under control of LinuxOS\n\n"
		   "Compression schemes include: LZINT\n\n"
		   "Modules marked with ""+"" sign are compressed modules\n\n"
		   "\tBug reports mailto: "SftEMail"\n"
		   "\t\tCompiled: %s, %s with \n\t\t%s",__DATE__,__TIME__,__VERSION__);
	    printf("\n");
	    return( 0x20 );
	}
	if (strcmp(argv[x], "-x") == 0)
	    ID = ID ^ 0x10;
	if (strcmp(argv[x], "-l") == 0)
	    ID = ID ^ 0x01;
	if (strcmp(argv[x], "-d") == 0)
	    ID = ID ^ 0x80;
    }

    return (ID);
    return(0);
}

static void
PrintHeader(char *EOL)
{
    printf("\n%c%s%c%s", 0x4, "-="SftName", version "SftVersion"=-", 0x4, EOL);
}

static void
PrintUsage()
{
    PrintHeader("");
    printf("%s",CopyRights);
    printf("\n\nUsage: %s <AmiBIOS.BIN> [Options]",SftName);
    printf("\n"
	   "\t\tOptions:"
	   "\n\t\t\t\"-l\" List Bios Structure"
	   "\n\t\t\t\"-x\" eXtract Bios Modules"
	   "\n\t\t\t\"-d\" create Directory"
	   "\n\t\t\t\"-h\" Help statistics"
	   );
    printf("\n\n\t*%s*\n",Url);
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

/*---------------------------------
        Xtract95
----------------------------------*/
static uint8_t
Xtract95(FILE *ptx, uint8_t Action, uint32_t ConstOff, uint32_t Offset, char *fname)
{
    FILE *pto;
    uint8_t PartTotal = 0;
    PARTTag part;
    char Buf[64];
    char MyDirName[64] = "--DECO--";
    uint32_t i;
    uint8_t sLen = 0;

    uint8_t doDir   = 0;
    /* For the case of multiple 0x20 modules */
    uint8_t Multiple = 0;

    sLen = strlen(fname);
    for (i = sLen; i > 0; i--)
	if (*(fname + i) == '/' || *(fname + i ) == '\\') {
	    i++;
	    break;
	}

    memcpy(MyDirName, (fname + i), sLen - i);
    for (i = 0; i < strlen(MyDirName); i++)
	if (MyDirName[i] == '.' )
	    MyDirName[i] = '\x0';

    sprintf(MyDirName, "%s.---", MyDirName);
    printf("\nDirName\t\t: %s", MyDirName);

    if ((Action & 0x80) && (Action & 0x10)) {
	doDir = 1;
	if (!mkdir(MyDirName, 0755))
	    printf("\nOperation mkdir() is permitted");
	else
	    printf("\nOperation mkdir() isn't permitted. Directory already exist?");
    }

    Action = Action & 0x7F;

    printf("\n"
	   "+------------------------------------------------------------------------------+\n"
	   "| Class.Instance (Name)        Packed --->  Expanded      Compression   Offset |\n"
	   "+------------------------------------------------------------------------------+\n"
	   );

    while((part.PrePartLo != 0xFFFF || part.PrePartHi != 0xFFFF) && PartTotal < 0x80) {
	fseek(ptx, Offset - ConstOff, SEEK_SET);
	fread(&part, 1, sizeof(part), ptx);
	PartTotal++;

	switch(Action){
	case List:
	    printf("\n   %.2X %.2i (%17.17s)    %5.5X (%6.5u) => %5.5X (%6.5u)  %.2s   %5.5Xh",
		   part.PartID, PartTotal, ModuleNameGet(part.PartID, TRUE),
		   (part.IsComprs!=0x80) ? (part.ROMSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (part.ROMSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (part.ExpSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (part.ExpSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? ("+") : (" "),
		   Offset - ConstOff);
	    break;
	case Xtract: /* Xtracting Part */
	    if (part.PartID == 0x20) {
		if (doDir)
		    sprintf(Buf, "%s/amipci_%.2X.%.2X", MyDirName, Multiple++, part.PartID);
		else
		    sprintf(Buf, "amipci_%.2X.%.2X", Multiple++, part.PartID);
	    } else {
		if (doDir)
		    sprintf(Buf, "%s/amibody.%.2x", MyDirName, part.PartID);
		else
		    sprintf(Buf,"amibody.%.2x", part.PartID);
	    }

	    if ((pto = fopen(Buf, "wb")) == NULL) {
		printf("\nFile %s I/O error..Exit", Buf);
		exit(1);
	    }

	    if (part.IsComprs != 0x80)
                decode(ptx, part.ROMSize, pto, part.ExpSize);
	    else {
		fseek(ptx, -8L, SEEK_CUR);
		for (i = 0; i < part.CSize; i++) {
		    fread(&Buf[0], 1, 1, ptx);
		    fwrite(&Buf[0], 1, 1, pto);
		};
	    }
	    fclose(pto);
	    break;
	}

	Offset = ((uint32_t)part.PrePartHi << 4) + (uint32_t)part.PrePartLo;
    }

    return (PartTotal);
}

/*---------------------------------
        Xtract0725
----------------------------------*/
static uint8_t
Xtract0725(FILE *ptx, uint8_t Action, uint32_t Offset)
{
    BIOS94 b94;
    FILE *pto;
    char Buf[12];
    uint8_t PartTotal = 0;
    uint8_t Module = 0;

    while((b94.PackLenLo != 0x0000 || b94.RealLenLo != 0x0000) && PartTotal < 0x80) {
	fseek(ptx, Offset, SEEK_SET);
	fread(&b94, 1, sizeof(b94), ptx);

	if(b94.PackLenLo==0x0000 && b94.RealLenLo==0x0000)
	    break;

	PartTotal++;

        switch(Action) {
	case List:
	    break;

	case Xtract: /* Xtracting Part */
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

    return (PartTotal);
}

/*---------------------------------
        Xtract1010
----------------------------------*/
static uint8_t
Xtract1010(FILE *ptx, uint8_t Action, uint32_t Offset)
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

    case List:
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

    case Xtract: /* Xtracting Part */

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

    return (GlobalMods);
}

int
main(int argc, char *argv[])
{
    FILE *ptx;
    uint32_t fLen;
    int ABCOffset;
    char Date[9];
    uint32_t Offset;
    uint8_t AMIVer = 0;
    uint8_t PartTotal = 0;
    uint8_t Action = 0;
    uint8_t HelpID = 0;

    HelpID = HelpSystem(argc, argv);
    switch (HelpID & 0x7F) {
    case 0x20:
	return 0;
    case 0x10:
	Action = Xtract;
	break;
    case 0x01:
	Action = List;
	break;
    default:
	PrintUsage();
	printf("\n");
	return 0;
    }

    if (!(ptx = fopen(argv[1],"rb"))) {
	printf("\nFile %s opening error...\n", argv[1]);
	return 0;
    };

    PrintHeader("\n\n");

    fseek(ptx, 0, SEEK_END);
    fLen = ftell(ptx);
    rewind(ptx);
    printf("FileLength\t: %X (%u bytes)\n", fLen, fLen);
    printf("FileName\t: %s\n", argv[1]);

    /*
     * Look for AMI bios header.
     */
    {
	char Temp[] = "AMIBIOSC";
	char *BufBlk = (char *) calloc(BLOCK, 1);
	int i = 0;

	if (!BufBlk)
	    exit(1);

	while (!feof(ptx)) {
	    uint32_t RealRead;

	    fseek(ptx, i, SEEK_SET);

	    RealRead = fread(BufBlk, 1, BLOCK, ptx);

	    ABCOffset = FoundAt(ptx, BufBlk, Temp, RealRead);
	    if (ABCOffset != 0) {
		printf("AMIBIOS 95 header found at offset 0x%05X\n", ABCOffset);
		AMIVer = 95;
		break;
	    }

	    i = ftell(ptx) - 0x100;
	}

	free(BufBlk);

	if (AMIVer != 95) {
	    char Buf[12];

	    printf("AMI'95 hook not found..Turning to AMI'94\n");

	    fseek(ptx, 0, SEEK_SET);
	    fread(&Buf, 1, 8, ptx);

	    if (memcmp(Buf, Temp, 8) != 0) {
		printf("Obviously not even AMIBIOS standard..Exit\n");
		return 0;
	    } else {
		AMIDATE amidate;

		fread(&amidate, 1, sizeof(amidate), ptx);
		if (atoi(amidate.Day) == 10 && atoi(amidate.Month) == 10)
		    AMIVer = 10;
		else
		    AMIVer = 94;
	    }
	}
    }

    /* Get Date */
    fseek(ptx, -11L, SEEK_END);
    fread(Date, 1, 8, ptx);
    Date[8] = 0;

    printf("\n\tAMIBIOS information:");

    switch (AMIVer) {
    case 95:
	{
	    uint32_t ConstOff;
	    ABCTag abc;

	    fseek(ptx, ABCOffset, SEEK_SET);
	    fread(&abc, 1, sizeof(abc), ptx);

	    printf("\nAMI95 Version\t\t: %.4s", abc.Version);
	    printf("\nPacked Data\t: %X (%u bytes)", (uint32_t) abc.CRCLen * 8, (uint32_t) abc.CRCLen * 8);

	    Offset = (((uint32_t) abc.BeginHi) << 4) + (uint32_t) abc.BeginLo;
	    printf("\nStart\t\t: %X", Offset);

	    printf("\nPacked Offset\t: %X", fLen - 0x100000 + Offset);

	    ConstOff = 0x100000 - fLen;
	    printf("\nOffset\t\t: %X", ConstOff);
	    printf("\nReleased\t: %s", Date);

	    PartTotal = Xtract95(ptx, HelpID, ConstOff, Offset, argv[1]);

	    printf("\nTotal Sections\t: %i\n", PartTotal);
	}
	break;
    case 94:
	printf("\n AMI94.");
	Offset = 0x10;
	printf("\nStart\t\t: %X", Offset);
	printf("\nReleased\t: %s", Date);

	PartTotal = Xtract0725(ptx, Action, Offset);

	printf("\nTotal Sections\t: %i\n", PartTotal);
	break;
    case 10:
	printf("\n AMI 10.");
	Offset = 0x30;
	printf("\nStart\t\t: %X", Offset);
	printf("\nReleased\t: %s", Date);

	PartTotal = Xtract1010(ptx, Action, 0x30);

	printf("\nTotal Sections\t: %i\n", PartTotal);
	break;
    default:
	break;
    }

    return 0;
}
