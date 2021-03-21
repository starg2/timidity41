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

   filter.h : written by Vincent Pagel ( pagel@loria.fr )

   implements fir antialiasing filter : should help when setting sample
   rates as low as 8Khz.
   */

#ifndef ___FILTER_H_
#define ___FILTER_H_

#ifdef HAVE_CONFIH_H
#include "config.h"
#endif

#include "sysdep.h"

#include "mix.h"

enum{
	FILTER_NONE =0,
	FILTER_LPF12, // ov
	FILTER_LPF24,
	FILTER_LPF_BW,
	FILTER_LPF12_2,
	FILTER_LPF24_2,
	FILTER_LPF6,
	FILTER_LPF18, // ov
	FILTER_LPF_TFO,
// test
	FILTER_HPF_BW,
	FILTER_BPF_BW,
	FILTER_PEAK1,
	FILTER_NOTCH1,
	FILTER_LPF12_3, // ov
	FILTER_HPF12_3, // ov
	FILTER_BPF12_3, // ov
	FILTER_BCF12_3, // ov
	FILTER_HPF6,
	FILTER_HPF12_2,
// hybrid
	FILTER_HBF_L6L12,
	FILTER_HBF_L12L6,
	FILTER_HBF_L12H6,
	FILTER_HBF_L24H6,
	FILTER_HBF_L24H12,
	FILTER_HBF_L12OCT,
	FILTER_HBF_L24OCT,
// multi
	FILTER_LPF6x2,
	FILTER_LPF6x3,
	FILTER_LPF6x4,
	FILTER_LPF6x8,
	FILTER_LPF6x16,
	FILTER_LPF_BWx2,
	FILTER_LPF_BWx3,
	FILTER_LPF_BWx4,
	FILTER_LPF24_2x2,
//
	FILTER_LPF_FIR,
// equalizer
	FILTER_SHELVING_LOW, // q
	FILTER_SHELVING_HI, // q
	FILTER_PEAKING, // q
	FILTER_BIQUAD_LOW,
	FILTER_BIQUAD_HI,
	FILTER_LIST_MAX, // last
};

enum{
	VOICE_FILTER2_NONE =0,
	VOICE_FILTER2_HPF_BW,
	VOICE_FILTER2_HPF12_3,
	VOICE_FILTER2_HPF6,
	VOICE_FILTER2_HPF12_2,
	VOICE_FILTER2_LIST_MAX, // last
};

enum{
	RESAMPLE_FILTER_NONE =0,
	RESAMPLE_FILTER_LPF_BW,
	RESAMPLE_FILTER_LPF_BWx2,
	RESAMPLE_FILTER_LPF_BWx3,
	RESAMPLE_FILTER_LPF_BWx4,
	RESAMPLE_FILTER_LPF24_2,
	RESAMPLE_FILTER_LPF24_2x2,
	RESAMPLE_FILTER_LPF6x8,
	RESAMPLE_FILTER_LPF6x16,
	RESAMPLE_FILTER_LPF_FIR,
	RESAMPLE_FILTER_LIST_MAX, // last
};

#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT)
#define FILTER_T int32
#else
#define FILTER_T FLOAT_T
#endif

#define FILTER_CF_NUM (40) // >= 8*3 (AVX
#define FILTER_FB_NUM (25) // >= 8*3+1 (AVX
#define FILTER_FB_L (0)
#define FILTER_FB_R (FILTER_FB_NUM) 

typedef struct _FilterCoefficients {
	int8 init, type;	/* filter type */
	FLOAT_T flt_rate, flt_rate_div2, div_flt_rate;
	FLOAT_T div_flt_rate_ov2, div_flt_rate_ov3, flt_rate_limit1, flt_rate_limit2; // for ov
	FLOAT_T freq, last_freq;
	FLOAT_T reso_dB, last_reso_dB;
	FLOAT_T q, last_q;
	FLOAT_T range[8];
	FILTER_T dc[FILTER_CF_NUM]; // f, q, p, other
	FILTER_T db[FILTER_FB_NUM * 2]; // feedback *2ch
	void (*recalc_filter)(struct _FilterCoefficients *fc);
	void (*sample_filter)(FILTER_T *dc, FILTER_T *db, DATA_T *sp);
} FilterCoefficients;

extern const char *filter_name[];
extern void recalc_filter(FilterCoefficients *fc);
extern void sample_filter(FilterCoefficients *fc, DATA_T *sp);
extern void sample_filter_stereo(FilterCoefficients *fc, DATA_T *spL, DATA_T *spR);
extern void sample_filter_stereo2(FilterCoefficients *fc, DATA_T *spLR);
extern void sample_filter_left(FilterCoefficients *fc, DATA_T *sp);
extern void sample_filter_right(FilterCoefficients *fc, DATA_T *sp);
extern void buffer_filter(FilterCoefficients *fc, DATA_T *sp, int32 count);
extern void buffer_filter_stereo(FilterCoefficients *fc, DATA_T *sp, int32 count);
extern void buffer_filter_left(FilterCoefficients *fc, DATA_T *sp, int32 count);
extern void buffer_filter_right(FilterCoefficients *fc, DATA_T *sp, int32 count);
extern void set_sample_filter_ext_rate(FilterCoefficients *fc, FLOAT_T freq);
extern void set_sample_filter_type(FilterCoefficients *fc, int type);
extern void set_sample_filter_freq(FilterCoefficients *fc, FLOAT_T freq);
extern void set_sample_filter_reso(FilterCoefficients *fc, FLOAT_T reso);
extern void set_sample_filter_q(FilterCoefficients *fc, FLOAT_T q);
extern void init_sample_filter(FilterCoefficients *fc, FLOAT_T freq, FLOAT_T reso, int type);
extern void init_sample_filter2(FilterCoefficients *fc, FLOAT_T freq, FLOAT_T reso, FLOAT_T q, int type);

extern void set_voice_filter1_ext_rate(FilterCoefficients *fc, FLOAT_T freq);
extern void set_voice_filter1_type(FilterCoefficients *fc, int type);
extern void set_voice_filter1_freq(FilterCoefficients *fc, FLOAT_T freq);
extern void set_voice_filter1_reso(FilterCoefficients *fc, FLOAT_T reso);
extern void set_voice_filter1_orig_freq(FilterCoefficients *fc, FLOAT_T freq);
extern void set_voice_filter1_orig_reso(FilterCoefficients *fc, FLOAT_T reso);
extern void voice_filter1(FilterCoefficients *fc, DATA_T *sp, int32 count);

extern void set_voice_filter2_ext_rate(FilterCoefficients *fc, FLOAT_T freq);
extern void set_voice_filter2_type(FilterCoefficients *fc, int type);
extern void set_voice_filter2_freq(FilterCoefficients *fc, FLOAT_T freq);
extern void set_voice_filter2_reso(FilterCoefficients *fc, FLOAT_T reso);
extern void set_voice_filter2_orig_freq(FilterCoefficients *fc, FLOAT_T freq);
extern void set_voice_filter2_orig_reso(FilterCoefficients *fc, FLOAT_T reso);
extern void voice_filter2(FilterCoefficients *fc, DATA_T *sp, int32 count);

extern void voice_filter(int v, DATA_T *sp, int32 count);

extern void set_resample_filter_type(FilterCoefficients *fc, int type);
extern void set_resample_filter_ext_rate(FilterCoefficients *fc, FLOAT_T freq);
extern void set_resample_filter_freq(FilterCoefficients *fc, FLOAT_T freq);
extern void resample_filter(int v, DATA_T *sp, int32 count);

#ifdef MIX_VOICE_BATCH
extern void voice_filter_batch(int batch_size, int *vs, DATA_T **sps, int32 *counts);
#endif // MIX_VOICE_BATCH


/*************** antialiasing ********************/

/* Order of the FIR filter = 20 should be enough ! */
#define LPF_FIR_ORDER (20)
#define LPF_FIR_ORDER2 (10) // LPF_FIR_ORDER/2
#define LPF_FIR_ANTIALIASING_ATT (40.0)

extern void antialiasing(int16 *data, int32 data_length, int32 sample_rate, int32 output_rate);
extern void antialiasing_int8(int8 *data, int32 data_length, int32 sample_rate, int32 output_rate);
extern void antialiasing_int32(int32 *data, int32 data_length, int32 sample_rate, int32 output_rate);
extern void antialiasing_float(float *data, int32 data_length, int32 sample_rate, int32 output_rate);
extern void antialiasing_double(double *data, int32 data_length, int32 sample_rate, int32 output_rate);


/*************** fir_eq ********************/

//#define TEST_FIR_EQ // filter test master effect.
#define FIR_EQ_SIZE_MAX (1<<12) // 1<<n  max12bit
#define FIR_EQ_BAND_MAX (16)

typedef struct _FIR_EQ {
	int8 init, st, band, band_p, bit, bit_p;
	int32 size, count;
	FLOAT_T freq[FIR_EQ_BAND_MAX], gain[FIR_EQ_BAND_MAX];
	FLOAT_T dc[FIR_EQ_SIZE_MAX];
#if (USE_X86_EXT_INTRIN >= 3) && defined(FLOAT_T_DOUBLE) && defined(DATA_T_DOUBLE)
	DATA_T buff[2][FIR_EQ_SIZE_MAX * 2]; // max12bit
#else
	DATA_T buff[2][FIR_EQ_SIZE_MAX]; // max12bit
#endif
} FIR_EQ;

extern void init_fir_eq(FIR_EQ *fc);
extern void apply_fir_eq(FIR_EQ *fc, DATA_T *buf, int32 count);


/* effect control */

extern double ext_filter_shelving_gain;
extern double ext_filter_shelving_reduce;
extern double ext_filter_shelving_q;

extern double ext_filter_peaking_gain;
extern double ext_filter_peaking_reduce;
extern double ext_filter_peaking_q;


#endif /* ___FILTER_H_ */
