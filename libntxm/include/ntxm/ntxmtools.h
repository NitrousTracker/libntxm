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

#ifndef _NTXMTOOLS_H_
#define _NTXMTOOLS_H_

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include "common.h"

static inline void ntxm_flush_dcache(void) {
#if defined(__NDS__)
#ifdef ARM9
	DC_FlushAll();
#endif
#endif
}

// ntxm_cmalloc() - checked malloc() - crashes on OOM
// ntxm_umalloc() - unchecked malloc() - can return null

extern void *__ntxm_cmalloc(size_t size, const char *file, int line);
extern void *__ntxm_crealloc(void *ptr, size_t size, const char *file, int line);
extern void *__ntxm_ccalloc(size_t nelem, size_t size, const char *file, int line);
extern void *__ntxm_cmemalign(size_t align, size_t size, const char *file, int line);
extern char *__ntxm_cstrdup(const char *text, const char *file, int line);
extern void __ntxm_free(void *ptr, const char *file, int line);
extern void *__ntxm_smpmalloc(size_t size, const char *file, int line);
extern void *__ntxm_smprealloc(void *ptr, size_t size, const char *file, int line);
extern void *__ntxm_smpcalloc(size_t nelem, size_t size, const char *file, int line);
extern void __ntxm_smpfree(void *ptr, const char *file, int line);

static inline void *ntxm_umalloc(size_t size) {
	return malloc(size);
}

static inline void *ntxm_urealloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

static inline void *ntxm_ucalloc(size_t nelem, size_t size) {
	return calloc(nelem, size);
}

static inline void *ntxm_umemalign(size_t align, size_t size) {
	return memalign(align, size);
}

static inline char *ntxm_ustrdup(const char *text) {
	return strdup(text);
}

#if defined(DEBUG)
#define ntxm_cmalloc(size) __ntxm_cmalloc(size, __FILE__, __LINE__)
#define ntxm_crealloc(ptr, size) __ntxm_crealloc(ptr, size, __FILE__, __LINE__)
#define ntxm_ccalloc(nelem, size) __ntxm_ccalloc(nelem, size, __FILE__, __LINE__)
#define ntxm_cmemalign(align, size) __ntxm_cmemalign(align, size, __FILE__, __LINE__)
#define ntxm_cstrdup(text) __ntxm_cstrdup(text, __FILE__, __LINE__)
#define ntxm_free(ptr) __ntxm_free(ptr, __FILE__, __LINE__)

#define ntxm_smpmalloc(size) __ntxm_smpmalloc(size, __FILE__, __LINE__)
#define ntxm_smprealloc(ptr, size) __ntxm_smprealloc(ptr, size, __FILE__, __LINE__)
#define ntxm_smpcalloc(nelem, size) __ntxm_smpcalloc(nelem, size, __FILE__, __LINE__)
#define ntxm_smpfree(ptr) __ntxm_smpfree(ptr, __FILE__, __LINE__)
#else
#define ntxm_cmalloc(size) __ntxm_cmalloc(size, NULL, 0)
#define ntxm_crealloc(ptr, size) __ntxm_crealloc(ptr, size, NULL, 0)
#define ntxm_ccalloc(nelem, size) __ntxm_ccalloc(nelem, size, NULL, 0)
#define ntxm_cmemalign(align, size) __ntxm_cmemalign(align, size, NULL, 0)
#define ntxm_cstrdup(text) __ntxm_cstrdup(text, NULL, 0)
#define ntxm_free(ptr) __ntxm_free(ptr, NULL, 0)

#define ntxm_smpmalloc(size) __ntxm_smpmalloc(size, NULL, 0)
#define ntxm_smprealloc(ptr, size) __ntxm_smprealloc(ptr, size, NULL, 0)
#define ntxm_smpcalloc(nelem, size) __ntxm_smpcalloc(nelem, size, NULL, 0)
#define ntxm_smpfree(ptr) __ntxm_smpfree(ptr, NULL, 0)
#endif

// mark non-ntxm-annotated functions as deprecated
void *malloc(size_t size) __attribute__((deprecated));
void *realloc(void *ptr, size_t size) __attribute__((deprecated));
void *calloc(size_t nelem, size_t size) __attribute__((deprecated));
char *strdup(const char *text) __attribute__((deprecated));
void *memalign(size_t align, size_t size) __attribute__((deprecated));
void free(void *ptr) __attribute__((deprecated));

// debug printf() calls
#if defined(DEBUG)
#define ntxm_dprintf printf
#else
static inline void ntxm_dprintf(...) {}
#endif 

int ntxm_getFreeMem(void);

bool ntxm_isFileExists(const char *name);

inline s32 ntxm_clamp(s32 val, s32 min, s32 max)
{
	if(val < min)
		return min;
	if(val > max)
		return max;
	return val;
}

u32 ntxm_getFileSize(const char *filename);

void ntxm_unsigned2signed_8(uint8_t *buffer, size_t count);
void ntxm_unsigned2signed_16(uint16_t *buffer, size_t count);

#endif
