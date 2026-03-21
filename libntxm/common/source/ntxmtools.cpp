/*
 * libNTXM - XM Player Library for the Nintendo DS
 *
 *    Copyright (C) 2005-2008 Tobias Weyand (0xtob)
 *                         me@nitrotracker.tobw.net
 *
 */

/***** BEGIN LICENSE BLOCK *****
 * 
 * Version: Noncommercial zLib License / GPL 3.0
 * 
 * The contents of this file are subject to the Noncommercial zLib License 
 * (the "License"); you may not use this file except in compliance with
 * the License. You should have recieved a copy of the license with this package.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 3 or later (the "GPL"),
 * in which case the provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only under the terms of
 * either the GPL, and not to allow others to use your version of this file under
 * the terms of the Noncommercial zLib License, indicate your decision by
 * deleting the provisions above and replace them with the notice and other
 * provisions required by the GPL. If you do not delete the provisions above,
 * a recipient may use your version of this file under the terms of any one of
 * the GPL or the Noncommercial zLib License.
 * 
 ***** END LICENSE BLOCK *****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include "ntxm/common.h"

__attribute__((noreturn))
static inline void ntxm_crash(const char *text) {
#if defined(__NDS__)
	libndsCrash(text);
#else
	exit(1);
#endif
}

__attribute__((noreturn))
static void out_of_memory_error(const char *func, const char *file, int line) {
#ifdef DEBUG
	char text[256];
	text[sizeof(text) - 1] = 0;
	snprintf(text, sizeof(text) - 1, "%s() out of memory - %s:%d", func, file, line);
	ntxm_crash(text);
#else
	ntxm_crash(func);
#endif
}

#ifdef DEBUG
__attribute__((noreturn))
static void double_free_error(const char *file, int line) {
	char text[256];
	text[sizeof(text) - 1] = 0;
	snprintf(text, sizeof(text) - 1, "double free - %s:%d", file, line);
	libndsCrash(text);
}
#endif

void *__ntxm_cmalloc(size_t size, const char *file, int line) {
	void *ptr = malloc(size);
	if (ptr == NULL)
		out_of_memory_error("ntxm_cmalloc", file, line);
	return ptr;
}

void *__ntxm_crealloc(void *ptr, size_t size, const char *file, int line) {
	ptr = realloc(ptr, size);
	if (ptr == NULL)
		out_of_memory_error("ntxm_crealloc", file, line);
	return ptr;
}

void *__ntxm_ccalloc(size_t nelem, size_t size, const char *file, int line) {
	void *ptr = calloc(nelem, size);
	if (ptr == NULL)
		out_of_memory_error("ntxm_ccalloc", file, line);
	return ptr;
}

void *__ntxm_cmemalign(size_t align, size_t size, const char *file, int line) {
	void *ptr = memalign(align, size);
	if (ptr == NULL)
		out_of_memory_error("ntxm_cmemalign", file, line);
	return ptr;
}

char *__ntxm_cstrdup(const char *text, const char *file, int line) {
	char *ptr = strdup(text);
	if (ptr == NULL)
		out_of_memory_error("ntxm_cstrdup", file, line);
	return ptr;
}

void __ntxm_free(void *ptr, const char *file, int line) {
#ifdef DEBUG
	if (ptr == NULL)
		double_free_error(file, line);
#endif
	free(ptr);
}

#if defined(__3DS__) || defined(__NDS__)
/* https://devkitpro.org/viewtopic.php?f=6&t=3057 */

extern u8 *fake_heap_end;
extern u8 *fake_heap_start;

int ntxm_getFreeMem(void) {
	struct mallinfo mi = mallinfo();
	return mi.fordblks + (fake_heap_end - (u8*)sbrk(0));
}
#else
int ntxm_getFreeMem(void) { return 1048576; }
#endif

#include "ntxm/ntxmtools.h"

bool ntxm_isFileExists(const char *filename)
{
	bool res;
	FILE* f = fopen(filename,"r");
	if(f == NULL) {
		res = false;
	} else {
		fclose(f);
		res = true;
	}

	return res;
}

u32 ntxm_getFileSize(const char *filename)
{
	FILE *file = fopen(filename, "r");
	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fclose(file);
	return filesize;
}

void ntxm_unsigned2signed_8(uint8_t *buffer, size_t count)
{
	uint32_t *buf32 = (uint32_t*) buffer;
	size_t count32 = count >> 2;
	size_t i;

	for (i = 0; i < count32; i++) {
		buf32[i] ^= 0x80808080;
	}
	i <<= 2;
	for (; i < count; i++) {
		buffer[i] ^= 0x80;
	}
}

void ntxm_unsigned2signed_16(uint16_t *buffer, size_t count)
{
	uint32_t *buf32 = (uint32_t*) buffer;
	size_t count32 = count >> 1;

	for (size_t i = 0; i < count32; i++) {
		buf32[i] ^= 0x80008000;
	}
	if (count & 1) buffer[count - 1] ^= 0x8000;
}
