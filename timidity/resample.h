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

    resample.h
*/

#ifndef ___RESAMPLE_H_
#define ___RESAMPLE_H_

#ifdef HAVE_CONFIH_H
#include "config.h"
#endif

#ifndef TIMIDITY_H_INCLUDED
#define TIMIDITY_H_INCLUDED 1
#include "timidity.h"
#endif

///r
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)

#if 1 // SAMPLE_TYPE_FLOAT // pre_resample_t float
typedef float pre_resample_t;
#define PRE_RESAMPLE_DATA_TYPE (SAMPLE_TYPE_FLOAT)
#else // SAMPLE_TYPE_DOUBLE // pre_resample_t double
typedef double pre_resample_t;
#define PRE_RESAMPLE_DATA_TYPE (SAMPLE_TYPE_DOUBLE)
#endif

#else // DATA_T_INT32

#if defined(LOOKUP_HACK)
typedef sample_t pre_resample_t; // sample_t
#define PRE_RESAMPLE_DATA_TYPE (SAMPLE_TYPE_INT16) // SAMPLE_TYPE_INT16
#elif 0 // SAMPLE_TYPE_INT16 // pre_resample_t sample_t
typedef sample_t pre_resample_t;
#define PRE_RESAMPLE_DATA_TYPE (SAMPLE_TYPE_INT16)
#else // SAMPLE_TYPE_INT32 // pre_resample_t int32
typedef int32 pre_resample_t;
#define PRE_RESAMPLE_DATA_TYPE (SAMPLE_TYPE_INT32)
#endif // check pre_resample()

#endif

#ifndef CLIP_INT8
#define CLIP_INT8(x)  ((x > 127) ? 127 : (x < -128) ? -128 : x)
#define CLIP_INT16(x) ((x > 32767) ? 32767 : (x < -32768) ? -32768 : x)
#define CLIP_INT32(x) ((x > 2147483647) ? 2147483647 : (x < -2147483648) ? -2147483648 : x)
#define CLIP_FLOAT(x) ((x > 1.0) ? 1.0 : (x < -1.0) ? -1.0 : x)
#endif



///r
enum {
// cfg sort
	RESAMPLE_NONE = 0,
	RESAMPLE_LINEAR,
	RESAMPLE_CSPLINE,
	RESAMPLE_LAGRANGE,
	RESAMPLE_NEWTON,
	RESAMPLE_GAUSS,
	RESAMPLE_SHARP,
	RESAMPLE_LINEAR_P,
	RESAMPLE_SINE,
	RESAMPLE_SQUARE,
	RESAMPLE_LANCZOS,
	RESAMPLE_MAX,
};

enum {
	RESAMPLE_MODE_PLAIN = 0,
	RESAMPLE_MODE_LOOP,
	RESAMPLE_MODE_BIDIR_LOOP,
};


#define RESAMPLE_NEWTON_VOICE // if multi thread need

typedef struct resample_rec {
	splen_t loop_start;
	splen_t loop_end;
	splen_t data_length;	
	splen_t offset;
	int32 increment;
	int8 mode; // 0:plain 1:loop 2:bidir_loop
	int8 plain_flag;
	int buffer_offset;
	DATA_T (*current_resampler)(const sample_t*, splen_t, struct resample_rec *);
	// newton
#ifdef RESAMPLE_NEWTON_VOICE
	int newt_grow;
	int32 newt_old_trunc_x;
	const void *newt_old_src;
	double newt_divd[60][60];
#endif
} resample_rec_t;

#define RESAMPLE_OVER_SAMPLING_MAX 16

///r
extern int opt_resample_type;
extern int opt_resample_param;
extern int opt_resample_filter;
extern int opt_pre_resamplation;
extern int opt_resample_over_sampling;
extern FLOAT_T div_over_sampling_ratio;
//extern void set_cache_resample_filter(Sample *sp, splen_t ofs);

extern int get_current_resampler(void);
extern int set_current_resampler(int type);
extern void initialize_resampler_coeffs(void);
extern void uninitialize_resampler_coeffs(void);
extern int set_resampler_parm(int val);
extern void set_resampler_over_sampling(int val);
extern void resample_down_sampling(DATA_T *ptr, int32 count);
//extern void free_gauss_table(void);

/* exported for recache.c */
extern void set_resamplation(int data_type);
extern DATA_T do_resamplation(const sample_t *src, splen_t ofs, resample_rec_t *rec);

extern void init_voice_resample(int v);
extern void resample_voice(int v, DATA_T *ptr, int32 count);
extern void pre_resample(Sample *sp);

#endif /* ___RESAMPLE_H_ */
