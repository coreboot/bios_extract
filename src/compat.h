/*
 * Decompression utility for AMI BIOSes.
 *
 * Copyright (C) 2009-2010 coresystems GmbH
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

#ifdef __APPLE__
void *memmem(const void *haystack, size_t haystacklen, const void *needle,
	     size_t needlelen);
size_t strnlen(const char *s, size_t maxlen);
char *strndup(const char *s, size_t n);
#endif

// "endian.h" does not exist on (at least) these platforms:
// NetBSD, OSF/Tru64, HP-UX 10, Solaris, A/UX, Ultrix and
// AIX. It exists on FreeBSD, Linux and Irix.
#ifdef __linux__
#include <endian.h>
#include <sys/stat.h>
#elif __FreeBSD__
#include <sys/endian.h>
#include <sys/stat.h>
#endif

#if !defined(le32toh) || !defined(le16toh)
#if BYTE_ORDER == LITTLE_ENDIAN
#define le32toh(x) (x)
#define le16toh(x) (x)
#else
#include <byteswap.h>
#define le32toh(x) bswap_32(x)
#define le16toh(x) bswap_16(x)
#endif
#endif
