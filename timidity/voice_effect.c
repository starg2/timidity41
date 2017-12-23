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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "mix.h"
#include "envelope.h"
#include "voice_effect.h"



/*        voice_effect        */
#ifdef VOICE_EFFECT

#ifndef POW2
#if 1 // lite
#define POW2(val) exp((float)(M_LN2 * val))
#else // precision
#define POW2(val) pow(2.0, val)
#endif
#endif /* POW2 */

/* math */

static inline FLOAT_T compute_math_mul(FLOAT_T in, FLOAT_T var)
{
	return in * var;
}

static inline FLOAT_T compute_math_add(FLOAT_T in, FLOAT_T var)
{
	return in + var;
}

static inline FLOAT_T compute_math_pow(FLOAT_T in, FLOAT_T var)
{
	if(var == 1.0){
		return in;
	}else{
		if(in == 0.0)
			return 0.0; // 0
		else if(in < 0)
			return -pow((FLOAT_T)-in, var); // -
		else
			return pow((FLOAT_T)in, var); // +
	}
}

static inline FLOAT_T compute_math_sine(FLOAT_T in, FLOAT_T var)
{
	return sin((FLOAT_T)in * (FLOAT_T)var * M_PI2);
}


#define MATH_LIST_MAX 4

typedef FLOAT_T (*compute_math_t)(FLOAT_T in, FLOAT_T var);

static compute_math_t compute_math[] = {
// cfg sort
	compute_math_mul,
	compute_math_add,
	compute_math_pow,
	compute_math_sine,
};




/*  delay  */
/* 
最初にディレイタイム(ms)を指定して初期化 init_delay()  // <500ms 384kHz
先に read_delay() // ディレイカウント変更なし
次に write_delay() // ディレイカウント処理
read/writeは1サンプルに1回
終了時にディレイバッファを開放 uninit_delay()
*/

static void init_delay(Info_Delay *info, FLOAT_T delay_time_ms)
{
	info->delay = (int32)(delay_time_ms * playmode_rate_ms); // <500ms 384kHz
	if(info->delay < 1)	{info->delay = 1;} 
	info->count = 0;
}

static inline FLOAT_T read_delay(Info_Delay *info)
{
	return info->ptr[info->count];
}

static inline void write_delay(Info_Delay *info, FLOAT_T in)
{
	info->ptr[info->count] = in;
	if (++info->count >= info->delay) {info->count -= info->delay;}
}


/* pitch shifter */
/* 
最初にプリディレイタイム(ms),ディレイタイム(ms),リードレート(cent)を指定して初期化 init_pitch_shifter() 
先に read_pitch_shifter() // リードディレイカウント処理
次に write_pitch_shifter() // ライトディレイカウント処理
read/writeは1サンプルに1回
リードレートを可変にする場合 calc_pitch_shifter_rate() でレート再設定
リードレートはサンプリングレートに対しての比率
[cent] (100 = +1key , -1200 = -1oct = 0.5倍 , 0 = 1倍 , 1200 = +1oct = 2倍 , 2400 = +2oct = 4倍

リードカウントとライトカウントで周回差が発生する部分のノイズ対策
リードバッファを複数(phase)にして クロスフェード
*/


static inline void calc_pitch_shifter_rate(Info_PitchShifter *info, FLOAT_T cent)
{
	info->rate = POW2(cent * DIV_1200) - 1.0; // pitch shift cent // 
	info->rate *= DIV_2; // ovx2
} 

static void init_pitch_shifter(Info_PitchShifter *info, FLOAT_T cent)
{
	const FLOAT_T div_phase = 1.0 / (FLOAT_T)VFX_PS_CF_PHASE;
	int bytes, i;

	calc_pitch_shifter_rate(info, cent); // pitch shift cent
	info->wcount = 0; // write count
	info->wcycle = 0;	
	info->rsdelay = (FLOAT_T)VFX_PS_CF_DELAY * playmode_rate_ms; // < VFX_PS_BUFFER_SIZE
	for(i = 0; i < VFX_PS_CF_PHASE; i++){
		info->rscount[i] = info->rsdelay * (FLOAT_T)i * div_phase; // reflesh count //  start offset
		info->rcount[i] = info->wcount - info->rsdelay + info->rscount[i]; // read count
		if(info->rcount[i] < 0)
			info->rcount[i] += VFX_PS_BUFFER_SIZE;
	}
	info->div_cf = 1.0 / (FLOAT_T)info->rsdelay;
}

static inline void do_pitch_shifter(FLOAT_T *inout, Info_PitchShifter *info)
{
	int32 i;
	FLOAT_T out = 0;
	FLOAT_T delay = info->wcount - info->rsdelay;
	const int32 mask = VFX_PS_BUFFER_SIZE - 1;
	
	for(i = 0; i < VFX_PS_CF_PHASE; i++){
		int32 index;
		FLOAT_T fp1, fp2, v1, v2, cf, tmp;

		// read buffer
		fp1 = info->rcount[i];
		fp2 = floor(fp1);
		index = fp2;
		v1 = info->ptr[index];
		v2 = info->ptr[(index + 1) & mask];
		// linear interpolation
		tmp = v1 + (v2 - v1) * (fp1 - fp2);
		// cross fade ratio // read_delayサイズを1周期とする三角半波 カウントリセット部分が0になる
		cf = info->rscount[i] * info->div_cf;		
		cf -= floor(cf);
		if(cf > 0.5) cf = 1.0 - cf;
		// cross fade mix 
		out += tmp * cf; // cf * 2.0(ratiomax) * DIV_2(ovx2)
		// read count
		info->rscount[i] += info->rate;
		if (info->rscount[i] >= info->rsdelay)
			info->rscount[i] -= info->rsdelay;
		else if (info->rscount[i] < -info->rsdelay)
			info->rscount[i] += info->rsdelay;
		info->rcount[i] = delay + info->rscount[i];
		if(info->rcount[i] < 0)
			info->rcount[i] += VFX_PS_BUFFER_SIZE;	
		else if (info->rcount[i] >= VFX_PS_BUFFER_SIZE)
			info->rcount[i] -= VFX_PS_BUFFER_SIZE;
		
		// read buffer
		fp1 = info->rcount[i];
		fp2 = floor(fp1);
		index = fp2;
		v1 = info->ptr[index];
		v2 = info->ptr[(index + 1) & mask];
		// linear interpolation
		tmp = v1 + (v2 - v1) * (fp1 - fp2);
		// cross fade ratio // read_delayサイズを1周期とする三角半波 カウントリセット部分が0になる
		cf = info->rscount[i] * info->div_cf;		
		cf -= floor(cf);
		if(cf > 0.5) cf = 1.0 - cf;
		// cross fade mix 
		out += tmp * cf; // cf * 2.0(ratiomax) * DIV_2(ovx2)
		// read count
		info->rscount[i] += info->rate;
		if (info->rscount[i] >= info->rsdelay)
			info->rscount[i] -= info->rsdelay;
		else if (info->rscount[i] < -info->rsdelay)
			info->rscount[i] += info->rsdelay;
		info->rcount[i] = delay + info->rscount[i];
		if(info->rcount[i] < 0)
			info->rcount[i] += VFX_PS_BUFFER_SIZE;	
		else if (info->rcount[i] >= VFX_PS_BUFFER_SIZE)
			info->rcount[i] -= VFX_PS_BUFFER_SIZE;
	}
	// write buffer
	info->ptr[info->wcount] = *inout;
	// write count // 0 ~ (VFX_PS_BUFFER_SIZE-1) のリングバッファ
	info->wcount = (++info->wcount) & mask;
	// output
	*inout = out;
}


/*  allpass filter  */
/* 
*/
#define VFX_AP_FEEDBACK (0.93333333333333)

static void init_allpass(Info_Allpass *info, FLOAT_T delay_time_ms, FLOAT_T feedback)
{
	info->delay = (int32)(delay_time_ms * playmode_rate_ms);
	if(info->delay < 1)	{info->delay = 1;} 
	else if(info->delay > VFX_ALLPASS_BUFFER_SIZE)	{info->delay = VFX_ALLPASS_BUFFER_SIZE;} 
	memset(info->ptr, 0, sizeof(FLOAT_T) * info->delay);
	info->count = 0;	
	info->feedback = feedback * VFX_AP_FEEDBACK;
}

static inline void do_allpass(Info_Allpass *info, FLOAT_T *inout)
{
	FLOAT_T bufout;

	bufout = info->ptr[info->count];
	info->ptr[info->count] = *inout + bufout * info->feedback;
	if (++info->count >= info->delay) {info->count = 0;}
	*inout = bufout - *inout;
}


/*  comb filter */
/* 
*/
#define VFX_COMB_FEEDBACK (0.93333333333333)

static inline int isprime_comb(int32 val)
{
	int32 i;

	if (val & 1) {
		for (i = 3; i < (int)sqrt((double)val) + 1; i += 2) {
			if ((val % i) == 0) {return 0;}
		}
		return 1; /* prime */
	} else {return 0;} /* even */
}

static void init_comb2(Info_Comb2 *info, FLOAT_T delay_time_ms, FLOAT_T feedback, FLOAT_T freq, FLOAT_T reso, int type)
{
	int32 size = delay_time_ms * playmode_rate_ms;
	
	if(size < 10) size = 10; 
	while(!isprime_comb(size)) size++;
	if(info->delay > VFX_COMB_BUFFER_SIZE)	info->delay = VFX_COMB_BUFFER_SIZE; 
	memset(info->ptr, 0, sizeof(FLOAT_T) * info->delay);	
	info->delay = size;
	info->count = 0;	
	info->feedback = feedback * VFX_COMB_FEEDBACK;	
	// filter
	init_sample_filter(&info->fc, freq, reso, type);
}

static inline void do_comb2(Info_Comb2 *info, FLOAT_T *in, FLOAT_T *out)
{
	FLOAT_T bufout;
	DATA_T flt;
	
	flt = bufout = info->ptr[info->count];
	sample_filter(&info->fc, &flt);
	info->ptr[info->count] = *in + flt * info->feedback;
	if (++info->count >= info->delay) {info->count = 0;}
	*out += bufout;
}





/*  VFX  */

static void init_vfx_none(int v, VoiceEffect *vfx){}

static void uninit_vfx_none(int v, VoiceEffect *vfx){}

static void noteoff_vfx_none(int v, VoiceEffect *vfx){}

static void damper_vfx_none(int v, VoiceEffect *vfx, int8 damper){}

static inline void do_vfx_none(int v, VoiceEffect *vfx, DATA_T *sp, int32 count){}

static void init_vfx_math(int v, VoiceEffect *vfx)
{
	InfoVFX_Math *info = (InfoVFX_Math *)vfx->info;

	if(vfx->param[1] < 0 || vfx->param[1] > 4) // vfx->param[1] = math type
		vfx->param[1] = 0;
	info->type = vfx->param[1];
	info->var = (FLOAT_T)vfx->param[2] * DIV_100; // vfx->param[2] = %
//	info->var2 = (FLOAT_T)vfx->param[3] * DIV_100; // vfx->param[3] = %
}

static inline void do_vfx_math(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Math *info = (InfoVFX_Math *)vfx->info;
	int32 i = 0;

	switch(info->type){
	case 0: // mul
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256d vec_var = _mm256_set1_pd((double)info->var);
		for (i = 0; i < count2; i += 8) {
			MM256_LSU_MUL_PD(&sp[i], vec_var);
			MM256_LSU_MUL_PD(&sp[i + 4], vec_var);
		}
		}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vec_var = _mm256_set1_ps((float)info->var);
		for (i = 0; i < count2; i += 8) {
			MM256_LSU_MUL_PS(&sp[i], vec_var);
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128d vec_var = _mm_set1_pd((double)info->var);
		for (i = 0; i < count2; i += 8) {
			MM_LSU_MUL_PD(&sp[i], vec_var);
			MM_LSU_MUL_PD(&sp[i + 2], vec_var);
			MM_LSU_MUL_PD(&sp[i + 4], vec_var);
			MM_LSU_MUL_PD(&sp[i + 6], vec_var);
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vec_var = _mm_set1_ps((float)info->var);
		for (i = 0; i < count2; i += 8) {
			MM_LSU_MUL_PS(&sp[i], vec_var);
			MM_LSU_MUL_PS(&sp[i + 4], vec_var);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			sp[i] *= info->var;
		}
		break;
	case 1: // add
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256d vec_var = _mm256_set1_pd((double)info->var);
		for (i = 0; i < count2; i += 8) {
			MM256_LSU_ADD_PD(&sp[i], vec_var);
			MM256_LSU_ADD_PD(&sp[i + 4], vec_var);
		}
		}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vec_var = _mm256_set1_ps((float)info->var);
		for (i = 0; i < count2; i += 8) {
			MM256_LSU_ADD_PS(&sp[i], vec_var);
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128d vec_var = _mm_set1_pd((double)info->var);
		for (i = 0; i < count2; i += 8) {
			MM_LSU_ADD_PD(&sp[i], vec_var);
			MM_LSU_ADD_PD(&sp[i + 2], vec_var);
			MM_LSU_ADD_PD(&sp[i + 4], vec_var);
			MM_LSU_ADD_PD(&sp[i + 6], vec_var);
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vec_var = _mm_set1_ps((float)info->var);
		for (i = 0; i < count2; i += 8) {
			MM_LSU_ADD_PS(&sp[i], vec_var);
			MM_LSU_ADD_PS(&sp[i + 4], vec_var);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			sp[i] += info->var;
		}
		break;
	case 2: // pow
		for (i = 0; i < count; i++) {
			if(sp[i] == 0.0)
				sp[i] = 0.0; // 0
			else if(sp[i] < 0)
				sp[i] = -pow((FLOAT_T)-sp[i], info->var); // -
			else
				sp[i] = pow((FLOAT_T)sp[i], info->var); // +
		}
		break;
	case 3: // sin
		for (i = 0; i < count; i++) {
			sp[i] = sin((FLOAT_T)sp[i] * (FLOAT_T)info->var * M_PI2);
		}
		break;
	case 4: // tanh
		for (i = 0; i < count; i++) {
			sp[i] = tanh((FLOAT_T)sp[i] * (FLOAT_T)info->var);
		}
		break;
	default:
		break;
	}
}


static void init_vfx_distortion(int v, VoiceEffect *vfx)
{
	InfoVFX_Distortion *info = (InfoVFX_Distortion *)vfx->info;
	if(vfx->param[1] < 0)
		vfx->param[1] = 0;
	info->type = vfx->param[1];
	if(vfx->param[2] < 1)
		vfx->param[2] = 1;
	if(vfx->param[3] < 1)
		vfx->param[3] = 1;
	info->gain = vfx->param[2] * DIV_100;
	info->level = vfx->param[3] * DIV_100;
	if(info->type == 7){
		info->velgain = pow((FLOAT_T)vfx->param[4] * DIV_100, 1.0 - (FLOAT_T)voice[v].velocity * DIV_127);
		info->div_gain = 1.0 / info->gain;
	}else{
		info->velgain = info->gain * pow((FLOAT_T)vfx->param[4] * DIV_100, 1.0 - (FLOAT_T)voice[v].velocity * DIV_127);
		info->div_gain = 1.0;
	}
}

static inline void do_vfx_distortion(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Distortion *info = (InfoVFX_Distortion *)vfx->info;
	int32 i = 0;

	switch(info->type){
	default:
	case 0:
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		__m256 vmax = _mm256_set1_ps(1.0);
		__m256 vmin = _mm256_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1;
			vtmp1 = MM256_SET2X_PS(
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i])),
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i + 4])) ); 
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vtmp1 = _mm256_min_ps(vtmp1, vmax);
			vtmp1 = _mm256_max_ps(vtmp1, vmin);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_pd(&sp[i], _mm256_cvtps_pd(_mm256_extractf128_ps(vtmp1, 0x0)));
			_mm256_storeu_pd(&sp[i + 4], _mm256_cvtps_pd(_mm256_extractf128_ps(vtmp1, 0x1)));
		}
		}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		__m256 vmax = _mm256_set1_ps(1.0);
		__m256 vmin = _mm256_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1;
			vtmp1 = _mm256_loadu_ps(&sp[i]);	
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vtmp1 = _mm256_min_ps(vtmp1, vmax);
			vtmp1 = _mm256_max_ps(vtmp1, vmin);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_ps(&sp[i], _mm256_mul_ps(vtmp1, vlevel));
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		__m128 vmax = _mm_set1_ps(1.0);
		__m128 vmin = _mm_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2;
			vtmp1 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 2])), 0x44);
			vtmp2 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 4])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 6])), 0x44);			
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vtmp1 = _mm_min_ps(vtmp1, vmax);
			vtmp2 = _mm_min_ps(vtmp2, vmax);
			vtmp1 = _mm_max_ps(vtmp1, vmin);
			vtmp2 = _mm_max_ps(vtmp2, vmin);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_pd(&sp[i], _mm_cvtps_pd(vtmp1));
			_mm_storeu_pd(&sp[i + 2], _mm_cvtps_pd(_mm_movehl_ps(vtmp1,vtmp1)));
			_mm_storeu_pd(&sp[i + 4], _mm_cvtps_pd(vtmp2));
			_mm_storeu_pd(&sp[i + 6], _mm_cvtps_pd(_mm_movehl_ps(vtmp2,vtmp2)));
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		__m128 vmax = _mm_set1_ps(1.0);
		__m128 vmin = _mm_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2;
			vtmp1 = _mm_loadu_ps(&sp[i]);
			vtmp2 = _mm_loadu_ps(&sp[i + 4]);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vtmp1 = _mm_min_ps(vtmp1, vmax);
			vtmp2 = _mm_min_ps(vtmp2, vmax);
			vtmp1 = _mm_max_ps(vtmp1, vmin);
			vtmp2 = _mm_max_ps(vtmp2, vmin);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_ps(&sp[i], vtmp1);
			_mm_storeu_ps(&sp[i + 4], vtmp2);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			if(tmp > 1.0)
				tmp = 1.0;
			else if(tmp < -1.0)
				tmp = -1.0;
			sp[i] = tmp * info->level;
		}
		break;
	case 1:
#if 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		const __m256 vvq = _mm256_set1_ps(0.25);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = MM256_SET2X_PS(
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i])),
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i + 4])) ); 			
			vtmp1 = _mm256_mul_ps(vtmp1, vgain); _mm256_cmp_ps(
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_sub_ps(vtmp1, vvp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vvq);
			vtmp1 = _mm256_add_ps(vtmp1, vvp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_pd(&sp[i], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x0)));
			_mm256_storeu_pd(&sp[i + 4], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x1)));
		}
		}
#elif 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		const __m256 vvq = _mm256_set1_ps(0.25);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = _mm256_loadu_ps(&sp[i]);
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_sub_ps(vtmp1, vvp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vvq);
			vtmp1 = _mm256_add_ps(vtmp1, vvp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_ps(&sp[i], vtmp1);
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		const __m128 vvq = _mm_set1_ps(0.25);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			vtmp1 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 2])), 0x44);
			vtmp2 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 4])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 6])), 0x44);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_sub_ps(vtmp1, vvp1);
			vtmp2 = _mm_sub_ps(vtmp2, vvp1);
			vtmp1 = _mm_mul_ps(vtmp1, vvq);
			vtmp2 = _mm_mul_ps(vtmp2, vvq);
			vtmp1 = _mm_add_ps(vtmp1, vvp1);
			vtmp2 = _mm_add_ps(vtmp2, vvp1);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_pd(&sp[i], _mm_cvtps_pd(vtmp1));
			_mm_storeu_pd(&sp[i + 2], _mm_cvtps_pd(_mm_movehl_ps(vtmp1,vtmp1)));
			_mm_storeu_pd(&sp[i + 4], _mm_cvtps_pd(vtmp2));
			_mm_storeu_pd(&sp[i + 6], _mm_cvtps_pd(_mm_movehl_ps(vtmp2,vtmp2)));
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		const __m128 vvq = _mm_set1_ps(0.25);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			vtmp1 = _mm_loadu_ps(&sp[i]);
			vtmp2 = _mm_loadu_ps(&sp[i + 4]);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_sub_ps(vtmp1, vvp1);
			vtmp2 = _mm_sub_ps(vtmp2, vvp1);
			vtmp1 = _mm_mul_ps(vtmp1, vvq);
			vtmp2 = _mm_mul_ps(vtmp2, vvq);
			vtmp1 = _mm_add_ps(vtmp1, vvp1);
			vtmp2 = _mm_add_ps(vtmp2, vvp1);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_ps(&sp[i], vtmp1);
			_mm_storeu_ps(&sp[i + 4], vtmp2);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			if(tmp > 1.0)
				tmp = 1.0 + (tmp - 1.0) * 0.25;
			else if(tmp < -1.0)
				tmp = -1.0 - (tmp + 1.0) * 0.25;
			sp[i] = tmp * info->level;
		}
		break;
	case 2:
#if 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = MM256_SET2X_PS(
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i])),
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i + 4])) ); 			
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_sqrt_ps(vtmp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_pd(&sp[i], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x0)));
			_mm256_storeu_pd(&sp[i + 4], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x1)));
		}
		}
#elif 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = _mm256_loadu_ps(&sp[i]);
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_sqrt_ps(vtmp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_ps(&sp[i], vtmp1);
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			__m128d vtmpd1, vtmpd2;
			vtmp1 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 2])), 0x44);
			vtmp2 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 4])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 6])), 0x44);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_sqrt_ps(vtmp1);
			vtmp2 = _mm_sqrt_ps(vtmp2);	
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_pd(&sp[i], _mm_cvtps_pd(vtmp1));
			_mm_storeu_pd(&sp[i + 2], _mm_cvtps_pd(_mm_movehl_ps(vtmp1,vtmp1)));
			_mm_storeu_pd(&sp[i + 4], _mm_cvtps_pd(vtmp2));
			_mm_storeu_pd(&sp[i + 6], _mm_cvtps_pd(_mm_movehl_ps(vtmp2,vtmp2)));
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			vtmp1 = _mm_loadu_ps(&sp[i]);
			vtmp2 = _mm_loadu_ps(&sp[i + 4]);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_sqrt_ps(vtmp1);
			vtmp2 = _mm_sqrt_ps(vtmp2);	
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_ps(&sp[i], vtmp1);
			_mm_storeu_ps(&sp[i + 4], vtmp2);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			if(tmp > 1.0)
				tmp = sqrt(tmp);
			else if(tmp < -1.0)
				tmp = -sqrt(-tmp);
			sp[i] = tmp * info->level;
		}
		break;
	case 4:
#if 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		const __m256 vvp2 = _mm256_set1_ps(2.0);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = MM256_SET2X_PS(
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i])),
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i + 4])) ); 			
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_sqrt_ps(vtmp1);
			vtmp1 = _mm256_sub_ps(vvp2, vtmp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_pd(&sp[i], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x0)));
			_mm256_storeu_pd(&sp[i + 4], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x1)));
		}
		}
#elif 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		const __m256 vvp2 = _mm256_set1_ps(2.0);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = _mm256_loadu_ps(&sp[i]);
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_sqrt_ps(vtmp1);
			vtmp1 = _mm256_sub_ps(vvp2, vtmp1);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_ps(&sp[i], vtmp1);
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		const __m128 vvp2 = _mm_set1_ps(2.0);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			__m128d vtmpd1, vtmpd2;
			vtmp1 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 2])), 0x44);
			vtmp2 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 4])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 6])), 0x44);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_sqrt_ps(vtmp1);
			vtmp2 = _mm_sqrt_ps(vtmp2);
			vtmp1 = _mm_sub_ps(vvp2, vtmp1);
			vtmp2 = _mm_sub_ps(vvp2, vtmp2);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_pd(&sp[i], _mm_cvtps_pd(vtmp1));
			_mm_storeu_pd(&sp[i + 2], _mm_cvtps_pd(_mm_movehl_ps(vtmp1,vtmp1)));
			_mm_storeu_pd(&sp[i + 4], _mm_cvtps_pd(vtmp2));
			_mm_storeu_pd(&sp[i + 6], _mm_cvtps_pd(_mm_movehl_ps(vtmp2,vtmp2)));
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		const __m128 vvp2 = _mm_set1_ps(2.0);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			vtmp1 = _mm_loadu_ps(&sp[i]);
			vtmp2 = _mm_loadu_ps(&sp[i + 4]);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_sqrt_ps(vtmp1);
			vtmp2 = _mm_sqrt_ps(vtmp2);
			vtmp1 = _mm_sub_ps(vvp2, vtmp1);
			vtmp2 = _mm_sub_ps(vvp2, vtmp2);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_ps(&sp[i], vtmp1);
			_mm_storeu_ps(&sp[i + 4], vtmp2);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			if(tmp > 1.0)
				tmp = 2.0 - sqrt(tmp);
			else if(tmp < -1.0)
				tmp = -2.0 + sqrt(-tmp);
			sp[i] = tmp * info->level;
		}
		break;
	case 5:
		for (i = 0; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			if(tmp > 1.0 || tmp < -1.0)
				tmp = fabs(fabs(fmod(tmp - 1.0, 4.0)) - 2.0) - 1.0;
			sp[i] = tmp * info->level;
		}
		break;
	case 6:
#if 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		const __m256 vvq = _mm256_set1_ps(-0.15);
		const __m256 vv11 = _mm256_set1_ps(1.1);
		const __m256 vmsign = _mm256_set1_ps(-0.0f);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = MM256_SET2X_PS(
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i])),
				_mm256_cvtpd_ps(_mm256_loadu_pd(&sp[i + 4])) ); 			
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_andnot_ps(vtmp1, vmsign);
			vtmp1 = _mm256_mul_ps(vtmp1, vvq);
			vtmp1 = _mm256_add_ps(vtmp1, vv11);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_pd(&sp[i], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x0)));
			_mm256_storeu_pd(&sp[i + 4], _mm256_cvtps_pd(_mm256_extract_ps(vtmp1, 0x1)));
		}
		}
#elif 0 && (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m256 vgain = _mm256_set1_ps((float)info->velgain);
		__m256 vlevel = _mm256_set1_ps((float)info->level);
		const __m256 vvp1 = _mm256_set1_ps(1.0);
		const __m256 vvn1 = _mm256_set1_ps(-1.0);
		const __m256 vvq = _mm256_set1_ps(-0.15);
		const __m256 vv11 = _mm256_set1_ps(1.1);
		const __m256 vmsign = _mm256_set1_ps(-0.0f);
		for (i = 0; i < count2; i += 8) {
			__m256 vtmp1, vme, vsp, vsn, vsign1, vbase1;
			vtmp1 = _mm256_loadu_ps(&sp[i]);
			vtmp1 = _mm256_mul_ps(vtmp1, vgain);
			vsp = _mm256_and_ps(vvp1, _mm256_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm256_and_ps(vvn1, _mm256_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm256_or_ps(vsp, vsn);			
			vme = _mm256_and_ps(_mm256_cmple_ps(vtmp1, vvp1), _mm256_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm256_and_ps(vtmp1, vme);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1)
			vtmp1 = _mm256_andnot_ps(vtmp1, vmsign);
			vtmp1 = _mm256_mul_ps(vtmp1, vvq);
			vtmp1 = _mm256_add_ps(vtmp1, vv11);
			vtmp1 = _mm256_mul_ps(vtmp1, vsign1);
			vtmp1 = _mm256_add_ps(vtmp1, vbase1);
			vtmp1 = _mm256_mul_ps(vtmp1, vlevel);
			_mm256_storeu_ps(&sp[i], vtmp1);
		}
		}		
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		const __m128 vvq = _mm_set1_ps(-0.15);
		const __m128 vv11 = _mm_set1_ps(1.1);
		const __m128 vmsign = _mm_set1_ps(-0.0f);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			vtmp1 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 2])), 0x44);
			vtmp2 = _mm_shuffle_ps(
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 4])), 
				_mm_cvtpd_ps(_mm_loadu_pd(&sp[i + 6])), 0x44);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_andnot_ps(vtmp1, vmsign);
			vtmp2 = _mm_andnot_ps(vtmp2, vmsign);
			vtmp1 = _mm_mul_ps(vtmp1, vvq);
			vtmp2 = _mm_mul_ps(vtmp2, vvq);
			vtmp1 = _mm_add_ps(vtmp1, vv11);
			vtmp2 = _mm_add_ps(vtmp2, vv11);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_pd(&sp[i], _mm_cvtps_pd(vtmp1));
			_mm_storeu_pd(&sp[i + 2], _mm_cvtps_pd(_mm_movehl_ps(vtmp1,vtmp1)));
			_mm_storeu_pd(&sp[i + 4], _mm_cvtps_pd(vtmp2));
			_mm_storeu_pd(&sp[i + 6], _mm_cvtps_pd(_mm_movehl_ps(vtmp2,vtmp2)));
		}
		}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
		{
		const int32 req_count_mask = ~(0x7);
		int32 count2 = count & req_count_mask;
		__m128 vgain = _mm_set1_ps((float)info->velgain);
		__m128 vlevel = _mm_set1_ps((float)info->level);
		const __m128 vvp1 = _mm_set1_ps(1.0);
		const __m128 vvn1 = _mm_set1_ps(-1.0);
		const __m128 vvq = _mm_set1_ps(-0.15);
		const __m128 vv11 = _mm_set1_ps(1.1);
		const __m128 vmsign = _mm_set1_ps(-0.0f);
		for (i = 0; i < count2; i += 8) {
			__m128 vtmp1, vtmp2, vme, vsp, vsn, vsign1, vsign2, vbase1, vbase2;
			vtmp1 = _mm_loadu_ps(&sp[i]);
			vtmp2 = _mm_loadu_ps(&sp[i + 4]);
			vtmp1 = _mm_mul_ps(vtmp1, vgain);
			vtmp2 = _mm_mul_ps(vtmp2, vgain);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp1, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp1, vvn1));	
			vsign1 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp1, vvp1), _mm_cmpge_ps(vtmp1, vvn1));
			vbase1 = _mm_and_ps(vtmp1, vme);
			vsp = _mm_and_ps(vvp1, _mm_cmpgt_ps(vtmp2, vvp1));	
			vsn = _mm_and_ps(vvn1, _mm_cmplt_ps(vtmp2, vvn1));	
			vsign2 = _mm_or_ps(vsp, vsn);			
			vme = _mm_and_ps(_mm_cmple_ps(vtmp2, vvp1), _mm_cmpge_ps(vtmp2, vvn1));
			vbase2 = _mm_and_ps(vtmp2, vme);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_andnot_ps(vtmp1, vmsign);
			vtmp2 = _mm_andnot_ps(vtmp2, vmsign);
			vtmp1 = _mm_mul_ps(vtmp1, vvq);
			vtmp2 = _mm_mul_ps(vtmp2, vvq);
			vtmp1 = _mm_add_ps(vtmp1, vv11);
			vtmp2 = _mm_add_ps(vtmp2, vv11);
			vtmp1 = _mm_mul_ps(vtmp1, vsign1);
			vtmp2 = _mm_mul_ps(vtmp2, vsign2);
			vtmp1 = _mm_add_ps(vtmp1, vbase1);
			vtmp2 = _mm_add_ps(vtmp2, vbase2);
			vtmp1 = _mm_mul_ps(vtmp1, vlevel);
			vtmp2 = _mm_mul_ps(vtmp2, vlevel);
			_mm_storeu_ps(&sp[i], vtmp1);
			_mm_storeu_ps(&sp[i + 4], vtmp2);
		}
		}
#endif // USE_X86_EXT_INTRIN
		for (; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			tmp = tmp * (1.1 - fabs(tmp) * 0.15);
			sp[i] = tmp * info->level;
		}
		break;
	case 7:
		for (i = 0; i < count; i++) {
			FLOAT_T tmp = sp[i] * info->velgain;
			if(tmp < 0.0) 
				tmp = -pow(-tmp, info->div_gain);
			else
				tmp = pow(tmp, info->div_gain);
			sp[i] = tmp * info->level;
		}
		break;
	}
}

static void init_vfx_equalizer(int v, VoiceEffect *vfx)
{
	InfoVFX_Equalizer *info = (InfoVFX_Equalizer *)vfx->info;
	
	switch(vfx->param[1]){ // eq type
	case 0: //low
		init_sample_filter2(&info->eq, vfx->param[2], vfx->param[3], (FLOAT_T)vfx->param[4] * DIV_100, FILTER_SHELVING_LOW);
		break;
	case 1: //hi
		init_sample_filter2(&info->eq, vfx->param[2], vfx->param[3], (FLOAT_T)vfx->param[4] * DIV_100, FILTER_SHELVING_HI);
		break;
	case 2: //mid
		init_sample_filter2(&info->eq, vfx->param[2], vfx->param[3], (FLOAT_T)vfx->param[4] * DIV_100, FILTER_PEAKING);
		break;
	default:
		init_sample_filter2(&info->eq, 0, 0, 0, FILTER_NONE);
		break;
	}
}

static inline void do_vfx_equalizer(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Equalizer *info = (InfoVFX_Equalizer *)vfx->info;
	int32 i;

	buffer_filter(&info->eq, sp, count);
}

static void init_vfx_filter(int v, VoiceEffect *vfx)
{
	InfoVFX_Filter *info = (InfoVFX_Filter *)vfx->info;
	
	init_sample_filter(&info->fc, vfx->param[2], vfx->param[3], vfx->param[1]);
	info->wet = (FLOAT_T)vfx->param[4] * DIV_100; // wet
	info->dry = 1.0 - info->wet; // dry
}

static inline void do_vfx_filter(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Filter *info = (InfoVFX_Filter *)vfx->info;
	int32 i;
	DATA_T tmp;

	for(i = 0; i < count; i++){
		tmp = sp[i];
		sample_filter(&info->fc, &tmp);
		sp[i] = tmp * info->wet + sp[i] * info->dry;
	}
}


ALIGN const FLOAT_T FF_vowel_coeff[5][16] =
{
	{ 8.11044e-06, 8.943665402, -36.83889529, 92.01697887, -154.337906, 181.6233289,
		-151.8651235, 89.09614114, -35.10298511, 8.388101016, -0.923313471, 0, 0, 0, 0, 0 },  ///A
	{ 3.33819e-06, 8.893102966, -36.49532826, 90.96543286, -152.4545478, 179.4835618,
		-150.315433, 88.43409371, -34.98612086, 8.407803364, -0.932568035, 0, 0, 0, 0, 0 },  ///I
	{ 4.09431e-07, 8.997322763, -37.20218544, 93.11385476, -156.2530937, 183.7080141,
		-153.2631681, 89.59539726, -35.12454591, 8.338655623, -0.910251753, 0, 0, 0, 0, 0 },  ///U
	{ 4.36215e-06, 8.90438318, -36.55179099, 91.05750846, -152.422234, 179.1170248,
		-149.6496211, 87.78352223, -34.60687431, 8.282228154, -0.914150747, 0, 0, 0, 0, 0 },  ///E
	{ 1.13572e-06, 8.994734087, -37.2084849, 93.22900521, -156.6929844, 184.596544,
		-154.3755513, 90.49663749, -35.58964535, 8.478996281, -0.929252233, 0, 0, 0, 0, 0 },   ///O
};

static void init_vfx_formant(int v, VoiceEffect *vfx)
{
	InfoVFX_Formant *info = (InfoVFX_Formant *)vfx->info;
	if(vfx->param[1] < 0 || vfx->param[1] > 4)
		vfx->param[1] = 0;
	info->type = vfx->param[1];
}

static inline void do_vfx_formant(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Formant *info = (InfoVFX_Formant *)vfx->info;
	int32 i;
	FLOAT_T *db = info->db;

	for (i = 0; i < count; i++) {
		FLOAT_T fmt = 0;
		db[0] = sp[i];
		fmt += db[0] * FF_vowel_coeff[info->type][0];
		fmt += db[1] * FF_vowel_coeff[info->type][1];
		fmt += db[2] * FF_vowel_coeff[info->type][2];
		fmt += db[3] * FF_vowel_coeff[info->type][3];
		fmt += db[4] * FF_vowel_coeff[info->type][4];
		fmt += db[5] * FF_vowel_coeff[info->type][5];
		fmt += db[6] * FF_vowel_coeff[info->type][6];
		fmt += db[7] * FF_vowel_coeff[info->type][7];
		fmt += db[8] * FF_vowel_coeff[info->type][8];
		fmt += db[9] * FF_vowel_coeff[info->type][9];
		fmt += db[10] * FF_vowel_coeff[info->type][10];		
		db[10] = db[9];
		db[9] = db[8];
		db[8] = db[7];
		db[7] = db[6];
		db[6] = db[5];
		db[5] = db[4];
		db[4] = db[3];
		db[3] = db[2];
		db[2] = db[1];                     
		sp[i] = db[1] = fmt;
	}
}

static void init_vfx_delay(int v, VoiceEffect *vfx)
{
	InfoVFX_Delay *info = (InfoVFX_Delay *)vfx->info;
	int bytes;
	
	if(vfx->param[1] < 1)
		vfx->param[1] = 1;
	else if(vfx->param[1] > 500)
		vfx->param[1] = 500;
	init_delay(&info->dly, (FLOAT_T)vfx->param[1]); // param[1] = delay time ms
	info->wet = (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = delay level
	info->dry = 1.0 - info->wet;
	info->feedback = (FLOAT_T)vfx->param[3] * DIV_100; // param[3] = feedback level
}

static inline void do_vfx_delay(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Delay *info = (InfoVFX_Delay *)vfx->info;
	int32 i;
	FLOAT_T tmp; 

	for (i = 0; i < count; i++) {
		tmp = read_delay(&info->dly);
		write_delay(&info->dly, sp[i] + tmp * info->feedback);
		sp[i] = tmp * info->wet + sp[i] * info->dry;
	}
}

static void init_vfx_compressor(int v, VoiceEffect *vfx)
{
	InfoVFX_Compressor *info = (InfoVFX_Compressor *)vfx->info;

	if(vfx->param[3] < 1)
		vfx->param[3] = 1;
	info->threshold = (FLOAT_T)vfx->param[3] * DIV_100; // vfx->param[3] = threshold %
	if(vfx->param[4] < 0)
		vfx->param[4] = 0;
	info->ratio = (FLOAT_T)vfx->param[4] * DIV_100; // vfx->param[4] =  ratio %
	info->attack = (vfx->param[1] == 0) ? (0.0) : exp (-1.0 / (playmode_rate_ms * vfx->param[1])); // param[1] = attack time ms
	info->release = (vfx->param[2] == 0) ? (0.0) : exp (-1.0 / (playmode_rate_ms * vfx->param[2])); // param[2] = release time ms
	info->lhsmp = (int)(playmode_rate_ms * 5); // lookahead 5(ms) // sample offset to lookahead wnd start		
	info->nrms = (int)(playmode_rate_ms * 1); // window 1 (ms) // samples count in lookahead window
	info->div_nrms = 1.0 / (playmode_rate_ms * 1); // window 1 (ms)
	info->delay1 = info->nrms; // < 1ms
	info->count1 = 0;
	info->delay2 = info->lhsmp; // < 2ms
	info->count2 = 0;
	info->env = 0;
}

static inline void do_vfx_compressor(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Compressor *info = (InfoVFX_Compressor *)vfx->info;
	double theta = 0.0, rms, summ, gain;
	int32 i, j;

	for (i = 0; i < count; ++i)
	{		
		FLOAT_T data = info->ptr2[info->count2];
		info->ptr2[info->count2] = sp[i];
		info->ptr1[info->count1] = sp[i] * sp[i];
		if (++info->count1 >= info->delay1) {info->count1 -= info->delay1;}
		if (++info->count2 >= info->delay2) {info->count2 -= info->delay2;}
		summ = 0;
		for (j = 0; j < info->nrms; ++j)
			summ += info->ptr1[j]; // level float
		rms = sqrt(summ * info->div_nrms);
		theta = rms > info->env ? info->attack : info->release;
		info->env = (1.0 - theta) * rms + theta * info->env;		
	//	gain = (info->env > info->threshold) ? (1.0 - (info->env - info->threshold)) : 1.0;
		gain = (info->env > info->threshold) ? (info->threshold / info->env) : 1.0;
		sp[i] = (gain < info->ratio) ? (data * info->ratio) : (data * gain);		
	}
}

static void init_vfx_pitch_shifter(int v, VoiceEffect *vfx)
{
	InfoVFX_PitchShifter *info = (InfoVFX_PitchShifter *)vfx->info;
	int bytes;

// pitch shift
	init_pitch_shifter(&info->ps, (FLOAT_T)vfx->param[1]); // param[1] = pitch shift
	info->wet = (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = delay level
	info->dry = 1.0 - info->wet;
}

static inline void do_vfx_pitch_shifter(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_PitchShifter *info = (InfoVFX_PitchShifter *)vfx->info;
	int32 i;
    FLOAT_T tmp;

	for (i = 0; i < count; i++) {
		tmp = sp[i];
		do_pitch_shifter(&tmp, &info->ps);
		sp[i] = tmp * info->wet + sp[i] * info->dry;
	}
}

static void init_vfx_feedbacker(int v, VoiceEffect *vfx)
{
	InfoVFX_Feedbacker *info = (InfoVFX_Feedbacker *)vfx->info;
	double delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt;
	int bytes;
	
	init_sample_filter(&info->fc, vfx->param[1], vfx->param[2], FILTER_LPF_BW); // lpf
	init_envelope3(&info->env, 0.0, (FLOAT_T)vfx->param[3] * playmode_rate_ms);
	reset_envelope3(&info->env, 1.0, ENVELOPE_KEEP);
	info->feedback = (FLOAT_T)vfx->param[4] * DIV_100; // sub delay feedback
}

static inline void do_vfx_feedbacker(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Feedbacker *info = (InfoVFX_Feedbacker *)vfx->info;
	int32 i;
	Envelope3 *env = &info->env;

	FLOAT_T tmp;
	DATA_T fb_data = info->fb_data;
	compute_envelope3(env, count);		
	for(i = 0; i < count; i++) {
		tmp = sp[i] + fb_data * env->vol * info->feedback;
		if(tmp > 1.0)
			tmp = 1.0;
		else if(tmp < -1.0)
			tmp = -1.0;
		sp[i] = fb_data = tmp;
		sample_filter(&info->fc, &fb_data);
	}
	info->fb_data = fb_data;
}

static void init_vfx_reverb(int v, VoiceEffect *vfx)
{
	InfoVFX_Reverb *info = (InfoVFX_Reverb *)vfx->info;
	FLOAT_T delay;
	
	if(vfx->param[1] < 1)
		vfx->param[1] = 1;
	else if(vfx->param[1] > 300)
		vfx->param[1] = 300;
	delay = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = delay time
	init_delay(&info->dly1, delay);
	init_delay(&info->dly2, delay * 0.8);
	init_delay(&info->dly3, delay * 1.3);
	init_delay(&info->dly4, delay * 1.1);
	info->wet = (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = wet
	info->dry = 1.0 - info->wet;
	info->feedback = 0.30 * (FLOAT_T)vfx->param[3] * DIV_100; // param[3] = feedback level
	info->fb_data = 0.0;
	init_sample_filter(&info->fc, vfx->param[4], 0, FILTER_LPF6x2); // lpf
}

static inline void do_vfx_reverb(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Reverb *info = (InfoVFX_Reverb *)vfx->info;
	int32 i;
	FLOAT_T tmp, tmp1, tmp2, tmp3, tmp4; 
	DATA_T fb_data = info->fb_data;
	
	for (i = 0; i < count; i++) {
		tmp = sp[i] + fb_data * info->feedback;
		tmp1 = read_delay(&info->dly1);
		write_delay(&info->dly1, tmp + tmp1 * info->feedback);
		tmp2 = read_delay(&info->dly2);
		write_delay(&info->dly2, tmp1 + tmp2 * info->feedback);
		tmp3 = read_delay(&info->dly3);
		write_delay(&info->dly3, tmp2 + tmp3 * info->feedback);
		tmp4 = read_delay(&info->dly4);
		write_delay(&info->dly4, tmp3 + tmp4 * info->feedback);
		fb_data = (tmp1 + tmp2 + tmp3 + tmp4) * DIV_4;
		sample_filter(&info->fc, &fb_data);
		sp[i] = fb_data * info->wet + sp[i] * info->dry;
	}
	info->fb_data = fb_data;
}

static void init_vfx_envelope_reverb(int v, VoiceEffect *vfx)
{
	InfoVFX_Envelope_Reverb *info = (InfoVFX_Envelope_Reverb *)vfx->info;
	FLOAT_T delay;
	Envelope1 *env = &info->env;
	double delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt;
	
	if(vfx->param[1] < 1)
		vfx->param[1] = 1;
	else if(vfx->param[1] > 300)
		vfx->param[1] = 300;
	delay = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = delay time
	init_delay(&info->dly1, delay);
	init_delay(&info->dly2, delay * 0.8);
	init_delay(&info->dly3, delay * 1.3);
	init_delay(&info->dly4, delay * 1.1);
	info->wet = (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = wet
	info->dry = 1.0 - info->wet;
	info->feedback = 0.30 * (FLOAT_T)vfx->param[3] * DIV_100; // param[3] = feedback level
	info->fb_data = 0.0;
	init_sample_filter(&info->fc, vfx->param[4], 0, FILTER_LPF6x2); // lpf
	delay_cnt = 0;
	attack_cnt = (FLOAT_T)vfx->param[5] * playmode_rate_ms;
	hold_cnt = 0;
	decay_cnt = (FLOAT_T)vfx->param[6] * playmode_rate_ms;
	sustain_vol = (FLOAT_T)vfx->param[7] * DIV_100;
	sustain_cnt = MAX_ENV1_LENGTH;
	release_cnt = (FLOAT_T)vfx->param[8] * playmode_rate_ms;
	init_envelope1(env, delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt, LINEAR_CURVE, DEF_VOL_CURVE);
}

static void noteoff_vfx_envelope_reverb(int v, VoiceEffect *vfx)
{	
	InfoVFX_Envelope_Reverb *info = (InfoVFX_Envelope_Reverb *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_release(env, ENVELOPE_KEEP);
}

static void damper_vfx_envelope_reverb(int v, VoiceEffect *vfx, int8 damper)
{	
	InfoVFX_Envelope_Reverb *info = (InfoVFX_Envelope_Reverb *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_half_damper(env, damper);
}

static inline void do_vfx_envelope_reverb(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Envelope_Reverb *info = (InfoVFX_Envelope_Reverb *)vfx->info;
	int32 i;
	FLOAT_T tmp, tmp1, tmp2, tmp3, tmp4; 
	Envelope1 *env = &info->env;
	DATA_T fb_data = info->fb_data;

	compute_envelope1(env, count);
	for (i = 0; i < count; i++) {
		tmp = sp[i] + fb_data * info->feedback;
		tmp1 = read_delay(&info->dly1);
		write_delay(&info->dly1, tmp + tmp1 * info->feedback);
		tmp2 = read_delay(&info->dly2);
		write_delay(&info->dly2, tmp1 + tmp2 * info->feedback);
		tmp3 = read_delay(&info->dly3);
		write_delay(&info->dly3, tmp2 + tmp3 * info->feedback);
		tmp4 = read_delay(&info->dly4);
		write_delay(&info->dly4, tmp3 + tmp4 * info->feedback);
		fb_data = (tmp1 + tmp2 + tmp3 + tmp4) * DIV_4;
		sample_filter(&info->fc, &fb_data);
		sp[i] = fb_data * info->env.volume * info->wet + sp[i] * info->dry;
	}
	info->fb_data = fb_data;
}

static void init_vfx_envelope(int v, VoiceEffect *vfx)
{
	InfoVFX_Envelope *info = (InfoVFX_Envelope *)vfx->info;
	Envelope1 *env = &info->env;
	double delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt;
		
	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[2] = delay level
	info->dry = 1.0 - info->wet;
	delay_cnt = 0;
	attack_cnt = (FLOAT_T)vfx->param[2] * playmode_rate_ms;
	hold_cnt = 0;
	decay_cnt = (FLOAT_T)vfx->param[3] * playmode_rate_ms;
	sustain_vol = (FLOAT_T)vfx->param[4] * DIV_100;
	sustain_cnt = MAX_ENV1_LENGTH;
	release_cnt = (FLOAT_T)vfx->param[5] * playmode_rate_ms;
	init_envelope1(env, delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt, LINEAR_CURVE, DEF_VOL_CURVE);
}

static void noteoff_vfx_envelope(int v, VoiceEffect *vfx)
{	
	InfoVFX_Envelope *info = (InfoVFX_Envelope *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_release(env, ENVELOPE_KEEP);
}

static void damper_vfx_envelope(int v, VoiceEffect *vfx, int8 damper)
{	
	InfoVFX_Envelope *info = (InfoVFX_Envelope *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_half_damper(env, damper);
}

static inline void do_vfx_envelope(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Envelope *info = (InfoVFX_Envelope *)vfx->info;
	int32 i = 0;
	Envelope1 *env = &info->env;	
	FLOAT_T amp = info->env.volume * info->wet + info->dry;
	
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m256d vamp = _mm256_set1_pd(amp);
	for(i = 0; i < count2; i += 8){
		MM256_LSU_MUL_PD(&sp[i], vamp);
		MM256_LSU_MUL_PD(&sp[i + 4], vamp);
	}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m256 vamp = _mm256_set1_ps(amp);
	for(i = 0; i < count2; i += 8){
		MM256_LSU_MUL_PS(&sp[i], vamp);
	}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m128d vamp = _mm_set1_pd(amp);
	for(i = 0; i < count2; i += 8){
		MM_LSU_MUL_PD(&sp[i], vamp);
		MM_LSU_MUL_PD(&sp[i + 2], vamp);
		MM_LSU_MUL_PD(&sp[i + 4], vamp);
		MM_LSU_MUL_PD(&sp[i + 6], vamp);
	}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m128 vamp = _mm_set1_ps(amp);
	for(i = 0; i < count2; i += 8){
		MM_LSU_MUL_PS(&sp[i], vamp);
		MM_LSU_MUL_PS(&sp[i + 4], vamp);
	}
#endif // USE_X86_EXT_INTRIN
	compute_envelope1(env, count);
	for(; i < count; i++){
		sp[i] *= amp;
	}
}

static void init_vfx_envelope_filter(int v, VoiceEffect *vfx)
{
	InfoVFX_Envelope_Filter *info = (InfoVFX_Envelope_Filter *)vfx->info;
	Envelope1 *env = &info->env;
	double delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt;
		
	info->freq = (FLOAT_T)vfx->param[2]; // freq	
	info->reso = (FLOAT_T)vfx->param[3]; // reso
	delay_cnt = 0;
	attack_cnt = (FLOAT_T)vfx->param[4] * playmode_rate_ms;
	hold_cnt = 0;
	decay_cnt = (FLOAT_T)vfx->param[5] * playmode_rate_ms;
	sustain_vol = (FLOAT_T)vfx->param[6] * DIV_100;
	sustain_cnt = MAX_ENV1_LENGTH;
	release_cnt = (FLOAT_T)vfx->param[7] * playmode_rate_ms;
	init_envelope1(env, delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt, SF2_CONVEX, SF2_CONCAVE);
	info->cent = (FLOAT_T)vfx->param[8] * DIV_1200;
	info->vel_freq = POW2(((FLOAT_T)vfx->param[9] * DIV_1200) * (1.0 - (FLOAT_T)voice[v].velocity * DIV_127));
	info->key_freq = pow((FLOAT_T)voice[v].frequency / (FLOAT_T)freq_table[60], (FLOAT_T)vfx->param[10] * DIV_1200);
	info->vel_reso = (FLOAT_T)vfx->param[11] * DIV_10 * (FLOAT_T)voice[v].velocity * DIV_127;

	info->freq = info->freq * info->vel_freq * info->key_freq;
	info->reso = info->reso + info->vel_reso;
	init_sample_filter(&info->fc, info->freq, info->reso, vfx->param[1]);
}

static void noteoff_vfx_envelope_filter(int v, VoiceEffect *vfx)
{	
	InfoVFX_Envelope_Filter *info = (InfoVFX_Envelope_Filter *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_release(env, ENVELOPE_KEEP);
}

static void damper_vfx_envelope_filter(int v, VoiceEffect *vfx, int8 damper)
{	
	InfoVFX_Envelope_Filter *info = (InfoVFX_Envelope_Filter *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_half_damper(env, damper);
}

static inline void do_vfx_envelope_filter(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Envelope_Filter *info = (InfoVFX_Envelope_Filter *)vfx->info;
	int32 i;
	Envelope1 *env = &info->env;	
	FLOAT_T tmp;

	compute_envelope1(env, count);
	set_sample_filter_freq(&info->fc, info->freq * POW2(info->cent * info->env.volume));	
	buffer_filter(&info->fc, sp, count);
}

static void init_vfx_lofi(int v, VoiceEffect *vfx)
{
	InfoVFX_Lofi *info = (InfoVFX_Lofi *)vfx->info;
	
	if(vfx->param[1] < 2)
		vfx->param[1] = 2;
	info->mult = POW2((int)(--vfx->param[1]));
	info->div = 1.0 / info->mult;
}

static inline void do_vfx_lofi(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Lofi *info = (InfoVFX_Lofi *)vfx->info;
	int32 i = 0;

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m256d vmul = _mm256_set1_pd(info->mult);
	__m256d vdiv = _mm256_set1_pd(info->div);
	for(i = 0; i < count2; i += 8){
		__m256d vtmp1 = _mm256_loadu_pd(&sp[i]);
		__m256d vtmp2 = _mm256_loadu_pd(&sp[i + 4]);
		vtmp1 = _mm256_mul_pd(vtmp1, vmul);
		vtmp2 = _mm256_mul_pd(vtmp2, vmul);
		vtmp1 = _mm256_cvtepi32_pd(_mm256_cvtpd_epi32(vtmp1)); 
		vtmp2 = _mm256_cvtepi32_pd(_mm256_cvtpd_epi32(vtmp2)); 
		vtmp1 = _mm256_mul_pd(vtmp1, vdiv);
		vtmp2 = _mm256_mul_pd(vtmp2, vdiv);
		_mm256_storeu_pd(&sp[i], vtmp1);
		_mm256_storeu_pd(&sp[i + 4], vtmp2);
	}
#elif (USE_X86_EXT_INTRIN >= 9) && defined(DATA_T_FLOAT)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m256 vmul = _mm256_set1_ps(info->mult);
	__m256 vdiv = _mm256_set1_ps(info->div);
	for(i = 0; i < count2; i += 8){
		__m256 vtmp1 = _mm256_loadu_ps(&sp[i]);
		vtmp1 = _mm256_mul_ps(vtmp1, vmul);
		vtmp1 = _mm256_cvtepi32_ps(_mm256_cvtps_epi32(vtmp1)); 
		vtmp1 = _mm256_mul_ps(vtmp1, vdiv);
		_mm256_storeu_ps(&sp[i], vtmp1);
	}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m128d vmul = _mm_set1_pd(info->mult);
	__m128d vdiv = _mm_set1_pd(info->div);
	for(i = 0; i < count2; i += 8){
		__m128d vtmp1,vtmp2;
		vtmp1 = _mm_loadu_pd(&sp[i]);
		vtmp2 = _mm_loadu_pd(&sp[i + 2]);
		vtmp1 = _mm_mul_pd(vtmp1, vmul);
		vtmp2 = _mm_mul_pd(vtmp2, vmul);
		vtmp1 = _mm_cvtepi32_pd(_mm_cvtpd_epi32(vtmp1)); 
		vtmp2 = _mm_cvtepi32_pd(_mm_cvtpd_epi32(vtmp2)); 
		vtmp1 = _mm_mul_pd(vtmp1, vdiv);
		vtmp2 = _mm_mul_pd(vtmp2, vdiv);
		_mm_storeu_pd(&sp[i], vtmp1);
		_mm_storeu_pd(&sp[i + 2], vtmp2);
		vtmp1 = _mm_loadu_pd(&sp[i + 4]);
		vtmp2 = _mm_loadu_pd(&sp[i + 6]);
		vtmp1 = _mm_mul_pd(vtmp1, vmul);
		vtmp2 = _mm_mul_pd(vtmp2, vmul);
		vtmp1 = _mm_cvtepi32_pd(_mm_cvtpd_epi32(vtmp1)); 
		vtmp2 = _mm_cvtepi32_pd(_mm_cvtpd_epi32(vtmp2)); 
		vtmp1 = _mm_mul_pd(vtmp1, vdiv);
		vtmp2 = _mm_mul_pd(vtmp2, vdiv);
		_mm_storeu_pd(&sp[i + 4], vtmp1);
		_mm_storeu_pd(&sp[i + 6], vtmp2);
	}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m128 vmul = _mm_set1_ps(info->mult);
	__m128 vdiv = _mm_set1_ps(info->div);
	for(i = 0; i < count2; i += 8){
		__m128 vtmp1 = _mm_loadu_ps(&sp[i]);
		__m128 vtmp2 = _mm_loadu_ps(&sp[i + 4]);
		vtmp1 = _mm_mul_ps(vtmp1, vmul);
		vtmp2 = _mm_mul_ps(vtmp2, vmul);
		vtmp1 = _mm_cvtepi32_ps(_mm_cvtps_epi32(vtmp1)); 
		vtmp2 = _mm_cvtepi32_ps(_mm_cvtps_epi32(vtmp2)); 
		vtmp1 = _mm_mul_ps(vtmp1, vdiv);
		vtmp2 = _mm_mul_ps(vtmp2, vdiv);
		_mm_storeu_ps(&sp[i], vtmp1);
		_mm_storeu_ps(&sp[i + 4], vtmp2);
	}
#endif // USE_X86_EXT_INTRIN
	for(; i < count; i++) {
		sp[i] = (FLOAT_T)((int)(sp[i] * info->mult)) * info->div;
	}
}

static void init_vfx_down_sample(int v, VoiceEffect *vfx)
{
	InfoVFX_DownSample *info = (InfoVFX_DownSample *)vfx->info;
	int32 i, ratio;
	
	ratio = (FLOAT_T)play_mode->rate / 44100;
	if(ratio <= 1){
		info->ratio = 0; // off
		return;
	}else if(ratio > 16)
		info->ratio = 16;
	else
		info->ratio = ratio;
	info->div_ratio = 1.0 / (FLOAT_T)info->ratio;
	info->index = 0;	
	for (i = 0; i < 16; i++)
		info->buf[i] = 0;
}

static inline void do_vfx_down_sample(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_DownSample *info = (InfoVFX_DownSample *)vfx->info;
	int32 i;
	FLOAT_T out = 0;

	if(!info->ratio)
		return;
	for(i = 0; i < count; i++) {
		info->buf[info->index] = sp[i];
		for (i = 0; i < info->ratio; i++){
			int32 index = info->index - i;
			if(index < 0)
				index += 16;
			out += info->buf[index];
		}
		sp[i] = out * info->div_ratio;
		if(++info->index >= 16)
			info->index -= 16;
	}

}

#define VFX_CHORUS_FEEDBACK 0.6
#define VFX_CHORUS_LEVEL 2.5

static void init_vfx_chorus(int v, VoiceEffect *vfx)
{
	InfoVFX_Chorus *info = (InfoVFX_Chorus *)vfx->info;
	int32 i;
	FLOAT_T div_phase;
	
	if(vfx->param[2] < 1 || vfx->param[2] > 4000)
		vfx->param[2] = 100; // param[2] = rate [0.01Hz]
	if(vfx->param[3] < 1 || vfx->param[3] > 1000)
		vfx->param[3] = 100; // param[3] = depth [0.01ms]	
	if(vfx->param[5] < 1 || vfx->param[5] > VFX_CHORUS_PHASE_MAX)
		vfx->param[5] = 3; // param[5] = phase 1~8
	info->delay_size = VFX_CHORUS_BUFFER_SIZE;
	info->delay_count = 0;	
	info->phase = vfx->param[5]; // param[6] = phase 1~8
	div_phase = 1.0 / (FLOAT_T)info->phase; // phase > 0
	info->div_out = pow((FLOAT_T)div_phase, 0.666666666);	
	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = chorus level %
	info->dry = 1.0 - info->wet;
	info->wet *= VFX_CHORUS_LEVEL;
	info->lfo_rate = (FLOAT_T)vfx->param[2] * DIV_100 * div_playmode_rate; // param[2] = rate 0.01Hz
	info->depthc = (FLOAT_T)vfx->param[3] * DIV_100 * playmode_rate_ms; // param[3] = depth 0.01ms = pp_max -depth		
	info->delayc = 0; // must pre_delay > depth
	info->feedback = (FLOAT_T)vfx->param[4] * DIV_100 * VFX_CHORUS_FEEDBACK; // param[5] = feedback level %
	if(info->phase == 1)
		info->lfo_phase[0] = 0;
	else for (i = 0; i < info->phase; i++)
		info->lfo_phase[i] = (FLOAT_T)i * div_phase;
	info->hist = 0;
}

static inline void do_vfx_chorus(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Chorus *info = (InfoVFX_Chorus *)vfx->info;
	int32 i, j;
	int32 phase = info->phase, size = info->delay_size, delay_count = info->delay_count;
	FLOAT_T feedback = info->feedback, div_out = info->div_out, wet = info->wet, dry = info->dry;
	FLOAT_T delayc = info->delayc, depthc = info->depthc, hist = info->hist, lfo_count = info->lfo_count, lfo_rate = info->lfo_rate;
	FLOAT_T *ebuf = info->buf, *lfo_phase = info->lfo_phase;
	FLOAT_T input, output;
	FLOAT_T sub_count;
	
	for (i = 0; i < count; i++) {
		if(!delay_count){ ebuf[VFX_CHORUS_BUFFER_SIZE] = ebuf[0];} // for linear interpolation
		if(++delay_count >= VFX_CHORUS_BUFFER_SIZE) {delay_count -= VFX_CHORUS_BUFFER_SIZE;	}
		if((lfo_count += lfo_rate) >= 1.0) {lfo_count -= 1.0;}
		sub_count = (FLOAT_T)delay_count - delayc;
		input = sp[i];
		ebuf[delay_count] = input + hist * feedback;
		output = 0;
		for (j = 0; j < phase; j++) {
			int32 index;
			DATA_T v1, v2;
			FLOAT_T fp1, fp2;
			fp1 = (FLOAT_T)sub_count - depthc * lookup2_sine_p((lfo_count + lfo_phase[j]));
			if(fp1 < 0) {fp1 += VFX_CHORUS_BUFFER_SIZE;}
			fp2 = floor(fp1);
			index = fp2;
			v1 = ebuf[index]; v2 = ebuf[index + 1];
			output += v1 + (v2 - v1) * (fp1 - fp2); // linear interpolation
		}
		hist = output * div_out;
		sp[i] = hist * wet + sp[i] * dry;
	}
	info->delay_count = delay_count, info->lfo_count = lfo_count, info->hist = hist;
}


////// ALLPASS

static void init_vfx_allpass(int v, VoiceEffect *vfx)
{
	InfoVFX_Allpass *info = (InfoVFX_Allpass *)vfx->info;
	int i;
	FLOAT_T dt, fb;

	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = wet
	info->dry = 1.0 - info->wet; // dry	
	info->wet *= (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = ap level
	if(vfx->param[3] > VFX_ALLPASS_NUM_MAX)
		vfx->param[3] = VFX_ALLPASS_NUM_MAX;
	else if(vfx->param[3] < 1)
		vfx->param[3] = 1;
	info->ap_num = vfx->param[3]; // param[2] = ap num	
//	info->wet *= pow(0.90, log((double)info->ap_num)); // ap extgain	
	if(vfx->param[4] > 200)
		vfx->param[4] = 200; // max 200ms
	else if(vfx->param[4] < 1)
		vfx->param[4] = 1; // min 1ms
	dt = (FLOAT_T)vfx->param[4]; // param[4] = feedback delay time ms
	if(vfx->param[5] > 100)
		vfx->param[5] = 100;
	else if(vfx->param[5] < 0)
		vfx->param[5] = 0;
	fb = (FLOAT_T)vfx->param[5] * DIV_100; // param[5] = feedback delay level %
	for(i = 0; i < info->ap_num; i++){
		init_allpass(&info->ap[i], dt, fb);		
		dt *= 1.2599210498948731647672106072782;
		fb *= 0.79370052598409973737585281963615;
	}
}

static inline void do_vfx_allpass(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	Voice *vp = voice + v;
	InfoVFX_Allpass *info = (InfoVFX_Allpass *)vfx->info;
	int32 i, j;
	FLOAT_T tmp;
	
	for (i = 0; i < count; i++){
		tmp = sp[i];
		for (j = 0; j < info->ap_num; j++)	
			do_allpass(&info->ap[j], &tmp);
		sp[i] = tmp * info->wet + sp[i] * info->dry;
	}
}


////// COMB

static void init_vfx_comb(int v, VoiceEffect *vfx)
{
	InfoVFX_Comb *info = (InfoVFX_Comb *)vfx->info;
	FLOAT_T dt, fb, damp1, damp2, freq, reso;
	int i, type;

	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = wet
	info->dry = 1.0 - info->wet;
	info->wet *= (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = comb level %	
	if(vfx->param[3] > VFX_COMB_NUM_MAX)
		vfx->param[3] = VFX_COMB_NUM_MAX;
	else if(vfx->param[3] < 1)
		vfx->param[3] = 1;
	info->comb_num = vfx->param[3]; // param[3] = comb num
//	info->wet *= pow(0.5, log((double)info->comb_num)); // comb extgain	
	if(vfx->param[4] > 200)
		vfx->param[4] = 200; // max 200ms
	else if(vfx->param[4] < 1)
		vfx->param[4] = 1; // min 1ms
	dt = (FLOAT_T)vfx->param[4]; // param[4] = feedback delay time ms
	if(vfx->param[5] > 100)
		vfx->param[5] = 100;
	else if(vfx->param[5] < 0)
		vfx->param[5] = 0;
	fb = (FLOAT_T)vfx->param[5] * DIV_100; // param[5] = feedback
	freq = vfx->param[6];
	reso = vfx->param[7];
	type = vfx->param[8];
	for(i = 0; i < info->comb_num; i++){
		init_comb2(&info->comb[i], dt, fb, freq, reso, type);
		dt *= 1.2599210498948731647672106072782;
		fb *= 0.79370052598409973737585281963615;
	}
}

static inline void do_vfx_comb(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	Voice *vp = voice + v;
	InfoVFX_Comb *info = (InfoVFX_Comb *)vfx->info;
	int32 i, j;
	FLOAT_T in, out;
	
	for (i = 0; i < count; i++){
		in = sp[i];
		out = 0;
		for(j = 0; j < info->comb_num; j++)	
			do_comb2(&info->comb[j], &in, &out);
		sp[i] = out * info->wet + in * info->dry;
	}
}


////// reverb2

static void init_vfx_reverb2(int v, VoiceEffect *vfx)
{
	InfoVFX_Reverb2 *info = (InfoVFX_Reverb2 *)vfx->info;
	FLOAT_T dt1, fb1, dt2, fb2, damp1, damp2, freq, reso;
	int i, type;

	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = wet
	info->dry = 1.0 - info->wet;
	info->wet *= (FLOAT_T)vfx->param[2] * DIV_100; // param[2] = rev level %	
	if(vfx->param[3] < 1)
		vfx->param[3] = 1;
	else if(vfx->param[3] > VFX_COMB_NUM_MAX)
		vfx->param[3] = VFX_COMB_NUM_MAX;
	info->flt_num1 = vfx->param[3]; // param[3] = rev num
	if(vfx->param[3] > VFX_ALLPASS_NUM_MAX)
		vfx->param[3] = VFX_ALLPASS_NUM_MAX;
	info->flt_num2 = vfx->param[3]; // param[3] = rev num
//	info->wet *= pow(0.90, log((double)info->flt_num)); // ap extgain	
//	info->wet *= pow(0.5, log((double)info->flt_num)); // comb extgain	
	if(vfx->param[4] > 200)
		vfx->param[4] = 200; // max 200ms
	else if(vfx->param[4] < 1)
		vfx->param[4] = 1; // min 1ms
	dt1 = dt2 = (FLOAT_T)vfx->param[4]; // param[4] = filter delay time ms
	if(vfx->param[5] > 100)
		vfx->param[5] = 100;
	else if(vfx->param[5] < 0)
		vfx->param[5] = 0;
	fb1 = fb2 = (FLOAT_T)vfx->param[5] * DIV_100; // param[5] = filter feedback
	freq = vfx->param[6];
	reso = vfx->param[7];
	type = vfx->param[8];
	for(i = 0; i < info->flt_num1; i++){
	//	init_bwcomb(&info->comb[i], dt1, fb1, freq, reso);
		init_comb2(&info->comb[i], dt1, fb1, freq, reso, type);
		dt1 *= 1.1224620483093729814335330496791 * 1.1224620483093729814335330496791;
		fb1 *= 0.89089871814033930474022620559043 * 0.89089871814033930474022620559043;
	}
	for(i = 0; i < info->flt_num2; i++){
		dt2 *= 1.1224620483093729814335330496791;
		fb2 *= 0.89089871814033930474022620559043;
		init_allpass(&info->ap[i], dt2, fb2);
		dt2 *= 1.1224620483093729814335330496791;
		fb2 *= 0.89089871814033930474022620559043;
	}
}

static inline void do_vfx_reverb2(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	Voice *vp = voice + v;
	InfoVFX_Reverb2 *info = (InfoVFX_Reverb2 *)vfx->info;
	int32 i, j;
	FLOAT_T in, out;
	
	for (i = 0; i < count; i++){
		in = sp[i];
		out = 0;
		for(j = 0; j < info->flt_num1; j++)
			do_comb2(&info->comb[j], &in, &out);
		for(j = 0; j < info->flt_num2; j++)
			do_allpass(&info->ap[j], &out);
		sp[i] = out * info->wet + in * info->dry;
	}
}


////// Ring modulator

static void init_vfx_ring_modulator(int v, VoiceEffect *vfx)
{
	InfoVFX_RingModulator *info = (InfoVFX_RingModulator *)vfx->info;
	Envelope1 *env = &info->env;
	double delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt;
	int num = 1, tmpi;
	
	
	info->wet = (FLOAT_T)vfx->param[num++] * DIV_100; // osc wet
	info->dry = 1.0 - info->wet; // dry	

	tmpi = vfx->param[num++]; // mod level	
	if(tmpi < 1)
		tmpi = 1;	
	info->mod_level = (FLOAT_T)tmpi * DIV_100;
		
	switch(vfx->param[num++]){ // mode 0:OSC(cent) 1:OSC(cHz)
	default:
	case 0:
		info->mode = 0;
		tmpi = vfx->param[num++]; // osc1= freq / mlt
		info->freq_mlt = POW2((FLOAT_T)tmpi * DIV_1200); // cent to mlt
		info->freq = (FLOAT_T)voice[v].frequency * DIV_1000 * info->freq_mlt;
		break;
	case 1:
		info->mode = 1;
		tmpi = vfx->param[num++]; // osc1= freq / mlt
		if(tmpi < 1)
			tmpi = 1;
		info->freq_mlt = (FLOAT_T)tmpi * DIV_100; // cHz to Hz
		info->freq = info->freq_mlt;
		break;
	}
	tmpi = vfx->param[num++]; // osc_wave_type	
	info->osc_ptr = get_osc_ptr(check_osc_wave_type(tmpi, OSC_TYPE_SQUARE));
	tmpi = vfx->param[num++]; // osc wave_width %
	if(tmpi < 0)
		tmpi = 0;
	else if(tmpi > 200)
		tmpi = 200;
	info->wave_width1 = (FLOAT_T)tmpi * DIV_200;	
	if(info->wave_width1 >= 1.0){
		info->wave_width1 = 1.0;
		info->rate_width1 = 0.5;
		info->rate_width2 = 0.0;
	}else if(info->wave_width1 <= 0.0){
		info->wave_width1 = 0.0;
		info->rate_width1 = 0.0;
		info->rate_width2 = 0.5;
	}else{
		info->rate_width1 = 0.5 / (info->wave_width1);
		info->rate_width2 = 0.5 / (1.0 - info->wave_width1);
	}
	info->rate = 0;	

	delay_cnt = 0;
	attack_cnt = (FLOAT_T)vfx->param[num++] * playmode_rate_ms;
	hold_cnt = 0;
	decay_cnt = (FLOAT_T)vfx->param[num++] * playmode_rate_ms;
	sustain_vol = (FLOAT_T)vfx->param[num++] * DIV_100;
	sustain_cnt = MAX_ENV1_LENGTH;
	release_cnt = (FLOAT_T)vfx->param[num++] * playmode_rate_ms;
	init_envelope1(env, delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt, LINEAR_CURVE, LINEAR_CURVE);
}

static void noteoff_vfx_ring_modulator(int v, VoiceEffect *vfx)
{	
	InfoVFX_RingModulator *info = (InfoVFX_RingModulator *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_release(env, ENVELOPE_KEEP);
}

static void damper_vfx_ring_modulator(int v, VoiceEffect *vfx, int8 damper)
{	
	InfoVFX_RingModulator *info = (InfoVFX_RingModulator *)vfx->info;
	Envelope1 *env = &info->env;

	reset_envelope1_half_damper(env, damper);
}

static inline void do_vfx_ring_modulator(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_RingModulator *info = (InfoVFX_RingModulator *)vfx->info;
	int32 i;
	Envelope1 *env = &info->env;
	FLOAT_T freq, osc;
	
	compute_envelope1(env, count);
	if(!info->mode)
		freq = (FLOAT_T)voice[v].frequency * DIV_1000 * info->freq_mlt;
	else if(voice[v].pitchfactor)
		freq = (FLOAT_T)info->freq * voice[v].pitchfactor;
	else 
		freq = (FLOAT_T)info->freq;
	for (i = 0; i < count; i++)
	{
		FLOAT_T tmp = sp[i];
		info->rate += freq * div_playmode_rate; // +1/sr = 1Hz
		info->rate -= floor(info->rate); // reset count
		if(info->rate < info->wave_width1)
			osc = info->osc_ptr(info->rate * info->rate_width1);
		else
			osc = info->osc_ptr(0.5 + (info->rate - info->wave_width1) * info->rate_width2);
		tmp *= 1.0 - (osc * DIV_2 + 0.5) * info->env.volume * info->mod_level;
//		tmp *= 1.0 + (osc * info->env.volume - 1.0) * info->mod_level;
		sp[i] = tmp * info->wet + sp[i] * info->dry; // mix level
//		osc *= info->env.volume * (1.0 - ((FLOAT_T)sp[i] * DIV_2 + 0.5) * info->mod_level);
//		sp[i] = osc * info->wet + sp[i] * info->dry; // mod level
	}
}




////// WAH

#define VFX_WAH_LEVEL (4.0)

static void init_vfx_wah(int v, VoiceEffect *vfx)
{
	InfoVFX_Wah *info = (InfoVFX_Wah *)vfx->info;
	FLOAT_T div_phase, reso;
	int i;
	
	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = chorus level %
	info->dry = 1.0 - info->wet;	
	info->freq = vfx->param[2]; // param[2] = freq [Hz]		
	reso = vfx->param[3]; // param[3] = reso [dB]	
	if(vfx->param[4] < 1 || vfx->param[4] > VFX_WAH_PHASE_MAX)
		vfx->param[4] = 1;
	if(vfx->param[5] < 0){
		vfx->param[5] = 0; // off
		vfx->param[4] = 1; // 1 phase
	}else if(vfx->param[5] > 4000)
		vfx->param[5] = 4000;		
	info->phase = vfx->param[4]; // param[4] = phase 1~8
	div_phase = 1.0 / (FLOAT_T)info->phase; // phase > 0
	info->wet *= VFX_WAH_LEVEL * pow((FLOAT_T)div_phase, 0.666666666);	
	info->lfo_rate = (FLOAT_T)vfx->param[5] * DIV_100 * div_playmode_rate; // param[5] = rate [0.01Hz]
	info->lfo_count = 0; // min point
	info->multi = vfx->param[6] * DIV_1200;	 // param[6] = cent
	for(i = 0; i < info->phase; i++){
		info->lfo_phase[i] = (FLOAT_T)i * div_phase;
		init_sample_filter(&info->fc[i], info->freq, reso, FILTER_BPF12_3);
	}
}

static inline void do_vfx_wah(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	Voice *vp = voice + v;
	InfoVFX_Wah *info = (InfoVFX_Wah *)vfx->info;
	int32 i, k;
	DATA_T tmp;

	if(info->lfo_rate){
		if ((info->lfo_count += (info->lfo_rate * count)) >= 1.0) {info->lfo_count -= 1.0;}
		for(k = 0; k < info->phase; k++){
			set_sample_filter_freq(&info->fc[k], info->freq * POW2(info->multi * lookup2_sine_linear(info->lfo_count + info->lfo_phase[k])));
			recalc_filter(&info->fc[k]);
		}
	}
	for(i = 0; i < count; i++) {
		FLOAT_T sum = 0;
		for(k = 0; k < info->phase; k++){
			tmp = sp[i];
			sample_filter(&info->fc[k], &tmp);
			sum += tmp;	
		}
		sp[i] = sum * info->wet + sp[i] * info->dry;
	}
}



////// TREMOLO

static void init_vfx_tremolo(int v, VoiceEffect *vfx)
{
	InfoVFX_Tremolo *info = (InfoVFX_Tremolo *)vfx->info;
	FLOAT_T div_phase, reso;
	int i;
		
	if(vfx->param[1] < 1)
		vfx->param[1] = 1;
	info->mod_level = (FLOAT_T)vfx->param[1] * DIV_100; // tremolo depth
	if(vfx->param[2] < 1)
		vfx->param[2] = 1; // off
	else if(vfx->param[2] > 4000)
		vfx->param[2] = 4000; // param[3] = rate [0.01Hz]	
	info->lfo_rate = (FLOAT_T)vfx->param[2] * div_playmode_rate; // +1/sr = 1Hz
	info->lfo_count = 0;		
	info->osc_ptr = get_osc_ptr(check_osc_wave_type(vfx->param[3], OSC_TYPE_SQUARE));	 // osc_wave_type
}

static inline void do_vfx_tremolo(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	Voice *vp = voice + v;
	InfoVFX_Tremolo *info = (InfoVFX_Tremolo *)vfx->info;
	int32 i = 0;
	FLOAT_T osc;
	
	info->lfo_count += info->lfo_rate * count; // +1/sr = 1Hz
	if(info->lfo_count >= 1.0)
		info->lfo_count -= floor(info->lfo_count); // reset count
	osc = info->osc_ptr(info->lfo_count);
	osc = 1.0 - (osc * DIV_2 + 0.5) * info->mod_level;
	
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
	{
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m256d vamp = _mm256_set1_pd(osc);
	for(i = 0; i < count2; i += 8){
		MM256_LSU_MUL_PD(&sp[i], vamp);
		MM256_LSU_MUL_PD(&sp[i + 4], vamp);
	}
	}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
	{
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m256 vamp = _mm256_set1_ps(osc);
	for(i = 0; i < count2; i += 8){
		MM256_LSU_MUL_PS(&sp[i], vamp);
	}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
	{
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m128d vamp = _mm_set1_pd(osc);
	for(i = 0; i < count2; i += 8){
		MM_LSU_MUL_PD(&sp[i], vamp);
		MM_LSU_MUL_PD(&sp[i + 2], vamp);
		MM_LSU_MUL_PD(&sp[i + 4], vamp);
		MM_LSU_MUL_PD(&sp[i + 6], vamp);
	}
	}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
	{
	const int32 req_count_mask = ~(0x7);
	int32 count2 = count & req_count_mask;
	__m128 vamp = _mm_set1_ps(osc);
	for(i = 0; i < count2; i += 8){
		MM_LSU_MUL_PS(&sp[i], vamp);
		MM_LSU_MUL_PS(&sp[i + 4], vamp);
	}
	}
#endif // USE_X86_EXT_INTRIN

	for (; i < count; i++)
		sp[i] *= osc;
}



////// SYMPHONIC
#define VFX_RND ((double)((int32)(rand() & 0x7FFF) - M_14BIT) * DIV_14BIT) // -1.0~1.0
#define VFX_SYMPHONIC_FEEDBACK 0.6
#define VFX_SYMPHONIC_LEVEL 1.25

static void init_vfx_symphonic(int v, VoiceEffect *vfx)
{
	InfoVFX_Symphonic *info = (InfoVFX_Symphonic *)vfx->info;
	FLOAT_T div_phase, depthc_def, rate_def, delayc_def;
	int i;

	if(vfx->param[2] < 1 || vfx->param[2] > 4000)
		vfx->param[2] = 100; // param[2] = rate [0.01Hz]
	if(vfx->param[3] < 1 || vfx->param[3] > 1000)
		vfx->param[3] = 100; // param[3] = depth [0.01ms]	
	if(vfx->param[5] < 1 || vfx->param[5] > VFX_SYMPHONIC_PHASE_MAX)
		vfx->param[5] = 3; // param[5] = phase 1~8
	info->phase = vfx->param[5]; // param[6] = phase 1~8
	div_phase = 1.0 / (FLOAT_T)info->phase; // phase > 0
	info->div_out = pow((FLOAT_T)div_phase, 0.666666666);
	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = chorus level %
	info->dry = 1.0 - info->wet;
	info->wet *= VFX_SYMPHONIC_LEVEL;
	rate_def = (FLOAT_T)vfx->param[2] * DIV_100 * div_playmode_rate; // param[2] = rate 0.01Hz
	depthc_def = (FLOAT_T)vfx->param[3] * DIV_100 * playmode_rate_ms; // param[3] = depth 0.01ms = pp_max , -depth
	delayc_def = 6.0 * playmode_rate_ms; // delay_max 26ms - depth_max 
	for(i = 0; i < info->phase; i++) {
		info->lfo_count[i] = (FLOAT_T)i * div_phase; // lfo offset
		info->lfo_rate[i] = rate_def * pow(1.5, VFX_RND); // lfo rate
		info->depthc[i] = depthc_def * pow(1.5, VFX_RND); // depth
		info->delayc[i] = delayc_def * pow(1.5, VFX_RND); // delay
	}	
	info->feedback = (FLOAT_T)vfx->param[4] * DIV_100 * VFX_SYMPHONIC_FEEDBACK; // param[4] = feedback level %
	info->delay_size = VFX_SYMPHONIC_BUFFER_SIZE;
	info->delay_count = 0;	
	info->hist = 0;
}

static inline void do_vfx_symphonic(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	Voice *vp = voice + v;
	InfoVFX_Symphonic *info = (InfoVFX_Symphonic *)vfx->info;
	int32 i, j;
	int32 phase = info->phase, size = info->delay_size, delay_count = info->delay_count;
	FLOAT_T feedback = info->feedback, div_out = info->div_out, wet = info->wet, dry = info->dry;
	FLOAT_T hist = info->hist;
	FLOAT_T *ebuf = info->buf;
	FLOAT_T input, output;
	
	for (i = 0; i < count; i++) {
		if(!delay_count) {ebuf[VFX_SYMPHONIC_BUFFER_SIZE] = ebuf[0];} // for linear interpolation
		if(++delay_count >= VFX_SYMPHONIC_BUFFER_SIZE){delay_count -= VFX_SYMPHONIC_BUFFER_SIZE;}
		input = sp[i];
		ebuf[delay_count] = input + hist * feedback;
		output = 0;
		for (j = 0; j < phase; j++) {
			int32 index;
			DATA_T v1, v2;
			FLOAT_T fp1, fp2;			
			if((info->lfo_count[j] += info->lfo_rate[j]) >= 1.0) {info->lfo_count[j] -= 1.0;}
			fp1 = (FLOAT_T)delay_count - info->delayc[j] - info->depthc[j] * lookup2_sine_p(info->lfo_count[j]);
			if(fp1 < 0) {fp1 += VFX_SYMPHONIC_BUFFER_SIZE;}
			fp2 = floor(fp1);
			index = fp2;
			v1 = ebuf[index]; v2 = ebuf[index + 1];
			output += v1 + (v2 - v1) * (fp1 - fp2); // linear interpolation
		}
		hist = output * div_out;
		sp[i] = hist * wet + sp[i] * dry;
	}
	info->delay_count = delay_count, info->hist = hist;
}


//// PHASER
#define VFX_PHASER_FEEDBACK 0.6
#define VFC_PHASER_LEVEL (4.0)

static void init_vfx_phaser(int v, VoiceEffect *vfx)
{
	InfoVFX_Phaser *info = (InfoVFX_Phaser *)vfx->info;
	FLOAT_T div_phase, reso;
	int i;

	if(vfx->param[2] < 1 || vfx->param[2] > 4000)
		vfx->param[2] = 100; // param[2] = rate [0.01Hz]
	if(vfx->param[3] < 1 || vfx->param[3] > 1000)
		vfx->param[3] = 100; // param[3] = depth [0.01ms]	
	if(vfx->param[5] < 1 || vfx->param[5] > VFX_PHASER_PHASE_MAX)
		vfx->param[5] = 3; // param[5] = phase 1~8
	info->delay_size = VFX_PHASER_BUFFER_SIZE;
	info->delay_count = 0;	
	info->phase = vfx->param[5]; // param[6] = phase 1~8
	div_phase = 1.0 / (FLOAT_T)info->phase; // phase > 0
	info->div_out = pow((FLOAT_T)div_phase, 0.666666666);	
	info->wet = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = chorus level %
	info->dry = 1.0 - info->wet;
	info->wet *= VFC_PHASER_LEVEL;
	info->lfo_rate = (FLOAT_T)vfx->param[2] * DIV_100 * div_playmode_rate; // param[2] = rate 0.01Hz
	info->depthc = (FLOAT_T)vfx->param[3] * DIV_100 * playmode_rate_ms; // param[3] = depth 0.01ms = pp_max -depth		
	info->delayc = 0; // 
	info->feedback = (FLOAT_T)vfx->param[4] * DIV_100 * VFX_PHASER_FEEDBACK; // param[4] = feedback level %
	if(info->phase == 1)
		info->lfo_phase[0] = 0;
	else for (i = 0; i < info->phase; i++)
		info->lfo_phase[i] = (FLOAT_T)i * div_phase;
	init_sample_filter(&info->fc, vfx->param[6], vfx->param[7], FILTER_BPF12_3);
	info->hist = 0;
}

static inline void do_vfx_phaser(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Phaser *info = (InfoVFX_Phaser *)vfx->info;
	int32 i, j;
	int32 phase = info->phase, size = info->delay_size, delay_count = info->delay_count ;
	FLOAT_T feedback = info->feedback, div_out = info->div_out, wet = info->wet, dry = info->dry;
	FLOAT_T delayc = info->delayc, depthc = info->depthc, lfo_count = info->lfo_count, lfo_rate = info->lfo_rate;
	FLOAT_T *lfo_phase = info->lfo_phase;
	FLOAT_T *ebuf = info->buf ,input, output;
	FLOAT_T sub_count;
	DATA_T hist = info->hist;

	for (i = 0; i < count; i++) {
		if(!delay_count){ebuf[VFX_PHASER_BUFFER_SIZE] = ebuf[0];} // for linear interpolation
		if(++delay_count >= VFX_PHASER_BUFFER_SIZE){delay_count -= VFX_PHASER_BUFFER_SIZE;}
		if((lfo_count += lfo_rate) >= 1.0) {lfo_count -= 1.0;}
		sub_count = (FLOAT_T)delay_count - delayc;
		input = sp[i];
		ebuf[delay_count] = input + hist * feedback;
		output = 0;
		for (j = 0; j < phase; j++) {
			int32 index;
			DATA_T v1, v2;
			FLOAT_T fp1, fp2;
			if((lfo_count += lfo_rate) >= 1.0) {lfo_count -= 1.0;}
			fp1 = (FLOAT_T)sub_count - depthc * lookup2_sine_p((lfo_count + lfo_phase[j]));
			if(fp1 < 0) {fp1 += VFX_PHASER_BUFFER_SIZE;}
			fp2 = floor(fp1);
			index = fp2;
			v1 = ebuf[index]; v2 = ebuf[index + 1];
			output += v1 + (v2 - v1) * (fp1 - fp2); // linear interpolation
		}
		output *= div_out;
		hist = output - input;
		sp[i] = hist * wet + sp[i] * dry;
		sample_filter(&info->fc, &hist);
	}
	info->delay_count = delay_count, info->lfo_count = lfo_count, info->hist = hist;
}


////// ENHANCER

static void init_vfx_enhancer(int v, VoiceEffect *vfx)
{
	InfoVFX_Enhancer *info = (InfoVFX_Enhancer *)vfx->info;
	FLOAT_T div_phase;
	int i;

	if(vfx->param[1] < 0)
		vfx->param[1] = 100;
	if(vfx->param[2] < 0)
		vfx->param[2] = 0;
	else if(vfx->param[2] > 100)
		vfx->param[2] = 100;
	if(vfx->param[3] < 400)
		vfx->param[3] = 400;	
	info->mix = (FLOAT_T)vfx->param[1] * DIV_100; // param[1] = mix level %
	info->feedback = 0.5 + 0.375 * (FLOAT_T)vfx->param[2] * DIV_100;  // param[2] = thres [%] 0.5~0.875
	info->delay_size = 4.0 * playmode_rate_ms; // 1/250Hz
	info->delay_count = 0;
	init_sample_filter(&info->fc, vfx->param[3], 0, FILTER_HPF_BW); // param[3] = freq [Hz]
	init_sample_filter2(&info->eq, vfx->param[3] * 2.0, vfx->param[2] * DIV_100 * 12.0, 0.5, FILTER_SHELVING_HI); // param[3] = freq [Hz] // vfx->param[2] ~6dB
}

static inline void do_vfx_enhancer(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Enhancer *info = (InfoVFX_Enhancer *)vfx->info;
	int32 i, j;
	int32 delay_count = info->delay_count, delay_size = info->delay_size;
	DATA_T input, output;
	
	for (i = 0; i < count; i++) {
		input = sp[i] + info->buf[delay_count] * info->feedback;
		sample_filter(&info->fc, &input);
		info->buf[delay_count] = input;	
		sample_filter(&info->eq, &input);
		sp[i] += input * info->mix;		
		if(++delay_count >= delay_size){
			delay_count = 0;
		}
	}
	info->delay_count = delay_count;
}




////// TEST

#define VFC_TEST_LEVEL (6.0)

static void init_vfx_test(int v, VoiceEffect *vfx)
{
	InfoVFX_Test *info = (InfoVFX_Test *)vfx->info;
	FLOAT_T div_phase, reso;
	int i;
	//FilterCoefficients *fc = &info->fc;
	//FLOAT_T *dc = fc->dc, f, q ,p, r;
	//
	//vfx->param[1] = 100;
	//vfx->param[2] = 6;
	//vfx->param[3] = 3400; 
	//vfx->param[4] = 100;
	//vfx->param[5] = 3;
	//vfx->param[6] = 3400;
	//vfx->param[7] = 12;
	//
	//fc->freq = 800;
	//fc->reso_dB = 96;

	//	f = fc->freq * div_playmode_rate;
	//	r = 1.0 - 1.0 / pow(10.0, fc->reso_dB * DIV_40);
	//	q = SQRT_2 - r * SQRT_2; // q>0.1
	//	p = 1.0 / tan(f * M_PI); // ?
	//	dc[0] = 1.0 / ( 1.0 + q * p + p * p);
	//	dc[1] = 2.0 * dc[0];
	//	dc[2] = dc[0];
	//	dc[3] = -2.0 * ( 1.0 - p * p) * dc[0]; // -
	//	dc[4] = -(1.0 - q * p + p * p) * dc[0]; // -


	//	info->freq = 5.0 * div_playmode_rate;
	//	info->rate = 0;

	info->feedback = 0.9;

	info->delay1 = 10 * playmode_rate_ms;
	info->delay2 = info->delay1 * pow(1.5, VFX_RND);
	info->size = VFX_TEST_BUFFER_SIZE;
	info->delay_count = 0;

	info->freq = 2.0 * div_playmode_rate * pow(1.5, VFX_RND);
	info->rate = 0;

}

static void uninit_vfx_test(int v, VoiceEffect *vfx)
{
	InfoVFX_Test *info = (InfoVFX_Test *)vfx->info;
	int i;

}

#define VFX_TEST_MAX 1.0

static inline void do_vfx_test(int v, VoiceEffect *vfx, DATA_T *sp, int32 count)
{
	InfoVFX_Test *info = (InfoVFX_Test *)vfx->info;
	int32 i, j;
//	int32 delay_count = info->delay_count, delayc = info->delayc;
//	DATA_T input, output;
//	FilterCoefficients *fc = &info->fc;
//	FLOAT_T *db = fc->db, *dc = fc->dc, f, q ,p, r;
	

/*	fc->freq = 1000 * pow(4.0, sin((info->rate - 0.25) * M_PI2));
	fc->reso_dB = 48;*/	
	// bw
	//f = fc->freq * div_playmode_rate;
	//r = 1.0 - 1.0 / pow(10.0, fc->reso_dB * DIV_40);
	//q = SQRT_2 - r * SQRT_2; // q>0.1
	//p = 1.0 / tan(f * M_PI); // ?
	//dc[0] = 1.0 / ( 1.0 + q * p + p * p);
	//dc[1] = 2.0 * dc[0];
	//dc[2] = dc[0];
	//dc[3] = -2.0 * ( 1.0 - p * p) * dc[0]; // -
	//dc[4] = -(1.0 - q * p + p * p) * dc[0]; // -
	// lpf12-2
	//f = 2.0 * M_PI * fc->freq * div_playmode_rate;
	//q = 1.0 - f / (2.0 * (pow(10.0, fc->reso_dB * DIV_40) + 0.5 / (1.0 + f)) + f - 2.0);
	//dc[0] = q * q;
	//dc[1] = dc[0] + 1.0 - 2.0 * cos(f) * q;
	// 24-2
	//f = tan(M_PI * fc->freq * div_playmode_rate); // cutoff freq rate/2
	//q = 2.0 / pow(10.0, fc->reso_dB * DIV_40);
	//r = f * f;
	//p = 1 + (q * f) + r;
	//dc[0] = r / p;
	//dc[1] = dc[0] * 2;
	//dc[2] = r / p;
	//dc[3] = 2 * (r - 1) / (-p);
	//dc[4] = (1 - (q * f) + r) / (-p);
	// tfo
	//dc[0] = 2 * fc->freq * div_playmode_rate;
	//q = 1.0 - 1.0 / pow(10.0, fc->reso_dB * DIV_40 * 2.0);
	//dc[1] = q + q / (1.01 - dc[0]);
	
	//if(info->rate < 0.5)
	//	info->freq = 5.0 * div_playmode_rate;
	//else
	//	info->freq = 0.25 * div_playmode_rate;
	//if((info->rate += info->freq * count) >= 1.0)
	//	info->rate -= 1.0;

	for (i = 0; i < count; i++) {
		FLOAT_T cf;
		DATA_T input = sp[i], output;
		// read index
		int32 index1 = info->delay_count - info->delay1;
		int32 index2 = info->delay_count - info->delay2;
		if(index1 < 0)
			index1 += info->size;
		if(index2 < 0)
			index2 += info->size;
		// cross fade ratio // 三角波 0.0~1.0
		cf = info->rate * 2.0;
		if(cf > 1.0)
			cf = 2.0 - cf;
		// read buffer // cross fade mix 
		output = info->buf[index1] * (1.0 - cf) + info->buf[index2] * cf;
		// write buffer
		info->buf[info->delay_count] = input + output * info->feedback;
		// output
		sp[i] = input * 0.0 + output * 1.0;
		// write count // 0 ~ size のリングバッファ
		if (++info->delay_count >= info->size)
			info->delay_count -= info->size;
		// lfo count
		if((info->rate += info->freq) >= 1.0)
			info->rate -= 1.0;	
	

		// bw
		//db[0] = sp[i];
		//r = dc[0] * db[0] + dc[1] * db[1] + dc[2] * db[2] + dc[3] * db[3] + dc[4] * db[4];
		//if(r < -VFX_TEST_MAX)
		//	r = -sqrt(-r);
		//else if(r > VFX_TEST_MAX)
		//	r = sqrt(r);
		//db[4] = db[3];
		//db[3] = r;
		//db[2] = db[1];
		//db[1] = db[0];
		//sp[i] = r;
		// 12-2
		//db[1] += (sp[i] - db[0]) * dc[1];
		//db[0] += db[1];
		//db[1] *= dc[0];
		//if(db[0] < -VFX_TEST_MAX)
		//	db[0] = -sqrt(-db[0]);
		//else if(db[0] > VFX_TEST_MAX)
		//	db[0] = sqrt(db[0]);
		//sp[i] = db[0];
		// 24-2
		//db[0] = sp[i];
		//db[5] = dc[0] * db[0] + db[1];
		//db[1] = dc[1] * db[0] + dc[3] * db[5] + db[2];
		//db[2] = dc[2] * db[0] + dc[4] * db[5];
		//db[0] = db[5];
		//db[5] = dc[0] * db[0] + db[3];
		//db[3] = dc[1] * db[0] + dc[3] * db[5] + db[4];
		//db[4] = dc[2] * db[0] + dc[4] * db[5];
		//if(db[0] < -VFX_TEST_MAX)
		//	db[0] = -sqrt(-db[0]);
		//else if(db[0] > VFX_TEST_MAX)
		//	db[0] = sqrt(db[0]);
		//sp[i] = db[0];
		// tfo
		//db[0] = db[0] + dc[0] * (sp[i] - db[0] + dc[1] * (db[0] - db[1]));
		//db[1] = db[1] + dc[0] * (db[0] - db[1]);
		//if(db[0] < -VFX_TEST_MAX)
		//	db[0] = -sqrt(-db[0]);
		//else if(db[0] > VFX_TEST_MAX)
		//	db[0] = sqrt(db[0]);
		//sp[i] = db[1];
	}

}


struct _VFX_Engine vfx_engine[] = {
{	VFX_NONE, init_vfx_none, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_none, sizeof(InfoVFX_None),},
{	VFX_MATH, init_vfx_math, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_math, sizeof(InfoVFX_Math),},
{	VFX_DISTORTION, init_vfx_distortion, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_distortion, sizeof(InfoVFX_Distortion),},
{	VFX_EQUALIZER, init_vfx_equalizer, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_equalizer, sizeof(InfoVFX_Equalizer),},
{	VFX_FILTER, init_vfx_filter, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_filter, sizeof(InfoVFX_Filter),},
{	VFX_FORMANT, init_vfx_formant, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_formant, sizeof(InfoVFX_Formant),},
{	VFX_DELAY, init_vfx_delay, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_delay, sizeof(InfoVFX_Delay),},
{	VFX_COMPRESSOR, init_vfx_compressor, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_compressor, sizeof(InfoVFX_Compressor),},
{	VFX_PITCH_SHIFTER, init_vfx_pitch_shifter, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_pitch_shifter, sizeof(InfoVFX_PitchShifter),},
{	VFX_FEEDBACKER, init_vfx_feedbacker, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_feedbacker, sizeof(InfoVFX_Feedbacker),},
{	VFX_REVERB, init_vfx_reverb, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_reverb, sizeof(InfoVFX_Reverb),},
{	VFX_ENVELOPE_REVERB, init_vfx_envelope_reverb, uninit_vfx_none, noteoff_vfx_envelope_reverb, damper_vfx_envelope_reverb, do_vfx_envelope_reverb, sizeof(InfoVFX_Envelope_Reverb),},
{	VFX_ENVELOPE, init_vfx_envelope, uninit_vfx_none, noteoff_vfx_envelope, damper_vfx_envelope, do_vfx_envelope, sizeof(InfoVFX_Envelope),},
{	VFX_ENVELOPE_FILTER, init_vfx_envelope_filter, uninit_vfx_none, noteoff_vfx_envelope_filter, damper_vfx_envelope_filter, do_vfx_envelope_filter, sizeof(InfoVFX_Envelope_Filter),},
{	VFX_LOFI, init_vfx_lofi, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_lofi, sizeof(InfoVFX_Lofi),},
{	VFX_DOWN_SAMPLE, init_vfx_down_sample, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_down_sample, sizeof(InfoVFX_DownSample),},
{	VFX_CHORUS, init_vfx_chorus, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_chorus, sizeof(InfoVFX_Chorus),},
{	VFX_ALLPASS, init_vfx_allpass, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_allpass, sizeof(InfoVFX_Allpass),},
{	VFX_COMB, init_vfx_comb, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_comb, sizeof(InfoVFX_Comb),},
{	VFX_REVERB2, init_vfx_reverb2, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_reverb2, sizeof(InfoVFX_Reverb2),},
{	VFX_RING_MODULATOR, init_vfx_ring_modulator, uninit_vfx_none, noteoff_vfx_ring_modulator, damper_vfx_ring_modulator, do_vfx_ring_modulator, sizeof(InfoVFX_RingModulator),},
{	VFX_WAH, init_vfx_wah, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_wah, sizeof(InfoVFX_Wah),},
{	VFX_TREMOLO, init_vfx_tremolo, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_tremolo, sizeof(InfoVFX_Tremolo),},
{	VFX_SYMPHONIC, init_vfx_symphonic, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_symphonic, sizeof(InfoVFX_Symphonic),},
{	VFX_PHASER, init_vfx_phaser, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_phaser, sizeof(InfoVFX_Phaser),},
{	VFX_ENHANCER, init_vfx_enhancer, uninit_vfx_none, noteoff_vfx_none, damper_vfx_none, do_vfx_enhancer, sizeof(InfoVFX_Enhancer),},

{	VFX_TEST, init_vfx_test, uninit_vfx_test, noteoff_vfx_none, damper_vfx_none, do_vfx_test, sizeof(InfoVFX_Test),},
{	-1, NULL, NULL, NULL, NULL, NULL, 0, },
};



void free_voice_effect(int v)
{
	int i;

	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		VoiceEffect *vfx = &voice[v].vfx[i];
		
		if(vfx->engine == &(vfx_engine[VFX_NONE]))
			continue;
		vfx->engine->uninit_vfx(v, vfx);
		vfx->engine = &(vfx_engine[VFX_NONE]);
		safe_free(vfx->info);
		vfx->info = NULL;
	}
	voice[v].voice_effect_flg = 0;
}

void init_voice_effect(int v)
{
	int i, j, error = 0, flg = 0;

	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		VoiceEffect *vfx = &voice[v].vfx[i];
		
		// param[0] = effect type
	    memset(&(vfx->param), 0, sizeof(int) * VOICE_EFFECT_PARAM_NUM);
		vfx->engine = NULL;
		for(j = 0; j < VOICE_EFFECT_PARAM_NUM; j++){
			vfx->param[j] = voice[v].sample->vfx[i][j];
		}
		if(vfx->param[0] < 0 || vfx->param[0] >= VFX_LIST_MAX)
			vfx->param[0] = 0;		

		for(j = 0; vfx_engine[j].type != -1; j++) {
			if (vfx_engine[j].type == vfx->param[0]) {
				vfx->engine = &(vfx_engine[j]);
				break;
			}
		}
		if (vfx->engine == NULL) { // error
			error = 1;
			break;
		}
		if (vfx->info) {
			safe_free(vfx->info);
			vfx->info = NULL;
		}
		if(vfx->param[0] == 0) // effect_none 
			continue;
		vfx->info = safe_large_malloc(vfx->engine->info_size);
		memset(vfx->info, 0, vfx->engine->info_size);
		vfx->engine->init_vfx(v, vfx);
		flg += 1; // effect on
	}
	voice[v].voice_effect_flg = flg ? 1 : 0;
	if(error)
		free_voice_effect(v);
}

void uninit_voice_effect(int v)
{
	int i;
	
	if(!voice[v].voice_effect_flg)
		return;
	free_voice_effect(v);
}

void noteoff_voice_effect(int v)
{
	int i;
	
	if(!voice[v].voice_effect_flg)
		return;
	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		voice[v].vfx[i].engine->noteoff_vfx(v, &voice[v].vfx[i]);
	}
}

void damper_voice_effect(int v, int8 damper)
{
	int i;
	
	if(!voice[v].voice_effect_flg)
		return;
	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		voice[v].vfx[i].engine->damper_vfx(v, &voice[v].vfx[i], damper);
	}
}

void voice_effect(int v, DATA_T *sp, int32 count)
{
	int i;

	if(!voice[v].voice_effect_flg)
		return;
	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		voice[v].vfx[i].engine->do_vfx(v, &voice[v].vfx[i], sp, count);
	}
}






#undef POW2 


#endif // VOICE_EFFECT



