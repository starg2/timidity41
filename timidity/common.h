/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   common.h
*/

#ifndef ___COMMON_H_
#define ___COMMON_H_

#include "sysdep.h"
#include "url.h"
#include "mblock.h"
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#elif defined(HAVE_STRING_H)
#include <string.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifndef VERSION
#define VERSION "current"
#endif



#ifdef TIMIDITY_LEAK_CHECK
#define safe_malloc(s)           (safe_ptrchk(malloc(s)))
#define safe_realloc(p, s)       (safe_ptrchk(realloc(p, s)))
#define safe_calloc(n, s)        (safe_ptrchk(calloc(n, s)))
#define safe_large_malloc(s)     (safe_ptrchk(malloc(s)))
#define safe_large_realloc(p, s) (safe_ptrchk(realloc(p, s)))
#define safe_large_calloc(n, s)  (safe_ptrchk(calloc(n, s)))
#define safe_strdup(s)           ((char*)safe_ptrchk(strdup(s ? s : "")))
#endif
#ifndef HAVE_SAFE_FREE
#define safe_free free
#endif





#define FILEPATH_MAX 32000

extern char *program_name, current_filename[];
extern const char *note_name[];

typedef struct {
  char *path;
  void *next;
} PathList;

struct timidity_file
{
    URL url;
    char *tmpname;
};

/* Noise modes for open_file */
#define OF_SILENT	0
#define OF_NORMAL	1
#define OF_VERBOSE	2


#ifdef __cplusplus
extern "C" {
#endif
	
extern void add_to_pathlist(const char *s);
extern void clean_up_pathlist(void);
extern int is_url_prefix(const char *name);
extern struct timidity_file *open_file(const char *name, int decompress,
                                       int noise_mode);
extern struct timidity_file *open_file_r(const char *name, int decompress,
                                         int noise_mode);
extern struct timidity_file *open_with_mem(char *mem, ptr_size_t memlen,
                                           int noise_mode);
extern struct timidity_file *open_with_constmem(const char *mem, ptr_size_t memlen,
                                           int noise_mode);
extern void close_file(struct timidity_file *tf);
extern void skip(struct timidity_file *tf, size_t len);
extern char *tf_gets(char *buff, size_t n, struct timidity_file *tf);
#define tf_getc(tf) (url_getc((tf)->url))
extern size_t tf_read(void *buff, size_t size, size_t nitems,
                      struct timidity_file *tf);
extern off_size_t tf_seek(struct timidity_file *tf, off_size_t offset, int whence);
extern off_size_t tf_seek_uint64(struct timidity_file *tf, uint64 offset, int whence);
extern off_size_t tf_tell(struct timidity_file *tf);
extern int int_rand(int n); /* random [0..n-1] */
extern int check_file_extension(const char *filename, char *ext, int decompress);

#ifdef TIMIDITY_LEAK_CHECK
extern void *safe_ptrchk(void *ptr);
#endif
#ifndef safe_malloc
extern void *safe_malloc(size_t count);
#endif
#ifndef safe_realloc
extern void *safe_realloc(void *old_ptr, size_t new_size);
#endif
#ifndef safe_calloc
extern void *safe_calloc(size_t n, size_t count);
#endif
#ifndef safe_large_malloc
extern void *safe_large_malloc(size_t count);
#endif
///r
#ifndef safe_large_realloc
extern void *safe_large_realloc(void *old_ptr, size_t new_size);
#endif
#ifndef safe_large_calloc
extern void *safe_large_calloc(size_t n, size_t count);
#endif
#ifndef safe_strdup
extern char *safe_strdup(const char *s);
#endif
#ifndef safe_free
extern void safe_free(void *ptr);
#endif

#ifndef aligned_malloc
extern void *aligned_malloc(size_t count, size_t align_size);
#endif
#ifndef aligned_free
extern void aligned_free(void *ptr);
#endif

extern void free_ptr_list(void *ptr_list, int count);
extern int string_to_7bit_range(const char *s, int *start, int *end);
extern char **expand_file_archives(char **files, int *nfiles_in_out);
extern void randomize_string_list(char **strlist, int nstr);
extern int pathcmp(const char *path1, const char *path2, int ignore_case);
extern void sort_pathname(char **files, int nfiles);
extern int load_table(const char *file);
extern char *pathsep_strrchr(const char *path);
extern char *pathsep_strchr(const char *path);
extern int str2mID(const char *str);
extern size_t floatpoint_grooming(char *str);
extern int fp_equals(float a, float b, float tolerance);
#define FP_EQ(a, b) (fp_equals(a, b, 0.001f))
#define FP_EQ_0(a) (FP_EQ(a, 0.0f))
#define FP_NE_0(a) ((a) > 0.0 || (a) < 0.0)


/* code:
 * "EUC"	- Extended Unix Code
 * NULL		- Auto conversion
 * "JIS"	- Japanese Industrial Standard code
 * "SJIS"	- shift-JIS code
 */
extern void code_convert(char *in, char *out, size_t outsiz,
			 char *in_code, char *out_code);

extern void safe_exit(int status);

extern const char *timidity_version;
extern const char *timidity_compile_date;
extern const char *arch_string; /* optcode.c */
extern MBlockList tmpbuffer;
extern char *output_text_code;


#ifdef __W32__
extern int w32_reset_dll_directory(void);
extern char *w32_mbs_to_utf8(const char *str);
extern char *w32_utf8_to_mbs(const char *str);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ___COMMON_H_ */
