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
#include "compat.h"
#include "bios_extract.h"

static void
HelpPrint(char *name)
{
    printf("\n");
    printf("Program to extract compressed modules from BIOS images.\n");
    printf("Supports AMI, Award, Asus and Phoenix BIOSes.\n");
    printf("\n");
    printf("Usage:\n\t%s <filename>\n", name);
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
		     uint32_t Offset1, uint32_t Offset2);
} BIOSIdentification[] = {
    {"AMIBOOT ROM", "AMIBIOSC", AMI95Extract},
    {"$ASUSAMI$", "AMIBIOSC", AMI95Extract},
    {"AMIEBBLK", "AMIBIOSC", AMI95Extract},
    {"Award BootBlock", "= Award Decompression Bios =", AwardExtract},
    {"Phoenix FirstBIOS", "BCPSEGMENT", PhoenixExtract},
    {"PhoenixBIOS 4.0", "BCPSEGMENT", PhoenixExtract},
    {"Phoenix ServerBIOS 3", "BCPSEGMENT", PhoenixExtract},
    {"Phoenix TrustedCore", "BCPSEGMENT", PhoenixExtract},
    {NULL, NULL, NULL},
};

/*
 *
 */
int
main(int argc, char *argv[])
{
    int FileLength = 0;
    uint32_t BIOSOffset = 0;
    unsigned char *BIOSImage = NULL;
    int fd;
    uint32_t Offset1, Offset2;
    int i, len;
    unsigned char *tmp;

    if ((argc != 2) || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")){ 
	HelpPrint(argv[0]);
	return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "Error: Failed to open %s: %s\n",
		argv[1], strerror(errno));
	return 1;
    }

    FileLength = lseek(fd, 0, SEEK_END);
    if (FileLength < 0) {
	fprintf(stderr, "Error: Failed to lseek \"%s\": %s\n",
		argv[1], strerror(errno));
	return 1;
    }

    BIOSOffset = (0x100000 - FileLength) & 0xFFFFF;

    BIOSImage = mmap(NULL, FileLength, PROT_READ, MAP_PRIVATE, fd, 0);
    if (BIOSImage < 0) {
	fprintf(stderr, "Error: Failed to mmap %s: %s\n",
		argv[1], strerror(errno));
	return 1;
    }

    printf("Using file \"%s\" (%ukB)\n", argv[1], FileLength >> 10);

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

	if (BIOSIdentification[i].Handler(BIOSImage, FileLength, BIOSOffset,
					  Offset1, Offset2))
	    return 0;
	else
	    return 1;
    }

    fprintf(stderr, "Error: Unable to detect BIOS Image type.\n");
    return 1;
}
