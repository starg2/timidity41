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

   filter.c: written by Vincent Pagel ( pagel@loria.fr )

   implements fir antialiasing filter : should help when setting sample
   rates as low as 8Khz.

   April 95
      - first draft

   22/5/95
      - modify "filter" so that it simulate leading and trailing 0 in the buffer
   */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <stdlib.h>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "mix.h"
#include "filter.h"


double ext_filter_shelving_gain = 1.0;
double ext_filter_shelving_reduce = 1.0;
double ext_filter_shelving_q = 1.0;
double ext_filter_peaking_gain = 1.0;
double ext_filter_peaking_reduce = 1.0;
double ext_filter_peaking_q = 1.0;
const double ext_filter_margin = 0.010; // 1cB,+-20centより小さい 負荷減少小
//const double ext_filter_margin = 0.05; // 5cB,+-100centより小さい 負荷減少大


///r
/*        sample_filter       */
/*
voice_filter1(LPF), voice_filter2(HPF), resample_filter
フィルタ部分共通
フィルタ自体は freq[Hz], reso_dB[dB] ,(EQの場合 q[0.0~1.0]

voice_filter(LPF) の場合
	playmidi.c init_voice_filter(int i)

input freq 20 < freq < 20000 , input 0 < reso < 96

0<n<∞, 0<n<1, 1>n>0 (各フィルタ側レゾナンス部分値域がこんな感じでバラバラ
n=f(rez) (各フィルタのf()が何なのか
q, 1/q, 1-1/q こうすると共通した値域 1<q<∞ になる
0<rez なので q=X^rez で qの値域に変換
これに制限やら係数がつく

処理順序
1: typeを指定 set_type() (初回またはtype変化の場合 FilterCoefficients 0クリア
2: 特殊なサンプルレートの場合は set_ext_rate()
3: orig_freq,orig_resoを使用する場合は set_orig_freq() set_orig_reso() 
4: freq,resoを指定 set_freq() set_reso() (EQの場合 set_q()
5: sample_filterの場合は 係数計算 recalc_filter()
6: フィルタ処理 filter() (buffer_filterは処理前に係数計算される

1:~5:をまとめた init_sample_filter()でもいい (EQの場合 init_sample_filter2()

*/

/*
filter spec
num	filter_define		type	cutoff_limit (oversampling)	desc					
00	FILTER_NONE,		OFF									filter OFF
01	FILTER_LPF12,		LPF		sr / 6 0.16666 (~x3)		Chamberlin 12dB/oct
02	FILTER_LPF24,		LPF		sr / 2						Moog VCF 24dB/oct		
03	FILTER_LPF_BW,		LPF		sr / 2						butterworth	elion add	
04	FILTER_LPF12_2,		LPF		sr / 2						Resonant IIR 12dB/oct	
05	FILTER_LPF24_2,		LPF		sr / 2						amSynth 24dB/oct		
06	FILTER_LPF6,		LPF		sr / 2						One pole 6dB/oct nonrez
07	FILTER_LPF18,		LPF		sr / 2.25 0.44444 (~x2)		3pole 18dB/oct	
08	FILTER_LPF_TFO,		LPF		sr / 2						two first order		
// test
09	FILTER_HPF_BW,		HPF		sr / 2						butterworth elion+
10	FILTER_BPF_BW,		BPF		sr / 2						butterworth elion+	
11	FILTER_PEAK1,		peak	sr / 2
12	FILTER_NOTCH1,		notch	sr / 2
13	FILTER_LPF12_3,		LPF		sr / 4.6 0.21875 (~x3)		Chamberlin2 12dB/oct
14	FILTER_HPF12_3,		HPF		sr / 4.6 0.21875 (~x3)		Chamberlin2 12dB/oct
15	FILTER_BPF12_3,		BPF		sr / 4.6 0.21875 (~x3)		Chamberlin2 12dB/oct
16	FILTER_BCF12_3,		BCF		sr / 4.6 0.21875 (~x3)		Chamberlin2 12dB/oct	
17	FILTER_HPF6,		HPF		sr / 2						One pole 6dB/oct nonrez
18	FILTER_HPF12_2,		HPF		sr / 2						Resonant IIR 12dB/oct
// hybrid
19	FILTER_HBF_L6L12,	HBF		sr / 2							
20	FILTER_HBF_L12L6,	HBF		sr / 2						
21	FILTER_HBF_L12H6,	HBF		sr / 2						
22	FILTER_HBF_L24H6,	HBF		sr / 2						
23	FILTER_HBF_L24H12,	HBF		sr / 2						
24	FILTER_HBF_L12OCT,	HBF		sr / 2						
25	FILTER_HBF_L24OCT,	HBF		sr / 2						
// multi
26	FILTER_LPF6x2,		LPF		sr / 2
27	FILTER_LPF6x3,		LPF		sr / 2
28	FILTER_LPF6x4,		LPF		sr / 2
29	FILTER_LPF6x8,		LPF		sr / 2
30	FILTER_LPF6x16,		LPF		sr / 2
31	FILTER_LPF_BWx2,	LPF		sr / 2
32	FILTER_LPF_BWx3,	LPF		sr / 2
33	FILTER_LPF_BWx4,	LPF		sr / 2
34	FILTER_LPF24_2x2,	LPF		sr / 2
35	FILTER_LPF_FIR,
// equalizer
36	FILTER_SHELVING_LOW,EQ_LOW	sr / 2					
37	FILTER_SHELVING_HI,	EQ_HI	sr / 2
38	FILTER_PEAKING,		EQ_MID	sr / 2
39	FILTER_BIQUAD_LOW,	LPF		sr / 2
40	FILTER_BIQUAD_HI,	HPF		sr / 2
// last
41	FILTER_LIST_MAX,
cutoff_limit sr/2未満のものはoversamplingでsr/2を扱えるようにする
*/



#if 1 // recalc filter margin
/*
係数再計算は負荷が大きく ボイスフィルタで使用回数も多いので ある程度削って処理回数を減らす
ボイスフィルタは変動してるものだから100centズレても違いはわからない EQは変動しないし
*/

#define INIT_MARGIN_VAL { \
	fc->range[0] = fc->range[1] = fc->range[2] = fc->range[3] = fc->range[4] = fc->range[5] = fc->range[6] = fc->range[7] = 0; }
#define FLT_FREQ_MARGIN (fc->freq < fc->range[0] || fc->freq > fc->range[1])
#define FLT_RESO_MARGIN (fc->reso_dB < fc->range[2] || fc->reso_dB > fc->range[3])
#define FLT_WIDTH_MARGIN (fc->q < fc->range[4] || fc->q > fc->range[5])

#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
#define CALC_MARGIN_VAL __m128d vec_range = _mm_set_pd(1.0 + ext_filter_margin, 1.0 - ext_filter_margin);
#define CALC_FREQ_MARGIN { _mm_storeu_pd(&fc->range[0], _mm_mul_pd(MM_LOAD1_PD(&fc->freq), vec_range));}
#define CALC_RESO_MARGIN { _mm_storeu_pd(&fc->range[2], _mm_mul_pd(MM_LOAD1_PD(&fc->reso_dB), vec_range));}
#define CALC_WIDTH_MARGIN { _mm_storeu_pd(&fc->range[4], _mm_mul_pd(MM_LOAD1_PD(&fc->q), vec_range));}
#else
#define CALC_MARGIN_VAL
#define CALC_FREQ_MARGIN {fc->range[0] = fc->freq * (1.0 - ext_filter_margin); fc->range[1] = fc->freq * (1.0 + ext_filter_margin);}
#define CALC_RESO_MARGIN {fc->range[2] = fc->reso_dB * (1.0 - ext_filter_margin); fc->range[3] = fc->reso_dB * (1.0 + ext_filter_margin);}
#define CALC_WIDTH_MARGIN {fc->range[4] = fc->q * (1.0 - ext_filter_margin); fc->range[5] = fc->q * (1.0 + ext_filter_margin);}
#endif

#else // ! recalc filter margin

#define INIT_MARGIN_VAL
#define CALC_MARGIN_VAL
#define FLT_FREQ_MARGIN (fc->freq != fc->last_freq)
#define FLT_RESO_MARGIN (fc->reso_dB != fc->last_reso_dB)
#define FLT_WIDTH_MARGIN (fc->q != fc->last_q)
#define CALC_FREQ_MARGIN {fc->last_freq = fc->freq;}
#define CALC_RESO_MARGIN {fc->last_reso_dB = fc->reso_dB;}
#define CALC_WIDTH_MARGIN {fc->last_q = fc->q;}
#endif

#if 1 // resonance use table
#define RESO_DB_CF_P(db) filter_cb_p_table[(int)(db * 10.0)]
#define RESO_DB_CF_M(db) filter_cb_m_table[(int)(db * 10.0)]
#elif 1 // resonance calc function lite
#define RESO_DB_CF_P(db) (FLOAT_T)(exp((float)(M_LN10 * DIV_40 * db)))
#define RESO_DB_CF_M(db) (FLOAT_T)(exp((float)(M_LN10 * -DIV_40 * db)))
#else // resonance calc function
#define RESO_DB_CF_P(db) pow(10.0, DIV_40 * db)
#define RESO_DB_CF_M(db) pow(10.0, -DIV_40 * db)
#endif




#if (OPT_MODE == 1) && !defined(DATA_T_DOUBLE) && !defined(DATA_T_FLOAT) /* fixed-point implementation */

static inline void sample_filter_none(FILTER_T *dc, FILTER_T *db, DATA_T *sp) {}

static inline void recalc_filter_none(FilterCoefficients *fc) {}

static inline void sample_filter_LPF12(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + imuldiv28(db[2], dc[0]);
	db[1] = (*sp << 4) - db[0] - imuldiv28(db[2], dc[1]);
	db[2] = imuldiv28(db[1], dc[0]) + db[2];
	*sp = db[0] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF12_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(db[2], dc[0]);
	db[1] = input - db[0] - imuldiv28(db[2], dc[1]);
	db[2] = imuldiv28(db[1], dc[0]) + db[2];
	*sp = db[0] >> 4; /* 4.28 to 8.24 */
	// ov2
	db[0] = db[0] + imuldiv28(db[2], dc[0]);
	db[1] = input - db[0] - imuldiv28(db[2], dc[1]);
	db[2] = imuldiv28(db[1], dc[0]) + db[2];
}

static inline void sample_filter_LPF12_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(db[2], dc[0]);
	db[1] = input - db[0] - imuldiv28(db[2], dc[1]);
	db[2] = imuldiv28(db[1], dc[0]) + db[2];
	*sp = db[0] >> 4; /* 4.28 to 8.24 */
	// ov2
	db[0] = db[0] + imuldiv28(db[2], dc[0]);
	db[1] = input - db[0] - imuldiv28(db[2], dc[1]);
	db[2] = imuldiv28(db[1], dc[0]) + db[2];
	// ov3
	db[0] = db[0] + imuldiv28(db[2], dc[0]);
	db[1] = input - db[0] - imuldiv28(db[2], dc[1]);
	db[2] = imuldiv28(db[1], dc[0]) + db[2];
}

static inline void recalc_filter_LPF12(FilterCoefficients *fc)
{
	int32 *dc = fc->dc;

/* copy with applying Chamberlin's lowpass filter. */
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		if(fc->freq < fc->flt_rate_limit1){ // <sr*DIV_6
			fc->sample_filter = sample_filter_LPF12;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate), 28); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*DIV_6
			fc->sample_filter = sample_filter_LPF12_ov2;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2), 28); // sr*2
		}else{ // <sr*3*DIV_6
			fc->sample_filter = sample_filter_LPF12_ov3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3), 28); // sr*3
		}
		fc->last_reso_dB = fc->reso_dB;
		dc[1] = TIM_FSCALE(RESO_DB_CF_M(fc->reso_dB), 28);
	}
}

static inline void sample_filter_LPF24(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T da[6];

	da[0] = (*sp << 4) - imuldiv28(db[4], dc[2]);	/* feedback */
	da[1] = db[1];
	da[2] = db[2];
	da[3] = db[3];
	db[1] = imuldiv28((db[0] + da[0]), dc[0]) - imuldiv28(db[1], dc[1]);
	db[2] = imuldiv28((db[1] + da[1]), dc[0]) - imuldiv28(db[2], dc[1]);
	db[3] = imuldiv28((db[2] + da[2]), dc[0]) - imuldiv28(db[3], dc[1]);
	db[4] = imuldiv28((db[3] + da[3]), dc[0]) - imuldiv28(db[4], dc[1]);
	db[0] = da[0];
	*sp = db[4] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF24(FilterCoefficients *fc)
{
	FLOAT_T f, q, p, tmp;
	int32 *dc = fc->dc;

/* copy with applying Moog lowpass VCF. */
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		f = 2.0 * fc->freq * fc->div_flt_rate;
		p = 1.0 - f;
		fc->last_reso_dB = fc->reso_dB;
		q = 0.80 * (1.0 - RESO_DB_CF_M(fc->reso_dB)); // 0.0f <= c < 0.80f
		dc[0] = TIM_FSCALE(tmp = f + 0.8 * f * p, 28);
		dc[1] = TIM_FSCALE(tmp + tmp - 1.0, 28);
		dc[2] = TIM_FSCALE(q * (1.0 + 0.5 * p * (1.0 - p + 5.6 * p * p)), 28);		
	}
}

static inline void sample_filter_LPF_BW(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp << 4;
	// LPF
	db[2] = imuldiv28(db[0], dc[0])
	      + imuldiv28(db[1], dc[1])
	      + imuldiv28(db[2], dc[2])
	      - imuldiv28(db[3], dc[3])
	      - imuldiv28(db[4], dc[4]);
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[3] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF_BW(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T q ,p, p2, qp, tmp;

// elion butterworth
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		p = 1.0 / tan(M_PI * fc->freq * fc->div_flt_rate); // ?
		q = RESO_DB_CF_M(fc->reso_dB) * SQRT_2; // q>0.1
		p2 = p * p;
		qp = q * p;
		dc[0] = TIM_FSCALE(tmp = 1.0 / (1.0 + qp + p2), 28);
		dc[1] = TIM_FSCALE(2.0 * tmp, 28);
		dc[2] = dc[0];
		dc[3] = TIM_FSCALE(2.0 * (1.0 - p2) * tmp, 28);
		dc[4] = TIM_FSCALE((1.0 - qp + p2) * tmp, 28);
	}
}

static inline void sample_filter_LPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[1] += imuldiv28(((*sp << 4) - db[0]), dc[1]);
	db[0] += db[1];
	db[1] = imuldiv28(db[1], dc[0]);
	*sp = db[0] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF12_2(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, q, tmp;

// Resonant IIR lowpass (12dB/oct) Olli Niemitalo //r
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		//q = 1.0 - f / (2.0 * ((fc->reso_dB * DIV_96 + 1.0) + 0.5 / (1.0 + f)) + f - 2.0);
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		dc[0] = TIM_FSCALE(tmp = q * q, 28);
		dc[1] = TIM_FSCALE(tmp + 1.0 - 2.0 * cos(f) * q, 28);
	}
}

static inline void buffer_filter_LPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp, int32 count)
{
	int32 i;
	FILTER_T db0 = db[0], db1 = db[1], dc0 = dc[0], dc1 = dc[1];

	for (i = 0; i < count; i++) {
		db1 += imuldiv28(((sp[i] << 4) - db0), dc1);
		db0 += db1;
		db1 = imuldiv28(db1, dc0);
		sp[i] = db0 >> 4; /* 4.28 to 8.24 */
	}
	db[0] = db0;
	db[1] = db1;
}

static inline void sample_filter_LPF24_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	db[5] = imuldiv28(db[0], dc[0]) + db[1];
	db[1] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[2];
	db[2] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	db[0] = db[5];
	db[5] = imuldiv28(db[0], dc[0]) + db[3];
	db[3] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[4];
	db[4] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	*sp = db[0] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF24_2(FilterCoefficients *fc)
{
	FLOAT_T f, q, p, r, dc0;
	int32 *dc = fc->dc;

// amSynth 24dB/ocatave resonant low-pass filter. Nick Dowell //r
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		//q = 2 * (1 - fc->reso_dB * DIV_96); // maxQ = 0.9995
		q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		p = 1.0 / (1.0 + (q * f) + r);
		dc0 = r * p;
		dc[0] = TIM_FSCALE(dc0, 28);
		dc[1] = TIM_FSCALE(dc0 * 2, 28);
		dc[2] = dc[0];
		dc[3] = TIM_FSCALE(-2.0 * (r - 1) * p, 28);
		dc[4] = TIM_FSCALE((-1.0 + (q * f) - r) * p, 28);
	}
}

static inline void sample_filter_LPF6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[1] = imuldiv28((*sp << 4), dc[0]) + imuldiv28(db[1], dc[1]);
	*sp = db[1] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF6(FilterCoefficients *fc)
{
	FLOAT_T f;
	int32 *dc = fc->dc;

// One pole filter, LP 6dB/Oct scoofy no resonance //r
	if (!FP_EQ(fc->freq, fc->last_freq)) {
		fc->last_freq = fc->freq;
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[0] = TIM_FSCALE(1.0 - f, 28);
		dc[1] = TIM_FSCALE(f, 28);
	}
}

static inline void sample_filter_LPF18(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T da[3];

	da[0] = db[0];
	da[1] = db[1];
	da[2] = db[2];
	db[0] = (*sp << 4) - imuldiv28(db[3], dc[2]);
	db[1] = imuldiv28((db[0] + da[0]), dc[1]) - imuldiv28(db[1], dc[0]);
	db[2] = imuldiv28((db[1] + da[1]), dc[1]) - imuldiv28(db[2], dc[0]);
	db[3] = imuldiv28((db[2] + da[2]), dc[1]) - imuldiv28(db[3], dc[0]);
	*sp = imuldiv28(db[3], dc[3]) >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF18_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T da[3], input = *sp << 4;

	da[0] = db[0];
	da[1] = db[1];
	da[2] = db[2];
	db[0] = input - imuldiv28(db[3], dc[2]);
	db[1] = imuldiv28((db[0] + da[0]), dc[1]) - imuldiv28(db[1], dc[0]);
	db[2] = imuldiv28((db[1] + da[1]), dc[1]) - imuldiv28(db[2], dc[0]);
	db[3] = imuldiv28((db[2] + da[2]), dc[1]) - imuldiv28(db[3], dc[0]);
	// ov2
	da[0] = db[0];
	da[1] = db[1];
	da[2] = db[2];
	db[0] = input - imuldiv28(db[3], dc[2]);
	db[1] = imuldiv28((db[0] + da[0]), dc[1]) - imuldiv28(db[1], dc[0]);
	db[2] = imuldiv28((db[1] + da[1]), dc[1]) - imuldiv28(db[2], dc[0]);
	db[3] = imuldiv28((db[2] + da[2]), dc[1]) - imuldiv28(db[3], dc[0]);
	*sp = imuldiv28(db[3], dc[3]) >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF18(FilterCoefficients *fc)
{
	FLOAT_T f, q, p, tmp;
	int32 *dc = fc->dc;

// LPF18 low-pass filter //r
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {		
		fc->last_freq = fc->freq;
		if(fc->freq < fc->flt_rate_limit1){ // <sr/2.25
			fc->sample_filter = sample_filter_LPF18;
			f = 2.0 * fc->freq * fc->div_flt_rate; // *1.0
		}else{ // <sr*2/2.25
			fc->sample_filter = sample_filter_LPF18_ov2;
			f = 2.0 * fc->freq * fc->div_flt_rate_ov2; // sr*2
		}
		dc[0] = TIM_FSCALE(tmp = ((-2.7528 * f + 3.0429) * f + 1.718) * f - 0.9984, 28);
		fc->last_reso_dB = fc->reso_dB;
		//q = fc->reso_dB * DIV_96;
		q = 0.789 * (1.0 - RESO_DB_CF_M(fc->reso_dB)); // 0<q<0.78125
		p = tmp + 1.0;
		dc[1] = TIM_FSCALE(0.5 * p, 28);
		dc[2] = TIM_FSCALE(tmp = q * (((-2.7079 * p + 10.963) * p - 14.934) * p + 8.4974), 28);
		dc[3] = TIM_FSCALE(1.0 + (0.25 * (1.5 + 2.0 * tmp * (1.0 - f))), 28);
	}
}

static inline void sample_filter_LPF_TFO(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + imuldiv28(((*sp << 4) - db[0] + imuldiv28((db[0] - db[1]), dc[1])), dc[0]);
	db[1] = db[1] + imuldiv28((db[0] - db[1]), dc[0]);
	*sp = db[1] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF_TFO(FilterCoefficients *fc)
{
	FLOAT_T q, tmp;
	int32 *dc = fc->dc;

// two first order low-pass filter //r
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		dc[0] = TIM_FSCALE(tmp = 2 * fc->freq * fc->div_flt_rate, 28);
		q = 1.0 - RESO_DB_CF_M(fc->reso_dB);
		dc[1] = TIM_FSCALE(q + q / (1.01 - tmp), 28);
	}
}

static inline void sample_filter_HPF_BW(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp << 4;
	// LPF
	db[2] = imuldiv28(db[0], dc[0])
	      + imuldiv28(db[1], dc[1])
	      + imuldiv28(db[2], dc[2])
	      - imuldiv28(db[3], dc[3])
	      - imuldiv28(db[4], dc[4]);
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[3] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_HPF_BW(FilterCoefficients *fc)
{
	int32 *dc = fc->dc;	
	FLOAT_T q, p, p2, qp, tmp;

// elion butterworth HPF //r
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		q = RESO_DB_CF_M(fc->reso_dB) * SQRT_2; // q>0.1
		p = tan(M_PI * fc->freq * fc->div_flt_rate); // hpf ?		
		p2 = p * p;
		qp = q * p;
		dc[0] = TIM_FSCALE(tmp = 1.0 / (1.0 + qp + p2), 28);
		dc[1] = TIM_FSCALE(-2 * tmp, 28); // hpf
		dc[2] = dc[0];
		dc[3] = TIM_FSCALE(2.0 * (p2 - 1.0) * tmp, 28); // hpf
		dc[4] = TIM_FSCALE((1.0 - qp + p2) * tmp, 28);
	}
}

static inline void sample_filter_BPF_BW(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp << 4;
	// LPF
	db[2] = imuldiv28(db[0], dc[0])
	      + imuldiv28(db[1], dc[1])
	      + imuldiv28(db[2], dc[2])
	      - imuldiv28(db[3], dc[3])
	      - imuldiv28(db[4], dc[4]);
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// HPF
	db[7] = imuldiv28(db[3], dc[5])
	      + imuldiv28(db[6], dc[6])
	      + imuldiv28(db[7], dc[7])
	      - imuldiv28(db[8], dc[8])
	      - imuldiv28(db[9], dc[9]);
	db[9] = db[8];
	db[8] = db[7]; // flt out
	db[7] = db[6];
	db[6] = db[3]; // flt in
	// output
	*sp = db[8] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_BPF_BW(FilterCoefficients *fc)
{
	FLOAT_T f, q, r, pl, ph, sl, sh, tmp;
	int32 *dc = fc->dc;

// elion butterworth
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		f = fc->freq * fc->div_flt_rate;
		r = 1.0 - RESO_DB_CF_M(fc->reso_dB);
		q = SQRT_2 - r * SQRT_2; // q>0.1
		// LPF
		pl = 1.0 / tan(M_PI * f); // ?
		sl = pl * pl;
		dc[0] = TIM_FSCALE(tmp = 1.0 / (1.0 + q * pl + sl), 28);
		dc[1] = TIM_FSCALE(2.0 * tmp, 28);
		dc[2] = dc[0];
		dc[3] = TIM_FSCALE(2.0 * (1.0 - sl) * tmp, 28);
		dc[4] = TIM_FSCALE((1.0 - q * pl + sl) * tmp, 28);
		// HPF
		f = f * 0.80; // bandwidth = LPF-HPF
		ph = tan(M_PI * f); // hpf ?
		sh = ph * ph;
		dc[5] = TIM_FSCALE(tmp = 1.0 / (1.0 + q * ph + sh), 28);
		dc[6] = TIM_FSCALE(-2 * tmp, 28); // hpf
		dc[7] = dc[5];
		dc[8] = TIM_FSCALE(2.0 * (sh - 1.0) * tmp, 28); // hpf
		dc[9] = TIM_FSCALE((1.0 - q * ph + sh) * tmp, 28);
	}
}

static inline void recalc_filter_peak1(FilterCoefficients *fc)
{
	FLOAT_T f, q, r, pl ,ph, sl, sh;
	int32 *dc = fc->dc;

// elion butterworth
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;		
		f = cos(M_PI2 * fc->freq * fc->div_flt_rate);
		r = 1.0 - RESO_DB_CF_M(fc->reso_dB); // r < 0.99609375
		r *= 0.99609375;
		dc[0] = TIM_FSCALE((1 - r) * sqrt(r * (r - 4 * (f * f) + 2.0) + 1.0), 28);
		dc[1] = TIM_FSCALE(2 * f * r, 28);
		dc[2] = TIM_FSCALE(-(r * r), 28);
	}
}

static inline void sample_filter_peak1(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T r = 0;

	db[0] = *sp << 4;
	r += imuldiv28(dc[0], db[0]);
	r += imuldiv28(dc[1], db[1]);
	r += imuldiv28(dc[2], db[2]);
	db[2] = db[1];
	db[1] = r;
	*sp = r >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_notch1(FilterCoefficients *fc)
{
	FLOAT_T f, q, r, pl ,ph, sl, sh;
	int32 *dc = fc->dc;

// elion butterworth
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;	
		f = cos(M_PI2 * fc->freq * fc->div_flt_rate);
		r = (1.0 - RESO_DB_CF_M(fc->reso_dB)) * 0.99609375; // r < 0.99609375
		dc[0] = TIM_FSCALE((1 - r) * sqrt(r * (r - 4 * (f * f) + 2.0) + 1.0), 28);
		dc[1] = TIM_FSCALE(2 * f * r, 28);
		dc[2] = TIM_FSCALE(-(r * r), 28);
	}
}

static inline void sample_filter_notch1(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T r = 0;

	db[0] = *sp << 4;
	r += imuldiv28(dc[0], db[0]);
	r += imuldiv28(dc[1], db[1]);
	r += imuldiv28(dc[2], db[2]);
	db[2] = db[1];
	db[1] = r;
	*sp = (db[0] - r) >> 4; // notch
}

static inline void sample_filter_LPF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], *sp << 4) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[0] >> 4; // (db[1] + db[0]) >> 4; // notch
}

static inline void sample_filter_LPF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[0] >> 4; // (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void sample_filter_LPF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[0] >> 4; // (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	// ov3
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void recalc_filter_LPF12_3(FilterCoefficients *fc)
{
	int32 *dc = fc->dc;

	/* Chamberlin2's lowpass filter. */
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_LPF12_3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate), 28); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_LPF12_3_ov2;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2), 28); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_LPF12_3_ov3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3), 28); // sr*3
		}
		fc->last_reso_dB = fc->reso_dB;
		dc[1] = TIM_FSCALE(RESO_DB_CF_M(fc->reso_dB), 28);
	}
}

static inline void sample_filter_HPF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], *sp << 4) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[1] >> 4; // (db[1] + db[0]) >> 4; // notch
}

static inline void sample_filter_HPF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[1] >> 4; // (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void sample_filter_HPF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[1] >> 4; // (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	// ov3
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void recalc_filter_HPF12_3(FilterCoefficients *fc)
{
	int32 *dc = fc->dc;

	/* Chamberlin2's lowpass filter. */
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_HPF12_3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate), 28); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_HPF12_3_ov2;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2), 28); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_HPF12_3_ov3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3), 28); // sr*3
		}
		fc->last_reso_dB = fc->reso_dB;
		dc[1] = TIM_FSCALE(RESO_DB_CF_M(fc->reso_dB), 28);
	}
}

static inline void sample_filter_BPF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], *sp << 4) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[2] >> 4; // (db[1] + db[0]) >> 4; // notch
}

static inline void sample_filter_BPF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[2] >> 4; // (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void sample_filter_BPF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = db[2] >> 4; // (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	// ov3
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void recalc_filter_BPF12_3(FilterCoefficients *fc)
{
	int32 *dc = fc->dc;

	/* Chamberlin2's lowpass filter. */
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_BPF12_3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate), 28); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_BPF12_3_ov2;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2), 28); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_BPF12_3_ov3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3), 28); // sr*3
		}
		fc->last_reso_dB = fc->reso_dB;
		dc[1] = TIM_FSCALE(RESO_DB_CF_M(fc->reso_dB), 28);
	}
}

static inline void sample_filter_BCF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], *sp << 4) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = (db[1] + db[0]) >> 4; // notch
}

static inline void sample_filter_BCF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;
	
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void sample_filter_BCF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4;

	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	*sp = (db[1] + db[0]) >> 4; // notch
	// ov2
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
	// ov3
	db[0] = db[0] + imuldiv28(dc[0], db[2]); // low
	db[1] = imuldiv28(dc[1], input) - db[0] - imuldiv28(dc[1], db[2]); // high
	db[2] = imuldiv28(dc[0], db[1]) + db[2]; // band
}

static inline void recalc_filter_BCF12_3(FilterCoefficients *fc)
{
	int32 *dc = fc->dc;

	/* Chamberlin2's lowpass filter. */
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_BCF12_3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate), 28); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_BCF12_3_ov2;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2), 28); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_BCF12_3_ov3;
			dc[0] = TIM_FSCALE(2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3), 28); // sr*3
		}
		fc->last_reso_dB = fc->reso_dB;
		dc[1] = TIM_FSCALE(RESO_DB_CF_M(fc->reso_dB), 28);
	}
}

static inline void sample_filter_HPF6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	*sp -= (db[1] = imuldiv28(dc[0], *sp << 4) + imuldiv28(dc[1], db[1])) >> 4;
}

static inline void sample_filter_HPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[1] += imuldiv28(((*sp << 4) - db[0]), dc[1]);
	db[0] += db[1];
	db[1] = imuldiv28(db[1], dc[0]);
	*sp -= db[0] >> 4; /* 4.28 to 8.24 */
}


// hybrid

static inline void sample_filter_HBF_L6L12(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var1 = TIM_FSCALE(0.75, 28);
	const FILTER_T var2 = TIM_FSCALE(0.25, 28);
	const FILTER_T var3 = TIM_FSCALE(1.0, 28);
	// filter1
	db[1] += imuldiv28((input - db[0]), dc[1]);
	db[0] += db[1];
	db[1] = imuldiv28(db[1], dc[0]);
	out1 = db[0];
	// filter2
	db[11] = imuldiv28(input, dc[10]) + imuldiv28(db[11], dc[11]);
	out2 = db[11];
	// output
	dc[16] = imuldiv28(dc[16], var1) + imuldiv28(dc[15], var2);
	*sp = imuldiv28(out1, dc[16]) + imuldiv28(out2, var3 - dc[16]);
} 

static inline void recalc_filter_HBF_L6L12(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, p, q, t;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		p = q * q;
		dc[0] = TIM_FSCALE(p, 28);
		dc[1] = TIM_FSCALE(p + 1.0 - 2.0 * cos(f) * q, 28);
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = TIM_FSCALE(1.0 - f, 28);
		dc[11] = TIM_FSCALE(f, 28);
		//
		dc[15] = TIM_FSCALE(1.0 - RESO_DB_CF_M(fc->reso_dB), 28);
	}
}

static inline void sample_filter_HBF_L12L6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var = TIM_FSCALE(DIV_2, 28);
	// filter1
	db[1] += imuldiv28((input - db[0]), dc[1]);
	db[0] += db[1];
	db[1] = imuldiv28(db[1], dc[0]);
	out1 = db[0];
	// filter2
	db[11] = imuldiv28(input, dc[10]) + imuldiv28(db[11], dc[11]);
	out2 = db[11];
	// output	
	*sp = (out1 + imuldiv28(out2, var)) >> 4; /* 4.28 to 8.24 */
} 

static inline void recalc_filter_HBF_L12L6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		p = q * q;
		dc[0] = TIM_FSCALE(p, 28);
		dc[1] = TIM_FSCALE(p + 1.0 - 2.0 * cos(f) * q, 28);
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = TIM_FSCALE(1.0 - f, 28);
		dc[11] = TIM_FSCALE(f, 28);
	}
}

static inline void sample_filter_HBF_L12H6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var = TIM_FSCALE(DIV_2, 28);
	// filter1
	db[1] += imuldiv28((input - db[0]), dc[1]);
	db[0] += db[1];
	db[1] = imuldiv28(db[1], dc[0]);
	out1 = db[0];
	// filter2
	db[11] = imuldiv28(input, dc[10]) + imuldiv28(db[11], dc[11]);
	out2 = input - db[11];
	// output	
	*sp = (out1 + imuldiv28(out2, var)) >> 4; /* 4.28 to 8.24 */
} 

static inline void recalc_filter_HBF_L12H6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		p = q * q;
		dc[0] = TIM_FSCALE(p, 28);
		dc[1] = TIM_FSCALE(p + 1.0 - 2.0 * cos(f) * q, 28);
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = TIM_FSCALE(1.0 - f, 28);
		dc[11] = TIM_FSCALE(f, 28);
	}
}

static inline void sample_filter_HBF_L24H6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var = TIM_FSCALE(DIV_2, 28);
	// filter1
	db[0] = input;
	db[5] = imuldiv28(db[0], dc[0]) + db[1];
	db[1] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[2];
	db[2] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	db[0] = db[5];
	db[5] = imuldiv28(db[0], dc[0]) + db[3];
	db[3] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[4];
	db[4] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	out1 = db[0];
	// filter2
	db[11] = imuldiv28(input, dc[10]) + imuldiv28(db[11], dc[11]);
	out2 = input - db[11];
	// output	
	*sp = (out1 + imuldiv28(out2, var)) >> 4; /* 4.28 to 8.24 */
} 

static inline void recalc_filter_HBF_L24H6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p, s;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M( fc->reso_dB);
		r = f * f;
		p = 1 + (q * f) + r;
		s = r / p;
		dc[0] = TIM_FSCALE(s, 28);
		dc[1] = TIM_FSCALE(s * 2, 28);
		dc[2] = TIM_FSCALE(r / p, 28);
		dc[3] = TIM_FSCALE(2 * (r - 1) / (-p), 28);
		dc[4] = TIM_FSCALE((1 - (q * f) + r) / (-p), 28);
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = TIM_FSCALE(1.0 - f, 28);
		dc[11] = TIM_FSCALE(f, 28);
	}
}

static inline void sample_filter_HBF_L24H12(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var = TIM_FSCALE(DIV_2, 28);
	// filter1
	db[0] = input;
	db[5] = imuldiv28(db[0], dc[0]) + db[1];
	db[1] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[2];
	db[2] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	db[0] = db[5];
	db[5] = imuldiv28(db[0], dc[0]) + db[3];
	db[3] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[4];
	db[4] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	out1 = db[0];
	// filter2
	db[11] += imuldiv28((input - db[10]), dc[11]);
	db[10] += db[11];
	db[11] = imuldiv28(db[11], dc[10]);
	out2 = input - db[10];
	// output	
	*sp = (out1 + imuldiv28(out2, var)) >> 4; /* 4.28 to 8.24 */
} 

static inline void recalc_filter_HBF_L24H12(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p, s;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M( fc->reso_dB);
		r = f * f;
		p = 1 + (q * f) + r;
		s = r / p;
		dc[0] = TIM_FSCALE(s, 28);
		dc[1] = TIM_FSCALE(s * 2, 28);
		dc[2] = TIM_FSCALE(r / p, 28);
		dc[3] = TIM_FSCALE(2 * (r - 1) / (-p), 28);
		dc[4] = TIM_FSCALE((1 - (q * f) + r) / (-p), 28);
		// filter2
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		p = q * q;
		dc[10] = TIM_FSCALE(p, 28);
		dc[11] = TIM_FSCALE(p + 1.0 - 2.0 * cos(f) * q, 28);
	}
}

static inline void sample_filter_HBF_L12OCT(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var1 = TIM_FSCALE(DIV_3_2, 28);
	const FILTER_T var2 = TIM_FSCALE(DIV_3, 28);
	// filter1
	db[1] += imuldiv28((input - db[0]), dc[1]);
	db[0] += db[1];
	db[1] = imuldiv28(db[1], dc[0]);
	out1 = db[0];
	// filter2
	db[11] += imuldiv28((input - db[10]), dc[11]);
	db[10] += db[11];
	db[11] = imuldiv28(db[11], dc[10]);
	out2 = db[10];
	// output	
	*sp = (imuldiv28(out1, var1) + imuldiv28(out2, var2)) >> 4; /* 4.28 to 8.24 */
} 

static inline void recalc_filter_HBF_L12OCT(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		p = q * q;
		dc[0] = TIM_FSCALE(p, 28);
		dc[1] = TIM_FSCALE(p + 1.0 - 2.0 * cos(f) * q, 28);
		// filter2
		f = 2.0 * fc->freq * fc->div_flt_rate;
		if(f > DIV_2)
			f = DIV_2;
		f = M_PI2 * f;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		p = q * q;
		dc[10] = TIM_FSCALE(p, 28);
		dc[11] = TIM_FSCALE(p + 1.0 - 2.0 * cos(f) * q, 28);
	}
}

static inline void sample_filter_HBF_L24OCT(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp << 4, out1, out2;
	const FILTER_T var1 = TIM_FSCALE(DIV_3_2, 28);
	const FILTER_T var2 = TIM_FSCALE(DIV_3, 28);
	// filter1
	db[0] = input;
	db[5] = imuldiv28(db[0], dc[0]) + db[1];
	db[1] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[2];
	db[2] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	db[0] = db[5];
	db[5] = imuldiv28(db[0], dc[0]) + db[3];
	db[3] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[4];
	db[4] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	out1 = db[0];
	// filter2
	db[10] = input;
	db[15] = imuldiv28(db[10], dc[10]) + db[11];
	db[11] = imuldiv28(db[10], dc[11]) + imuldiv28(db[15], dc[13]) + db[12];
	db[12] = imuldiv28(db[10], dc[12]) + imuldiv28(db[15], dc[14]);
	db[10] = db[15];
	db[15] = imuldiv28(db[10], dc[10]) + db[3];
	db[13] = imuldiv28(db[10], dc[11]) + imuldiv28(db[15], dc[13]) + db[14];
	db[14] = imuldiv28(db[10], dc[12]) + imuldiv28(db[15], dc[14]);
	out2 = db[10];
	// output	
	*sp = (imuldiv28(out1, var1) + imuldiv28(out2, var2)) >> 4; /* 4.28 to 8.24 */
} 

static inline void recalc_filter_HBF_L24OCT(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p, s;
	
	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;		
		fc->last_reso_dB = fc->reso_dB;
		// filter1
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M( fc->reso_dB);
		r = f * f;
		p = 1 + (q * f) + r;
		s = r / p;
		dc[0] = TIM_FSCALE(s, 28);
		dc[1] = TIM_FSCALE(s * 2, 28);
		dc[2] = TIM_FSCALE(r / p, 28);
		dc[3] = TIM_FSCALE(2 * (r - 1) / (-p), 28);
		dc[4] = TIM_FSCALE((1 - (q * f) + r) / (-p), 28);
		// filter2
		f = 2.0 * fc->freq * fc->div_flt_rate;
		if(f > DIV_2)
			f = DIV_2;
		f = tan(M_PI * f); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M( fc->reso_dB);
		r = f * f;
		p = 1 + (q * f) + r;
		s = r / p;
		dc[10] = TIM_FSCALE(s, 28);
		dc[11] = TIM_FSCALE(s * 2, 28);
		dc[12] = TIM_FSCALE(r / p, 28);
		dc[13] = TIM_FSCALE(2 * (r - 1) / (-p), 28);
		dc[14] = TIM_FSCALE((1 - (q * f) + r) / (-p), 28);
	}
}

// multi

static inline void sample_filter_LPF_BWx2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[ 0] = *sp << 4;
	// filter1
	db[ 2] = imuldiv28(db[ 0], dc[0])
	       + imuldiv28(db[ 1], dc[1])
	       + imuldiv28(db[ 2], dc[2])
	       - imuldiv28(db[ 3], dc[3])
	       - imuldiv28(db[ 4], dc[4]);
	db[ 4] = db[ 3];
	db[ 3] = db[ 2]; // flt out
	db[ 2] = db[ 1];
	db[ 1] = db[ 0]; // flt in
	// filter2
	db[ 6] = imuldiv28(db[ 3], dc[0])
	       + imuldiv28(db[ 5], dc[1])
	       + imuldiv28(db[ 6], dc[2])
	       - imuldiv28(db[ 7], dc[3])
	       - imuldiv28(db[ 8], dc[4]);
	db[ 8] = db[ 7];
	db[ 7] = db[ 6]; // flt out
	db[ 6] = db[ 5];
	db[ 5] = db[ 3]; // flt in
	// output
	*sp = db[ 7] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF_BWx3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[ 0] = *sp << 4;
	// filter1
	db[ 2] = imuldiv28(db[ 0], dc[0])
	       + imuldiv28(db[ 1], dc[1])
	       + imuldiv28(db[ 2], dc[2])
	       - imuldiv28(db[ 3], dc[3])
	       - imuldiv28(db[ 4], dc[4]);
	db[ 4] = db[ 3];
	db[ 3] = db[ 2]; // flt out
	db[ 2] = db[ 1];
	db[ 1] = db[ 0]; // flt in
	// filter2
	db[ 6] = imuldiv28(db[ 3], dc[0])
	       + imuldiv28(db[ 5], dc[1])
	       + imuldiv28(db[ 6], dc[2])
	       - imuldiv28(db[ 7], dc[3])
	       - imuldiv28(db[ 8], dc[4]);
	db[ 8] = db[ 7];
	db[ 7] = db[ 6]; // flt out
	db[ 6] = db[ 5];
	db[ 5] = db[ 3]; // flt in
	// filter3
	db[10] = imuldiv28(db[ 7], dc[0])
	       + imuldiv28(db[ 9], dc[1])
	       + imuldiv28(db[10], dc[2])
	       - imuldiv28(db[11], dc[3])
	       - imuldiv28(db[12], dc[4]);
	db[12] = db[11];
	db[11] = db[10]; // flt out
	db[10] = db[ 9];
	db[ 9] = db[ 7]; // flt in
	// output
	*sp = db[11] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF_BWx4(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[ 0] = *sp << 4;
	// filter1
	db[ 2] = imuldiv28(db[ 0], dc[0])
	       + imuldiv28(db[ 1], dc[1])
	       + imuldiv28(db[ 2], dc[2])
	       - imuldiv28(db[ 3], dc[3])
	       - imuldiv28(db[ 4], dc[4]);
	db[ 4] = db[ 3];
	db[ 3] = db[ 2]; // flt out
	db[ 2] = db[ 1];
	db[ 1] = db[ 0]; // flt in
	// filter2
	db[ 6] = imuldiv28(db[ 3], dc[0])
	       + imuldiv28(db[ 5], dc[1])
	       + imuldiv28(db[ 6], dc[2])
	       - imuldiv28(db[ 7], dc[3])
	       - imuldiv28(db[ 8], dc[4]);
	db[ 8] = db[ 7];
	db[ 7] = db[ 6]; // flt out
	db[ 6] = db[ 5];
	db[ 5] = db[ 3]; // flt in
	// filter3
	db[10] = imuldiv28(db[ 7], dc[0])
	       + imuldiv28(db[ 9], dc[1])
	       + imuldiv28(db[10], dc[2])
	       - imuldiv28(db[11], dc[3])
	       - imuldiv28(db[12], dc[4]);
	db[12] = db[11];
	db[11] = db[10]; // flt out
	db[10] = db[ 9];
	db[ 9] = db[ 7]; // flt in
	// filter4
	db[14] = imuldiv28(db[11], dc[0])
	       + imuldiv28(db[13], dc[1])
	       + imuldiv28(db[14], dc[2])
	       - imuldiv28(db[15], dc[3])
	       - imuldiv28(db[16], dc[4]);
	db[16] = db[15];
	db[15] = db[14]; // flt out
	db[14] = db[13];
	db[13] = db[11]; // flt in
	// output
	*sp = db[15] >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_LPF24_2x2(FilterCoefficients *fc)
{
	FLOAT_T f, q, p, r, tmp;
	int32 *dc = fc->dc;

	if (!FP_EQ(fc->freq, fc->last_freq) || !FP_EQ(fc->reso_dB, fc->last_reso_dB)) {
		fc->last_freq = fc->freq;
		fc->last_reso_dB = fc->reso_dB;
		//f = 1.0 / tan(M_PI * fc->freq * fc->div_flt_rate);
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		//p = 1 + ((2.0) * f) + r;
		p = 1 + (q * f) + r;
		dc[0] = TIM_FSCALE(tmp = r / p, 28);
		dc[1] = TIM_FSCALE(tmp * 2, 28);
		dc[2] = TIM_FSCALE(r / p, 28);
		dc[3] = TIM_FSCALE(2 * (r - 1) / (-p), 28);
		//dc[4] = TIM_FSCALE((1 - ((2.0) * f) + r) / (-p), 28);
		dc[4] = TIM_FSCALE((1 - (q * f) + r) / (-p), 28);
	}
}

static inline void sample_filter_LPF24_2x2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	// filter1
	db[5] = imuldiv28(db[0], dc[0]) + db[1];
	db[1] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[2];
	db[2] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	db[10] = db[0] = db[5];
	db[5] = imuldiv28(db[0], dc[0]) + db[3];
	db[3] = imuldiv28(db[0], dc[1]) + imuldiv28(db[5], dc[3]) + db[4];
	db[4] = imuldiv28(db[0], dc[2]) + imuldiv28(db[5], dc[4]);
	// filter2
	db[15] = imuldiv28(db[10], dc[0]) + db[11];
	db[11] = imuldiv28(db[10], dc[1]) + imuldiv28(db[15], dc[3]) + db[12];
	db[12] = imuldiv28(db[10], dc[2]) + imuldiv28(db[15], dc[4]);
	db[10] = db[15];
	db[15] = imuldiv28(db[10], dc[0]) + db[13];
	db[13] = imuldiv28(db[10], dc[1]) + imuldiv28(db[15], dc[3]) + db[14];
	db[14] = imuldiv28(db[10], dc[2]) + imuldiv28(db[15], dc[4]);
	*sp = db[10] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF6x2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	db[1] = imuldiv28(db[0], dc[0]) + imuldiv28(db[1], dc[1]); // 6db
	db[2] = imuldiv28(db[0], dc[1]) + imuldiv28(db[1], dc[2]); // 12db
	*sp = db[2] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF6x3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	db[1] = imuldiv28(db[0], dc[0]) + imuldiv28(db[1], dc[1]); // 6db
	db[2] = imuldiv28(db[0], dc[1]) + imuldiv28(db[1], dc[2]); // 12db
	db[3] = imuldiv28(db[0], dc[2]) + imuldiv28(db[1], dc[3]);
	*sp = db[3] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF6x4(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	db[1] = imuldiv28(db[0], dc[0]) + imuldiv28(db[1], dc[1]); // 6db
	db[2] = imuldiv28(db[0], dc[1]) + imuldiv28(db[1], dc[2]); // 12db
	db[3] = imuldiv28(db[0], dc[2]) + imuldiv28(db[1], dc[3]);
	db[4] = imuldiv28(db[0], dc[3]) + imuldiv28(db[1], dc[4]); // 24db
	*sp = db[4] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF6x8(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	db[1] = imuldiv28(db[0], dc[0]) + imuldiv28(db[1], dc[1]); // 6db
	db[2] = imuldiv28(db[0], dc[1]) + imuldiv28(db[1], dc[2]); // 12db
	db[3] = imuldiv28(db[0], dc[2]) + imuldiv28(db[1], dc[3]);
	db[4] = imuldiv28(db[0], dc[3]) + imuldiv28(db[1], dc[4]); // 24db
	db[5] = imuldiv28(db[0], dc[4]) + imuldiv28(db[1], dc[5]);
	db[6] = imuldiv28(db[0], dc[5]) + imuldiv28(db[1], dc[6]); // 36db
	db[7] = imuldiv28(db[0], dc[6]) + imuldiv28(db[1], dc[7]);
	db[8] = imuldiv28(db[0], dc[7]) + imuldiv28(db[1], dc[8]); // 48db
	*sp = db[8] >> 4; /* 4.28 to 8.24 */
}

static inline void sample_filter_LPF6x16(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp << 4;
	db[1] = imuldiv28(db[0], dc[0]) + imuldiv28(db[1], dc[1]); // 6db
	db[2] = imuldiv28(db[0], dc[1]) + imuldiv28(db[1], dc[2]); // 12db
	db[3] = imuldiv28(db[0], dc[2]) + imuldiv28(db[1], dc[3]);
	db[4] = imuldiv28(db[0], dc[3]) + imuldiv28(db[1], dc[4]); // 24db
	db[5] = imuldiv28(db[0], dc[4]) + imuldiv28(db[1], dc[5]);
	db[6] = imuldiv28(db[0], dc[5]) + imuldiv28(db[1], dc[6]); // 36db
	db[7] = imuldiv28(db[0], dc[6]) + imuldiv28(db[1], dc[7]);
	db[8] = imuldiv28(db[0], dc[7]) + imuldiv28(db[1], dc[8]); // 48db
	db[9] = imuldiv28(db[0], dc[8]) + imuldiv28(db[1], dc[9]);
	db[10] = imuldiv28(db[0], dc[9]) + imuldiv28(db[1], dc[10]); // 60db
	db[11] = imuldiv28(db[0], dc[10]) + imuldiv28(db[1], dc[11]);
	db[12] = imuldiv28(db[0], dc[11]) + imuldiv28(db[1], dc[12]); // 72db
	db[13] = imuldiv28(db[0], dc[12]) + imuldiv28(db[1], dc[13]);
	db[14] = imuldiv28(db[0], dc[13]) + imuldiv28(db[1], dc[14]); // 84db
	db[15] = imuldiv28(db[0], dc[14]) + imuldiv28(db[1], dc[15]);
	db[16] = imuldiv28(db[0], dc[15]) + imuldiv28(db[1], dc[16]); // 96db
	*sp = db[16] >> 4; /* 4.28 to 8.24 */
}

// antialias
static inline void sample_filter_LPF_FIR(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
    int32 sum = 0;
	int i;
	for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += imuldiv24(db[i], dc[i]);	
	for (i = LPF_FIR_ORDER - 2; i >= 0; i--)
		db[i + 1] = db[i];
	db[0] = *sp;	
	*sp = sum;
}

static void designfir(FLOAT_T *g , FLOAT_T fc, FLOAT_T att);

static inline void recalc_filter_LPF_FIR(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;	
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
	FLOAT_T f;
	int i;

	if(FLT_FREQ_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		f = fc->freq * fc->div_flt_rate * 2.0;
		designfir(fir_coef, f, 40.0);
		for (i = 0; i < LPF_FIR_ORDER2; i++)
			dc[LPF_FIR_ORDER-1 - i] = dc[i] = TIM_FSCALE(fir_coef[LPF_FIR_ORDER2 - 1 - i], 24);
	}
}

// shelving 共通 
static inline void sample_filter_shelving(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[2] = imuldiv28(db[0], dc[0])
	      + imuldiv28(db[1], dc[1])
	      + imuldiv28(db[2], dc[2])
	      + imuldiv28(db[3], dc[3])
	      + imuldiv28(db[4], dc[4]);
	db[4] = db[3];
	db[3] = db[2];
	db[2] = db[1];
	db[1] = db[0];
	*sp = imuldiv28(db[3], dc[6]); /* 4.28 to 8.24 */ // spgain
}

static inline void recalc_filter_shelving_low(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, A, beta;

	if(fc->freq != fc->last_freq || fc->reso_dB != fc->last_reso_dB || fc->q != fc->last_q){
		fc->last_freq = fc->freq;	
		fc->last_reso_dB = fc->reso_dB;	
		fc->last_q = fc->q;			
		A = pow(10.0, fc->reso_dB * DIV_40 * ext_filter_shelving_gain);
		dc[6] = TIM_FSCALE(pow(10.0, -(fc->reso_dB) * DIV_80 * ext_filter_shelving_reduce), 28); // spgain
		omega = (FLOAT_T)2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		beta = sqrt(A) / (fc->q * ext_filter_shelving_q); // q > 0
		a0 = 1.0 / ((A + 1) + (A - 1) * cs + beta * sn);
		a1 = 2.0 * ((A - 1) + (A + 1) * cs);
		a2 = -((A + 1) + (A - 1) * cs - beta * sn);
		b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
		b1 = 2.0 * A * ((A - 1) - (A + 1) * cs);
		b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
		dc[4] = TIM_FSCALE(a2* a0, 28);
		dc[3] = TIM_FSCALE(a1* a0, 28);
		dc[2] = TIM_FSCALE(b2* a0, 28);
		dc[1] = TIM_FSCALE(b1* a0, 28);
		dc[0] = TIM_FSCALE(b0* a0, 28);
	}
}

static inline void recalc_filter_shelving_hi(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, A, beta;

	if(fc->freq != fc->last_freq || fc->reso_dB != fc->last_reso_dB || fc->q != fc->last_q){
		fc->last_freq = fc->freq;	
		fc->last_reso_dB = fc->reso_dB;	
		fc->last_q = fc->q;			
		A = pow(10.0, fc->reso_dB * DIV_40 * ext_filter_shelving_gain);
		dc[6] = TIM_FSCALE(pow(10.0, -(fc->reso_dB) * DIV_80 * ext_filter_shelving_reduce), 28); // spgain
		omega = (FLOAT_T)2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		beta = sqrt(A) / (fc->q * ext_filter_shelving_q); // q > 0
		a0 = 1.0 / ((A + 1) - (A - 1) * cs + beta * sn);
		a1 = (-2.0 * ((A - 1) - (A + 1) * cs));
		a2 = -((A + 1) - (A - 1) * cs - beta * sn);
		b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
		b1 = -2.0 * A * ((A - 1) + (A + 1) * cs);
		b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
		dc[4] = TIM_FSCALE(a2* a0, 28);
		dc[3] = TIM_FSCALE(a1* a0, 28);
		dc[2] = TIM_FSCALE(b2* a0, 28);
		dc[1] = TIM_FSCALE(b1* a0, 28);
		dc[0] = TIM_FSCALE(b0* a0, 28);
	}
}

// peaking 共通 
static inline void sample_filter_peaking(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[2] = imuldiv28(db[0], dc[0])
	      + imuldiv28(db[1], dc[1])
	      + imuldiv28(db[2], dc[2])
	      - imuldiv28(db[3], dc[3])
	      - imuldiv28(db[4], dc[4]);
	db[4] = db[3];
	db[3] = db[2];
	db[2] = db[1];
	db[1] = db[0];
	*sp = imuldiv28(db[3], dc[6]); // spgain
}

static inline void recalc_filter_peaking(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, A, beta;

	if(fc->freq != fc->last_freq || fc->reso_dB != fc->last_reso_dB || fc->q != fc->last_q){
		fc->last_freq = fc->freq;	
		fc->last_reso_dB = fc->reso_dB;	
		fc->last_q = fc->q;			
		A = pow(10.0, fc->reso_dB * DIV_40 * ext_filter_peaking_gain);
		dc[6] = TIM_FSCALE(pow(10.0, -(fc->reso_dB) * DIV_80 * ext_filter_peaking_reduce), 28); // spgain
		omega = (FLOAT_T)2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		beta = sn / (2.0 * fc->q * ext_filter_peaking_q); // q > 0
		a0 = 1.0 / (1.0 + beta / A);
		a1 = -2.0 * cs;
		a2 = 1.0 - beta / A;
		b0 = 1.0 + beta * A;
		b2 = 1.0 - beta * A;
		a2 *= a0;
		a1 *= a0;
		b2 *= a0;
		b0 *= a0;		
		dc[4] = TIM_FSCALE(a2, 28);
		dc[3] = TIM_FSCALE(a1, 28);
		dc[2] = TIM_FSCALE(b2, 28);
		dc[1] = TIM_FSCALE(a1, 28); // b1 = a1
		dc[0] = TIM_FSCALE(b0, 28);
	}
}

// biquad 共通 
static inline void sample_filter_biquad(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	DATA_T input = *sp << 4, r;

	r = imuldiv28(db[1], dc[1])
		+ imuldiv28(*sp + db[2], dc[2])
		- imuldiv28(db[3], dc[3])
		- imuldiv28(db[4], dc[4]); // -dc3 -dc4 
	db[2] = r;
	db[4] = db[3];
	db[3] = db[2];
	db[2] = db[1];
	db[1] = input;
	*sp = r >> 4; /* 4.28 to 8.24 */
}

static inline void recalc_filter_biquad_low(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, alpha;

	if(fc->freq != fc->last_freq || fc->q != fc->last_q){
		fc->last_freq = fc->freq;	
		fc->last_reso_dB = fc->reso_dB;	
		fc->last_q = fc->q;			
		omega = 2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		alpha = sn / (2.0 * fc->q); // q > 0
		a0 = 1.0 / (1.0 + alpha);
		dc[1] = TIM_FSCALE((1.0 - cs) * a0, 28);
		dc[2] = dc[0] = TIM_FSCALE(((1.0 - cs) * DIV_2) * a0, 28);
		dc[3] = TIM_FSCALE((-2.0 * cs) * a0, 28);
		dc[4] = TIM_FSCALE((1.0 - alpha) * a0, 28);
		//b2 = ((1.0 - cs) * DIV_2) * a0;
		//b1 = (1.0 - cs) * a0;
		//a1 = (-2.0 * cs) * a0;
		//a2 = (1.0 - alpha) * a0;
		//dc[0] = TIM_FSCALE(b2, 28);
		//dc[1] = TIM_FSCALE(b1, 28);
		//dc[2] = TIM_FSCALE(a1, 28);
		//dc[3] = TIM_FSCALE(a2, 28);
	}
}

static inline void recalc_filter_biquad_hi(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, alpha;

	if(fc->freq != fc->last_freq || fc->q != fc->last_q){
		fc->last_freq = fc->freq;	
		fc->last_reso_dB = fc->reso_dB;	
		fc->last_q = fc->q;			
		omega = 2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		alpha = sn / (2.0 * fc->q); // q > 0
		a0 = 1.0 / (1.0 + alpha);
		dc[1] = TIM_FSCALE((-(1.0 + cs)) * a0, 28);
		dc[2] = dc[0] = TIM_FSCALE(((1.0 + cs) * DIV_2) * a0, 28);
		dc[3] = TIM_FSCALE((-2.0 * cs) * a0, 28);
		dc[4] = TIM_FSCALE((1.0 - alpha) * a0, 28);
		//b2 = ((1.0 + cs) * DIV_2) * a0;
		//b1 = (-(1.0 + cs)) * a0;
		//a1 = (-2.0 * cs) * a0;
		//a2 = (1.0 - alpha) * a0;
		//dc[0] = TIM_FSCALE(b2, 28);
		//dc[1] = TIM_FSCALE(b1, 28);
		//dc[2] = TIM_FSCALE(a1, 28);
		//dc[3] = TIM_FSCALE(a2, 28);
	}
}

#else /* floating-point implementation */

#ifdef USE_PENTIUM_4
#define DENORMAL_FIX 1 // for pentium 4 float/double denormal fix
#define DENORMAL_ADD (5.4210108624275221703311375920553e-20) // 1.0/(1<<64)
const FLOAT_T denormal_add = DENORMAL_ADD; // 1.0/(1<<64)

#if (USE_X86_EXT_INTRIN >= 3) && defined(FLOAT_T_DOUBLE)
const __m128d vec_denormal_add = {DENORMAL_ADD, DENORMAL_ADD};
#elif (USE_X86_EXT_INTRIN >= 2) && defined(FLOAT_T_FLOAT)
const __m128 vec_denormal_add = {DENORMAL_ADD, DENORMAL_ADD, DENORMAL_ADD, DENORMAL_ADD};
#endif // USE_X86_EXT_INTRIN

#endif // USE_PENTIUM_4


static inline void sample_filter_none(FILTER_T *dc, FILTER_T *db, DATA_T *sp){}

static inline void recalc_filter_none(FilterCoefficients *fc){}

static inline void sample_filter_LPF12(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + db[2] * dc[0];
	db[1] = *sp - db[0] - db[2] * dc[1];
	db[2] = db[1] * dc[0] + db[2];
	*sp = db[0];
}

static inline void sample_filter_LPF12_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + db[2] * dc[0];
	db[1] = input - db[0] - db[2] * dc[1];
	db[2] = db[1] * dc[0] + db[2];
	*sp = db[0];
	// ov2
	db[0] = db[0] + db[2] * dc[0];
	db[1] = input - db[0] - db[2] * dc[1];
	db[2] = db[1] * dc[0] + db[2];
}

static inline void sample_filter_LPF12_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + db[2] * dc[0];
	db[1] = input - db[0] - db[2] * dc[1];
	db[2] = db[1] * dc[0] + db[2];
	*sp = db[0];
	// ov2
	db[0] = db[0] + db[2] * dc[0];
	db[1] = input - db[0] - db[2] * dc[1];
	db[2] = db[1] * dc[0] + db[2];
	// ov3
	db[0] = db[0] + db[2] * dc[0];
	db[1] = input - db[0] - db[2] * dc[1];
	db[2] = db[1] * dc[0] + db[2];
}

static inline void recalc_filter_LPF12(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;

/* copy with applying Chamberlin's lowpass filter. */
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		if(fc->freq < fc->flt_rate_limit1){ // <sr*DIV_6
			fc->sample_filter = sample_filter_LPF12;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*DIV_6
			fc->sample_filter = sample_filter_LPF12_ov2;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2); // sr*2
		}else{ // <sr*3*DIV_6
			fc->sample_filter = sample_filter_LPF12_ov3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3); // sr*3
		}
		dc[1] = RESO_DB_CF_M(fc->reso_dB);
	}
}

static inline void sample_filter_LPF24(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T da[6];

	da[0] = *sp - dc[2] * db[4];	/* feedback */
	da[1] = db[1];
	da[2] = db[2];
	da[3] = db[3];
	db[1] = (db[0] + da[0]) * dc[0] - db[1] * dc[1];
	db[2] = (db[1] + da[1]) * dc[0] - db[2] * dc[1];
	db[3] = (db[2] + da[2]) * dc[0] - db[3] * dc[1];
	db[4] = (db[3] + da[3]) * dc[0] - db[4] * dc[1];
	db[0] = da[0];
	*sp = db[4];
}

static inline void recalc_filter_LPF24(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc, f, q ,p, r;

/* copy with applying Moog lowpass VCF. */
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		f = 2.0 * fc->freq * fc->div_flt_rate;
		p = 1.0 - f;
		q = 0.80 * (1.0 - RESO_DB_CF_M(fc->reso_dB)); // 0.0f <= c < 0.80f
		dc[0] = f + 0.8 * f * p;
		dc[1] = dc[0] + dc[0] - 1.0;
		dc[2] = q * (1.0 + 0.5 * p * (1.0 - p + 5.6 * p * p));
	}
}

static inline void sample_filter_LPF_BW(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp;	
	// LPF
	db[2] = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2] + dc[3] * db[3] + dc[4] * db[4];
#if defined(DENORMAL_FIX)
	db[2] += denormal_add;
#endif	
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[3];
} 

static inline void recalc_filter_LPF_BW(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	double q ,p, p2, qp, dc0;

// elion butterworth
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		p = 1.0 / tan(M_PI * fc->freq * (double)fc->div_flt_rate); // ?
		q = RESO_DB_CF_M(fc->reso_dB) * SQRT_2; // q>0.1
		p2 = p * p;
		qp = q * p;
		dc0 = 1.0 / ( 1.0 + qp + p2);
		dc[0] = dc0;
		dc[1] = 2.0 * dc0;
		dc[2] = dc0;
		dc[3] = -2.0 * ( 1.0 - p2) * dc0; // -
		dc[4] = -(1.0 - qp + p2) * dc0; // -
	}
}

static inline void sample_filter_LPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[1] += (*sp - db[0]) * dc[1];
	db[0] += db[1];
	db[1] *= dc[0];
	*sp = db[0];
}

static inline void recalc_filter_LPF12_2(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, q ,p, r;
	FLOAT_T c0, c1, a0, b1, b2;

// Resonant IIR lowpass (12dB/oct) Olli Niemitalo //r
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		
		c0 = q * q;
		c1 = c0 + 1.0 - 2.0 * cos(f) * q;
		dc[0] = c0;
		dc[1] = c1;
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
		a0 = c1;
		b1 = 1 + c0 - c1;
		b2 = -c0;
		dc[2] = a0;
		dc[3] = a0 * b1;
		dc[4] = 0;
		dc[5] = a0;
		dc[6] = b2;
		dc[7] = b2 * b1;
		dc[8] = b1;
		dc[9] = b1 * b1 + b2;
#endif
	}
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
// SIMD optimization (double * 2)
static inline void buffer_filter_LPF12_2(FILTER_T* dc, FILTER_T* db, DATA_T* sp, int32 count)
{
	int32 i;
	__m256d vcx0 = _mm256_broadcast_pd((__m128d *)(dc + 2));
	__m256d vcx1 = _mm256_broadcast_pd((__m128d *)(dc + 4));
	__m128d vcym2 = _mm_loadu_pd(dc + 6);
	__m128d vcym1 = _mm_loadu_pd(dc + 8);
	__m128d vy = _mm_loadu_pd(db + 2);
	__m128d vym2 = _mm_unpacklo_pd(vy, vy);
	__m128d vym1 = _mm_unpackhi_pd(vy, vy);

	for (i = 0; i < count; i += 4)
	{
		__m256d vin = _mm256_loadu_pd(sp + i);
		__m256d vx0 = _mm256_unpacklo_pd(vin, vin);
		__m256d vx1 = _mm256_unpackhi_pd(vin, vin);
		__m256d vfma2x = MM256_FMA2_PD(vcx0, vx0, vcx1, vx1);

		__m128d vy0 = _mm_add_pd(_mm256_castpd256_pd128(vfma2x), MM_FMA2_PD(vcym2, vym2, vcym1, vym1));
		_mm_storeu_pd(sp + i, vy0);
		vym2 = _mm_unpacklo_pd(vy0, vy0);
		vym1 = _mm_unpackhi_pd(vy0, vy0);

		__m128d vy1 = _mm_add_pd(_mm256_extractf128_pd(vfma2x, 1), MM_FMA2_PD(vcym2, vym2, vcym1, vym1));
		_mm_storeu_pd(sp + i + 2, vy1);
		vym2 = _mm_unpacklo_pd(vy1, vy1);
		vym1 = _mm_unpackhi_pd(vy1, vy1);
		vy = vy1;
	}

	_mm_storeu_pd(db + 2, vy);
}

#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)
// SIMD optimization (double * 2)
static inline void buffer_filter_LPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp, int32 count)
{
	int32 i;
	__m128d vcx0 = _mm_loadu_pd(dc + 2);
	__m128d vcx1 = _mm_loadu_pd(dc + 4);
	__m128d vcym2 = _mm_loadu_pd(dc + 6);
	__m128d vcym1 = _mm_loadu_pd(dc + 8);
	__m128d vy = _mm_loadu_pd(db + 2);
	__m128d vym2 = _mm_unpacklo_pd(vy, vy);
	__m128d vym1 = _mm_unpackhi_pd(vy, vy);

	for (i = 0; i < count; i += 2) {
		__m128d vin = _mm_loadu_pd(sp + i);
		__m128d vx0 = _mm_unpacklo_pd(vin, vin);
		__m128d vx1 = _mm_unpackhi_pd(vin, vin);
		vy = MM_FMA4_PD(vcx0, vx0,  vcx1, vx1,  vcym2, vym2,  vcym1, vym1);
		_mm_storeu_pd(sp + i, vy);
		vym2 = _mm_unpacklo_pd(vy, vy);
		vym1 = _mm_unpackhi_pd(vy, vy);
	}
	_mm_storeu_pd(db + 2, vy);
}

#else // scalar
static inline void buffer_filter_LPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp, int32 count)
{
	int32 i;
	FILTER_T db0 = db[0], db1 = db[1], dc0 = dc[0], dc1 = dc[1];

	for (i = 0; i < count; i++) {
		db1 += (sp[i] - db0) * dc1;
		db0 += db1;
		sp[i] = db0;
		db1 *= dc0;
	}
	db[0] = db0;
	db[1] = db1;
}

#endif // (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE) && defined(FLOAT_T_DOUBLE)

static inline void sample_filter_LPF24_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[5] = dc[0] * db[0] + db[1];
	db[1] = dc[1] * db[0] + dc[3] * db[5] + db[2];
	db[2] = dc[2] * db[0] + dc[4] * db[5];
	db[0] = db[5];
	db[5] = dc[0] * db[0] + db[3];
	db[3] = dc[1] * db[0] + dc[3] * db[5] + db[4];
	db[4] = dc[2] * db[0] + dc[4] * db[5];
	*sp = db[0];
}

static inline void recalc_filter_LPF24_2(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc, f, q ,p, r, dc0;

// amSynth 24dB/ocatave resonant low-pass filter. Nick Dowell //r
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		p = 1.0 / (1.0 + (q * f) + r);
		dc0 = r * p;
		dc[0] = dc0;
		dc[1] = dc0 * 2;
		dc[2] = dc0;
		dc[3] = -2.0 * (r - 1) * p;
		dc[4] = (-1.0 + (q * f) - r) * p;
	}
}

static inline void sample_filter_LPF6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	*sp = (db[1] = dc[0] * *sp + dc[1] * db[1]);
#if defined(DENORMAL_FIX)
	db[1] += denormal_add;
#endif	
}

static inline void recalc_filter_LPF6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc, f;

// One pole filter, LP 6dB/Oct scoofy no resonance //r
	if(FLT_FREQ_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[0] = 1.0 - f;
		dc[1] = f;
	}
}

static inline void sample_filter_LPF18(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T da[6];

	da[0] = db[0];
	da[1] = db[1];
	da[2] = db[2];
	db[0] = *sp - dc[2] * db[3];
	db[1] = dc[1] * (db[0] + da[0]) - dc[0] * db[1];
	db[2] = dc[1] * (db[1] + da[1]) - dc[0] * db[2];
	db[3] = dc[1] * (db[2] + da[2]) - dc[0] * db[3];
	*sp = db[3] * dc[3];
}

static inline void sample_filter_LPF18_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T da[6], input = *sp;

	da[0] = db[0];
	da[1] = db[1];
	da[2] = db[2];
	db[0] = input - dc[2] * db[3];
	db[1] = dc[1] * (db[0] + da[0]) - dc[0] * db[1];
	db[2] = dc[1] * (db[1] + da[1]) - dc[0] * db[2];
	db[3] = dc[1] * (db[2] + da[2]) - dc[0] * db[3];
	*sp = db[3] * dc[3];
	// ov2
	da[0] = db[0];
	da[1] = db[1];
	da[2] = db[2];
	db[0] = input - dc[2] * db[3];
	db[1] = dc[1] * (db[0] + da[0]) - dc[0] * db[1];
	db[2] = dc[1] * (db[1] + da[1]) - dc[0] * db[2];
	db[3] = dc[1] * (db[2] + da[2]) - dc[0] * db[3];
}

static inline void recalc_filter_LPF18(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc, f, q , p;
// LPF18 low-pass filter //r
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		if(fc->freq < fc->flt_rate_limit1){ // <sr/2.25
			fc->sample_filter = sample_filter_LPF18;
			f = 2.0 * fc->freq * fc->div_flt_rate; // *1.0
		}else{ // <sr*2/2.25
			fc->sample_filter = sample_filter_LPF18_ov2;
			f = 2.0 * fc->freq * fc->div_flt_rate_ov2; // sr*2
		}
		dc[0] = ((-2.7528 * f + 3.0429) * f + 1.718) * f - 0.9984;
		q = 0.789 * (1.0 - RESO_DB_CF_M(fc->reso_dB)); // 0<q<0.78125
		p = dc[0] + 1.0;
		dc[1] = 0.5 * p;
		dc[2] = q * (((-2.7079 * p + 10.963) * p - 14.934) * p + 8.4974);
		dc[3] = 1.0 + (0.25 * (1.5 + 2.0 * dc[2] * (1.0 - f)));
	}
}

static inline void sample_filter_LPF_TFO(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + dc[0] * (*sp - db[0] + dc[1] * (db[0] - db[1]));
	db[1] = db[1] + dc[0] * (db[0] - db[1]);
	*sp = db[1];
}

static inline void recalc_filter_LPF_TFO(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc, q;

// two first order low-pass filter //r
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		dc[0] = 2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - RESO_DB_CF_M(fc->reso_dB);
		dc[1] = q + q / (1.01 - dc[0]);
	}
}

static inline void sample_filter_HPF_BW(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp;	
	// LPF
	db[2] = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2] + dc[3] * db[3] + dc[4] * db[4];
#if defined(DENORMAL_FIX)
	db[2] += denormal_add;
#endif	
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[3];
}

static inline void recalc_filter_HPF_BW(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	double q, p, p2, qp, dc0;

// elion butterworth HPF //r
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		q = RESO_DB_CF_M(fc->reso_dB) * SQRT_2; // q>0.1
		p = tan(M_PI * fc->freq * fc->div_flt_rate); // hpf ?		
		p2 = p * p;
		qp = q * p;
		dc0 = 1.0 / (1.0 + qp + p2);
		dc[0] = dc0;
		dc[1] = -2 * dc0; // hpf
		dc[2] = dc0;
		dc[3] = -2.0 * (p2 - 1.0) * dc0; // hpf
		dc[4] = -(1.0 - qp + p2) * dc0;		
	}
}

static inline void sample_filter_BPF_BW(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp;	
	// LPF
	db[2] = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2] + dc[3] * db[3] + dc[4] * db[4];
	// HPF
	db[10] = dc[8] * db[8] + dc[9] * db[9] + dc[10] * db[10] + dc[11] * db[11] + dc[12] * db[12];	
#if defined(DENORMAL_FIX)
	db[2] += denormal_add;
#endif	
	// HPF
	db[12] = db[11];
	db[11] = db[10]; // flt out
	db[10] = db[9];
	db[9] = db[8]; // flt in
	// con	
	db[8] = db[4]; // db[4]からdb[8]へは遅延してもいい
	// LPF
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[11];
}

static inline void recalc_filter_BPF_BW(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, q, pl, pl2, qpl, ph, ph2, qph, dc0;
	
// elion butterworth
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		f = fc->freq * fc->div_flt_rate;
		q = RESO_DB_CF_M(fc->reso_dB) * SQRT_2; // q>0.1
		// LPF
		pl = 1.0 / tan(M_PI * f);
		pl2 = pl * pl;
		qpl = q * pl;
		dc0 = 1.0 / ( 1.0 + qpl + pl2);
		dc[0] = dc0;
		dc[1] = 2.0 * dc0;
		dc[2] = dc0;
		dc[3] = -2.0 * ( 1.0 - pl2) * dc0; // -
		dc[4] = -(1.0 - qpl + pl2) * dc0; // -
		// HPF
		ph = tan(M_PI * f * 0.8); // hpf // f bandwidth = LPF-HPF
		ph2 = ph * ph;
		qph = q * ph;
		dc0 = 1.0 / (1.0 + qph + ph2);
		dc[8] = dc0;
		dc[9] = -2 * dc0; // hpf
		dc[10] = dc0;
		dc[11] = -2.0 * (ph2 - 1.0) * dc0; // hpf
		dc[12] = -(1.0 - qph + ph2) * dc0;
	}
}

static inline void sample_filter_peak1(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T r;

	db[0] = *sp;
	r = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2];
	db[2] = db[1];
	db[1] = r;
	*sp = r;
}

static inline void recalc_filter_peak1(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc, f, q, r, pl ,ph, sl, sh;
	
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN	
		f = cos(M_PI2 * fc->freq * fc->div_flt_rate);
		r = (1.0 - RESO_DB_CF_M(fc->reso_dB)) * 0.99609375; // r < 0.99609375
		dc[0] = (1 - r) * sqrt(r * (r - 4 * (f * f) + 2.0) + 1.0);
		dc[1] = 2 * f * r;
		dc[2] = -(r * r);
	}
}

static inline void sample_filter_notch1(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T r;

	db[0] = *sp;
	r = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2];
	db[2] = db[1];
	db[1] = r;
	*sp = db[0] - r; // notch
}

static inline void sample_filter_LPF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * *sp - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[0]; // db[1] + db[0]; // notch
}

static inline void sample_filter_LPF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[0]; // db[1] + db[0]; // notch
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void sample_filter_LPF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;

	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[0]; // db[1] + db[0]; // notch
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	// ov3
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void recalc_filter_LPF12_3(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;

/* Chamberlin2's lowpass filter. */
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_LPF12_3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_LPF12_3_ov2;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_LPF12_3_ov3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3); // sr*3
		}
		dc[1] = RESO_DB_CF_M(fc->reso_dB);
	}
}

static inline void sample_filter_HPF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * *sp - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[1]; // db[1] + db[0]; // notch
}

static inline void sample_filter_HPF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[1]; // db[1] + db[0]; // notch
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void sample_filter_HPF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[1]; // db[1] + db[0]; // notch
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	// ov3
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void recalc_filter_HPF12_3(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;

/* Chamberlin2's lowpass filter. */
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_HPF12_3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_HPF12_3_ov2;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_HPF12_3_ov3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3); // sr*3
		}
		dc[1] = RESO_DB_CF_M(fc->reso_dB);
	}
}

static inline void sample_filter_BPF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * *sp - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[2]; // db[1] + db[0]; // notch
#if defined(DENORMAL_FIX)
	db[0] += denormal_add;
#endif	
}

static inline void sample_filter_BPF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[2]; // db[1] + db[0]; // notch
#if defined(DENORMAL_FIX)
	db[0] += denormal_add;
#endif	
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void sample_filter_BPF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[2]; // db[1] + db[0]; // notch
#if defined(DENORMAL_FIX)
	db[0] += denormal_add;
#endif	
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	// ov3
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void recalc_filter_BPF12_3(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;

/* Chamberlin2's lowpass filter. */
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_BPF12_3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_BPF12_3_ov2;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_BPF12_3_ov3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3); // sr*3
		}
		dc[1] = RESO_DB_CF_M(fc->reso_dB);
	}
}

static inline void sample_filter_BCF12_3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * *sp - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[1] + db[0]; // notch
}

static inline void sample_filter_BCF12_3_ov2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;
	
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	*sp = db[1] + db[0]; // notch
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void sample_filter_BCF12_3_ov3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FILTER_T input = *sp;

	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	db[3] = db[1] + db[0]; // notch
	*sp = db[1] + db[0]; // notch
	// ov2
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
	// ov3
	db[0] = db[0] + dc[0] * db[2]; // low
	db[1] = dc[1] * input - db[0] - dc[1] * db[2]; // high
	db[2] = dc[0] * db[1] + db[2]; // band
}

static inline void recalc_filter_BCF12_3(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;

/* Chamberlin2's lowpass filter. */
	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN	
		if(fc->freq < fc->flt_rate_limit1){ // <sr*0.21875
			fc->sample_filter = sample_filter_BCF12_3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate); // *1.0
		}else if(fc->freq < fc->flt_rate_limit2){ // <sr*2*0.21875
			fc->sample_filter = sample_filter_BCF12_3_ov2;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov2); // sr*2
		}else{ // <sr*3*0.21875
			fc->sample_filter = sample_filter_BCF12_3_ov3;
			dc[0] = 2.0 * sin(M_PI * fc->freq * fc->div_flt_rate_ov3); // sr*3
		}
		dc[1] = RESO_DB_CF_M(fc->reso_dB);
	}
}

static inline void sample_filter_HPF6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	*sp -= (db[1] = dc[0] * *sp + dc[1] * db[1]);
#if defined(DENORMAL_FIX)
	db[1] += denormal_add;
#endif		
}

static inline void sample_filter_HPF12_2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[1] += (*sp - db[0]) * dc[1];
	db[0] += db[1];
	db[1] *= dc[0];
	*sp -= db[0];
}


// hybrid
static inline void sample_filter_HBF_L6L12(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[1] += (in - db[0]) * dc[1];
	db[0] += db[1];
	db[1] *= dc[0];
	out1 = db[0];
	// filter2
	db[11] = dc[10] * in + dc[11] * db[11];	
	out2 = db[11];
#if defined(DENORMAL_FIX)
	db[11] += denormal_add;
#endif	
	// output
	dc[16] = dc[16] * 0.75 + dc[15] * 0.25;
	*sp = (out1 * dc[16] + out2 * (1.0 - dc[16]));
} 

static inline void recalc_filter_HBF_L6L12(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q, t;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		dc[0] = q * q;
		dc[1] = dc[0] + 1.0 - 2.0 * cos(f) * q;
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = 1.0 - f;
		dc[11] = f;
		// 
		dc[15] = 1.0 - RESO_DB_CF_M(fc->reso_dB);
	}
}

static inline void sample_filter_HBF_L12L6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[1] += (in - db[0]) * dc[1];
	db[0] += db[1];
	db[1] *= dc[0];
	out1 = db[0];
	// filter2
	db[11] = dc[10] * in + dc[11] * db[11];	
	out2 = db[11];
#if defined(DENORMAL_FIX)
	db[11] += denormal_add;
#endif	
	// output	
	*sp = out1 + out2 * DIV_2;
} 

static inline void recalc_filter_HBF_L12L6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q, t;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		dc[0] = q * q;
		dc[1] = dc[0] + 1.0 - 2.0 * cos(f) * q;
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = 1.0 - f;
		dc[11] = f;
	}
}

static inline void sample_filter_HBF_L12H6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[1] += (in - db[0]) * dc[1];
	db[0] += db[1];
	db[1] *= dc[0];
	out1 = db[0];
	// filter2
	db[11] = dc[10] * in + dc[11] * db[11];	
	out2 = in - db[11];
#if defined(DENORMAL_FIX)
	db[11] += denormal_add;
#endif	
	// output	
	*sp = out1 + out2 * DIV_2;
} 

static inline void recalc_filter_HBF_L12H6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q, t;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(fc->reso_dB) + 0.5 / (1.0 + f)) + f - 2.0);
		dc[0] = q * q;
		dc[1] = dc[0] + 1.0 - 2.0 * cos(f) * q;
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = 1.0 - f;
		dc[11] = f;
	}
}

static inline void sample_filter_HBF_L24H6(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[0] = in;
	db[5] = dc[0] * db[0] + db[1];
	db[1] = dc[1] * db[0] + dc[3] * db[5] + db[2];
	db[2] = dc[2] * db[0] + dc[4] * db[5];
	db[0] = db[5];
	db[5] = dc[0] * db[0] + db[3];
	db[3] = dc[1] * db[0] + dc[3] * db[5] + db[4];
	db[4] = dc[2] * db[0] + dc[4] * db[5];
	out1 = db[0];
	// filter2
	db[11] = dc[10] * in + dc[11] * db[11];	
	out2 = in - db[11];
#if defined(DENORMAL_FIX)
	db[11] += denormal_add;
#endif	
	// output	
	*sp = out1 + out2 * DIV_2;
} 

static inline void recalc_filter_HBF_L24H6(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p, dc0;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		p = 1.0 / (1.0 + (q * f) + r);
		dc0 = r * p;
		dc[0] = dc0;
		dc[1] = dc0 * 2;
		dc[2] = dc0;
		dc[3] = -2.0 * (r - 1) * p;
		dc[4] = (-1.0 + (q * f) - r) * p;
		// filter2
		f = exp(-M_PI2 * fc->freq * fc->div_flt_rate);
		dc[10] = 1.0 - f;
		dc[11] = f;
	}
}


static inline void sample_filter_HBF_L24H12(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{	
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[0] = in;
	db[5] = dc[0] * db[0] + db[1];
	db[1] = dc[1] * db[0] + dc[3] * db[5] + db[2];
	db[2] = dc[2] * db[0] + dc[4] * db[5];
	db[0] = db[5];
	db[5] = dc[0] * db[0] + db[3];
	db[3] = dc[1] * db[0] + dc[3] * db[5] + db[4];
	db[4] = dc[2] * db[0] + dc[4] * db[5];
	out1 = db[0];
	// filter2
	db[11] += (in - db[10]) * dc[11];
	db[10] += db[11];
	db[11] *= dc[10];
	out2 = in - db[10];
	// output	
	*sp = out1 + out2 * DIV_2;
} 

static inline void recalc_filter_HBF_L24H12(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p, p2, qp, dc0;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		p = 1.0 / (1.0 + (q * f) + r);
		dc0 = r * p;
		dc[0] = dc0;
		dc[1] = dc0 * 2;
		dc[2] = dc0;
		dc[3] = -2.0 * (r - 1) * p;
		dc[4] = (-1.0 + (q * f) - r) * p;
		// filter2
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		q = 1.0 - f / (2.0 * (RESO_DB_CF_P(0) + 0.5 / (1.0 + f)) + f - 2.0);
		dc0 = q * q;
		dc[10] = dc0;
		dc[11] = dc0 + 1.0 - 2.0 * cos(f) * q;
	}
}

static inline void sample_filter_HBF_L12OCT(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[1] += (in - db[0]) * dc[1];
	db[0] += db[1];
	db[1] *= dc[0];
	out1 = db[0];
	// filter2
	db[11] += (in - db[10]) * dc[11];
	db[10] += db[11];
	db[11] *= dc[10];
	out2 = db[10];
	// output	
	*sp = out1 * DIV_3_2 + out2 * DIV_3;
} 

static inline void recalc_filter_HBF_L12OCT(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = M_PI2 * fc->freq * fc->div_flt_rate;
		r = RESO_DB_CF_P(fc->reso_dB);
		q = 1.0 - f / (2.0 * (r + 0.5 / (1.0 + f)) + f - 2.0);
		dc[0] = q * q;
		dc[1] = dc[0] + 1.0 - 2.0 * cos(f) * q;
		// filter2
		f = 2.0 * fc->freq * fc->div_flt_rate;
		if(f > DIV_2)
			f = DIV_2;
		f = M_PI2 * f;
		q = 1.0 - f / (2.0 * (r + 0.5 / (1.0 + f)) + f - 2.0);
		dc[10] = q * q;
		dc[11] = dc[10] + 1.0 - 2.0 * cos(f) * q;
	}
}

static inline void sample_filter_HBF_L24OCT(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	FLOAT_T in = *sp, out1, out2;
	// filter1
	db[0] = in;
	db[5] = dc[0] * db[0] + db[1];
	db[1] = dc[1] * db[0] + dc[3] * db[5] + db[2];
	db[2] = dc[2] * db[0] + dc[4] * db[5];
	db[0] = db[5];
	db[5] = dc[0] * db[0] + db[3];
	db[3] = dc[1] * db[0] + dc[3] * db[5] + db[4];
	db[4] = dc[2] * db[0] + dc[4] * db[5];
	out1 = db[0];
	// filter2
	db[10] = in;
	db[15] = dc[10] * db[10] + db[11];
	db[11] = dc[11] * db[10] + dc[13] * db[15] + db[12];
	db[12] = dc[12] * db[10] + dc[14] * db[15];
	db[10] = db[15];
	db[15] = dc[10] * db[10] + db[13];
	db[13] = dc[11] * db[10] + dc[13] * db[15] + db[14];
	db[14] = dc[12] * db[10] + dc[14] * db[15];
	out2 = db[10];
	// output	
	*sp = out1 * DIV_3_2 + out2 * DIV_3;
} 

static inline void recalc_filter_HBF_L24OCT(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T f, r, q ,p, dc0;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		// filter1
		f = tan(M_PI * fc->freq * fc->div_flt_rate); // cutoff freq rate/2
		q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		p = 1.0 / (1.0 + (q * f) + r);
		dc0 = r * p;
		dc[0] = dc0;
		dc[1] = dc0 * 2;
		dc[2] = dc0;
		dc[3] = -2.0 * (r - 1) * p;
		dc[4] = (-1.0 + (q * f) - r) * p;
		// filter2
		f = 2.0 * fc->freq * fc->div_flt_rate;
		if(f > 0.4999)
			f = 0.4999;		
		f = tan(M_PI * f); // cutoff freq rate/2
	//	q = 2.0 * RESO_DB_CF_M(fc->reso_dB);
		r = f * f;
		p = 1.0 / (1.0 + (q * f) + r);
		dc0 = r * p;
		dc[10] = dc0;
		dc[11] = dc0 * 2;
		dc[12] = dc0;
		dc[13] = -2.0 * (r - 1) * p;
		dc[14] = (-1.0 + (q * f) - r) * p;
	}
}


// multi

static inline void sample_filter_LPF_BWx2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp;
	// filter1		
	db[ 2] = dc[ 0] * db[ 0] + dc[ 1] * db[ 1] + dc[ 2] * db[ 2] + dc[ 3] * db[ 3] + dc[ 4] * db[ 4];
	db[ 4] = db[ 3];
	db[ 3] = db[ 2]; // flt out
	db[ 2] = db[ 1];
	db[ 1] = db[ 0]; // flt in
	// filter2
	db[ 6] = dc[ 0] * db[ 3] + dc[ 1] * db[ 5] + dc[ 2] * db[ 6] + dc[ 3] * db[ 7] + dc[ 4] * db[ 8];
	db[ 8] = db[ 7];
	db[ 7] = db[ 6]; // flt out
	db[ 6] = db[ 5];
	db[ 5] = db[ 3]; // flt in
	// output
	*sp = db[ 7];
}

static inline void sample_filter_LPF_BWx3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	// filter1		
	db[ 2] = dc[ 0] * db[ 0] + dc[ 1] * db[ 1] + dc[ 2] * db[ 2] + dc[ 3] * db[ 3] + dc[ 4] * db[ 4];
	db[ 4] = db[ 3];
	db[ 3] = db[ 2]; // flt out
	db[ 2] = db[ 1];
	db[ 1] = db[ 0]; // flt in
	// filter2
	db[ 6] = dc[ 0] * db[ 3] + dc[ 1] * db[ 5] + dc[ 2] * db[ 6] + dc[ 3] * db[ 7] + dc[ 4] * db[ 8];
	db[ 8] = db[ 7];
	db[ 7] = db[ 6]; // flt out
	db[ 6] = db[ 5];
	db[ 5] = db[ 3]; // flt in
	// filter3
	db[10] = dc[ 0] * db[ 7] + dc[ 1] * db[ 9] + dc[ 2] * db[10] + dc[ 3] * db[11] + dc[ 4] * db[12];
	db[12] = db[11];
	db[11] = db[10]; // flt out
	db[10] = db[ 9];
	db[ 9] = db[ 7]; // flt in
	// output
	*sp = db[11];
}

static inline void sample_filter_LPF_BWx4(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp;
	// filter1		
	db[ 2] = dc[ 0] * db[ 0] + dc[ 1] * db[ 1] + dc[ 2] * db[ 2] + dc[ 3] * db[ 3] + dc[ 4] * db[ 4];
	db[ 4] = db[ 3];
	db[ 3] = db[ 2]; // flt out
	db[ 2] = db[ 1];
	db[ 1] = db[ 0]; // flt in
	// filter2
	db[ 6] = dc[ 0] * db[ 3] + dc[ 1] * db[ 5] + dc[ 2] * db[ 6] + dc[ 3] * db[ 7] + dc[ 4] * db[ 8];
	db[ 8] = db[ 7];
	db[ 7] = db[ 6]; // flt out
	db[ 6] = db[ 5];
	db[ 5] = db[ 3]; // flt in
	// filter3
	db[10] = dc[ 0] * db[ 7] + dc[ 1] * db[ 9] + dc[ 2] * db[10] + dc[ 3] * db[11] + dc[ 4] * db[12];
	db[12] = db[11];
	db[11] = db[10]; // flt out
	db[10] = db[ 9];
	db[ 9] = db[ 7]; // flt in
	// filter4
	db[14] = dc[ 0] * db[11] + dc[ 1] * db[13] + dc[ 2] * db[14] + dc[ 3] * db[15] + dc[ 4] * db[16];
	db[16] = db[15];
	db[15] = db[14]; // flt out
	db[14] = db[13];
	db[13] = db[11]; // flt in
	// output
	*sp = db[15];
}

static inline void sample_filter_LPF24_2x2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	// filter1		
	db[5] = dc[0] * db[0] + db[1];
	db[1] = dc[1] * db[0] + dc[3] * db[5] + db[2];
	db[2] = dc[2] * db[0] + dc[4] * db[5];
	db[10] = db[0] = db[5];
	db[5] = dc[0] * db[0] + db[3];
	db[3] = dc[1] * db[0] + dc[3] * db[5] + db[4];
	db[4] = dc[2] * db[0] + dc[4] * db[5];
	// filter2	
	db[15] = dc[0] * db[10] + db[11];
	db[11] = dc[1] * db[10] + dc[3] * db[15] + db[12];
	db[12] = dc[2] * db[10] + dc[4] * db[15];
	db[10] = db[15];
	db[15] = dc[0] * db[10] + db[3];
	db[13] = dc[1] * db[10] + dc[3] * db[15] + db[14];
	db[14] = dc[2] * db[10] + dc[4] * db[15];
	*sp = db[10];
}

static inline void sample_filter_LPF6x2(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[1] = dc[0] * db[0] + dc[1] * db[1]; // 6db
	db[2] = dc[0] * db[1] + dc[1] * db[2]; // 12db
	*sp = db[2];
}

static inline void sample_filter_LPF6x3(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[1] = dc[0] * db[0] + dc[1] * db[1]; // 6db
	db[2] = dc[0] * db[1] + dc[1] * db[2]; // 12db
	db[3] = dc[0] * db[2] + dc[1] * db[3]; // 18db
	*sp = db[3];
}

static inline void sample_filter_LPF6x4(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[1] = dc[0] * db[0] + dc[1] * db[1]; // 6db
	db[2] = dc[0] * db[1] + dc[1] * db[2]; // 12db
	db[3] = dc[0] * db[2] + dc[1] * db[3]; // 18db
	db[4] = dc[0] * db[3] + dc[1] * db[4]; // 24db
	*sp = db[4];
}

static inline void sample_filter_LPF6x8(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[1] = dc[0] * db[0] + dc[1] * db[1]; // 6db
	db[2] = dc[0] * db[1] + dc[1] * db[2]; // 12db
	db[3] = dc[0] * db[2] + dc[1] * db[3];
	db[4] = dc[0] * db[3] + dc[1] * db[4]; // 24db
	db[5] = dc[0] * db[4] + dc[1] * db[5];
	db[6] = dc[0] * db[5] + dc[1] * db[6]; // 36db
	db[7] = dc[0] * db[6] + dc[1] * db[7];
	db[8] = dc[0] * db[7] + dc[1] * db[8]; // 48db
	*sp = db[8];
}

static inline void sample_filter_LPF6x16(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	db[0] = *sp;
	db[1] = dc[0] * db[0] + dc[1] * db[1]; // 6db
	db[2] = dc[0] * db[1] + dc[1] * db[2]; // 12db
	db[3] = dc[0] * db[2] + dc[1] * db[3];
	db[4] = dc[0] * db[3] + dc[1] * db[4]; // 24db
	db[5] = dc[0] * db[4] + dc[1] * db[5];
	db[6] = dc[0] * db[5] + dc[1] * db[6]; // 36db
	db[7] = dc[0] * db[6] + dc[1] * db[7];
	db[8] = dc[0] * db[7] + dc[1] * db[8]; // 48db
	db[9] = dc[0] * db[8] + dc[1] * db[9];
	db[10] = dc[0] * db[9] + dc[1] * db[10]; // 60db
	db[11] = dc[0] * db[10] + dc[1] * db[11];
	db[12] = dc[0] * db[11] + dc[1] * db[12]; // 72db
	db[13] = dc[0] * db[12] + dc[1] * db[13];
	db[14] = dc[0] * db[13] + dc[1] * db[14]; // 84db
	db[15] = dc[0] * db[14] + dc[1] * db[15];
	db[16] = dc[0] * db[15] + dc[1] * db[16]; // 96db
	*sp = db[16];
}


// antialias

static inline void sample_filter_LPF_FIR(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
#if (LPF_FIR_ORDER == 20) // optimize		
#if (USE_X86_EXT_INTRIN >= 3) && defined(FLOAT_T_DOUBLE)	
	 FLOAT_T input = *sp;
	__m128d xdc0 = _mm_loadu_pd(&dc[0]), xdc2 = _mm_loadu_pd(&dc[2]), 
		xdc4 = _mm_loadu_pd(&dc[4]), xdc6 = _mm_loadu_pd(&dc[6]), 
		xdc8 = _mm_loadu_pd(&dc[8]), xdc10 = _mm_loadu_pd(&dc[10]), 
		xdc12 = _mm_loadu_pd(&dc[12]), xdc14 = _mm_loadu_pd(&dc[14]), 
		xdc16 = _mm_loadu_pd(&dc[16]), xdc18 = _mm_loadu_pd(&dc[18]);
	__m128d xdb0 = _mm_loadu_pd(&db[0]), xdb2 = _mm_loadu_pd(&db[2]), 
		xdb4 = _mm_loadu_pd(&db[4]), xdb6 = _mm_loadu_pd(&db[6]), 
		xdb8 = _mm_loadu_pd(&db[8]), xdb10 = _mm_loadu_pd(&db[10]), 
		xdb12 = _mm_loadu_pd(&db[12]), xdb14 = _mm_loadu_pd(&db[14]), 
		xdb16 = _mm_loadu_pd(&db[16]), xdb18 = _mm_loadu_pd(&db[18]);
	__m128d	xsum = _mm_setzero_pd();	
	xsum = MM_FMA5_PD(xdb0, xdc0, xdb2, xdc2, xdb4, xdc4, xdb6, xdc6, xdb8, xdc8);
	xsum = MM_FMA5_PD(xdb10, xdc10, xdb12, xdc12, xdb14, xdc14, xdb16, xdc16, xdb18, xdc18);	
	xsum = _mm_add_pd(xsum, _mm_shuffle_pd(xsum, xsum, 0x1)); // v0=v0+v1 v1=v1+v0
#if defined(DATA_T_FLOAT)
	_mm_store_ss(sp, _mm_cvtsd_ss(_mm_setzero_ps(), xsum));
#else
	_mm_store_sd(sp, xsum);	
#endif	
	_mm_storeu_pd(&db[19], xdb18);
	_mm_storeu_pd(&db[17], xdb16);
	_mm_storeu_pd(&db[15], xdb14);
	_mm_storeu_pd(&db[13], xdb12);
	_mm_storeu_pd(&db[11], xdb10);
	_mm_storeu_pd(&db[9], xdb8);
	_mm_storeu_pd(&db[7], xdb6);
	_mm_storeu_pd(&db[5], xdb4);
	_mm_storeu_pd(&db[3], xdb2);
	_mm_storeu_pd(&db[1], xdb0);
	db[0] = input;
#elif (USE_X86_EXT_INTRIN >= 2) && defined(FLOAT_T_FLOAT)	
	 FLOAT_T input = *sp;
	__m128 xdc0 = _mm_loadu_ps(&dc[0]), xdc4 = _mm_loadu_ps(&dc[4]),
		xdc8 = _mm_loadu_ps(&dc[8]), xdc12 = _mm_loadu_ps(&dc[12]),
		xdc16 = _mm_loadu_ps(&dc[16]);
	__m128 xdb0 = _mm_loadu_ps(&db[0]), xdb4 = _mm_loadu_ps(&db[4]),
		xdb8 = _mm_loadu_ps(&db[8]), xdb12 = _mm_loadu_ps(&db[12]),
		xdb16 = _mm_loadu_ps(&db[16]);
	__m128 xsum = _mm_setzero_ps();	
	xsum = MM_FMA5_PS(xdb0, xdc0, xdb0, xdc0, xdb4, xdc4, xdb8, xdc8, xdb12, xdc12, xdb16, xdc16);
	xsum = _mm_add_ps(xsum, _mm_movehl_ps(xsum, xsum)); // v0=v0+v2 v1=v1+v3 v2=-- v3=--
	xsum = _mm_add_ps(xsum, _mm_shuffle_ps(xsum, xsum, 0xe1)); // v0=v0+v1	
#if defined(DATA_T_FLOAT)
	_mm_store_ss(sp, xsum);
#else	
#if (USE_X86_EXT_INTRIN >= 3)
	_mm_store_sd(sp, _mm_cvtss_sd(xsum));
#else
	{
		float out;
		_mm_store_ss(&out, xsum);
		*sp = (double)out;
	}
#endif	
#endif	
	_mm_storeu_ps(&db[17], xdb16);
	_mm_storeu_ps(&db[13], xdb12);
	_mm_storeu_ps(&db[9], xdb8);
	_mm_storeu_ps(&db[5], xdb4);
	_mm_storeu_ps(&db[1], xdb0);
	db[0] = input;
#else
    FLOAT_T sum = 0.0;
	sum += db[0] * dc[0];
	sum += db[1] * dc[1];
	sum += db[2] * dc[2];
	sum += db[3] * dc[3];
	sum += db[4] * dc[4];
	sum += db[5] * dc[5];
	sum += db[6] * dc[6];
	sum += db[7] * dc[7];
	sum += db[8] * dc[8];
	sum += db[9] * dc[9];
	sum += db[10] * dc[10];
	sum += db[11] * dc[11];
	sum += db[12] * dc[12];
	sum += db[13] * dc[13];
	sum += db[14] * dc[14];
	sum += db[15] * dc[15];
	sum += db[16] * dc[16];
	sum += db[17] * dc[17];
	sum += db[18] * dc[18];
	sum += db[19] * dc[19];
	db[19] = db[18];
	db[18] = db[17];
	db[17] = db[16];
	db[16] = db[15];
	db[15] = db[14];
	db[14] = db[13];
	db[13] = db[12];
	db[12] = db[11];
	db[11] = db[10];
	db[10] = db[9];
	db[9] = db[8];
	db[8] = db[7];
	db[7] = db[6];
	db[6] = db[5];
	db[5] = db[4];
	db[4] = db[3];
	db[3] = db[2];
	db[2] = db[1];
	db[1] = db[0];
	db[0] = *sp;	
	*sp = sum;	
#endif // INTRIN
#else // ! (LPF_FIR_ORDER == 20)
    FLOAT_T sum = 0.0;
	int i;
	for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += db[i] * dc[i];
	for (i = LPF_FIR_ORDER - 1; i >= 0; i--)
		db[i + 1] = db[i];
	db[0] = *sp;	
	*sp = sum;
#endif
}

static void designfir(FLOAT_T *g , FLOAT_T fc, FLOAT_T att);

static inline void recalc_filter_LPF_FIR(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;	
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
	FLOAT_T f;
	int i;

	if(FLT_FREQ_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		f = fc->freq * fc->div_flt_rate * 2.0;
		designfir(fir_coef, f, 40.0);
		for (i = 0; i < LPF_FIR_ORDER2; i++)
			dc[LPF_FIR_ORDER-1 - i] = dc[i] = fir_coef[LPF_FIR_ORDER2 - 1 - i];
	}
}

// shelving 共通 
static inline void sample_filter_shelving(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{	
	// input
	db[0] = *sp;	
	// LPF
	db[2] = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2] + dc[3] * db[3] + dc[4] * db[4];
#if defined(DENORMAL_FIX)
	db[2] += denormal_add;
#endif	
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[3] * dc[8]; // spgain
}

static inline void recalc_filter_shelving_low(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, A, beta;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN || FLT_WIDTH_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		CALC_WIDTH_MARGIN		
		A = pow(10.0, fc->reso_dB * DIV_40 * ext_filter_shelving_gain);
		if(fc->reso_dB > 0)
			dc[8] = pow((FLOAT_T)10.0, -(fc->reso_dB) * DIV_80 * ext_filter_shelving_reduce); // spgain
		else
			dc[8] = 1.0;
		omega = (FLOAT_T)2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		beta = sqrt(A) / (fc->q * ext_filter_shelving_q); // q > 0
		a0 = 1.0 / ((A + 1) + (A - 1) * cs + beta * sn);
		a1 = 2.0 * ((A - 1) + (A + 1) * cs);
		a2 = -((A + 1) + (A - 1) * cs - beta * sn);
		b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
		b1 = 2.0 * A * ((A - 1) - (A + 1) * cs);
		b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
		dc[4] = a2 * a0;
		dc[3] = a1 * a0;
		dc[2] = b2 * a0;
		dc[1] = b1 * a0;
		dc[0] = b0 * a0;
	}
}

static inline void recalc_filter_shelving_hi(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, A, beta;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN || FLT_WIDTH_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		CALC_WIDTH_MARGIN		
		A = pow(10.0, fc->reso_dB * DIV_40 * ext_filter_shelving_gain);
		if(fc->reso_dB > 0)
			dc[8] = pow((FLOAT_T)10.0, -(fc->reso_dB) * DIV_80 * ext_filter_shelving_reduce); // spgain
		else
			dc[8] = 1.0;
		omega = (FLOAT_T)2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		beta = sqrt(A) / (fc->q * ext_filter_shelving_q); // q > 0
		a0 = 1.0 / ((A + 1) - (A - 1) * cs + beta * sn);
		a1 = (-2.0 * ((A - 1) - (A + 1) * cs));
		a2 = -((A + 1) - (A - 1) * cs - beta * sn);
		b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
		b1 = -2.0 * A * ((A - 1) + (A + 1) * cs);
		b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
		dc[4] = a2 * a0;
		dc[3] = a1 * a0;
		dc[2] = b2 * a0;
		dc[1] = b1 * a0;
		dc[0] = b0 * a0;
	}
}

// peaking 共通 
static inline void sample_filter_peaking(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
	// input
	db[0] = *sp;	
	// LPF
	db[2] = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2] + dc[3] * db[3] + dc[4] * db[4];
#if defined(DENORMAL_FIX)
	db[2] += denormal_add;
#endif	
	db[4] = db[3];
	db[3] = db[2]; // flt out
	db[2] = db[1];
	db[1] = db[0]; // flt in
	// output
	*sp = db[3] * dc[8]; // spgain
}

static inline void recalc_filter_peaking(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	FLOAT_T a0, a1, a2, b1, b2, b0, omega, sn, cs, A, beta;

	if(FLT_FREQ_MARGIN || FLT_RESO_MARGIN || FLT_WIDTH_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		CALC_WIDTH_MARGIN		
		A = pow(10.0, fc->reso_dB * DIV_40 * ext_filter_peaking_gain);
		if(fc->reso_dB > 0)
			dc[8] = pow((FLOAT_T)10.0, -(fc->reso_dB) * DIV_80 * ext_filter_peaking_reduce); // spgain
		else
			dc[8] = 1.0;
		omega = (FLOAT_T)2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		beta = sn / (2.0 * fc->q * ext_filter_peaking_q); // q > 0
		a0 = 1.0 / (1.0 + beta / A);
		a1 = -2.0 * cs;
		dc[4] = -(1.0 - beta / A) * a0; // -
		dc[3] = -a1 * a0; // -
		dc[2] = (1.0 - beta * A) * a0;
		dc[1] = a1 * a0; // b1 = a1
		dc[0] = (1.0 + beta * A) * a0;
		dc[5] = 0.0;
	}
}

// biquad 共通 
static inline void sample_filter_biquad(FILTER_T *dc, FILTER_T *db, DATA_T *sp)
{
//	db[2] = db[0] * dc[0] + db[1] * dc[1] + db[2] * dc[2] + db[3] * dc[3] + db[4] * dc[4]; // dc[0]=dc[2] BWと同じ 
	db[2] = db[1] * dc[1] + (*sp + db[2]) * dc[2] + db[3] * dc[3] + db[4] * dc[4]; // -dc3 -dc4 
#if defined(DENORMAL_FIX)
	db[2] += denormal_add;
#endif	
	db[4] = db[3];
	db[3] = db[2];
	db[2] = db[1];
	db[1] = *sp;
	*sp = db[3];
}

static inline void recalc_filter_biquad_low(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	double a0, omega, sn, cs, alpha;

	if(FLT_FREQ_MARGIN || FLT_WIDTH_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		CALC_WIDTH_MARGIN		
		omega = 2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		alpha = sn / (2.0 * fc->q); // q > 0
		a0 = 1.0 / (1.0 + alpha);
		dc[1] = (1.0 - cs) * a0;
		dc[2] = dc[0] = ((1.0 - cs) * DIV_2) * a0;
		dc[3] = -(-2.0 * cs) * a0; // -
		dc[4] = -(1.0 - alpha) * a0; // -
	}
}

static inline void recalc_filter_biquad_hi(FilterCoefficients *fc)
{
	FILTER_T *dc = fc->dc;
	double a0, omega, sn, cs, alpha;

	if(FLT_FREQ_MARGIN || FLT_WIDTH_MARGIN){
		CALC_MARGIN_VAL
		CALC_FREQ_MARGIN
		CALC_RESO_MARGIN
		CALC_WIDTH_MARGIN			
		omega = 2.0 * M_PI * fc->freq * fc->div_flt_rate;
		sn = sin(omega);
		cs = cos(omega);
		alpha = sn / (2.0 * fc->q); // q > 0
		a0 = 1.0 / (1.0 + alpha);
		dc[1] = (-(1.0 + cs)) * a0;
		dc[2] = dc[0] = ((1.0 + cs) * DIV_2) * a0;
		dc[3] = -(-2.0 * cs) * a0; // -
		dc[4] = -(1.0 - alpha) * a0; // -
	}
}

#endif /* OPT_MODE == 1 */




// 1sample filter

const char *filter_name[] = 
{
	"NONE",
	"LPF12",
	"LPF24",
	"LPF_BW",
	"LPF12_2",
	"LPF24_2",
	"LPF6",
	"LPF18",
	"LPF_TFO",
// test
	"HPF_BW",
	"BPF_BW",
	"PEAK1",
	"NOTCH1",
	"LPF12_3", // ov
	"HPF12_3", // ov
	"BPF12_3", // ov
	"BCF12_3", // ov
	"HPF6",
	"HPF12_2",
// hybrid
	"HBF_L6L12",
	"HBF_L12L6",
	"HBF_L12H6",
	"HBF_L24H6",
	"HBF_L24H12",
	"HBF_L12OCT",
	"HBF_L24OCT",
// multi
	"LPF6x2",
	"LPF6x3",
	"LPF6x4",
	"LPF6x8",
	"LPF6x16",
	"LPF_BWx2",
	"LPF_BWx3",
	"LPF_BWx4",
	"LPF24_2x2",
//
	"LPF_FIR",
// equalizer
	"SHELVING_LOW",
	"SHELVING_HI",
	"PEAKING",
	"BIQUAD_LOW",
	"BIQUAD_HI",
	"LIST_MAX", // last
};

typedef void (*recalc_filter_t)(FilterCoefficients *fc);

static recalc_filter_t recalc_filters[] = {
// cfg sort
	recalc_filter_none,
	recalc_filter_LPF12,
	recalc_filter_LPF24,
	recalc_filter_LPF_BW,
	recalc_filter_LPF12_2,
	recalc_filter_LPF24_2,
	recalc_filter_LPF6,
	recalc_filter_LPF18,
	recalc_filter_LPF_TFO,
	recalc_filter_HPF_BW,
	recalc_filter_BPF_BW,
	recalc_filter_peak1,
	recalc_filter_peak1, // notch1
	recalc_filter_LPF12_3,
	recalc_filter_HPF12_3, // HPF12_3
	recalc_filter_BPF12_3, // BPF12_3
	recalc_filter_BCF12_3, // BCF12_3
	recalc_filter_LPF6, // HPF6
	recalc_filter_LPF12_2, // HPF12_2
	// hybrid
	recalc_filter_HBF_L6L12,
	recalc_filter_HBF_L12L6,
	recalc_filter_HBF_L12H6,
	recalc_filter_HBF_L24H6,
	recalc_filter_HBF_L24H12,
	recalc_filter_HBF_L12OCT,
	recalc_filter_HBF_L24OCT,
	// multi
	recalc_filter_LPF6, // LPF6x2
	recalc_filter_LPF6, // LPF6x3
	recalc_filter_LPF6, // LPF6x4
	recalc_filter_LPF6, // LPF6x8
	recalc_filter_LPF6, // LPF6x16
	recalc_filter_LPF_BW, // LPF_BWx2
	recalc_filter_LPF_BW, // LPF_BWx3
	recalc_filter_LPF_BW, // LPF_BWx4
	recalc_filter_LPF24_2, // LPF24_2x2
	//
	recalc_filter_LPF_FIR, // LPF_FIR
	// equalizer
	recalc_filter_shelving_low, // eq_low
	recalc_filter_shelving_hi, // eq_hi
	recalc_filter_peaking, // eq_mid
	recalc_filter_biquad_low,
	recalc_filter_biquad_hi,
};

typedef void (*sample_filter_t)(FILTER_T *dc, FILTER_T *db, DATA_T *sp);

static sample_filter_t sample_filters[] = {
// cfg sort
	sample_filter_none,
	sample_filter_LPF12,
	sample_filter_LPF24,
	sample_filter_LPF_BW,
	sample_filter_LPF12_2,
	sample_filter_LPF24_2,
	sample_filter_LPF6,
	sample_filter_LPF18,
	sample_filter_LPF_TFO,
	sample_filter_HPF_BW,
	sample_filter_BPF_BW,
	sample_filter_peak1,
	sample_filter_notch1,
	sample_filter_LPF12_3,
	sample_filter_HPF12_3,
	sample_filter_BPF12_3,
	sample_filter_BCF12_3,
	sample_filter_HPF6,
	sample_filter_HPF12_2,
	// hybrid
	sample_filter_HBF_L6L12,
	sample_filter_HBF_L12L6,
	sample_filter_HBF_L12H6,
	sample_filter_HBF_L24H6,
	sample_filter_HBF_L24H12,
	sample_filter_HBF_L12OCT,
	sample_filter_HBF_L24OCT,
	// multi
	sample_filter_LPF6x2,
	sample_filter_LPF6x3,
	sample_filter_LPF6x4,
	sample_filter_LPF6x8,
	sample_filter_LPF6x16,
	sample_filter_LPF_BWx2,
	sample_filter_LPF_BWx3,
	sample_filter_LPF_BWx4,
	sample_filter_LPF24_2x2,
	// 
	sample_filter_LPF_FIR,
	// equalizer
	sample_filter_shelving, // eq_low
	sample_filter_shelving, // eq_hi
	sample_filter_peaking, // eq_mid
	sample_filter_biquad,
	sample_filter_biquad,
};

void set_sample_filter_type(FilterCoefficients *fc, int type)
{		
	if(type < FILTER_NONE || type >= FILTER_LIST_MAX)
		type = FILTER_NONE;
	if(!fc->init || fc->type != type)
		memset(fc, 0, sizeof(FilterCoefficients));
	fc->type = type;
	fc->recalc_filter = recalc_filters[type];
	fc->sample_filter = sample_filters[type];
	INIT_MARGIN_VAL
	fc->init = 1;
}

const double sample_filter_limit_rate[] = {
	0.16666, // type0 OFF
	0.16666, // type1 Chamberlin 12dB/oct fc < rate / 6
	0.50000, // type2 Moog VCF 24dB/oct fc < rate / 2
	0.50000, // type3 butterworth fc < rate / 2 elion
	0.50000, // type4 Resonant IIR 12dB/oct fc < rate / 2
	0.50000, // type5 amSynth 24dB/oct fc < rate / 2
	0.50000, // type6 One pole 6dB/oct non rez fc < rate / 2
	0.44444, // type7 resonant 3 pole 18dB/oct fc < rate / 2.25
	0.50000, // type8 two first order fc < rate / 2
	// test
	0.50000, // type9 HPF butterworth fc < rate / 2 elion + 
	0.50000, // type10 BPF butterworth fc < rate / 2 elion + 
	0.50000, // type11 peak fc < rate / 2
	0.50000, // type12 notch fc < rate / 2
	0.21875, // type13 LPF Chamberlin2 12dB/oct fc < rate / 2
	0.21875, // type14 HPF Chamberlin2 12dB/oct fc < rate / 2
	0.21875, // type15 BPF Chamberlin2 12dB/oct fc < rate / 2
	0.21875, // type16 notch Chamberlin2 12dB/oct fc < rate / 2
	0.50000, // type17 HPF6
	0.50000, // type18 HPF12_2
	// hybrid
	0.50000, // type19 L6L12
	0.50000, // type20 L12L6
	0.50000, // type21 L12H6
	0.50000, // type22 L24H6
	0.50000, // type23 L24H12
	0.50000, // type24 L12OCT
	0.50000, // type25 L24OCT
	// multi
	0.50000, // LPF6x2
	0.50000, // LPF6x3
	0.50000, // LPF6x4
	0.50000, // LPF6x8
	0.50000, // LPF6x16
	0.50000, // LPFBWx2
	0.50000, // LPFBWx3
	0.50000, // LPFBWx4
	0.50000, // LPF24_2x2
	// 
	0.50000, // LPF_FIR
	// equalizer
	0.50000, // FILTER_SHELVING_LOW, // q
	0.50000, // FILTER_SHELVING_HI, // q
	0.50000, // FILTER_PEAKING, // q
	0.50000, // FILTER_BIQUAD_LOW, // q
	0.50000, // FILTER_BIQUAD_HI, // q
};

void set_sample_filter_ext_rate(FilterCoefficients *fc, FLOAT_T freq)
{
	if(freq > 0){
		fc->flt_rate = freq;
		fc->div_flt_rate = 1.0 / fc->flt_rate;
		fc->flt_rate_div2 = fc->flt_rate * DIV_2;
	}else{ // default
		fc->flt_rate = play_mode->rate;
		fc->div_flt_rate = div_playmode_rate;
		fc->flt_rate_div2 = playmode_rate_div2;
	}
	// for ov
	fc->flt_rate_limit1 = fc->flt_rate * sample_filter_limit_rate[fc->type]; // sr*limit
	fc->flt_rate_limit2 = fc->flt_rate_limit1 * 2.0; // sr*2*limit
	fc->div_flt_rate_ov2 = fc->div_flt_rate * DIV_2; // 1/sr*2
	fc->div_flt_rate_ov3 = fc->div_flt_rate * DIV_3; // 1/sr*3
}

void set_sample_filter_freq(FilterCoefficients *fc, FLOAT_T freq)
{
	if(fc->flt_rate == 0) // not init filter rate
		set_sample_filter_ext_rate(fc, 0);	
	if(freq < 0 || freq > fc->flt_rate_div2) // sr/2
		fc->freq = fc->flt_rate_div2;
	else if(freq < 10.0)
		fc->freq = 10.0;
	else
		fc->freq = freq;
}

void set_sample_filter_reso(FilterCoefficients *fc, FLOAT_T reso)
{
	const FLOAT_T limit = 96.0;

	if(reso > limit)
		fc->reso_dB = limit;
	else if(reso < 0.0)
		fc->reso_dB = 0.0;
	else
		fc->reso_dB = reso;
}

void set_sample_filter_q(FilterCoefficients *fc, FLOAT_T q)
{
	const FLOAT_T def = 0.7;
	const FLOAT_T limit = 12.0;
	const FLOAT_T min = 0.01;

	if(q <= 0)
		fc->q = def;
	else if(q > limit)
		fc->q = limit;
	else if(q < min)
		fc->q = min;
	else
		fc->q = q;
}

void init_sample_filter(FilterCoefficients *fc, FLOAT_T freq, FLOAT_T reso, int type)
{
	set_sample_filter_type(fc, type);
	set_sample_filter_freq(fc, freq);
	set_sample_filter_reso(fc, reso);
	recalc_filter(fc);
}

// eq q
void init_sample_filter2(FilterCoefficients *fc, FLOAT_T freq, FLOAT_T reso, FLOAT_T q, int type)
{
	set_sample_filter_type(fc, type);
	set_sample_filter_freq(fc, freq);
	set_sample_filter_reso(fc, reso);
	set_sample_filter_q(fc, q);
	recalc_filter(fc);
}


void recalc_filter(FilterCoefficients *fc)
{
#ifdef _DEBUG
	if(!fc)
		return; // error not init
#endif
	fc->recalc_filter(fc);
}

// sample_filter (1ch mono / 2ch left) 
inline void sample_filter(FilterCoefficients *fc, DATA_T *sp)
{
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], sp);
}

// sample_filter (2ch stereo)
inline void sample_filter_stereo(FilterCoefficients *fc, DATA_T *spL, DATA_T *spR)
{
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], spL);
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_R], spR);
}

inline void sample_filter_stereo2(FilterCoefficients *fc, DATA_T *spLR)
{
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], &spLR[0]);
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_R], &spLR[1]);
}

// sample_filter (2ch left) 
inline void sample_filter_left(FilterCoefficients *fc, DATA_T *sp)
{
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], sp);
}

// sample_filter (2ch left) 
inline void sample_filter_right(FilterCoefficients *fc, DATA_T *sp)
{
	fc->sample_filter(fc->dc, &fc->db[FILTER_FB_R], sp);
}

// buffer filter (1ch mono)
inline void buffer_filter(FilterCoefficients *fc, DATA_T *sp, int32 count)
{
	int32 i;
	
#ifdef _DEBUG
	if(!fc)
		return; // error not init
#endif
	if(!fc->type)
		return; // filter none

	if (fc->type == FILTER_LPF12_2) {
		recalc_filter_LPF12_2(fc);
		buffer_filter_LPF12_2(fc->dc, &fc->db[FILTER_FB_L], sp, count);
		return;
	}
	
	fc->recalc_filter(fc);
	for(i = 0; i < count; i++)
		fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], &sp[i]);
}

// buffer filter (2ch stereo)
inline void buffer_filter_stereo(FilterCoefficients *fc, DATA_T *sp, int32 count)
{
	int32 i;

#ifdef _DEBUG
	if(!fc)
		return; // error not init
#endif
	if(!fc->type)
		return; // filter none
	fc->recalc_filter(fc);
	for(i = 0; i < count; i++){
		fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], &sp[i]);
		i++;
		fc->sample_filter(fc->dc, &fc->db[FILTER_FB_R], &sp[i]);
	}
}

// buffer filter (2ch left)
inline void buffer_filter_left(FilterCoefficients *fc, DATA_T *sp, int32 count)
{
	int32 i;
	
#ifdef _DEBUG
	if(!fc)
		return; // error not init
#endif
	if(!fc->type)
		return; // filter none
	fc->recalc_filter(fc);
	for(i = 0; i < count; i++)
		fc->sample_filter(fc->dc, &fc->db[FILTER_FB_L], &sp[i++]);
}

// buffer filter (2ch right)
inline void buffer_filter_right(FilterCoefficients *fc, DATA_T *sp, int32 count)
{
	int32 i;
	
#ifdef _DEBUG
	if(!fc)
		return; // error not init
#endif
	if(!fc->type)
		return; // filter none
	fc->recalc_filter(fc);
	for(i = 0; i < count; i++)
		fc->sample_filter(fc->dc, &fc->db[FILTER_FB_R], &sp[++i]);
}



/// voice_filter1

void set_voice_filter1_type(FilterCoefficients *fc, int type)
{
	fc->init = 0;
	set_sample_filter_type(fc, type);	
}

void set_voice_filter1_ext_rate(FilterCoefficients *fc, FLOAT_T freq)
{
	set_sample_filter_ext_rate(fc, freq);
}

void set_voice_filter1_freq(FilterCoefficients *fc, FLOAT_T freq)
{
	set_sample_filter_freq(fc, freq);
}

void set_voice_filter1_reso(FilterCoefficients *fc, FLOAT_T reso)
{
	set_sample_filter_reso(fc, reso);
}

void voice_filter1(FilterCoefficients *fc, DATA_T *sp, int32 count)
{
	buffer_filter(fc, sp, count);
}




/// voice_filter2

static int conv_type_voice_filter2[] = {
// cfg sort
	FILTER_NONE,
	FILTER_HPF_BW, // 1
	FILTER_HPF12_3, // 2
	FILTER_HPF6, // 3
	FILTER_HPF12_2, // 4
};

void set_voice_filter2_type(FilterCoefficients *fc, int type)
{	
	fc->init = 0;
	if(type < VOICE_FILTER2_NONE || type >= VOICE_FILTER2_LIST_MAX)
		type = VOICE_FILTER2_NONE;
	set_sample_filter_type(fc, conv_type_voice_filter2[type]);
}

void set_voice_filter2_ext_rate(FilterCoefficients *fc, FLOAT_T freq)
{
	set_sample_filter_ext_rate(fc, freq);	
}

void set_voice_filter2_freq(FilterCoefficients *fc, FLOAT_T freq)
{
	set_sample_filter_freq(fc, freq);
}

void set_voice_filter2_reso(FilterCoefficients *fc, FLOAT_T reso)
{
	set_sample_filter_reso(fc, reso);
}

void voice_filter2(FilterCoefficients *fc, DATA_T *sp, int32 count)
{
	buffer_filter(fc, sp, count);
}

/// voice_filter1 + voice_filter2
void voice_filter(int v, DATA_T *sp, int32 count)
{
	Voice *vp = &voice[v];

	buffer_filter(&vp->fc, sp, count); // lpf
	buffer_filter(&vp->fc2, sp, count); // hpf
}




/// resample_filter

static int conv_type_resample_filter[] = {
// cfg sort
	FILTER_NONE, // 0
	FILTER_LPF_BW, // 1
	FILTER_LPF_BWx2,
	FILTER_LPF_BWx3,
	FILTER_LPF_BWx4,
	FILTER_LPF24_2,
	FILTER_LPF24_2x2,
	FILTER_LPF6x8,
	FILTER_LPF6x16,
	FILTER_LPF_FIR,
};

void set_resample_filter_type(FilterCoefficients *fc, int type)
{	
	fc->init = 0;
	if(type < RESAMPLE_FILTER_NONE || type >= RESAMPLE_FILTER_LIST_MAX)
		type = RESAMPLE_FILTER_NONE;
	set_sample_filter_type(fc, conv_type_resample_filter[type]);
}

void set_resample_filter_ext_rate(FilterCoefficients *fc, FLOAT_T freq)
{
	set_sample_filter_ext_rate(fc, freq);	
}

void set_resample_filter_freq(FilterCoefficients *fc, FLOAT_T freq)
{
	set_sample_filter_freq(fc, freq);
}

void resample_filter(int v, DATA_T *sp, int32 count)
{
	buffer_filter(&voice[v].rf_fc, sp, count);
}







/*************** antialiasing ********************/



/*  bessel  function   */
static FLOAT_T ino(FLOAT_T x)
{
    FLOAT_T y, de, e, sde;
    int i;

    y = x / 2;
    e = 1.0;
    de = 1.0;
    i = 1;
    do {
	de = de * y / (FLOAT_T) i;
	sde = de * de;
	e += sde;
    } while (!( (e * 1.0e-08 - sde > 0) || (i++ > 25) ));
    return(e);
}

/* Kaiser Window (symetric) */
static void kaiser(FLOAT_T *w,int n,FLOAT_T beta)
{
    FLOAT_T xind, xi;
    int i;

    xind = (2*n - 1) * (2*n - 1);
    for (i =0; i<n ; i++)
	{
	    xi = i + 0.5;
	    w[i] = ino((FLOAT_T)(beta * sqrt((double)(1. - 4 * xi * xi / xind))))
		/ ino((FLOAT_T)beta);
	}
}

/*
 * fir coef in g, cuttoff frequency in fc
 */
static void designfir(FLOAT_T *g , FLOAT_T fc, FLOAT_T att)
{
	/* attenuation  in  db */
    int i;
    FLOAT_T xi, omega, beta ;
    FLOAT_T w[LPF_FIR_ORDER2];

    for (i =0; i < LPF_FIR_ORDER2 ;i++)
	{
	    xi = (FLOAT_T) i + 0.5;
	    omega = M_PI * xi;
	    g[i] = sin( (double) omega * fc) / omega;
	}

    beta = (FLOAT_T) exp(log((double)0.58417 * (att - 20.96)) * 0.4) + 0.07886
	* (att - 20.96);
    kaiser( w, LPF_FIR_ORDER2, beta);

    /* Matrix product */
    for (i =0; i < LPF_FIR_ORDER2 ; i++)
	g[i] = g[i] * w[i];
}

/*
 * FIR filtering -> apply the filter given by coef[] to the data buffer
 * Note that we simulate leading and trailing 0 at the border of the
 * data buffer
 */
static void filter(int16 *result,int16 *data, int32 length,FLOAT_T coef[])
{
    int32 sample,i,sample_window;
    int16 peak = 0;
    FLOAT_T sum;

    /* Simulate leading 0 at the begining of the buffer */
     for (sample = 0; sample < LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += coef[i] *
		    ((sample_window<0)? 0.0 : data[sample_window++]) ;

	    /* Saturation ??? */
	    if (sum> 32767.) { sum=32767.; peak++; }
	    if (sum< -32768.) { sum=-32768; peak++; }
	    result[sample] = (int16) sum;
	}

    /* The core of the buffer  */
    for (sample = LPF_FIR_ORDER2; sample < length - LPF_FIR_ORDER + LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += data[sample_window++] * coef[i];

	    /* Saturation ??? */
	    if (sum> 32767.) { sum=32767.; peak++; }
	    if (sum< -32768.) { sum=-32768; peak++; }
	    result[sample] = (int16) sum;
	}

    /* Simulate 0 at the end of the buffer */
    for (sample = length - LPF_FIR_ORDER + LPF_FIR_ORDER2; sample < length ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += coef[i] *
		    ((sample_window>=length)? 0.0 : data[sample_window++]) ;

	    /* Saturation ??? */
	    if (sum> 32767.) { sum=32767.; peak++; }
	    if (sum< -32768.) { sum=-32768; peak++; }
	    result[sample] = (int16) sum;
	}

    if (peak)
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Saturation %2.3f %%.", 100.0*peak/ (FLOAT_T) length);
}

static void filter_int8(int8 *result,int8 *data, int32 length,FLOAT_T coef[])
{
    int32 sample,i,sample_window;
    int16 peak = 0;
    FLOAT_T sum;

    /* Simulate leading 0 at the begining of the buffer */
     for (sample = 0; sample < LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += coef[i] *
		    ((sample_window<0)? 0.0 : data[sample_window++]) ;

	    /* Saturation ??? */
	    if (sum> 127.) { sum=127.; peak++; }
	    if (sum< -128.) { sum=-128; peak++; }
	    result[sample] = (int8) sum;
	}

    /* The core of the buffer  */
    for (sample = LPF_FIR_ORDER2; sample < length - LPF_FIR_ORDER + LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += data[sample_window++] * coef[i];

	    /* Saturation ??? */
	    if (sum> 127.) { sum=127.; peak++; }
	    if (sum< -128.) { sum=-128; peak++; }
	    result[sample] = (int8) sum;
	}

    /* Simulate 0 at the end of the buffer */
    for (sample = length - LPF_FIR_ORDER + LPF_FIR_ORDER2; sample < length ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += coef[i] *
		    ((sample_window>=length)? 0.0 : data[sample_window++]) ;

	    /* Saturation ??? */
	    if (sum> 127.) { sum=127.; peak++; }
	    if (sum< -128.) { sum=-128; peak++; }
	    result[sample] = (int8) sum;
	}

    if (peak)
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Saturation %2.3f %%.", 100.0*peak/ (FLOAT_T) length);
}

static void filter_int32(int32 *result,int32 *data, int32 length,FLOAT_T coef[])
{
    int32 sample,i,sample_window;
    int16 peak = 0;
    FLOAT_T sum;

    /* Simulate leading 0 at the begining of the buffer */
     for (sample = 0; sample < LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += coef[i] *
		    ((sample_window<0)? 0.0 : data[sample_window++]) ;

	    /* Saturation ??? */
	    if (sum> 2147483647.) { sum=2147483647.; peak++; }
	    if (sum< -2147483648.) { sum=-2147483648.; peak++; }
	    result[sample] = (int32) sum;
	}

    /* The core of the buffer  */
    for (sample = LPF_FIR_ORDER2; sample < length - LPF_FIR_ORDER + LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += data[sample_window++] * coef[i];

	    /* Saturation ??? */
	    if (sum> 2147483647.) { sum=2147483647.; peak++; }
	    if (sum< -2147483648.) { sum=-2147483648.; peak++; }
	    result[sample] = (int32) sum;
	}

    /* Simulate 0 at the end of the buffer */
    for (sample = length - LPF_FIR_ORDER + LPF_FIR_ORDER2; sample < length ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
		sum += coef[i] *
		    ((sample_window>=length)? 0.0 : data[sample_window++]) ;

	    /* Saturation ??? */
	    if (sum> 2147483647.) { sum=2147483647.; peak++; }
	    if (sum< -2147483648.) { sum=-2147483648.; peak++; }
	    result[sample] = (int32) sum;
	}

    if (peak)
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Saturation %2.3f %%.", 100.0*peak/ (FLOAT_T) length);
}

static void filter_float(float *result,float *data, int32 length,FLOAT_T coef[])
{
    int32 sample,i,sample_window;
    FLOAT_T sum;

    /* Simulate leading 0 at the begining of the buffer */
     for (sample = 0; sample < LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
			sum += coef[i] * ((sample_window<0)? 0.0 : data[sample_window++]) ;
		result[sample] = (float)sum;
	}

    /* The core of the buffer  */
    for (sample = LPF_FIR_ORDER2; sample < length - LPF_FIR_ORDER + LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
			sum += data[sample_window++] * coef[i];
		result[sample] = (float)sum;	
	}

    /* Simulate 0 at the end of the buffer */
    for (sample = length - LPF_FIR_ORDER + LPF_FIR_ORDER2; sample < length ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
			sum += coef[i] * ((sample_window>=length)? 0.0 : data[sample_window++]) ;
		result[sample] = (float)sum;
	}
}

static void filter_double(double *result,double *data, int32 length,FLOAT_T coef[])
{
    int32 sample,i,sample_window;
    FLOAT_T sum;

    /* Simulate leading 0 at the begining of the buffer */
     for (sample = 0; sample < LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
			sum += coef[i] * ((sample_window<0)? 0.0 : data[sample_window++]) ;
		result[sample] = (double)sum;
	}

    /* The core of the buffer  */
    for (sample = LPF_FIR_ORDER2; sample < length - LPF_FIR_ORDER + LPF_FIR_ORDER2 ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
			sum += data[sample_window++] * coef[i];
		result[sample] = (double)sum;	
	}

    /* Simulate 0 at the end of the buffer */
    for (sample = length - LPF_FIR_ORDER + LPF_FIR_ORDER2; sample < length ; sample++ )
	{
	    sum = 0.0;
	    sample_window= sample - LPF_FIR_ORDER2;

	    for (i = 0; i < LPF_FIR_ORDER ;i++)
			sum += coef[i] * ((sample_window>=length)? 0.0 : data[sample_window++]) ;
		result[sample] = (double)sum;
	}
}
/***********************************************************************/
/* Prevent aliasing by filtering any freq above the output_rate        */
/*                                                                     */
/* I don't worry about looping point -> they will remain soft if they  */
/* were already                                                        */
/***********************************************************************/
void antialiasing(int16 *data, int32 data_length, int32 sample_rate, int32 output_rate)
{
    int i;
	int32 bytes;
	int16 *temp;
    FLOAT_T fir_symetric[LPF_FIR_ORDER];
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
    FLOAT_T freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/


    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: Fsample=%iKHz",
	      sample_rate);

    /* No oversampling  */
    if (output_rate>=sample_rate)
	return;

    freq_cut= (FLOAT_T)output_rate / (FLOAT_T)sample_rate;
    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: cutoff=%f%%",
	      freq_cut*100.);

    designfir(fir_coef,freq_cut, LPF_FIR_ANTIALIASING_ATT);

    /* Make the filter symetric */
    for (i = 0 ; i<LPF_FIR_ORDER2 ;i++)
	fir_symetric[LPF_FIR_ORDER-1 - i] = fir_symetric[i] = fir_coef[LPF_FIR_ORDER2-1 - i];

    /* We apply the filter we have designed on a copy of the patch */
	if(!data)
		return;

	bytes = sizeof(int16) * data_length;
	temp = (int16 *)safe_malloc(bytes);
	memcpy(temp, data, bytes);
	filter(data, temp, data_length, fir_symetric);
	free(temp);
}

void antialiasing_int8(int8 *data, int32 data_length, int32 sample_rate, int32 output_rate)
{
    int i;
	int32 bytes;
	int8 *temp;
    FLOAT_T fir_symetric[LPF_FIR_ORDER];
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
    FLOAT_T freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/


    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: Fsample=%iKHz",
	      sample_rate);

    /* No oversampling  */
    if (output_rate>=sample_rate)
	return;

    freq_cut= (FLOAT_T)output_rate / (FLOAT_T)sample_rate;
    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: cutoff=%f%%",
	      freq_cut*100.);

    designfir(fir_coef,freq_cut, LPF_FIR_ANTIALIASING_ATT);

    /* Make the filter symetric */
    for (i = 0 ; i<LPF_FIR_ORDER2 ;i++)
	fir_symetric[LPF_FIR_ORDER-1 - i] = fir_symetric[i] = fir_coef[LPF_FIR_ORDER2-1 - i];

    /* We apply the filter we have designed on a copy of the patch */
	if(!data)
		return;

	bytes = sizeof(int8) * data_length;
	temp = (int8 *)safe_malloc(bytes);
	memcpy(temp, data, bytes);
	filter_int8(data, temp, data_length, fir_symetric);
	free(temp);
}

void antialiasing_int32(int32 *data, int32 data_length, int32 sample_rate, int32 output_rate)
{
    int i;
	int32 bytes;
	int32 *temp;
    FLOAT_T fir_symetric[LPF_FIR_ORDER];
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
    FLOAT_T freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/


    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: Fsample=%iKHz",
	      sample_rate);

    /* No oversampling  */
    if (output_rate>=sample_rate)
	return;

    freq_cut= (FLOAT_T)output_rate / (FLOAT_T)sample_rate;
    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: cutoff=%f%%",
	      freq_cut*100.);

    designfir(fir_coef,freq_cut, LPF_FIR_ANTIALIASING_ATT);

    /* Make the filter symetric */
    for (i = 0 ; i<LPF_FIR_ORDER2 ;i++)
	fir_symetric[LPF_FIR_ORDER-1 - i] = fir_symetric[i] = fir_coef[LPF_FIR_ORDER2-1 - i];

    /* We apply the filter we have designed on a copy of the patch */
	if(!data)
		return;

	bytes = sizeof(int32) * data_length;
	temp = (int32 *)safe_malloc(bytes);
	memcpy(temp, data, bytes);
	filter_int32(data, temp, data_length, fir_symetric);
	free(temp);
}

void antialiasing_float(float *data, int32 data_length, int32 sample_rate, int32 output_rate)
{
    int i;
	int32 bytes;
	float *temp;
    FLOAT_T fir_symetric[LPF_FIR_ORDER];
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
    FLOAT_T freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/


    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: Fsample=%iKHz",
	      sample_rate);

    /* No oversampling  */
    if (output_rate>=sample_rate)
	return;

    freq_cut= (FLOAT_T)output_rate / (FLOAT_T)sample_rate;
    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: cutoff=%f%%",
	      freq_cut*100.);

    designfir(fir_coef,freq_cut, LPF_FIR_ANTIALIASING_ATT);

    /* Make the filter symetric */
    for (i = 0 ; i<LPF_FIR_ORDER2 ;i++)
	fir_symetric[LPF_FIR_ORDER-1 - i] = fir_symetric[i] = fir_coef[LPF_FIR_ORDER2-1 - i];

    /* We apply the filter we have designed on a copy of the patch */
	if(!data)
		return;

	bytes = sizeof(float) * data_length;
	temp = (float *)safe_malloc(bytes);
	memcpy(temp, data, bytes);
	filter_float(data, temp, data_length, fir_symetric);
	free(temp);

}

void antialiasing_double(double *data, int32 data_length, int32 sample_rate, int32 output_rate)
{
    int i;
	int32 bytes;
	double *temp;
    FLOAT_T fir_symetric[LPF_FIR_ORDER];
    FLOAT_T fir_coef[LPF_FIR_ORDER2];
    FLOAT_T freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/

    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: Fsample=%iKHz",
	      sample_rate);

    /* No oversampling  */
    if (output_rate>=sample_rate)
	return;

    freq_cut= (FLOAT_T)output_rate / (FLOAT_T)sample_rate;
    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: cutoff=%f%%",
	      freq_cut*100.);

    designfir(fir_coef,freq_cut, LPF_FIR_ANTIALIASING_ATT);

    /* Make the filter symetric */
    for (i = 0 ; i<LPF_FIR_ORDER2 ;i++)
	fir_symetric[LPF_FIR_ORDER-1 - i] = fir_symetric[i] = fir_coef[LPF_FIR_ORDER2-1 - i];

    /* We apply the filter we have designed on a copy of the patch */
	if(!data)
		return;

	bytes = sizeof(double) * data_length;
	temp = (double *)safe_malloc(bytes);
	memcpy(temp, data, bytes);
	filter_double(data, temp, data_length, fir_symetric);
	free(temp);

}


/*************** fir_eq ********************/

void init_fir_eq(FIR_EQ *fc)
{
	int32 i, j, k, l, count1 = 0, count2 = 0, flg = 0, size_2;
	double amp[FIR_EQ_BAND_MAX*16], bounds[FIR_EQ_BAND_MAX*16][2]; // max_band*16 , [0]:f [1]:g
	double fL, gL, fR, gR, log_fL, log_fR, ft, gt, kT, div_kT2, hk, gain, f0, f1, w0, w1;
	const double div_2pi = 1.0 / (2 * M_PI);
	
	if(fc->init < 0)
		memset(fc, 0, sizeof(FIR_EQ));

#ifdef TEST_FIR_EQ
	fc->st = 1; // stereo
	fc->band = 6;		
	fc->bit = 4;	
	fc->freq[0] = 20;
	fc->freq[1] = 400;
	fc->freq[2] = 800;
	fc->freq[3] = 6000;
	fc->freq[4] = 12000;
	fc->freq[5] = 20000;
	fc->gain[0] = 12.1;
	fc->gain[1] = 12.1;
	fc->gain[2] = 0.0;
	fc->gain[3] = 0.0;
	fc->gain[4] = 12.1;
	fc->gain[5] = 12.1;
#else
	{ // calc window size
		int32 t = play_mode->rate / 100; // sr/ 100Hz 
		int32 c = 4;
		while((1 << c) < t)
			c++;
		fc->bit = c;
	}
#endif
	if(fc->band < 4)
		fc->band = 4;
	else if(fc->band > FIR_EQ_BAND_MAX)
		fc->band = FIR_EQ_BAND_MAX;
	if(fc->band != fc->band_p)
		fc->init = 0;
	fc->band_p = fc->band;
	if(fc->bit < 4)
		fc->bit = 4;
	else if(fc->bit > 12)
		fc->bit = 12;	
	if(fc->bit != fc->bit_p)
		fc->init = 0;
	fc->bit_p = fc->bit;
	fc->size = 1 << fc->bit;	
    for(i = 0; i < FIR_EQ_SIZE_MAX; ++i){
        fc->dc[i] = 0;
    }		
    for(i = 0; i < fc->band; ++i){
		if(fc->gain[0] != 0)
			flg++;
    }
    if(!flg){
        fc->dc[0] = 1.0;
    }else{	
        size_2 = fc->size / 2;
		for(i = 0; i < fc->band; ++i){
			amp[i] = pow(10, fc->gain[i] * DIV_20);
		}
        bounds[0][1] = amp[0];
		for(i = 0; i < (fc->band - 2); i++){			
            fL = fc->freq[i];
            gL = amp[i];
            fR = fc->freq[i + 1];
            gR = amp[i + 1];
            log_fL = log(fL);
            log_fR = log(fR);
            for(j = 0; j < 16; ++j){
                ft = ((double)j + 0.5) * DIV_16;
                bounds[count1][0] = exp(log_fL * (1 - ft) + log_fR * ft);
                gt = (double)j * DIV_16;
                bounds[count1][1] = gL * (1.0 - gt) + gR * gt;
     			count1++;
            }
        }
        for(k = 0; k < size_2; ++k){
			kT = k * div_playmode_rate;
			div_kT2 = 2.0 / kT;
			hk = 0;
			count2 = 0;
			while(count2 != count1){
				gain = bounds[count2][1];
				f0 = bounds[count2][0];
				++count2;
				f1 = count2 == count1 ? play_mode->rate * DIV_2 : bounds[count2][0];
				w0 = f0 * 2 * M_PI;
				w1 = f1 * 2 * M_PI;
				if(k == 0){
					hk += gain * (w1 - w0 + (-w0) - (-w1));
				}else{
					hk += gain * (sin(w1 * kT) - sin(w0 * kT)) * div_kT2; //  * 2 / kT
				}
			}
			hk *= div_playmode_rate * div_2pi; // / (2 * M_PI);
			fc->dc[size_2 - 1 - k] = hk;
			fc->dc[size_2 - 1 + k] = hk;
		}
	}
	if(!fc->init){
		memset(fc->buff, 0, sizeof(fc->buff));
		fc->count = 0;
	}
	fc->init = 1;
}

void apply_fir_eq(FIR_EQ *fc, DATA_T *buf, int32 count)
{
	int32 i, j, offset;
	const int32 mask = FIR_EQ_SIZE_MAX - 1;

	if(!fc->init)
		return;	
#if (USE_X86_EXT_INTRIN >= 3) && defined(FLOAT_T_DOUBLE) && defined(DATA_T_DOUBLE)
	if(fc->st){ // stereo
		__m128d vout[2], tmp[2]; // DATA_T out[2];
		for(i = 0; i < count; i += 2){
			int32 sbuff = fc->count + FIR_EQ_SIZE_MAX;
			fc->buff[0][fc->count] = buf[i];
			fc->buff[0][sbuff] = buf[i]; // for SIMD
			fc->buff[1][fc->count] = buf[i + 1];
			fc->buff[1][sbuff] = buf[i + 1]; // for SIMD
			fc->count = (fc->count + 1) & mask;
			offset = (fc->count - fc->size) & mask;
			vout[0] = _mm_setzero_pd(); // out[0] = 0;
			vout[1] = _mm_setzero_pd(); // out[1] = 0;
			for(j = 0; j < fc->size; j += 2){
				int32 ofs = offset + j;
				__m128d vdc = _mm_loadu_pd(&fc->dc[j]);
				vout[0] = MM_FMA_PD(vdc, _mm_loadu_pd(&fc->buff[0][ofs]), vout[0]); // out[0] += fc->dc[j] * fc->buff[0][ofs];
				vout[1] = MM_FMA_PD(vdc, _mm_loadu_pd(&fc->buff[1][ofs]), vout[1]); // out[1] += fc->dc[j] * fc->buff[1][ofs];
			}
			// vout[0](L0,L1) vout[1](R0,R1)
			tmp[0] = _mm_unpacklo_pd(vout[0], vout[1]); // (L0,R0)
			tmp[1] = _mm_unpackhi_pd(vout[0], vout[1]); // (L1,R1)
			tmp[0] = _mm_add_pd(tmp[0], tmp[1]); // (L0+L1,R0+R1)
			_mm_store_pd(&buf[i], tmp[0]); // buf[i] = out[0]; buf[i + 1] = out[1];
		}
	}else{ // mono
		__m128d vout; //DATA_T out;
		for(i = 0; i < count; i++){
			int32 sbuff = fc->count +  FIR_EQ_SIZE_MAX;
			fc->buff[0][fc->count] = buf[i];
			fc->buff[0][sbuff] = buf[i]; // for SIMD
			fc->count = (fc->count + 1) & mask;
			offset = (fc->count - fc->size) & mask;
			vout = _mm_setzero_pd(); // out = 0;
			for(j = 0; j < fc->size; j += 2){
				int32 ofs = (offset + j) & mask;
				__m128d vdc = _mm_loadu_pd(&fc->dc[j]);
				vout = MM_FMA_PD(vdc, _mm_loadu_pd(&fc->buff[0][ofs]), vout); // out += fc->dc[j] * fc->buff[0][ofs];
			}
			vout= _mm_add_pd(vout, _mm_shuffle_pd(vout, vout, 0x1)); // v0=v0+v1 v1=v1+v0
			_mm_store_sd(&buf[i], vout); // buf[i] = out;
		}
	}
#else
	if(fc->st){ // stereo
		DATA_T out[2];
		for(i = 0; i < count; i += 2){
			fc->buff[0][fc->count] = buf[i];
			fc->buff[1][fc->count] = buf[i + 1];
			fc->count = (fc->count + 1) & mask;
			offset = fc->count - fc->size;
			//offset &= fc->mask;
			//buf[i] = fc->buff[0][offset];
			//buf[i + 1] = fc->buff[1][offset];
			out[0] = 0; out[1] = 0;
			for(j = 0; j < fc->size; ++j){
				int32 ofs = (offset + j) & mask;
				out[0] += fc->dc[j] * fc->buff[0][ofs];
				out[1] += fc->dc[j] * fc->buff[1][ofs];
			}
			buf[i] = out[0]; buf[i + 1] = out[1];
		}
	}else{ // mono
		DATA_T out;
		for(i = 0; i < count; i++){
			fc->buff[0][fc->count] = buf[i];
			fc->count = (fc->count + 1) & mask;
			offset = fc->count - fc->size;
			out = 0;
			for(j = 0; j < fc->size; ++j){
				int32 ofs = (offset + j) & mask;
				out += fc->dc[j] * fc->buff[0][ofs];
			}
			buf[i] = out;
		}
	}
#endif
}






