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
#include <sys/mman.h>

#include "compat.h"
#include "bios_extract.h"
#include "lh5_extract.h"

struct bcpHeader {
	char signature[6];
	uint8_t major_revision;
	uint8_t minor_revision;
	uint16_t length;
};

/* Our own structure just to store important parameters */
struct Phoenix {
	uint8_t version;
	uint8_t type;
	uint8_t compression;
};

static struct Phoenix phx = { 0, 0, 0 };

#define COMP_LZSS 0
#define COMP_LZARI 1
#define COMP_LZHUF 2
#define COMP_LZINT 3

struct bcpCompress {
	struct bcpHeader head;
	uint8_t flags;
	uint8_t alg;
	uint16_t unc_start_offset;
	uint32_t size_comp_data;
	uint16_t bcpiRamBiosStart;
	uint16_t bcpiWorkAreaStart;
	uint16_t bcpiLowMemStart;
	uint16_t bcpiLowMemSize;
	uint8_t commonCharacterLZSS;
	uint16_t oldRamBiosStart;
	uint16_t oldSetupScanStart;
	uint16_t oldSetupScanSize;
};

#define GUID_FFVMODULE "FED91FBA-D37B-4EEA-8729-2EF29FB37A78"
#define GUID_ESCD "FD21E8FD-2525-4A95-BB90-47EC5763FF9E"
#define GUID_RAWCODE "F6AE0F63-5F8C-4316-A2EA-76B9AF762756"

/* -------------- Phoenix module file type parsing -------------- */

/* See http://wiki.phoenix.com/wiki/index.php/EFI_FV_FILETYPE for
 * additional information */

struct PhoenixFFVFileType {
	uint8_t Id;
	char *Name;
};

static struct PhoenixFFVFileType
 PhoenixFFVFileTypes[] = {
	{0x00, "ALL"},
	{0x01, "BIN"},
	{0x02, "SECTION"},
	{0x03, "CEIMAIN"},
	{0x04, "PEIMAIN"},
	{0x05, "DXEMAIN"},
	{0x06, "PEI"},
	{0x07, "DXE"},
	{0x08, "COMBINED_PEIM_DRIVER"},
	{0x09, "APP"},
	{0x0B, "FFV"},
	{0xC2, "CEI"},
	{0xC3, "XIP"},
	{0xC4, "BB"},
	{0xD0, "SDXE"},
	{0xD1, "DXESDXE"},
	{0xF0, "GAP"},
	{0, NULL},
};

static char *get_file_type(uint8_t id)
{
	short i = 0;
	while (PhoenixFFVFileTypes[i].Name != NULL) {
		if (PhoenixFFVFileTypes[i].Id == id)
			return PhoenixFFVFileTypes[i].Name;
		i++;
	}
	return "UNKNOWN";
}

/* -------------- Phoenix section file type parsing -------------- */

/* See http://wiki.phoenix.com/wiki/index.php/EFI_SECTION_TYPE for
 * additional information */

struct PhoenixFFVSectionType {
	uint8_t Id;
	char *Name;
};

static struct PhoenixFFVSectionType PhoenixFFVSectionTypes[] = {
	{0x01, "COMPRESSION"},
	{0x02, "GUID_DEFINED"},
	{0x10, "PE32"},
	{0x11, "PIC"},
	{0x12, "TE"},
	{0x13, "DXE_DEPEX"},
	{0x14, "VERSION"},
	{0x15, "USER_INTERFACE"},
	{0x16, "COMPATIBILITY16"},
	{0x17, "FIRMWARE_VOLUME_IMAGE"},
	{0x18, "FREEFORM_SUBTYPE_GUID"},
	{0x19, "BIN"},
	{0x1A, "PE64"},
	{0x1B, "PEI_DEPEX"},
	{0xC0, "SOURCECODE"},
	{0xC1, "FFV"},
	{0xC2, "RE32"},
	{0xC3, "XIP16"},
	{0xC4, "XIP32"},
	{0xC5, "XIP64"},
	{0xC6, "PLACE16"},
	{0xC7, "PLACE32"},
	{0xC8, "PLACE64"},
	{0xCF, "PCI_DEVICE"},
	{0xD0, "PDB"},
	{0, NULL},
};

static char *get_section_type(uint8_t id)
{
	short i = 0;
	while (PhoenixFFVSectionTypes[i].Name != NULL) {
		if (PhoenixFFVSectionTypes[i].Id == id)
			return PhoenixFFVSectionTypes[i].Name;
		i++;
	}
	return "UNKNOWN";
}

/* -------------- Phoenix module name parsing -------------- */

struct PhoenixModuleName {
	char Id;
	char *Name;
};

static struct PhoenixModuleName PhoenixModuleNames[] = {
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
	{'H', "tcpa_H"},	/* TCPA (Trusted Computing), USBKCLIB? */
	{'K', "tcpa_K"},	/* TCPA (Trusted Computing), "AUTH"? */
	{'Q', "tcpa_Q"},	/* TCPA (Trusted Computing), "SROM"? */
	{'<', "tcpa_<"},
	{'*', "tcpa_*"},
	{'?', "tcpa_?"},
	{'$', "biosentry"},
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
	uint16_t Checksum;	/* Can be splitted to header and data checksums */
	uint16_t LengthLo;
	uint8_t LengthHi;
	uint8_t FileType;
	char Name[16];		/* GUID name */
};

struct PhoenixFFVSectionHeader {
	uint16_t SizeLo;
	uint8_t SizeHi;
	uint8_t Type;
};

struct PhoenixFFVCompressionHeader {
	uint16_t TotalLengthLo;
	uint8_t TotalLengthHi;
	uint8_t CompType;
	uint16_t PackedLenLo;
	uint8_t PackedLenHi;
	uint8_t Unk2;
	uint16_t RealLenLo;
	uint8_t RealLenHi;
	uint8_t Unk3;
};

static char *PhoenixModuleNameGet(char Id)
{
	int i;

	for (i = 0; PhoenixModuleNames[i].Name; i++)
		if (PhoenixModuleNames[i].Id == Id)
			return PhoenixModuleNames[i].Name;
	return NULL;
}

static void
phx_write_file(unsigned char *BIOSImage, char *filename, short filetype,
	       int offset, uint32_t length)
{
	int fd;

	if (filename[0] == '\0') {
		sprintf(filename, "%s_0x%08x-0x%08x", get_file_type(filetype),
			offset, offset + length);
	}
	fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Error: unable to open %s: %s\n\n", filename,
			strerror(errno));
		return;
	}
	write(fd, BIOSImage + offset + 0x18, length - 0x18);
	close(fd);
}

/* ---------- Extraction code ---------- */

static int PhoenixModule(unsigned char *BIOSImage, int BIOSLength, int Offset)
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

	Module = (struct PhoenixModule *)(BIOSImage + Offset);

	if (Module->Signature[0] || (Module->Signature[1] != 0x31)
	    || (Module->Signature[2] != 0x31)) {
		fprintf(stderr, "Error: Invalid module signature at 0x%05X\n",
			Offset);
		return 0;
	}

	if ((Offset + Module->HeadLen + 4 + le32toh(Module->FragLength)) >
	    BIOSLength) {
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
			fprintf(stderr,
				"Error: First fragment exceeds total size at %05X\n",
				Offset);
			return le32toh(Module->Previous);
		}

		/* This over-allocates, but the total compressed size is not available here */
		ModuleData = malloc(le32toh(Module->ExpLen));
		if (!ModuleData) {
			fprintf(stderr,
				"Error: Can't reassemble fragments, no memory for %d bytes\n",
				le32toh(Module->ExpLen));
			return le32toh(Module->Previous);
		}

		memcpy(ModuleData, BIOSImage + Offset + Module->HeadLen,
		       FragLength);

		Packed = FragLength;
		FragOffset = le32toh(Module->NextFrag) & (BIOSLength - 1);

		printf("extra fragments: ");
		while (FragOffset) {
			Fragment =
			    (struct PhoenixFragment *)(BIOSImage + FragOffset);
			FragLength = le32toh(Fragment->FragLength);
			printf("(%05X, %d bytes) ", FragOffset, FragLength);

			if (Packed + FragLength > le32toh(Module->ExpLen)) {
				fprintf(stderr,
					"\nFragment too big at %05X for %05X\n",
					FragOffset, Offset);
				free(ModuleData);
				return le32toh(Module->Previous);
			}
			memcpy(ModuleData + Packed, BIOSImage + FragOffset + 9,
			       FragLength);
			Packed += FragLength;
			FragOffset =
			    le32toh(Fragment->NextFrag) & (BIOSLength - 1);
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
		fprintf(stderr, "Error: unable to open %s: %s\n\n", filename,
			strerror(errno));
		free(filename);
		if ((le32toh(Module->NextFrag) & 0xF0000000) == 0xF0000000)
			free(ModuleData);
		return le32toh(Module->Previous);
	}

	switch (Module->Compression) {
	case 5:		/* LH5 */
		printf("0x%05X (%6d bytes)   ->   %s\t(%d bytes)",
		       Offset + Module->HeadLen + 4, Packed, filename,
		       le32toh(Module->ExpLen));
		Buffer = MMapOutputFile(filename, le32toh(Module->ExpLen));
		if (!Buffer)
			break;

		/* The first 4 bytes of the LH5 packing method is just the total
		 *      expanded length; skip them */
		LH5Decode(ModuleData + 4, Packed - 4, Buffer,
			  le32toh(Module->ExpLen));
		munmap(Buffer, le32toh(Module->ExpLen));
		break;

		/* case 3 *//* LZSS */
	case 0:		/* not compressed at all */
		printf("0x%05X (%6d bytes)   ->   %s", Offset + Module->HeadLen,
		       Packed, filename);
		write(fd, ModuleData, Packed);
		break;

	default:
		fprintf(stderr, "Unsupported compression type for %s: %d\n",
			filename, Module->Compression);
		printf("0x%05X (%6d bytes)   ->   %s\t(%d bytes)",
		       Offset + Module->HeadLen, Packed, filename,
		       le32toh(Module->ExpLen));
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
		printf("\t [0x%04X:0x%04X]\n", le16toh(Module->Segment) << 12,
		       le16toh(Module->Offset));
	} else
		printf("\n");

	return le32toh(Module->Previous);
}

static int
PhoenixExtractFFV(unsigned char *BIOSImage, int BIOSLength, int Offset)
{
	struct PhoenixFFVSectionHeader *SectionHeader;
	struct PhoenixFFVCompressionHeader *CompHeader;
	struct PhoenixFFVModule *Module;
	char Name[16], filename[24];
	char *ModuleName;
	uint32_t Length, PackedLen, RealLen;
	unsigned char *RealData;

	Module = (struct PhoenixFFVModule *)(BIOSImage + Offset);

	if (Module->Signature != 0xF8) {
		/* ignore and move on to the next byte... */
		return 1;
	}

	Length = ((le16toh(Module->LengthHi) << 16) | Module->LengthLo) - 1;
	if ((Offset + Length) >= BIOSLength) {
		fprintf(stderr, "Error: Module overruns buffer at 0x%05X\n",
			Offset);
		return 1;
	}

	/* TODO: Improve module name parsing */
	if (Module->FileType == 0xF0) {
		strcpy(Name, "GAP");
		filename[0] = '\0';
	} else if ((uint8_t) Module->Name[8] != 0xFF) {
		strcpy(Name, "GUID?");
		filename[0] = '\0';
	} else {
		/* get rid of the pesky 0xFF in the middle of the name */
		memcpy(Name, Module->Name, 8);
		memcpy(Name + 8, Module->Name + 9, 7);
		Name[15] = '\0';

		if (Name[0] == '_' && strlen(Name) == 4) {
			ModuleName = PhoenixModuleNameGet(Name[1]);
			if (ModuleName) {
				snprintf(filename, sizeof(filename),
					 "%s_%c%c.rom", ModuleName, Name[2],
					 Name[3]);
			} else {
				snprintf(filename, sizeof(filename), "%s.rom",
					 Name);
			}
		} else {
			strncpy(filename, Name, sizeof(filename));
		}
	}

	printf("\t%-15s (%08X-%08X) %08X %02X %02X %s [%s]\n",
	       Name, Offset, Offset + Length, Length, Module->Flags,
	       Module->FileType, filename, get_file_type(Module->FileType));

	switch (Module->FileType) {
	case 0xF0:
		break;

		/* ---------- SECTION file type ---------- */
	case 0x02:
		SectionHeader =
		    (struct PhoenixFFVSectionHeader *)(BIOSImage + Offset +
						       0x18);
		if (Name[1] == 'G' || !*filename) {
			break;
		}

		/* COMPRESSION section */
		if (SectionHeader->Type == 0x01) {
			CompHeader =
			    (struct PhoenixFFVCompressionHeader *)(BIOSImage +
								   Offset +
								   0x18);
			/* some blocks have a (8 byte?) header we need to skip */
			if (CompHeader->TotalLengthLo != Length - 0x18
			    && CompHeader->Unk3) {
				/* FIXME more advanced parsing of sections */
				CompHeader =
				    (struct PhoenixFFVCompressionHeader *)
				    ((unsigned char *)CompHeader +
				     CompHeader->TotalLengthLo);
			}
			PackedLen =
			    (CompHeader->
			     PackedLenHi << 16) | CompHeader->PackedLenLo;
			RealLen =
			    (CompHeader->
			     RealLenHi << 16) | CompHeader->RealLenLo;
			//printf("CompHeader->Type = %d\n", CompHeader->CompType);

			if (CompHeader->CompType == 0)	/* Not compressed at all */
				break;

			if (!RealLen)	/* FIXME temporary hack */
				break;

			RealData = MMapOutputFile(filename, RealLen);
			if (!RealData) {
				fprintf(stderr,
					"Failed to mmap file for uncompressed data.\n");
				break;
			}
			if ((phx.compression == COMP_LZHUF)
			    || (phx.compression == COMP_LZINT)) {
				if (LH5Decode
				    ((unsigned char *)CompHeader +
				     sizeof(struct PhoenixFFVCompressionHeader),
				     PackedLen, RealData, RealLen) == -1) {
					munmap(RealData, RealLen);
					fprintf(stderr,
						"Failed to uncompress section with LHA5.\n");
					/* dump original section in this case */
					phx_write_file(BIOSImage, filename,
						       Module->FileType, Offset,
						       Length);
				} else
					printf("COMPRESSED\n");
			} else
				printf("Unsupported compression!\n");
			munmap(RealData, RealLen);
			break;
		}
		printf("\t\tSECTION: %s\n",
		       get_section_type(SectionHeader->Type));
		phx_write_file(BIOSImage, filename, Module->FileType, Offset,
			       Length);
		break;

	default:
		phx_write_file(BIOSImage, filename, Module->FileType, Offset,
			       Length);
		break;
	}
	return Length;
}

/* Parse initial volumedir layout:
 * - 1 byte Type indicates either raw code or an FFV module
 * - 4 byte Base provides the offset into the image to find the specified volume
 * - 4 byte Length
 */
void
PhoenixVolume1(unsigned char *BIOSImage, int BIOSLength, int Offset, int ModLen)
{
	struct PhoenixVolumeDirEntry {
		uint8_t Type;
		uint32_t Base;
		uint32_t Length;
	} *Modules;

	char Name[16];
	int fd, HoleNum = 0;
	uint8_t Type;
	uint32_t Base, Length, NumModules, ModNum;

	Modules = (struct PhoenixVolumeDirEntry *)(BIOSImage + Offset + 0x18);
	NumModules = (ModLen - 0x18) / sizeof(struct PhoenixVolumeDirEntry);

	printf("FFV modules: %u\n", NumModules);

	for (ModNum = 0; ModNum < NumModules; ModNum++) {
		Type = Modules[ModNum].Type;
		Base = Modules[ModNum].Base & (BIOSLength - 1);
		Length = Modules[ModNum].Length - 1;
		printf("[%2u]: (%08X-%08X) %02x\n", ModNum, Base, Base + Length,
		       Type);

		switch (Type) {
		case 0x01:
			printf("\tHole (raw code)\n");
			snprintf(Name, sizeof(Name), "hole_%02x.bin",
				 HoleNum++);
			fd = open(Name, O_RDWR | O_CREAT | O_TRUNC,
				  S_IRUSR | S_IWUSR);
			if (fd < 0) {
				fprintf(stderr,
					"Error: unable to open %s: %s\n\n",
					Name, strerror(errno));
				continue;
			}
			write(fd, BIOSImage + Base, Length);
			close(fd);
			break;

		case 0x02:
			/* FFV modules */
			Offset = Base;
			while (Offset < Base + Length) {
				Offset +=
				    PhoenixExtractFFV(BIOSImage, BIOSLength,
						      Offset);
			}
			break;
		}
	}
}

/* Parse GUID-based volumedir layout:
 * - 4 bytes of unknown data
 * - 4 byte Length
 * - array of module entries:
 *   - 16 byte GUID indicating module type
 *   - 4 byte Base
 *   - 4 byte Length
 */
void PhoenixVolume2(unsigned char *BIOSImage, int BIOSLength, int Offset)
{
	struct PhoenixVolumeDirEntry2 {
		/* these are stored little endian */
		uint32_t guid1;
		uint16_t guid2;
		uint16_t guid3;
		/* these are big endian */
		uint16_t guid4;
		/*uint48_t guid5; */
		uint16_t guid5;
		uint32_t guid6;
		uint32_t Base;
		uint32_t Length;
	};

	struct PhoenixVolumeDir2 {
		uint16_t Unk1;
		uint16_t Unk2;
		uint32_t Length;
		struct PhoenixVolumeDirEntry2 Modules[];
	} *Volume;

	char Name[16], guid[37];
	int fd, HoleNum = 0;
	uint32_t Base, Length, NumModules, ModNum;

	Volume = (struct PhoenixVolumeDir2 *)(BIOSImage + Offset + 0x18);
	NumModules =
	    (Volume->Length - 8) / sizeof(struct PhoenixVolumeDirEntry2);

	printf("FFV modules: %u\n", NumModules);

	for (ModNum = 0; ModNum < NumModules; ModNum++) {
		sprintf(guid, "%08X-%04X-%04X-%04X-%04X%08X",
			le32toh(Volume->Modules[ModNum].guid1),
			le16toh(Volume->Modules[ModNum].guid2),
			le16toh(Volume->Modules[ModNum].guid3),
			be16toh(Volume->Modules[ModNum].guid4),
			be16toh(Volume->Modules[ModNum].guid5),
			be32toh(Volume->Modules[ModNum].guid6)
		    );
		Base = Volume->Modules[ModNum].Base & (BIOSLength - 1);
		Length = Volume->Modules[ModNum].Length - 1;
		printf("[%2u]: (%08X-%08X) %s\n", ModNum, Base, Base + Length,
		       guid);

		if (!strcmp(guid, GUID_FFVMODULE)) {
			/* FFV modules */
			Offset = Base;
			while (Offset < Base + Length) {
				Offset +=
				    PhoenixExtractFFV(BIOSImage, BIOSLength,
						      Offset);
			}
		} else if (!strcmp(guid, GUID_ESCD)) {
			/* Extended System Configuration Data (and similar?) */
			printf("\tESCD\n");
			fd = open("ESCD.bin", O_RDWR | O_CREAT | O_TRUNC,
				  S_IRUSR | S_IWUSR);
			if (fd < 0) {
				fprintf(stderr,
					"Error: unable to open ESCD.bin: %s\n\n",
					strerror(errno));
				continue;
			}
			write(fd, BIOSImage + Base, Length);
			close(fd);
		} else if (!strcmp(guid, GUID_RAWCODE)) {
			/* Raw BIOS code */
			printf("\tHole (raw code)\n");
			snprintf(Name, sizeof(Name), "hole_%02x.bin",
				 HoleNum++);
			fd = open(Name, O_RDWR | O_CREAT | O_TRUNC,
				  S_IRUSR | S_IWUSR);
			if (fd < 0) {
				fprintf(stderr,
					"Error: unable to open %s: %s\n\n",
					Name, strerror(errno));
				continue;
			}
			write(fd, BIOSImage + Base, Length);
			close(fd);
		} else {
			fprintf(stderr, "\tUnknown FFV module GUID: %s\n",
				guid);
		}
	}
}

void PhoenixFFVDirectory(unsigned char *BIOSImage, int BIOSLength, int Offset)
{
	char Name[16];
	uint32_t Length;
	struct PhoenixFFVModule *Module;

	Module = (struct PhoenixFFVModule *)(BIOSImage + Offset);

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
	if (!strcmp(Name, "volumedir.bin")) {
		PhoenixVolume1(BIOSImage, BIOSLength, Offset, Length);
	} else if (!strcmp(Name, "volumedir.bin2")) {
		PhoenixVolume2(BIOSImage, BIOSLength, Offset);
	} else {
		fprintf(stderr,
			"FFV points to something other than the volumedir: %s\n",
			Name);
	}
}

Bool PhoenixFFV(unsigned char *BIOSImage, int BIOSLength, struct PhoenixID *FFV)
{
	uint32_t Offset;

	Offset =
	    le32toh(*((uint32_t *) (((char *)FFV) + 0xA))) & (BIOSLength - 1);

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

	printf("Found Phoenix BIOS \"%s\"\n", (char *)(BIOSImage + Offset1));

	/* TODO: Print more information about image */
	/* TODO: Group modules by firmware volumes */

	/*
	 * For newer Phoenix BIOSes, the BIOS has a trailing block that does not
	 * match the signature as tested in PhoenixModule. We adjust the length
	 * variable to handle that scenario. For example try the new BIOSes for the
	 * SuperMicro motherboards X7DA8 and X7DB8. X7DB8 is supported by Coreboot.
	 */
	if (BIOSLength > 0x100000 && BIOSOffset > 0) {
		BIOSLength = BIOSLength + BIOSOffset - 0x100000;
	}

	for (ID = (struct PhoenixID *)(BIOSImage + BCPSegmentOffset + 10);
	     ((void *)ID < (void *)(BIOSImage + BIOSLength)) && ID->Name[0];
	     ID =
	     (struct PhoenixID *)(((unsigned char *)ID) +
				  le16toh(ID->Length))) {
#if 0
		printf
		    ("PhoenixID: Name %c%c%c%c%c%c, Flags 0x%04X, Length %d\n",
		     ID->Name[0], ID->Name[1], ID->Name[2], ID->Name[3],
		     ID->Name[4], ID->Name[5], le16toh(ID->Flags),
		     le16toh(ID->Length));
#endif
		if (!strncmp(ID->Name, "BCPSYS", 6)) {
			SYS = ID;
			if (FFV)
				break;
		} else if (!strncmp(ID->Name, "BCPFFV", 6)) {
			FFV = ID;
			if (SYS)
				break;
		}
	}

	if (!SYS) {
		fprintf(stderr, "Error: Failed to locate BCPSYS offset.\n");
		return FALSE;
	}

	/* BCPCMP parsing */

	unsigned char *bcpcmp = memmem(BIOSImage, BIOSLength - 6, "BCPCMP", 6);
	if (!bcpcmp) {
		fprintf(stderr, "Error: Failed to locate BCPCMP offset.\n");
		return FALSE;
	}

	uint32_t bcpoff = bcpcmp - BIOSImage;
	struct bcpCompress *bcpComp =
	    (struct bcpCompress *)(BIOSImage + bcpoff);
	phx.compression = bcpComp->alg;

	/* Get some info */
	char Date[9], Time[9], Version[9];

	strncpy(Date, ((char *)SYS) + 0x0F, 8);
	Date[8] = 0;
	strncpy(Time, ((char *)SYS) + 0x18, 8);
	Time[8] = 0;
	strncpy(Version, ((char *)SYS) + 0x37, 8);
	Version[8] = 0;

	printf("Version \"%s\", created on %s at %s.\n", Version, Date, Time);

	Offset = le32toh(*((uint32_t *) (((char *)SYS) + 0x77)));
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
