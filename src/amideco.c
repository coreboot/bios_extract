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

#if defined(LINUX) || defined(__LINUX__) || defined(__linux__) || defined(__FreeBSD_kernel__)
#include <memory.h>
#define IDSign "+"
#else
#include <mem.h>
#include <conio.h>
#define IDSign ""
#endif

typedef unsigned char   byte;
typedef unsigned short  word;
typedef unsigned long   dword;

#define Xtract  0x10
#define List    0x01
#define Delete  0x12
#define XtractD 0x13

#define BLOCK   0x8000

typedef struct {
    FILE *infile;
    FILE *outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
} interfacing;

typedef struct {
    byte    Version[4];
    word    CRCLen;
    dword   CRC32;
    word    BeginLo;
    word    BeginHi;
} ABCTag;

typedef struct
{
    word    PrePartLo;      /*      Previous part LO word   */
    word    PrePartHi;      /*      Previous part HI word   */
    /*      value 0xFFFFFFFF = last */
    word    CSize;          /*      This module header Len  */
    byte    PartID;         /*      ID for this header      */
    byte    IsComprs;       /*      Value 0x80 means that
				    this module not
				    compressed, 0x00 - is   */
    dword   RealCS;         /*      Address in RAM where
				    expand to               */
    dword   ROMSize;        /*      Compressed Len          */
    dword   ExpSize;        /*      Expanded Len            */
} PARTTag;

typedef struct
{
    byte    HeadLen;
    byte    HeadCrc;
    byte    Method[5];
    dword   PackLen;
    dword   RealLen;
    dword   TStamp;
    byte    Attr;
    byte    Level;
    byte    FilenameLen;
    byte    FileName[12];
    word    CRC16;
    byte    DOS;
    word    Empty;
} LZHHead;

typedef struct
{
    word    PackLenLo;
    word    PackLenHi;
    word    RealLenLo;
    word    RealLenHi;
} BIOS94;

typedef struct
{
    byte    Month[2];
    byte    rsrv1;
    byte    Day[2];
    byte    rsrv2;
    byte    Year[2];
} AMIDATE;

typedef struct
{
    dword Offset;
    byte  ModID;
    byte  IsPacked;
} AMIHEAD94;

#define SftName "AmiBIOSDeco"
#define SftEMail "Anton Borisov, anton.borisov@gmail.com"

/********SoftWare*************/
byte CopyRights[] = "\n(C) Anton Borisov, 2000, 2002-2003, 2006. Portions (C) 1999-2000";
byte Url[] = "Bug-reports direct to "SftEMail;

#define SftVersion "0.31e"

byte
StrLen(byte *Str)
{
    int i = 0;

    while( *(Str+i) != 0x0 )
	i++;
    return(i);
};

byte
StrCmp(byte *Dst, byte *Src)
{
    byte i;
    for(i = 0; i <= StrLen(Src); i++)
	if(Dst[i] != Src[i])
	    return(1);
    return(0);
}

byte
HelpSystem(byte argc, byte *argv[])
{
    byte x;
    byte ID = 0;

    for (x = 1; x < argc; x++) {
	if (StrCmp(argv[x], "-h") == 0) {
	    printf("\n"SftName" HelpSystem Starting Now!\n");
	    printf("\nThis Program Version Number %s",SftVersion);
	    printf(
		   "\n"SftName" - Decompressor for AmiBIOSes only.\n"
		   "\tSupported formats: AMIBIOS'94, AMIBIOS'95 or later\n\n"
		   ""SftName" performs on 386 or better CPU systems\n"
		   "under control of LinuxOS\n\n"
		   "Compression schemes include: LZINT\n\n"
		   "Modules marked with "IDSign" sign are compressed modules\n\n"
		   "\tBug reports mailto: "SftEMail"\n"
		   "\t\tCompiled: %s, %s with \n\t\t%s",__DATE__,__TIME__,__VERSION__);
	    printf("\n");
	    return( 0x20 );
	}
	if (StrCmp(argv[x], "-x") == 0)
	    ID = ID ^ 0x10;
	if (StrCmp(argv[x], "-l") == 0)
	    ID = ID ^ 0x01;
	if (StrCmp(argv[x], "-d") == 0)
	    ID = ID ^ 0x80;
    }

    return (ID);
    return(0);
}

void
PrintHeader(byte* EOL)
{
    printf("\n%c%s%c%s", 0x4, "-="SftName", version "SftVersion"=-", 0x4, EOL);
}

void
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

byte *RecordList[] = {
    "POST",
    "Setup Server",
    "Runtime",
    "DIM",
    "Setup Client",
    "Remote Server",
    "DMI Data",
    "GreenPC",
    "Interface",
    "MP",
    "Notebook",
    "Int-10",
    "ROM-ID",
    "Int-13",
    "OEM Logo",
    "ACPI Table",
    "ACPI AML",
    "P6 MicroCode",
    "Configuration",
    "DMI Code",
    "System Health",
    "UserDefined",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "","",
    "PCI AddOn ROM",
    "Multilanguage",
    "UserDefined",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Font Database",
    "OEM Logo Data",
    "Graphic Logo Code",
    "Graphic Logo Data",
    "Action Logo Code",
    "Action Logo Data",
    "Virus",
    "Online Menu",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
};


byte* GetModuleName(byte ID)
{
    /* -------------- New Name Conventions -------------- */

    switch(ID){
    case 0x00:
	return("POST");
    case 0x01:
	return("Setup Server");
    case 0x02:
	return("RunTime");
    case 0x03:
	return("DIM");
    case 0x04:
	return("Setup Client");
    case 0x05:
	return("Remote Server");
    case 0x06:
	return("DMI Data");
    case 0x07:
	return("Green PC");
    case 0x08:
	return("Interface");
    case 0x09:
	return("MP");
    case 0x0A:
	return("Notebook");
    case 0x0B:
	return("Int-10");
    case 0x0C:
	return("ROM-ID");
    case 0x0D:
	return("Int-13");
    case 0x0E:
	return("OEM Logo");
    case 0x0F:
	return("ACPI Table");
    case 0x10:
	return("ACPI AML");
    case 0x11:
	return("P6 Microcode");
    case 0x12:
	return("Configuration");
    case 0x13:
	return("DMI Code");
    case 0x14:
	return("System Health");
    case 0x15:
	return("Memory Sizing");
    case 0x16:
	return("Memory Test");
    case 0x17:
	return("Debug");
    case 0x18:
	return("ADM (Display MGR)");
    case 0x19:
	return("ADM Font");
    case 0x1A:
	return("Small Logo");
    case 0x1B:
	return("SLAB");

    case 0x20:
	return("PCI AddOn ROM");
    case 0x21:
	return("Multilanguage");
    case 0x22:
	return("UserDefined");
    case 0x23:
	return("ASCII Font");
    case 0x24:
	return("BIG5 Font");
    case 0x25:
	return("OEM Logo");
    case 0x2A:
	return("User ROM");
    case 0x2B:
	return("PXE Code");
    case 0x2C:
	return("AMI Font");
    case 0x2E:
	return("User ROM");
    case 0x2D:
	return("Battery Refresh");

    case 0x30:
	return("Font Database");
    case 0x31:
	return("OEM Logo Data");
    case 0x32:
	return("Graphic Logo Code");
    case 0x33:
	return("Graphic Logo Data");
    case 0x34:
	return("Action Logo Code");
    case 0x35:
	return("Action Logo Data");
    case 0x36:
	return("Virus");
    case 0x37:
	return("Online Menu");
    case 0x38:
	return("Lang1 as ROM");
    case 0x39:
	return("Lang2 as ROM");
    case 0x3A:
	return("Lang3 as ROM");

    case 0x70:
	return("OSD Bitmaps");

    default:
	return("User-Defined ;)");

    }
}

byte *
GetFullDate(byte *mon, byte *day, byte *year)
{
    byte *Months[] = {"",
		    "January",
		    "February",
		    "March",
		    "April",
		    "May",
		    "June",
		    "July",
		    "August",
		    "September",
		    "October",
		    "November",
		    "December"};
    byte Buf[20];
    sprintf(Buf, "%2.2s", year);

    if((atoi(Buf) >= 0) && (atoi(Buf) < 70))
	sprintf(Buf, "%.2s %s %s%.2s", day, Months[atoi(mon)], "20", year);
    else
	sprintf(Buf, "%.2s %s %s%.2s", day, Months[atoi(mon)], "19", year);

    return(Buf);
}

/*---------------------------------
        FindHook
----------------------------------*/

dword
FoundAt(FILE *ptx, byte *Buf, byte *Pattern, dword BLOCK_LEN)
{
    dword i, Ret;
    word Len;
    Len = StrLen(Pattern);

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
byte
Xtract95(FILE *ptx, byte Action, dword ConstOff, dword Offset, byte* fname)
{
    FILE *pto;
    interfacing interface;
    byte PartTotal = 0;
    PARTTag part;
    byte Buf[64];
    byte MyDirName[64]     =       "--DECO--";
    dword i;
    byte sLen = 0;

    byte doDir   = 0;
    /* For the case of multiple 0x20 modules */
    byte Multiple = 0, j = 0;

    sLen = StrLen(fname);
    for (i = sLen; i > 0; i--)
	if(*(fname + i) == '/' || *(fname + i ) == '\\') {
	    i++;
	    break;
	}

    memcpy(MyDirName, (fname + i), sLen - i);
    for (i = 0; i < StrLen(MyDirName); i++)
	if (MyDirName[i] == '.' )
	    MyDirName[i] = '\x0';

    sprintf(MyDirName, "%s.---", MyDirName);
    printf("\nDirName\t\t: %s", MyDirName);

    if ((Action & 0x80) && (Action & 0x10)) {
	doDir = 1;
	if(!mkdir(MyDirName, 0755))
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
	fseek(ptx, Offset-ConstOff, 0);
	fread(&part, 1, sizeof(part), ptx);
	PartTotal++;

	switch(Action){
	case List:
	    printf("\n   %.2X %.2i (%17.17s)    %5.5lX (%6.5lu) => %5.5lX (%6.5lu)  %.2s   %5.5lXh",
		   part.PartID, PartTotal, GetModuleName(part.PartID),
		   (part.IsComprs!=0x80) ? (part.ROMSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (part.ROMSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (part.ExpSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (part.ExpSize) : (part.CSize),
		   (part.IsComprs!=0x80) ? (IDSign) : (" "),
		   Offset-ConstOff);
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

	    if ((pto = fopen(Buf,"wb")) == NULL) {
		printf("\nFile %s I/O error..Exit", Buf);
		exit(1);
	    }

	    interface.infile = ptx;
	    interface.outfile = pto;
	    interface.original = part.ExpSize ;
	    interface.packed = part.ROMSize;

	    interface.dicbit = 13;
	    interface.method = 5;

	    if (part.IsComprs != 0x80)
                decode(interface);
	    else {
		fseek(ptx, -8L, 1);
		for (i = 0; i < part.CSize; i++) {
		    fread(&Buf[0], 1, 1, ptx);
		    fwrite(&Buf[0], 1, 1, pto);
		};
	    }
	    fclose(pto);
	    break;
	}

	Offset = ((dword)part.PrePartHi << 4) + (dword)part.PrePartLo;
    }

    return (PartTotal);
}

/*---------------------------------
        Xtract0725
----------------------------------*/
byte
Xtract0725(FILE *ptx, byte Action, dword Offset)
{
    BIOS94 b94;
    FILE *pto;
    interfacing interface;
    byte Buf[12];
    byte PartTotal = 0;
    byte Module = 0;

    while((b94.PackLenLo != 0x0000 || b94.RealLenLo != 0x0000) && PartTotal < 0x80) {
	fseek(ptx, Offset, 0);
	fread(&b94, 1, sizeof(b94), ptx);

	if(b94.PackLenLo==0x0000 && b94.RealLenLo==0x0000)
	    break;

	PartTotal++;

        switch(Action) {
	case List:
	    break;

	case Xtract: /* Xtracting Part */
	    sprintf(Buf,"amibody.%.2x",Module++);
	    pto = fopen(Buf,"wb");

	    interface.infile = ptx;
	    interface.outfile = pto;

	    interface.original = b94.RealLenLo;
	    interface.packed = b94.PackLenLo;
	    interface.dicbit = 13;
	    interface.method = 5;

	    decode(interface);
	    fclose(pto);
	    break;
	}

	Offset = Offset + b94.PackLenLo;

    }
    printf("\n\nThis Scheme Usually Contains: \n\t%s\n\t%s\n\t%s",
	   RecordList[0], RecordList[1], RecordList[2]);

    return (PartTotal);
}

/*---------------------------------
        Xtract1010
----------------------------------*/
byte
Xtract1010(FILE *ptx, byte Action, dword Offset)
{
    FILE *pto;
    word ModsInHead = 0;
    word GlobalMods = 0;
    PARTTag *Mods94;
    BIOS94 ModHead;
    word Tmp;
    dword i, ii;
    interfacing interface;
    byte Buf[12];
    byte Module = 0;

    fseek(ptx, 0x10, 0);
    fread(&ModsInHead, 1, sizeof(word), ptx);
    GlobalMods = ModsInHead - 1;

    Mods94 = (PARTTag*) calloc(ModsInHead, sizeof(PARTTag));

    for (i = 0; i < ModsInHead; i++) {
	fseek(ptx, 0x14 + i*4, 0);
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
	    fseek(ptx, Mods94[i].RealCS, 0);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);
	    printf("\n%.2s %.2X (%17.17s) %5.5lX (%5.5lu) => %5.5lX (%5.5lu), %5.5lXh",
		   (Mods94[i].IsComprs == 0) ? (IDSign) : (" "),
		   Mods94[i].PartID,
		   (StrCmp(RecordList[Mods94[i].PartID],"") == 0) ? ("UserDefined") : (RecordList[Mods94[i].PartID]),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.PackLenLo),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.PackLenLo),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.RealLenLo),
		   (Mods94[i].IsComprs == 1) ? (0x10000 - Mods94[i].RealCS) : (ModHead.RealLenLo),
		   Mods94[i].RealCS);
	}

	Offset = 0x10000;
	while(Offset <= Mods94[0].RealCS) {
	    fseek(ptx, Offset, 0);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    if(ModHead.RealLenHi && ModHead.PackLenHi) {
		Offset += 0x1000;
		if ((ModHead.RealLenHi == 0xFFFF) && (ModHead.PackLenHi == 0xFFFF) && ((Offset % 0x1000) != 0))
		    Offset = 0x1000 * ( Offset / 0x1000 );
	    } else {
		printf("\nNext Module (%i): %5.5X (%5.5u) => %5.5X (%5.5u),  %5.5lXh",
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
	    fseek(ptx, Mods94[i].RealCS, 0);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    sprintf(Buf,"amibody.%.2x",Module++);
	    pto = fopen(Buf,"wb");
	    if (!pto) {
		printf("\nFile %s I/O Error..Exit");
		exit(1);
	    }

	    interface.infile = ptx;
	    interface.outfile = pto;

	    interface.original = ModHead.RealLenLo;
	    interface.packed = ModHead.PackLenLo;
	    interface.dicbit = 13;
	    interface.method = 5;

	    if(Mods94[i].IsComprs == 1) {
		fseek(ptx, -8L, 1);
		for(ii = 0; ii < (0x10000 - (dword) Mods94[i].RealCS); ii++) {
		    fread(&Buf[0], 1, 1, ptx);
		    fwrite(&Buf[0], 1, 1, pto);
		};
	    } else
		decode(interface);
	    fclose(pto);
	}

	/*------- Hidden Parts -----------*/

	Offset = 0x10000;
	while(Offset < Mods94[0].RealCS) {
	    fseek(ptx, Offset, 0);
	    fread(&ModHead, 1, sizeof(ModHead), ptx);

	    if(ModHead.RealLenHi && ModHead.PackLenHi) {
		Offset += 0x1000;
		if ((ModHead.RealLenHi == 0xFFFF) && (ModHead.PackLenHi == 0xFFFF) && ((Offset % 0x1000) != 0)) {
		    Offset = 0x1000 * (Offset/0x1000);
		    continue;
		}
		continue;
	    } else {
		GlobalMods++;
		sprintf(Buf,"amibody.%.2x",Module++);

		pto = fopen(Buf,"wb");
		if(!pto) {
		    printf("\nFile %s I/O Error..Exit");
		    exit(1);
		}

		interface.infile = ptx;
		interface.outfile = pto;
		interface.original = ModHead.RealLenLo;
		interface.packed = ModHead.PackLenLo;
		interface.dicbit = 13;
		interface.method = 5;

		decode(interface);
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
main(byte argc, byte *argv[])
{
    FILE *ptx, *pto;
    dword fLen, i, RealRead = 0;
    byte Temp[] = "AMIBIOSC", *BufBlk;
    byte Buf[12];
    byte BufAdd[0x30];
    ABCTag abc;

    dword ConstOff, Offset, BODYOff = 0;
    LZHHead FHead;

    byte AMIVer = 0;
    AMIDATE amidate;

    byte PartTotal = 0;
    byte Action = 0;
    byte HelpID = 0;

    HelpID = HelpSystem(argc,argv);
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

    if(!(ptx = fopen(argv[1],"rb"))) {
	printf("\nFile %s opening error...\n", argv[1]);
	return 0;
    };

    PrintHeader("\n\n");

    fseek( ptx, 0, 2 );
    fLen = ftell(ptx);
    rewind(ptx);
    printf("FileLength\t: %lX (%lu bytes)\n", fLen, fLen);
    printf("FileName\t: %s\n", argv[1]);

    /*------- Memory Alloc --------*/
    if(!(BufBlk = (byte *) calloc(BLOCK, sizeof(byte))))
	exit(1);
    i = 0;

    while (!feof(ptx)) {
	fseek(ptx, i, 0);
	RealRead = fread(BufBlk, 1, BLOCK, ptx);
	if ((i = FoundAt(ptx, BufBlk, Temp, RealRead) ) != 0) {
	    fseek(ptx, i + 8, 0);
	    fread(&abc, 1, sizeof(abc), ptx);
	    AMIVer = 95;
	    break;
	}
	i = ftell(ptx) - 0x100;
    }

    if (AMIVer != 95) {
	printf("AMI'95 hook not found..Turning to AMI'94\n");
	rewind(ptx);
	fread(&Buf, 1, 8, ptx);
	if(memcmp(Buf, Temp, 8) != 0) {
	    printf("Obviously not even AMIBIOS standard..Exit\n");
	    return 0;
	} else
	    AMIVer = 94;
    };

    printf("\n\tAMIBIOS information:");

    switch (AMIVer) {
    case 95:
	fseek(ptx, -11L, 2);
	fread(&amidate, 1, sizeof(amidate), ptx);

	printf("\nVersion\t\t: %.4s", abc.Version);
	printf("\nPacked Data\t: %lX (%lu bytes)", (dword) abc.CRCLen * 8, (dword) abc.CRCLen * 8);
	printf("\nStart\t\t: %lX", Offset = ((dword) abc.BeginHi << 4) + (dword) abc.BeginLo);

	BODYOff = fLen - ( 0x100000 - ( Offset + 8 + sizeof(abc)) ) - 8 - sizeof(abc);
	printf("\nPacked Offset\t: %lX", BODYOff);

	ConstOff = Offset - BODYOff;
	printf("\nOffset\t\t: %lX", ConstOff);

	break;
    case 94:
	fread(&amidate, 1, sizeof(amidate), ptx);
	if (atoi(amidate.Day) == 10 && atoi(amidate.Month) == 10) {
	    Offset = 0x30;
	    AMIVer = 10;
	} else Offset = 0x10;
	fseek(ptx, -11L, 2);
	fread(&amidate, 1, sizeof(amidate), ptx);
	break;
    };
    printf("\nReleased\t: %s", GetFullDate(amidate.Month, amidate.Day, amidate.Year));

    switch (AMIVer) {
    case 95:
	PartTotal = Xtract95(ptx, HelpID, ConstOff, Offset, argv[1]);
	break;
    case 94:
	PartTotal = Xtract0725(ptx, Action, Offset);
	break;
    case 10:
	PartTotal = Xtract1010(ptx, Action, Offset);
	break;
    default:
	break;
    }
    printf("\nTotal Sections\t: %i", PartTotal);

    free(BufBlk);

    printf("\n");
    return 0;
}
