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

/*
 *
 */
Bool
PhoenixExtract(unsigned char *BIOSImage, int BIOSLength, int BIOSOffset,
	       uint32_t Offset1, uint32_t BCPSegmentOffset)
{
    struct PhoenixID {
	char Name[6];
	uint16_t Flags;
	uint16_t Length;
    } *ID;
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
	    break;
    }

    if (!ID->Name[0] || ((void *) ID >= (void *) (BIOSImage + BIOSLength))) {
	fprintf(stderr, "Error: Failed to locate BCPSYS offset.\n");
	return FALSE;
    }

    /* Get some info */
    {
	char Date[9], Time[9], Version[9];

	strncpy(Date, ((char *) ID) + 0x0F, 8);
	Date[8] = 0;
	strncpy(Time, ((char *) ID) + 0x18, 8);
	Time[8] = 0;
	strncpy(Version, ((char *) ID) + 0x37, 8);
	Version[8] = 0;

	printf("Version \"%s\", created on %s at %s.\n", Version, Date, Time);
    }

    Offset = le32toh(*((uint32_t *) (((char *) ID) + 0x77)));
    Offset &= (BIOSLength - 1);
    if (!Offset) {
	fprintf(stderr, "Error: retrieved invalid Modules offset.\n");
	return FALSE;
    }

    while (Offset) {
	Offset = PhoenixModule(BIOSImage, BIOSLength, Offset);
	Offset &= BIOSLength - 1;
    }

    return TRUE;
}
