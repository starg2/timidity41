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
*/

#ifndef ___RECACHE_H_
#define ___RECACHE_H_

#ifndef TIMIDITY_H_INCLUDED
#define TIMIDITY_H_INCLUDED 1
#include "timidity.h"
#endif

///r
#if defined(DATA_T_DOUBLE)

#if 0 // SAMPLE_TYPE_FLOAT // cache_t float
typedef float cache_t; // cache_t float
#define CACHE_DATA_TYPE (SAMPLE_TYPE_FLOAT) // SAMPLE_TYPE_FLOAT
#else // SAMPLE_TYPE_DOUBLE // cache_t double
typedef double cache_t; // cache_t double
#define CACHE_DATA_TYPE (SAMPLE_TYPE_DOUBLE) // SAMPLE_TYPE_DOUBLE
#endif

#elif defined(DATA_T_FLOAT)

typedef float cache_t; // cache_t float
#define CACHE_DATA_TYPE (SAMPLE_TYPE_FLOAT) // SAMPLE_TYPE_FLOAT

#else // DATA_T_INT32

#if defined(LOOKUP_HACK)
typedef sample_t cache_t; // sample_t
#define CACHE_DATA_TYPE (SAMPLE_TYPE_INT16) // SAMPLE_TYPE_INT16
#elif 0 // SAMPLE_TYPE_INT16 // cache_t sample_t
typedef sample_t cache_t;
#define CACHE_DATA_TYPE (SAMPLE_TYPE_INT16)
#else // SAMPLE_TYPE_INT32 // cache_t int32
typedef int32 cache_t;
#define CACHE_DATA_TYPE (SAMPLE_TYPE_INT32)
#endif // cache_resampling() check loop_connect()

#endif

struct cache_hash
{
    /* cache key */
    int note;
    Sample *sp;

    int32 cnt;			/* counter */
    double r;			/* size/refcnt */

    struct _Sample *resampled;
    struct cache_hash *next;
};

extern void resamp_cache_reset(void);
extern void resamp_cache_refer_on(Voice *vp, int32 sample_start);
extern void resamp_cache_refer_off(int ch, int note, int32 sample_end);
extern void resamp_cache_refer_alloff(int ch, int32 sample_end);
extern void resamp_cache_create(void);
extern struct cache_hash *resamp_cache_fetch(struct _Sample *sp, int note);
extern void free_cache_data(void);

extern int32 allocate_cache_size;

#endif /* ___RECACHE_H_ */
