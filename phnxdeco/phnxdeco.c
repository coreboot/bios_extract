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

#define SftPlatform	"Linux"
#define SftVersion      "0.33 ("SftPlatform")"
#define SftName         "PhoenixDeco"
#define SftEMail        "Anton Borisov, anton.borisov@gmail.com"

byte SoftName[] = "-=" SftName ", version " SftVersion "=-";
byte CopyRights[] = "\n(C) Anton Borisov, 2000, 2002-2004, Portions (C) 1999-2000";
byte Url[] = "Bug-reports direct to " SftEMail;

byte HelpSystem(byte argc, byte * argv[])
{
    byte x = 0, retcode = 0;

    for (x = 1; x < argc; x++) {
	if (StrCmp(argv[x], "-h") == 0) {
	    printf("\n" SftName " HelpSystem Starting Now!\n");
	    printf("\nThis Program Version Number %s", SftVersion);
	    printf("\n" SftName " - Decompressor for PhoenixBIOSes only.\n" "\tSupported formats: Phoenix BIOS 4.0, Phoenix FirstBIOS\n\n" "" SftName
		   " performs on 386 or better CPU systems\n" "under control of LinuxOS\n\n" "Compression schemes include: NONE, LZSS, LZINT\n\n"
		   "Modules marked with " IDSign " sign are compressed modules\n\n" "\tBug reports mailto: " SftEMail "\n" "\t\tCompiled: %s, %s with \n\t\t%s",
		   __DATE__, __TIME__, __VERSION__);
	    printf("\n");
	    retcode = 0x80;

	}

	if (StrCmp(argv[x], "-xs") == 0)
	    retcode = 0x20;
	if (StrCmp(argv[x], "-ls") == 0)
	    retcode = 0x21;
	if (StrCmp(argv[x], "-x") == 0)
	    retcode = 0x10;
	if (StrCmp(argv[x], "-l") == 0)
	    retcode = 0x11;
	if (StrCmp(argv[x], "-c") == 0)
	    retcode += 0x40;
    }
    return (retcode);

}

void PrintHeader(byte * EOL)
{

    printf("\n%c%s%c%s", 0x4, SoftName, 0x4, EOL);

}

void PrintUsage()
{

    PrintHeader("");
    printf("%s", CopyRights);
    printf("\n\nUsage: PhnxDeco <PhoenixBIOS.ROM> [Options]");
    printf("\n" "\t\tOptions:" "\n\t\t\t\"-ls\" List (System) Bios Structure" "\n\t\t\t\"-xs\" eXtract (System) Bios Modules"
	   "\n\t\t\t\"-l\" List Bios Structure" "\n\t\t\t\"-x\" eXtract Bios Modules" "\n\t\t\t\"-c\" show Bios Copyrights" "\n\t\t\t\"-h\" Help statistics");
    printf("\n\n\t*%s*\n", Url);

}

int main(byte argc, byte * argv[])
{
    FILE *ptx;
    byte *Buf;
    dword CurPos, fLen, Start, Offset;
    word i, Len, FirstBLK, BBsz, BANKsz;
    dword POSTOff, SYSOff, FCPOff, FirstBLKf;
    byte PhBIOS[] = "Phoenix FirstBIOS", PhVersion[3], PhRelease[3], phtime[8];
    byte BCPSEGMENT[] = "BCPSEGMENT";
    byte BCPFCP[] = "BCPFCP";
    AMIDATE phdate;
    byte TotalSections = 0, Action, Mods = 0;
    PHNXID IDMod;

    byte __COPY__ = 0;

    switch (HelpSystem(argc, argv)) {
    case 0x80:
	return 0;
    case 0x50:
	__COPY__ = 1;
    case 0x10:
	Action = Xtract;
	break;
    case 0x51:
	__COPY__ = 1;
    case 0x11:
	Action = List;
	break;
    case 0x60:
	__COPY__ = 1;
    case 0x20:
	Action = XtractM;
	break;
    case 0x61:
	__COPY__ = 1;
    case 0x21:
	Action = ListM;
	break;
    default:
	PrintUsage();
	printf("\n");
	return 0;
    }

    PrintHeader("\n\n");

    if ((ptx = fopen(argv[1], "rb")) == NULL) {
	printf("\nFATAL ERROR: File %s opening error...\n", argv[1]);
	return 0;
    };

    Buf = (byte *) calloc(BLOCK, 1);
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
    printf("Filelength\t: %lX (%lu bytes)\n", fLen, fLen);
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

    printf("PhoenixBIOS hook found at\t: %lX\n", CurPos);
    CurPos += 10;
    fseek(ptx, (dword) (CurPos), 0);

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
	fseek(ptx, (dword) (CurPos), 0);
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

    printf("System Information at\t\t: %lX\n", SYSOff);

    fseek(ptx, SYSOff + 0x7E, 0);
    fread(&BBsz, 1, sizeof(BBsz), ptx);

    fseek(ptx, SYSOff + 0x7B, 0);
    fread(&BANKsz, 1, sizeof(BANKsz), ptx);

    fseek(ptx, SYSOff + 15, 0);
    fread(&phdate, 1, sizeof(phdate), ptx);
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

    printf("BootBlock\t: %lX bytes\n", (BBsz == 0x0) ? (0x10000) : (BBsz));
    printf("BankSize\t: %li KB\n", BANKsz);
    printf("Version\t\t: %8.8s\n", Buf);
    printf("Start\t\t: %lX\n", Start);
    printf("Offset\t\t: %lX\n", 0xFFFF0000 - Offset);

    printf("BCP Modules\t: %i\n", Mods);
    printf("BCPFCP\t\t: %lX\n", FCPOff);
    fseek(ptx, FCPOff + 0x18, 0);
    fread(&FirstBLK, 1, sizeof(FirstBLK), ptx);
    FirstBLKf = (ftell(ptx) & 0xF0000) + FirstBLK;
    printf("FCP 1st module\t: %lX (%lX)\n", FirstBLK, FirstBLKf);

    printf("Released\t: %s at %8.8s\n", GetFullDate(phdate.Month, phdate.Day, phdate.Year), phtime);

    printf("/* Copyrighted Information */\n");

    /* If .rom begins with no additional trash this routine should be applied */
    if (__COPY__) {
	fseek(ptx, POSTOff + 0x1B, 0);

	fread(&CurPos, 1, sizeof(word), ptx);

	fseek(ptx, (POSTOff & 0xF0000) + (CurPos & 0xffff), 0);
    } else {


	CurPos = IsPhoenixBIOS(ptx, Buf);
	fseek(ptx, (dword) (CurPos), 0);
    };

    fread(Buf, 1, 0x100, ptx);

    printf("\t%.64s\n", Buf);

    /* If .rom begins with no additional trash this routine should be applied */
    if (__COPY__) {
	fseek(ptx, POSTOff + 0x38, 0);
	fread(&CurPos, 1, sizeof(word), ptx);
	fseek(ptx, (POSTOff & 0xF0000) + (CurPos & 0xffff), 0);
	fread((Buf + 0x100), 1, 0x100, ptx);
	i = 0x100;
	while (Buf[i++] != 0xD);
	Buf[i] = 0x0;
	printf("\t%s\n", Buf + 0x100);
    }

    printf("/* ----------------------- */\n");


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
