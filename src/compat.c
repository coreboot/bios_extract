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

#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
void *memmem(const void *haystack, size_t haystacklen, const void *needle,
	     size_t needlelen)
{
	char *searchpointer = (char *)haystack;
	char *patternpointer = (char *)needle;
	char *endofsearch = searchpointer + haystacklen - needlelen;

	if (!(haystack && needle && haystacklen && needlelen))
		return NULL;

	while (searchpointer <= endofsearch) {
		if (*searchpointer == *patternpointer)
			if (memcmp(searchpointer, patternpointer, needlelen) ==
			    0)
				return searchpointer;
		searchpointer++;
	}

	return NULL;
}

size_t strnlen(const char *s, size_t maxlen)
{
	const char *end = memchr(s, '\0', maxlen);
	return end ? (size_t) (end - s) : maxlen;
}

char *strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = malloc(len + 1);

	if (new == NULL)
		return NULL;

	new[len] = '\0';
	return memcpy(new, s, len);
}
#endif
