/*
 * Copyright 2009      Luc Verhaegen <libv@skynet.be>
 * Copyright 2000-2003 Anthony Borisow
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
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "bios_extract.h"
#include "lh5_extract.h"

struct PhoenixModuleName
{
    char Id;
    char *Name;
};

static struct PhoenixModuleName
PhoenixModuleNames[] = {
    {'A', "acpi"},
    {'B', "bioscode"},
    {'C', "update"},
    {'D', "display"},
    {'E', "setup"},
    {'F', "font"},
    {'G', "decompcode"},
    {'I', "bootblock"},
    {'L', "logo"},
    {'M', "miser"},
    {'N', "rompilotload"},
    {'O', "network"},
    {'P', "rompilotinit"},
    {'R', "oprom"},
    {'S', "strings"},
    {'T', "template"},
    {'U', "user"},
    {'X', "romexec"},
    {'W', "wav"},
    {'H', "tcpa_H"}, /* TCPA (Trusted Computing), USBKCLIB? */
    {'K', "tcpa_K"}, /* TCPA (Trusted Computing), "AUTH"? */
    {'Q', "tcpa_Q"}, /* TCPA (Trusted Computing), "SROM"? */
    {'<', "tcpa_<"},
    {'*', "tcpa_*"},
    {'?', "tcpa_?"},
    {'J', "SmartCardPAS"},
};

struct PhoenixID {
    char Name[6];
    uint16_t Flags;
    uint16_t Length;
};

struct PhoenixFFVModule {
    uint8_t Signature;
    uint8_t Flags;
    uint16_t Checksum;
    uint16_t LengthLo;
    uint8_t LengthHi;
    uint8_t Compression;
    char Name[16];
};

static char *
PhoenixModuleNameGet(char Id)
{
    int i;

    for (i = 0; PhoenixModuleNames[i].Name; i++)
	if (PhoenixModuleNames[i].Id == Id)
	    return PhoenixModuleNames[i].Name;

    return NULL;
}

static int
PhoenixModule(unsigned char *BIOSImage, int BIOSLength, int Offset)
{
    struct PhoenixModule {
	uint32_t Previous;
	uint8_t Signature[3];
	uint8_t Id;
	uint8_t Type;
	uint8_t HeadLen;
	uint8_t Compression;
	uint16_t Offset;
	uint16_t Segment;
	uint32_t ExpLen;
	uint32_t FragLength;
	uint32_t NextFrag;
    } *Module;
    char *filename, *ModuleName;
    unsigned char *Buffer;
    unsigned char *ModuleData;
    uint32_t Packed;
    int fd;

    Module = (struct PhoenixModule *) (BIOSImage + Offset);

    if (Module->Signature[0] || (Module->Signature[1] != 0x31) ||
	(Module->Signature[2] != 0x31)) {
	fprintf(stderr, "Error: Invalid module signature at 0x%05X\n",
		Offset);
	return 0;
    }

    if ((Offset + Module->HeadLen + 4 + le32toh(Module->FragLength)) > BIOSLength) {
	fprintf(stderr, "Error: Module overruns buffer at 0x%05X\n",
		Offset);
	return le32toh(Module->Previous);
    }

    /* NextFrag is either the unpacked length again *or* the virtual address
       of the next fragment. As long as BIOSses stay below 256MB, this works */
    if ((le32toh(Module->NextFrag) & 0xF0000000) == 0xF0000000) {
	struct PhoenixFragment {
	    uint32_t NextFrag;
	    uint8_t NextBank;
	    uint32_t FragLength;
	} *Fragment;
	int FragOffset;
	uint32_t FragLength = le32toh(Module->FragLength);

	if (FragLength > le32toh(Module->ExpLen)) {
	    fprintf(stderr, "Error: First fragment exceeds total size at"
		    " %05X\n", Offset);
	    return le32toh(Module->Previous);
	}

	/* This over-allocates, but the total compressed size is not available
	   here */
	ModuleData = malloc(le32toh(Module->ExpLen));
	if (!ModuleData) {
	    fprintf(stderr, "Error: Can't reassemble fragments,"
		    " no memory for %d bytes\n", le32toh(Module->ExpLen));
	    return le32toh(Module->Previous);
	}

	memcpy(ModuleData, BIOSImage + Offset + Module->HeadLen, FragLength);

	Packed = FragLength;
	FragOffset = le32toh(Module->NextFrag) & (BIOSLength - 1);

	printf("extra fragments: ");
	while(FragOffset) {
	    Fragment = (struct PhoenixFragment *) (BIOSImage + FragOffset);
	    FragLength = le32toh(Fragment->FragLength);
	    printf("(%05X, %d bytes) ", FragOffset, FragLength);

	    if (Packed + FragLength > le32toh(Module->ExpLen)) {
		fprintf(stderr, "\nFragment too big at %05X for %05X\n",
			FragOffset, Offset);
		free(ModuleData);
		return le32toh(Module->Previous);
	    }
	    memcpy(ModuleData + Packed, BIOSImage + FragOffset + 9, FragLength);

	    Packed += FragLength;
	    FragOffset = le32toh(Fragment->NextFrag) & (BIOSLength - 1);
	}
	printf("\n");
    } else {
	ModuleData = BIOSImage + Offset + Module->HeadLen;
	Packed = le32toh(Module->FragLength);
    }

    ModuleName = PhoenixModuleNameGet(Module->Type);
    if (ModuleName) {
	filename = malloc(strlen(ModuleName) + 7);
	sprintf(filename, "%s_%1d.rom", ModuleName, Module->Id);
    } else {
	filename = malloc(9);
	sprintf(filename, "%02X_%1d.rom", Module->Type, Module->Id);
    }

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
	fprintf(stderr, "Error: unable to open %s: %s\n\n",
		filename, strerror(errno));
	free(filename);
	if ((le32toh(Module->NextFrag) & 0xF0000000) == 0xF0000000)
	    free(ModuleData);

	return le32toh(Module->Previous);
    }

    switch (Module->Compression) {
    case 5: /* LH5 */
	printf("0x%05X (%6d bytes)   ->   %s\t(%d bytes)", Offset + Module->HeadLen + 4,
	       Packed, filename, le32toh(Module->ExpLen));

	Buffer = MMapOutputFile(filename, le32toh(Module->ExpLen));
	if (!Buffer)
	    break;

	/* The first 4 bytes of the LH5 packing method is just the total
	   expanded length; skip them */
	LH5Decode(ModuleData + 4, Packed - 4,
		  Buffer, le32toh(Module->ExpLen));

	munmap(Buffer, le32toh(Module->ExpLen));

	break;
    /* case 3 */ /* LZSS */
    case 0: /* not compressed at all */
	printf("0x%05X (%6d bytes)   ->   %s", Offset + Module->HeadLen,
	       Packed, filename);

	write(fd, ModuleData, Packed);
	break;
    default:
	fprintf(stderr, "Unsupported compression type for %s: %d\n",
		filename, Module->Compression);
	printf("0x%05X (%6d bytes)   ->   %s\t(%d bytes)", Offset + Module->HeadLen,
	       Packed, filename, le32toh(Module->ExpLen));

	write(fd, ModuleData, Packed);
	break;
    }

    close(fd);
    free(filename);
    if ((le32toh(Module->NextFrag) & 0xF0000000) == 0xF0000000)
	free(ModuleData);

    if (le16toh(Module->Offset) || le16toh(Module->Segment)) {
	if (!Module->Compression)
	    printf("\t\t");
	printf("\t [0x%04X:0x%04X]\n", le16toh(Module->Segment) << 12, le16toh(Module->Offset));
    } else
	printf("\n");

    return le32toh(Module->Previous);
}

static int
PhoenixExtractFFV(unsigned char *BIOSImage, int BIOSLength, int Offset)
{
    struct PhoenixFFVCompressionHeader {
	uint16_t TotalLengthLo;
	uint8_t TotalLengthHi;
	uint8_t Unk1;

	uint16_t PackedLenLo;
	uint8_t PackedLenHi;
	uint8_t Unk2;

	uint16_t RealLenLo;
	uint8_t RealLenHi;
	uint8_t Unk3;
    } *CompHeader;
    struct PhoenixFFVModule *Module;
    char Name[16], filename[24];
    char *ModuleName;
    int fd;
    uint32_t Length, PackedLen, RealLen;
    unsigned char *RealData;

    Module = (struct PhoenixFFVModule *) (BIOSImage + Offset);

    if (Module->Signature != 0xF8) {
	/* ignore and move on to the next byte... */
	return 1;
    }

    Length = (le16toh(Module->LengthHi) << 16) | Module->LengthLo;
    if ((Offset + ((le16toh(Module->LengthHi) << 16) | Module->LengthLo)) > BIOSLength) {
	fprintf(stderr, "Error: Module overruns buffer at 0x%05X\n",
		Offset);
	return 1;
    }

    if (Module->Compression == 0xF0) {
	strcpy(Name, "GAP");
	filename[0] = '\0';
    }
    else {
	/* get rid of the pesky 0xFF in the middle of the name */
	memcpy(Name, Module->Name, 8);
	memcpy(Name + 8, Module->Name + 9, 7);
	Name[15] = '\0';

	if (Name[0] == '_') {
	    ModuleName = PhoenixModuleNameGet(Name[1]);
	    if (ModuleName) {
		snprintf(filename, sizeof(filename), "%s_%c%c.rom", ModuleName, Name[2], Name[3]);
	    }
	    else {
		snprintf(filename, sizeof(filename), "%s.rom", Name);
	    }
	}
	else {
	    strncpy(filename, Name, sizeof(filename));
	}
    }

    printf("\t%-15s (%08X-%08X) %08X %02X %02X %s\n",
	   Name, Offset, Offset + Length, Length, Module->Flags, Module->Compression, filename
    );

    switch (Module->Compression) {
    case 0xF0:
	break;

    case 0x02:
	if (Name[1] == 'G') {
	    break;
	}
	else {
	    CompHeader = (struct PhoenixFFVCompressionHeader *) (BIOSImage + Offset + 0x18);
	}
	/* some blocks have a (8 byte?) header we need to skip */
	if (CompHeader->TotalLengthLo != Length - 0x18) {
	    CompHeader = (struct PhoenixFFVCompressionHeader *)
		((unsigned char *)CompHeader + CompHeader->TotalLengthLo);
	}
	PackedLen = (CompHeader->PackedLenHi << 16) | CompHeader->PackedLenLo;
	RealLen = (CompHeader->RealLenHi << 16) | CompHeader->RealLenLo;
	RealData = MMapOutputFile(filename, RealLen);
	if (!RealData) {
	    fprintf(stderr, "Failed to mmap file for uncompressed data.\n");
	    break;
	}
	LH5Decode((unsigned char *)CompHeader + sizeof(struct PhoenixFFVCompressionHeader),
		  PackedLen, RealData, RealLen);
	munmap(RealData, RealLen);
	break;

    case 0x01:
	fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
	    fprintf(stderr, "Error: unable to open %s: %s\n\n", filename, strerror(errno));
	    break;
	}
	write(fd, BIOSImage + Offset + 0x18, Length - 0x18);
	close(fd);
	break;

    default:
	fprintf(stderr, "\t\tUnsupported compression type!\n");
	break;
    }

    return Length;
}

void
PhoenixFFVDirectory(unsigned char *BIOSImage, int BIOSLength, int Offset)
{
    struct PhoenixVolumeDirEntry {
	/* these are stored little endian */
	uint32_t guid1;
	uint16_t guid2;
	uint16_t guid3;
	/* these are big endian */
	uint16_t guid4;
	/*uint48_t guid5;*/
	uint16_t guid5;
	uint32_t guid6;

	uint32_t Base;
	uint32_t Length;
    };
    struct PhoenixVolumeDir {
	uint16_t Unk1;
	uint16_t Unk2;
	uint32_t Length;
	struct PhoenixVolumeDirEntry Modules[];
    } *Volume;
    struct PhoenixFFVModule *Module;
    char Name[16], guid[37];
    int fd, HoleNum = 0;
    uint32_t Base, Length, NumModules, ModNum;

    Module = (struct PhoenixFFVModule *) (BIOSImage + Offset);
    Volume = (struct PhoenixVolumeDir *) (BIOSImage + Offset + 0x18);

    if (Module->Signature != 0xF8) {
	fprintf(stderr, "Error: Invalid module signature at 0x%05X\n",
		Offset);
	return;
    }

    Length = (le16toh(Module->LengthHi) << 16) | Module->LengthLo;
    if ((Offset + Length) > BIOSLength) {
	fprintf(stderr, "Error: Module overruns buffer at 0x%05X\n",
		Offset);
	return;
    }

    /* get rid of the pesky 0xFF in the middle of the name */
    memcpy(Name, Module->Name, 8);
    memcpy(Name + 8, Module->Name + 9, 7);
    Name[15] = '\0';
    if (strcmp(Name, "volumedir.bin2")) {
	fprintf(stderr, "FFV points to something other than volumedir.bin2: %s\n", Name);
	return;
    }

    NumModules = (Volume->Length - 8) / sizeof(struct PhoenixVolumeDirEntry);
    printf("FFV modules: %u\n", NumModules);

    for (ModNum = 0; ModNum < NumModules; ModNum++)
    {
	sprintf(guid, "%08X-%04X-%04X-%04X-%04X%08X",
		le32toh(Volume->Modules[ModNum].guid1),
		le16toh(Volume->Modules[ModNum].guid2),
		le16toh(Volume->Modules[ModNum].guid3),
		be16toh(Volume->Modules[ModNum].guid4),
		be16toh(Volume->Modules[ModNum].guid5),
		be32toh(Volume->Modules[ModNum].guid6)
	);
	Base = Volume->Modules[ModNum].Base & (BIOSLength - 1);
	Length = Volume->Modules[ModNum].Length;
	printf("[%2u]: (%08X-%08X) %s\n",
		ModNum, Base, Base + Length, guid
	);

	if (!strcmp(guid, "FED91FBA-D37B-4EEA-8729-2EF29FB37A78")) {
	    /* FFV modules */
	    Offset = Base;
	    while (Offset < Base + Length) {
		Offset += PhoenixExtractFFV(BIOSImage, BIOSLength, Offset);
	    }
	}
	else if (!strcmp(guid, "FD21E8FD-2525-4A95-BB90-47EC5763FF9E")) {
	    /* Extended System Configuration Data (and similar?) */
	    printf("\tESCD\n");
	    fd = open("ESCD.bin", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	    if (fd < 0) {
		fprintf(stderr, "Error: unable to open ESCD.bin: %s\n\n", strerror(errno));
		continue;
	    }
	    write(fd, BIOSImage + Base, Length);
	    close(fd);
	}
	else if (!strcmp(guid, "F6AE0F63-5F8C-4316-A2EA-76B9AF762756")) {
	    /* Raw BIOS code */
	    printf("\tHole (raw code)\n");
	    snprintf(Name, sizeof(Name), "hole_%02x.bin", HoleNum++);
	    fd = open(Name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	    if (fd < 0) {
		fprintf(stderr, "Error: unable to open %s: %s\n\n", Name, strerror(errno));
		continue;
	    }
	    write(fd, BIOSImage + Base, Length);
	    close(fd);
	}
	else {
	    fprintf(stderr, "\tUnknown FFV module GUID!\n");
	}
    }

    return;
}

Bool
PhoenixFFV(unsigned char *BIOSImage, int BIOSLength, struct PhoenixID *FFV)
{
    uint32_t Offset;

    Offset = le32toh(*((uint32_t *) (((char *) FFV) + 0xA))) & (BIOSLength - 1);

    if (!Offset) {
	fprintf(stderr, "BCPFFV module offset is NULL.\n");
	return FALSE;
    }

    PhoenixFFVDirectory(BIOSImage, BIOSLength, Offset);

    return TRUE;
}

/*
 *
 */
Bool
PhoenixExtract(unsigned char *BIOSImage, int BIOSLength, int BIOSOffset,
	       uint32_t Offset1, uint32_t BCPSegmentOffset)
{
    struct PhoenixID *ID, *SYS = NULL, *FFV = NULL;
    uint32_t Offset;

    printf("Found Phoenix BIOS \"%s\"\n", (char *) (BIOSImage + Offset1));

    for (ID = (struct PhoenixID *) (BIOSImage + BCPSegmentOffset + 10);
	 ((void *) ID < (void *) (BIOSImage + BIOSLength)) && ID->Name[0];
	 ID = (struct PhoenixID *) (((unsigned char *) ID) + le16toh(ID->Length))) {
#if 0
	printf("PhoenixID: Name %c%c%c%c%c%c, Flags 0x%04X, Length %d\n",
	       ID->Name[0],  ID->Name[1], ID->Name[2],  ID->Name[3],
	       ID->Name[4],  ID->Name[5], le16toh(ID->Flags), le16toh(ID->Length));
#endif
	if (!strncmp(ID->Name, "BCPSYS", 6))
	    SYS = ID;
	else if (!strncmp(ID->Name, "BCPFFV", 6))
	    FFV = ID;
    }

    if (!SYS) {
	fprintf(stderr, "Error: Failed to locate BCPSYS offset.\n");
	return FALSE;
    }

    /* Get some info */
    {
	char Date[9], Time[9], Version[9];

	strncpy(Date, ((char *) SYS) + 0x0F, 8);
	Date[8] = 0;
	strncpy(Time, ((char *) SYS) + 0x18, 8);
	Time[8] = 0;
	strncpy(Version, ((char *) SYS) + 0x37, 8);
	Version[8] = 0;

	printf("Version \"%s\", created on %s at %s.\n", Version, Date, Time);
    }

    Offset = le32toh(*((uint32_t *) (((char *) SYS) + 0x77)));
    Offset &= (BIOSLength - 1);
    if (!Offset) {
	fprintf(stderr, "BCPSYS module offset is NULL.\n");
	if (!FFV) {
	    return FALSE;
	}
	return PhoenixFFV(BIOSImage, BIOSLength, FFV);
    }

    while (Offset) {
	Offset = PhoenixModule(BIOSImage, BIOSLength, Offset);
	Offset &= BIOSLength - 1;
    }

    return TRUE;
}
