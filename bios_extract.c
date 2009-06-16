/*
 * Copyright 2009      Luc Verhaegen <libv@skynet.be>
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

#define _GNU_SOURCE /* memmem is useful */

#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "bios_extract.h"

static void
HelpPrint(char *name)
{
    printf("Program to extract AMI Bios images (AMIBIOS '94 and '95).\n\n");
    printf("Usage: %s <action> <filename>\n", name);
    printf("Actions:\n");
    printf("\"l\"\tList Bios Structure.\n");
    printf("\"x\"\tExtract Bios Modules.\n");
    printf("\"h\"\tPrint usage information.\n");
}

static char *
ArgumentsParse(int argc, char *argv[], Bool *Extract)
{
    char *FileName = NULL;
    Bool FoundAction = FALSE;
    int i;

    *Extract = FALSE;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "h"))
	    return NULL;
	else if (!strcmp(argv[i], "x")) {
	    if (!FoundAction) {
		*Extract = TRUE;
		FoundAction = TRUE;
	    } else {
		fprintf(stderr, "Error: wrong argument (%s)."
			" Please provide only one action.\n", argv[i]);
		return NULL;
	    }
	} else if (!strcmp(argv[i], "l")) {
	    if (!FoundAction) {
		*Extract = FALSE;
		FoundAction = TRUE;
	    } else {
		fprintf(stderr, "Error: wrong argument (%s)."
			" Please provide only one action.\n", argv[i]);
		return NULL;
	    }
	} else {
	    if (!FileName)
		FileName = argv[i];
	    else {
		fprintf(stderr, "Error: wrong argument (%s)."
			" Please provide only one filename.\n", argv[i]);
		return NULL;
	    }
	}
    }

    if (!FileName) {
	fprintf(stderr, "Error: Please provide a filename.\n");
	return NULL;
    }

    if (!FoundAction) {
	return NULL;
    }

    return FileName;
}

unsigned char *
MMapOutputFile(char *filename, int size)
{
    unsigned char* Buffer;
    int fd;

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
	fprintf(stderr, "Error: unable to open %s: %s\n\n",
		filename, strerror(errno));
	return NULL;
    }

    /* grow file */
    if (lseek(fd, size - 1, SEEK_SET) == -1) {
	fprintf(stderr, "Error: Failed to grow \"%s\": %s\n",
		filename, strerror(errno));
	close(fd);
	return NULL;
    }

    if (write(fd, "", 1) != 1) {
	fprintf(stderr, "Error: Failed to write to \"%s\": %s\n",
		filename, strerror(errno));
	close(fd);
	return NULL;
    }

    Buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (Buffer == ((void *) -1)) {
	fprintf(stderr, "Error: Failed to mmap %s: %s\n",
		filename, strerror(errno));
	close(fd);
	return NULL;
    }

    close(fd);

    return Buffer;
}

static struct {
    char *String1;
    char *String2;
    Bool (*Handler) (unsigned char *Image, int ImageLength, int ImageOffset,
		     Bool Extract, uint32_t Offset1, uint32_t Offset2);
} BIOSIdentification[] = {
    {"AMIBOOT ROM", "AMIBIOSC", AMI95Extract},
    {NULL, NULL, NULL},
};

/*
 *
 */
int
main(int argc, char *argv[])
{
    Bool Extract = FALSE;
    char *FileName = NULL;
    int FileLength = 0;
    uint32_t BIOSOffset = 0;
    unsigned char *BIOSImage = NULL;
    int fd;
    uint32_t Offset1, Offset2;
    int i, len;
    unsigned char *tmp;

    FileName = ArgumentsParse(argc, argv, &Extract);
    if (!FileName) {
	HelpPrint(argv[0]);
	return 1;
    }

    fd = open(FileName, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "Error: Failed to open %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

    FileLength = lseek(fd, 0, SEEK_END);
    if (FileLength < 0) {
	fprintf(stderr, "Error: Failed to lseek \"%s\": %s\n",
		FileName, strerror(errno));
	return 1;
    }
    BIOSOffset = 0x100000 - FileLength;

    BIOSImage = mmap(NULL, FileLength, PROT_READ, MAP_PRIVATE, fd, 0);
    if (BIOSImage < 0) {
	fprintf(stderr, "Error: Failed to mmap %s: %s\n",
		FileName, strerror(errno));
	return 1;
    }

    printf("Using file \"%s\" (%ukB)\n", FileName, FileLength >> 10);

    for (i = 0; BIOSIdentification[i].Handler; i++) {
	len = strlen(BIOSIdentification[i].String1);
	tmp = memmem(BIOSImage, FileLength - len, BIOSIdentification[i].String1, len);
	if (!tmp)
	    continue;
	Offset1 = tmp - BIOSImage;

	len = strlen(BIOSIdentification[i].String2);
	tmp = memmem(BIOSImage, FileLength - len, BIOSIdentification[i].String2, len);
	if (!tmp)
	    continue;
	Offset2 = tmp - BIOSImage;

	if (BIOSIdentification[i].Handler(BIOSImage, FileLength, BIOSOffset, Extract,
					  Offset1, Offset2))
	    return 0;
	else
	    return 1;
    }

    fprintf(stderr, "Error: Unable to detect BIOS Image type.\n");
    return 1;
}
