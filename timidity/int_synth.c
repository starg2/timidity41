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
#include <stdlib.h>
#include <math.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif


#include "timidity.h"
#include "instrum.h"
#include "common.h"

#include "controls.h"
#include "filter.h"
#include "instrum.h"
#include "output.h"
#include "playmidi.h"
#include "resample.h"
#include "tables.h"
#include "int_synth.h"

#include "thread.h"

#define MYINI_LIBRARY_DEFIND_VAR
#include "myini.h"

#if defined(IA_W32GUI) || defined(IA_W32G_SYN) || defined(KBTIM) || defined(WINVSTI)
#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>
#endif

/*
timidityの動作のためSample設定と音色設定をロード
Sample設定は extract_hoge_file(file, preset_number)でロード
音色設定はextractでiniからロード
SCC/MMSは必要な分だけロード SCC_DATAはSCC/MMS共通なので全ロード LA_DATAはMMSのextract初回でロード
*/

int32 opt_int_synth_sine = 0;
int32 opt_int_synth_rate = 0;
int32 opt_int_synth_update = 0;


#ifdef INT_SYNTH

#ifndef POW2
#if 1 // lite
#define POW2(val) exp((float)(M_LN2 * val))
#else // precision
#define POW2(val) pow(2.0, val)
#endif
#endif /* POW2 */

static IS_RS_DATA_T is_resample_buffer[AUDIO_BUFFER_SIZE + 8]; // + 8 oversampling

// la pcm
static int la_pcm_data_load_flg = 0; // 
//MT32
#define MT32_PCM_ROM_SIZE (512 * 1024) // rom 512kB
#define MT32_PCM_SIZE (MT32_PCM_ROM_SIZE / 2) // rom 512kB 16bit 262144sample
static FLOAT_T mt32_pcm_data[MT32_PCM_SIZE + 1];
static Info_PCM mt32_pcm_inf[MT32_DATA_MAX];
// CM32L
#define CM32L_PCM_ROM_SIZE (1024 * 1024) // rom 1MB
#define CM32L_PCM_SIZE (CM32L_PCM_ROM_SIZE / 2) // rom 1MB 16bit 524288sample
static FLOAT_T cm32l_pcm_data[CM32L_PCM_SIZE + 1];
static Info_PCM cm32l_pcm_inf[CM32L_DATA_MAX];
//#define PCM_FILE_OUT // convert signed 32bit PCM RAW out
#define LA_CONTROL_ROM_SIZE (64 * 1024) // rom 64kB


const char *osc_wave_name[];



//////// synth lite mode

static int thru_count_mlt = 1;

static void init_int_synth_lite(void)
{
	if(opt_int_synth_update >= 100){
		thru_count_mlt = 100;
	}else if(opt_int_synth_update <= 1){
		thru_count_mlt = 1;
	}
}



//////// synth sample rate (resample

const FLOAT_T is_output_level = (FLOAT_T)M_15BIT;
static int32 prev_opt_int_synth_rate = -2;
static FLOAT_T is_sample_rate = 0;
static FLOAT_T div_is_sample_rate = 0;
static FLOAT_T is_sample_rate_ms = 0;

const int32 is_fraction = (1L << FRACTION_BITS);
const FLOAT_T div_is_fraction = (FLOAT_T)1.0 / (FLOAT_T)(1L << FRACTION_BITS);
const int is_rs_buff_offset = 2; // linear
const int is_rs_over_sampling = 2; // normal 2~8 // optimize x2
static FLOAT_T div_is_rs_over_sampling = 1.0;
static int is_rs_mode = 0;
static int32 is_rs_increment = (1L << FRACTION_BITS);

static void set_sample_rate(void)
{
	FLOAT_T resample_ratio = 1.0;

	div_is_rs_over_sampling = 1.0 / (FLOAT_T)is_rs_over_sampling;
	
	if(opt_int_synth_rate == 0 || opt_int_synth_rate == play_mode->rate){	
		is_rs_mode = 0; // no resample
		is_sample_rate = play_mode->rate;
		div_is_sample_rate = div_playmode_rate;
		is_sample_rate_ms = playmode_rate_ms;
		resample_ratio = 1.0;
		is_rs_increment = is_fraction;
	}else if(opt_int_synth_rate == -1){	
		is_rs_mode = 1; // up resample , play_mode->rate / 2
		is_sample_rate = play_mode->rate / 2;
		div_is_sample_rate = 1.0 / is_sample_rate;	
		is_sample_rate_ms = is_sample_rate * DIV_1000;
		resample_ratio = is_sample_rate * div_playmode_rate * div_is_rs_over_sampling;
		is_rs_increment = resample_ratio * is_fraction;
	}else if(opt_int_synth_rate < 0){	
		is_rs_mode = 2; // down resample , play_mode->rate * 2
		is_sample_rate = play_mode->rate * -opt_int_synth_rate;
		div_is_sample_rate = 1.0 / is_sample_rate;	
		is_sample_rate_ms = is_sample_rate * DIV_1000;
		resample_ratio = is_sample_rate * div_playmode_rate * div_is_rs_over_sampling;
		is_rs_increment = resample_ratio * is_fraction;
	}else if(opt_int_synth_rate < play_mode->rate){	
		is_rs_mode = 1; // up resample
		is_sample_rate = opt_int_synth_rate;
		div_is_sample_rate = 1.0 / is_sample_rate;	
		is_sample_rate_ms = is_sample_rate * DIV_1000;
		resample_ratio = is_sample_rate * div_playmode_rate * div_is_rs_over_sampling;
		is_rs_increment = resample_ratio * is_fraction;
	}else{
		is_rs_mode = 2; // down resample
		is_sample_rate = opt_int_synth_rate;
		div_is_sample_rate = 1.0 / is_sample_rate;
		is_sample_rate_ms = is_sample_rate * DIV_1000;
		resample_ratio = is_sample_rate * div_playmode_rate * div_is_rs_over_sampling;
		is_rs_increment = resample_ratio * is_fraction;
	}
	if(is_rs_mode){	
		if((compute_buffer_size * resample_ratio + 1) * is_rs_over_sampling >= AUDIO_BUFFER_SIZE){ // mono is_resample_buffer[] size
			is_rs_mode = 0; // no resample
			is_sample_rate = play_mode->rate;
			div_is_sample_rate = div_playmode_rate;
			is_sample_rate_ms = playmode_rate_ms;
			is_rs_increment = is_fraction;
		}
	}
}

static inline void is_resample_init(Info_Resample *rs)
{
	rs->rs_count = 0;
	rs->offset = 0;
	rs->data[0] = 0;
	rs->data[1] = 0;
}


static inline void is_resample_pre(Info_Resample *rs, IS_RS_DATA_T *rs_buf, int32 count)
{
	rs->rs_count = (rs->offset + is_rs_increment * count * is_rs_over_sampling) >> FRACTION_BITS;
	rs_buf[0] = rs->data[0];
	rs_buf[1] = rs->data[1];
}

#if 1 // optimize over sampling x2

static inline void is_resample_core(Info_Resample *rs, DATA_T *is_buf, IS_RS_DATA_T *rs_buf, int32 count)
{
	int32 i = 0;
	int32 rs_ofs = rs->offset;
#if !(defined(_MSC_VER) || defined(MSC_VER))
	int32 *ofsp1, *ofsp2;
#endif
	
#if (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_FLOAT) // doubleも使えるけどやや遅いのでfloatのみ
	{ // offset:int32*4, resamp:float*4
	const int32 is_rs_count_mask = ~(0x1);	
	const int32 count2 = count & is_rs_count_mask;	
	const __m128 vec_divf = _mm_set1_ps(div_is_fraction), vec_divo = _mm_set1_ps(DIV_15BIT), vec_div2 = _mm_set1_ps(DIV_2);
	const __m128i vinc = _mm_set1_epi32(is_rs_increment * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128i vinc2 = _mm_set_epi32(0, is_rs_increment, is_rs_increment * 2, is_rs_increment * 3);
	__m128i vofs = _mm_sub_epi32(_mm_set1_epi32(rs_ofs), vinc2);
	const __m128 vmout = _mm_set1_ps(is_output_level);

#if (USE_X86_EXT_INTRIN >= 8)
	// 最適化レート = (ロードデータ数 - 初期オフセット小数部の最大値(1未満) - 補間ポイント数(linearは1) ) / オフセットデータ数
	// ロードデータ数は_mm_permutevar_psの変換後の(float)の4セットになる
	const int32 opt_inc1 = (1 << FRACTION_BITS) * (4 - 1 - 1) / 4; // (float*4) * 1セット
	const __m128i vvar1 = _mm_set1_epi32(1);
	if(is_rs_increment < opt_inc1){	// 1セット	
	for(; i < count2; i += 2) {
	__m128 vfp, vv1, vv2, vec_out, tmp1;
	__m128i vofsi1, vofsi2, vofsf, vofsib, vofsub1, vofsub2;
	int32 ofs0;
	vofs = _mm_add_epi32(vofs, vinc);
	vofsi1 = _mm_srli_epi32(vofs, FRACTION_BITS);
	vofsi2 = _mm_add_epi32(vofsi1, vvar1);
	vofsf = _mm_and_si128(vofs, vfmask);
	vfp = _mm_mul_ps(_mm_cvtepi32_ps(vofsf), vec_divf); // int32 to float // calc fp
	ofs0 = _mm_cvtsi128_si32(vofsi1);
#if defined(IS_RS_DATA_T_DOUBLE)
	tmp1 = _mm256_cvtpd_ps(_mm256_loadu_pd(&rs_buf[ofs0])); // ロード
#else // defined(IS_RS_DATA_T_FLOAT)
	tmp1 = _mm_loadu_ps(&rs_buf[ofs0]); // ロード
#endif // !(defined(_MSC_VER) || defined(MSC_VER))		
	vofsib = _mm_shuffle_epi32(vofsi1, 0x0); 
	vofsub1 = _mm_sub_epi32(vofsi1, vofsib); 
	vofsub2 = _mm_sub_epi32(vofsi2, vofsib); 
	vv1 = _mm_permutevar_ps(tmp1, vofsub1); // v1 ofsi
	vv2 = _mm_permutevar_ps(tmp1, vofsub2); // v2 ofsi+1
	vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1); // out
	// down sampling
	vec_out = _mm_add_ps(_mm_shuffle_ps(vec_out, vec_out, 0xD8), _mm_shuffle_ps(vec_out, vec_out, 0x8D)); // [0+1,2+3,2+0,3+1]=[0,2,1,3]+[1,3,0,2]
	vec_out = _mm_mul_ps(vec_out , vec_div2);
	// output 2
#if	defined(DATA_T_DOUBLE)
	_mm_storeu_pd(&is_buf[i], _mm_cvtps_pd(vec_out));
#elif defined(DATA_T_FLOAT)
	_mm_storel_ps(&is_buf[i], vec_out);
#else // DATA_T_INT32
	vec_out = _mm_mul_ps(vec_out, vmout);
	_mm_storeu_si128((__m128i *)&is_buf[i], _mm_cvtps_epi32(vec_out)); // only L64bit
#endif // DATA_T_INT32
	rs_ofs = MM_EXTRACT_EPI32(vofs, 0x3);
	}
	}else
#endif
	for(; i < count2; i += 2) {
	__m128 vfp, vv1, vv2, tmp1, tmp2, tmp3, tmp4, vec_out;	
	__m128i vofsi, vofsf;
	vofs = _mm_add_epi32(vofs, vinc);
	vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	vofsf = _mm_and_si128(vofs, vfmask);
	vfp = _mm_mul_ps(_mm_cvtepi32_ps(vofsf), vec_divf); // int32 to float // calc fp
#if defined(IS_RS_DATA_T_DOUBLE)
	tmp1 = _mm_cvtpd_ps(_mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,0)])); // ofsiとofsi+1をロード
	tmp2 = _mm_cvtpd_ps(_mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,1)])); // 次周サンプルも同じ
	tmp3 = _mm_cvtpd_ps(_mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,2)])); // 次周サンプルも同じ
	tmp4 = _mm_cvtpd_ps(_mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,3)])); // 次周サンプルも	
	tmp1 = _mm_shuffle_ps(tmp1, tmp2, 0x44);
	tmp3 = _mm_shuffle_ps(tmp3, tmp4, 0x44);
#else // defined(IS_RS_DATA_T_FLOAT)
	tmp1 = _mm_loadl_pi(tmp1, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,0)]); // L64bit ofsiとofsi+1をロード
	tmp1 = _mm_loadh_pi(tmp1, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,1)]); // H64bit 次周サンプルも同じ
	tmp3 = _mm_loadl_pi(tmp3, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,2)]); // L64bit 次周サンプルも同じ
	tmp3 = _mm_loadh_pi(tmp3, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,3)]); // H64bit 次周サンプルも同じ
#endif
	vv1 = _mm_shuffle_ps(tmp1, tmp3, 0x88); // v1[0,1,2,3]	// ofsiはv1に
	vv2 = _mm_shuffle_ps(tmp1, tmp3, 0xdd); // v2[0,1,2,3]	// ofsi+1はv2に移動
	vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);	
	// down sampling
	vec_out = _mm_add_ps(_mm_shuffle_ps(vec_out, vec_out, 0xD8), _mm_shuffle_ps(vec_out, vec_out, 0x8D)); // [0+1,2+3,2+0,3+1]=[0,2,1,3]+[1,3,0,2]
	vec_out = _mm_mul_ps(vec_out , vec_div2);
	// output 2
#if	defined(DATA_T_DOUBLE)
	_mm_storeu_pd(&is_buf[i], _mm_cvtps_pd(vec_out));
#elif defined(DATA_T_FLOAT)
	_mm_storel_ps(&is_buf[i], vec_out);
#else // DATA_T_INT32
	vec_out = _mm_mul_ps(vec_out, vmout);
	_mm_storeu_si128((__m128i *)&is_buf[i], _mm_cvtps_epi32(vec_out)); // only L64bit
#endif // DATA_T_INT32
	}
	rs_ofs = MM_EXTRACT_EPI32(vofs, 0x3);
	}

#elif (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_DOUBLE)
	{ // offset:int32*4, resamp:double*2*2
	const int32 is_rs_count_mask = ~(0x1);	
	const int32 count2 = count & is_rs_count_mask;	
	const __m128d vec_divf = _mm_set1_pd(div_is_fraction), vec_divo = _mm_set1_pd(DIV_15BIT), vec_div2 = _mm_set1_pd(DIV_2);
	const __m128i vinc = _mm_set1_epi32(is_rs_increment * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128i vinc2 = _mm_set_epi32(0, is_rs_increment, is_rs_increment * 2, is_rs_increment * 3);
	__m128i vofs = _mm_sub_epi32(_mm_set1_epi32(rs_ofs), vinc2);
	const __m128d vmout = _mm_set1_pd(is_output_level);
	for(; i < count2; i += 2) {
	__m128d vfp1, vfp2, vv11, vv12, vv21, vv22, tmp1, tmp2, tmp3, tmp4, vec_out1, vec_out2;
	__m128i vofsi, vofsf;
	vofs = _mm_add_epi32(vofs, vinc);
	vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	vofsf = _mm_and_si128(vofs, vfmask);
	vfp1 = _mm_mul_pd(_mm_cvtepi32_pd(vofsf), vec_divf); // int32 to double // calc fp
	vfp2 = _mm_mul_pd(_mm_cvtepi32_pd(_mm_shuffle_epi32(vofsf, 0x4E)), vec_divf); // int32 to double // calc fp
	tmp1 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード
	tmp2 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ
	tmp3 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ
	tmp4 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,3)]); // 次周サンプルも	
	vv11 = _mm_shuffle_pd(tmp1, tmp2, 0x00); // v1[0,1] // ofsiはv1に
	vv21 = _mm_shuffle_pd(tmp1, tmp2, 0x03); // v2[0,1] // ofsi+1はv2に移動
	vv12 = _mm_shuffle_pd(tmp3, tmp4, 0x00); // v1[2,3] // ofsiはv1に
	vv22 = _mm_shuffle_pd(tmp3, tmp4, 0x03); // v2[2,3] // ofsi+1はv2に移動	
	vec_out1 = MM_FMA_PD(_mm_sub_pd(vv21, vv11), vfp1, vv11); // out[0,1]
	vec_out2 = MM_FMA_PD(_mm_sub_pd(vv22, vv12), vfp2, vv12); // out[2,3]	
	// down sampling
	vec_out1 = _mm_add_pd(_mm_shuffle_pd(vec_out1, vec_out2, 0x0), _mm_shuffle_pd(vec_out1, vec_out2, 0x3)); // [0+1,2+3]=[0,2]+[1,3]
	vec_out1 = _mm_mul_pd(vec_out1 , vec_div2);
	// output 2
#if	defined(DATA_T_DOUBLE)
	_mm_storeu_pd(&is_buf[i], vec_out1);
#elif defined(DATA_T_FLOAT)
	_mm_storel_pi(&is_buf[i], _mm_shuffle_ps(_mm_cvtpd_ps(vec_out1), _mm_cvtpd_ps(vec_out2), 0x44)); // 2set
#else // DATA_T_INT32
	_mm_storeu_si128((__m128i *)&is_buf[i], _mm_cvtpd_epi32(_mm_mul_pd(vec_out1, vmout))); // L64bit
#endif // DATA_T_INT32
	}
	rs_ofs = MM_EXTRACT_EPI32(vofs, 0x3);
	}

#elif (USE_X86_EXT_INTRIN >= 2)
	{ // offset:int32*4, resamp:float*4, output:*2
	const int32 is_rs_count_mask = ~(0x1);	
	int32 count2 = count & is_rs_count_mask;	
	__m128 vec_divf = _mm_set1_ps((float)div_is_fraction), vec_div2 = _mm_set1_ps(DIV_2);
	__m128 vec_out;
	const __m128 vmout = _mm_set1_ps(is_output_level);
	for(; i < count2; i += 2) {
	int32 ofsi;		
	__m128 vec_out, vv1, vv2, vfp;
#if defined(IS_RS_DATA_T_DOUBLE)
	ALIGN float tmp1[4], tmp2[4], tmpfp[4];
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 		
	tmp1[0] = (float)rs_buf[ofsi], tmp2[0] = (float)rs_buf[++ofsi], tmpfp[0] = (float)(rs_ofs & FRACTION_MASK);
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmp1[1] = (float)rs_buf[ofsi], tmp2[1] = (float)rs_buf[++ofsi], tmpfp[1] = (float)(rs_ofs & FRACTION_MASK);
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmp1[2] = (float)rs_buf[ofsi], tmp2[2] = (float)rs_buf[++ofsi], tmpfp[2] = (float)(rs_ofs & FRACTION_MASK);
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmp1[3] = (float)rs_buf[ofsi], tmp2[3] = (float)rs_buf[++ofsi], tmpfp[3] = (float)(rs_ofs & FRACTION_MASK);
	vv1 = _mm_load_ps(tmp1);	
	vv2 = _mm_load_ps(tmp2);	
	vfp = _mm_load_ps(tmpfp);
#else // defined(IS_RS_DATA_T_FLOAT)
	ALIGN float tmpfp[4];
	__m128 tmp1, tmp2, tmp3, tmp4;
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmpfp[0] = (float)(rs_ofs & FRACTION_MASK);
	tmp1 = _mm_loadu_ps(&rs_buf[ofsi]); // L64bit ofsiとofsi+1をロード
	tmp1 = _mm_loadl_pi(tmp1, (__m64 *)&rs_buf[ofsi]); // L64bit ofsiとofsi+1をロード
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmpfp[1] = (float)(rs_ofs & FRACTION_MASK);
	tmp1 = _mm_loadh_pi(tmp1, (__m64 *)&rs_buf[ofsi]); // H64bit ofsiとofsi+1をロード
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmpfp[2] = (float)(rs_ofs & FRACTION_MASK);
	tmp3 = _mm_loadl_pi(tmp3, (__m64 *)&rs_buf[ofsi]); // L64bit ofsiとofsi+1をロード
	ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
	tmpfp[3] = (float)(rs_ofs & FRACTION_MASK);
	tmp3 = _mm_loadh_pi(tmp3, (__m64 *)&rs_buf[ofsi]); // H64bit ofsiとofsi+1をロード
	vv1 = _mm_shuffle_ps(tmp1, tmp3, 0x88); // v1[0,1,2,3]	// ofsiはv1に
	vv2 = _mm_shuffle_ps(tmp1, tmp3, 0xdd); // v2[0,1,2,3]	// ofsi+1はv2に移動	
	vfp = _mm_load_ps(tmpfp);
#endif
	vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1);
	// down sampling
	vec_out = _mm_add_ps(_mm_shuffle_ps(vec_out, vec_out, 0xD8), _mm_shuffle_ps(vec_out, vec_out, 0x8D));
	vec_out = _mm_mul_ps(vec_out , vec_div2);
	// output 2
#if defined(DATA_T_DOUBLE)
	{
	ALIGN float out[4];		
	_mm_storeu_ps(out, vec_out);
	is_buf[i] = (DATA_T)out[0];
	is_buf[i + 1] = (DATA_T)out[2];
	}
#elif defined(DATA_T_FLOAT)
	_mm_storel_pi(&is_buf[i], vec_out);
#else // DATA_T_IN32
	vec_out = _mm_mul_ps(vec_out, vmout);	
	is_buf[i] = _mm_cvt_ss2si(vec_out);
	is_buf[i + 1] = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xe5));
#endif // DATA_T_INT32
	}
	}

#endif // USE_X86_EXT_INTRIN
	
	{ // offset:int32*2, resamp:FLOAT_T*2, output:*1	
		int32 ofsi, ofsf;
		FLOAT_T v11, v21, v12, v22, fp1, fp2, out1, out2;
		int32 ofsi2, ofsf2;

		for (; i < count; i++){
			rs_ofs += is_rs_increment;
			ofsi = rs_ofs >> FRACTION_BITS;
			ofsf = rs_ofs & FRACTION_MASK;
			rs_ofs += is_rs_increment;
			ofsi2 = rs_ofs >> FRACTION_BITS;
			ofsf2 = rs_ofs & FRACTION_MASK;
			fp1 = (FLOAT_T)ofsf * div_is_fraction;
			fp2 = (FLOAT_T)ofsf2 * div_is_fraction;
			v11 = rs_buf[ofsi], v21 = rs_buf[ofsi + 1], fp1 = (FLOAT_T)ofsf * div_is_fraction;
			v12 = rs_buf[ofsi], v22 = rs_buf[ofsi + 1], fp2 = (FLOAT_T)ofsf * div_is_fraction;
			out1 = v11 + (v21 - v11) * fp1;
			out2 = v12 + (v22 - v12) * fp2;
			// down sampling
			out1 = (out1 + out2) * DIV_2;
			// output 1
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
			is_buf[i] = out1;
#else // DATA_T_INT32
			is_buf[i] = out1 * is_output_level;
#endif // DATA_T_INT32
		}

		// 
		ofsi = rs_ofs >> FRACTION_BITS;
		rs->offset = rs_ofs & FRACTION_MASK;
#if (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_DOUBLE)
		_mm_storeu_pd(rs->data, _mm_loadu_pd(&rs_buf[ofsi]));
#elif (USE_X86_EXT_INTRIN >= 2) && defined(IS_RS_DATA_T_FLOAT)
		{
		__m128 vtmp;
		_mm_storel_pi(rs->data, _mm_loadl_pi(vtmp, (__m64 *)&rs_buf[ofsi]));
		}
#else
		rs->data[0] = rs_buf[ofsi];
		rs->data[1] = rs_buf[ofsi + 1]; // interpolate		
#endif
	}
}

#else // normal over sampling x2~x8

static void is_resample_down_sampling(DATA_T *ptr, int32 count)
{
	int32 i;

	switch(is_rs_over_sampling){
	case 0:
		break;
#if 1 // optimize
	case 2:
		for(i = 0; i < count; i++){
			int ofs = i * 2;
			FLOAT_T tmp = ptr[ofs] + ptr[ofs + 1];
			ptr[i] = tmp * DIV_2;
		}
		break;
	case 4:
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{ // 4samples
			const __m128d divn = _mm_set1_pd(DIV_4);
			for(i = 0; i < count; i += 4){
				int32 ofs = i * 4;
				__m128d	sum1 = _mm_load_pd(&ptr[ofs + 0]);
				__m128d	sum2 = _mm_load_pd(&ptr[ofs + 2]);
				__m128d	sum3 = _mm_load_pd(&ptr[ofs + 4]);
				__m128d	sum4 = _mm_load_pd(&ptr[ofs + 6]);
				__m128d	sum5 = _mm_load_pd(&ptr[ofs + 8]);
				__m128d	sum6 = _mm_load_pd(&ptr[ofs + 10]);
				__m128d	sum7 = _mm_load_pd(&ptr[ofs + 12]);
				__m128d	sum8 = _mm_load_pd(&ptr[ofs + 14]);
				// ([1,2] [3,4]) ([5,6] [7,8])
				sum1 = _mm_add_pd(sum1, sum2);
				sum3 = _mm_add_pd(sum3, sum4);
				sum5 = _mm_add_pd(sum5, sum6);
				sum7 = _mm_add_pd(sum7, sum8);				
				sum2 = _mm_shuffle_pd(sum1, sum1, 0x1); // [v0v1] -> [v1v0]
				sum4 = _mm_shuffle_pd(sum3, sum3, 0x1); // [v0v1] -> [v1v0]
				sum6 = _mm_shuffle_pd(sum5, sum5, 0x1); // [v0v1] -> [v1v0]
				sum8 = _mm_shuffle_pd(sum7, sum7, 0x1); // [v0v1] -> [v1v0]
				sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v1 v1=v1+v0
				sum3 = _mm_add_pd(sum3, sum4); // v0=v0+v1 v1=v1+v0
				sum5 = _mm_add_pd(sum5, sum6); // v0=v0+v1 v1=v1+v0
				sum7 = _mm_add_pd(sum7, sum8); // v0=v0+v1 v1=v1+v0
				sum1 = _mm_shuffle_pd(sum1, sum3, 0x0);
				sum5 = _mm_shuffle_pd(sum5, sum7, 0x0);
				sum1 = _mm_mul_pd(sum1, divn);
				sum5 = _mm_mul_pd(sum5, divn);
				_mm_store_pd(&ptr[i    ], sum1);
				_mm_store_pd(&ptr[i + 2], sum5);
			}
		}
#else
		for(i = 0; i < count; i++){
			int32 ofs = i * 4;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1] + ptr[ofs + 2] + ptr[ofs + 3];
			ptr[i] = tmp * DIV_4;
		}
#endif
		break;
	case 8:
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{ // 4samples
			const __m128d divn = _mm_set1_pd(DIV_8);
			for(i = 0; i < count; i += 4){
				int32 ofs = i * 8;
				__m128d	sum1 = _mm_load_pd(&ptr[ofs + 0]);
				__m128d	sum2 = _mm_load_pd(&ptr[ofs + 2]);
				__m128d	sum3 = _mm_load_pd(&ptr[ofs + 4]);
				__m128d	sum4 = _mm_load_pd(&ptr[ofs + 6]);
				__m128d	sum5 = _mm_load_pd(&ptr[ofs + 8]);
				__m128d	sum6 = _mm_load_pd(&ptr[ofs + 10]);
				__m128d	sum7 = _mm_load_pd(&ptr[ofs + 12]);
				__m128d	sum8 = _mm_load_pd(&ptr[ofs + 14]);
				__m128d	sum9 = _mm_load_pd(&ptr[ofs + 16]);
				__m128d	sum10 = _mm_load_pd(&ptr[ofs + 18]);
				__m128d	sum11 = _mm_load_pd(&ptr[ofs + 20]);
				__m128d	sum12 = _mm_load_pd(&ptr[ofs + 22]);
				__m128d	sum13 = _mm_load_pd(&ptr[ofs + 24]);
				__m128d	sum14 = _mm_load_pd(&ptr[ofs + 26]);
				__m128d	sum15 = _mm_load_pd(&ptr[ofs + 28]);
				__m128d	sum16 = _mm_load_pd(&ptr[ofs + 30]);
				// ([1,2,3,4] [5,6,7,8]) ([9,10,11,12] [13,14,15,16])				
				sum1 = _mm_add_pd(sum1, sum2);
				sum3 = _mm_add_pd(sum3, sum4);
				sum5 = _mm_add_pd(sum5, sum6);
				sum7 = _mm_add_pd(sum7, sum8);
				sum9 = _mm_add_pd(sum9, sum10);
				sum11 = _mm_add_pd(sum11, sum12);
				sum13 = _mm_add_pd(sum13, sum14);
				sum15 = _mm_add_pd(sum15, sum16);
				sum1 = _mm_add_pd(sum1, sum3);	
				sum5 = _mm_add_pd(sum5, sum7);
				sum9 = _mm_add_pd(sum9, sum11);
				sum13 = _mm_add_pd(sum13, sum15);
				sum2 = _mm_shuffle_pd(sum1, sum1, 0x1); // [v0v1] -> [v1v0]
				sum6 = _mm_shuffle_pd(sum5, sum5, 0x1); // [v0v1] -> [v1v0]
				sum10 = _mm_shuffle_pd(sum9, sum9, 0x1); // [v0v1] -> [v1v0]
				sum14 = _mm_shuffle_pd(sum13, sum13, 0x1); // [v0v1] -> [v1v0]
				sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v1 v1=v1+v0
				sum5 = _mm_add_pd(sum5, sum6); // v0=v0+v1 v1=v1+v0
				sum9 = _mm_add_pd(sum9, sum10); // v0=v0+v1 v1=v1+v0
				sum13 = _mm_add_pd(sum13, sum14); // v0=v0+v1 v1=v1+v0
				sum1 = _mm_shuffle_pd(sum1, sum5, 0x0);
				sum9 = _mm_shuffle_pd(sum9, sum13, 0x0);
				sum1 = _mm_mul_pd(sum1, divn);
				sum9 = _mm_mul_pd(sum9, divn);
				_mm_store_pd(&ptr[i    ], sum1);
				_mm_store_pd(&ptr[i + 2], sum9);
			}
		}
#else
		for(i = 0; i < count; i++){
			int32 ofs = i * 8;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1] + ptr[ofs + 2] + ptr[ofs + 3]
				+ ptr[ofs + 4] + ptr[ofs + 5] + ptr[ofs + 6] + ptr[ofs + 7];
			ptr[i] = tmp * DIV_8;
		}
#endif
		break;
	case 16:
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		{ // 2sample
			const __m128d divn = _mm_set1_pd(DIV_16);
			for(i = 0; i < count; i += 2){
				int32 ofs = i * 16;
				__m128d	sum1 = _mm_load_pd(&ptr[ofs + 0]);
				__m128d	sum2 = _mm_load_pd(&ptr[ofs + 2]);
				__m128d	sum3 = _mm_load_pd(&ptr[ofs + 4]);
				__m128d	sum4 = _mm_load_pd(&ptr[ofs + 6]);
				__m128d	sum5 = _mm_load_pd(&ptr[ofs + 8]);
				__m128d	sum6 = _mm_load_pd(&ptr[ofs + 10]);
				__m128d	sum7 = _mm_load_pd(&ptr[ofs + 12]);
				__m128d	sum8 = _mm_load_pd(&ptr[ofs + 14]);
				__m128d	sum9 = _mm_load_pd(&ptr[ofs + 16]);
				__m128d	sum10 = _mm_load_pd(&ptr[ofs + 18]);
				__m128d	sum11 = _mm_load_pd(&ptr[ofs + 20]);
				__m128d	sum12 = _mm_load_pd(&ptr[ofs + 22]);
				__m128d	sum13 = _mm_load_pd(&ptr[ofs + 24]);
				__m128d	sum14 = _mm_load_pd(&ptr[ofs + 26]);
				__m128d	sum15 = _mm_load_pd(&ptr[ofs + 28]);
				__m128d	sum16 = _mm_load_pd(&ptr[ofs + 30]);
				// ([1,2,3,4,5,6,7,8] [9,10,11,12,13,14,15,16])
				sum1 = _mm_add_pd(sum1, sum2);
				sum3 = _mm_add_pd(sum3, sum4);
				sum5 = _mm_add_pd(sum5, sum6);
				sum7 = _mm_add_pd(sum7, sum8);
				sum9 = _mm_add_pd(sum9, sum10);
				sum11 = _mm_add_pd(sum11, sum12);
				sum13 = _mm_add_pd(sum13, sum14);
				sum15 = _mm_add_pd(sum15, sum16);
				sum1 = _mm_add_pd(sum1, sum3);
				sum5 = _mm_add_pd(sum5, sum7);				
				sum9 = _mm_add_pd(sum9, sum11);
				sum13 = _mm_add_pd(sum13, sum15);				
				sum1 = _mm_add_pd(sum1, sum5);
				sum9 = _mm_add_pd(sum9, sum13);				
				sum2 = _mm_shuffle_pd(sum1, sum1, 0x1); // [v0v1] -> [v1v0]
				sum10 = _mm_shuffle_pd(sum9, sum9, 0x1); // [v0v1] -> [v1v0]
				sum1 = _mm_add_pd(sum1, sum2); // v0=v0+v1 v1=v1+v0
				sum9 = _mm_add_pd(sum9, sum10); // v0=v0+v1 v1=v1+v0
				sum1 = _mm_shuffle_pd(sum1, sum9, 0x0);
				sum1 = _mm_mul_pd(sum1, divn);
				_mm_store_pd(&ptr[i], sum1);
			}
		}
#else
		for(i = 0; i < count; i++){
			int32 ofs = i * 16;
			FLOAT_T tmp = ptr[ofs + 0] + ptr[ofs + 1] + ptr[ofs + 2] + ptr[ofs + 3]
				+ ptr[ofs + 4] + ptr[ofs + 5] + ptr[ofs + 6] + ptr[ofs + 7]
				+ ptr[ofs + 8] + ptr[ofs + 9] + ptr[ofs + 10] + ptr[ofs + 11]
				+ ptr[ofs + 12] + ptr[ofs + 13] + ptr[ofs + 14] + ptr[ofs + 15];
			ptr[i] = tmp * DIV_16;
		}
#endif
		break;
#else
	default:
		for(i = 0; i < count; i++){
			int ofs = i * is_rs_over_sampling;
			FLOAT_T tmp = 0.0
			for(j = 0; j < is_rs_over_sampling; j++){
				tmp += ptr[ofs + j];
			}
			ptr[i] = tmp * div_is_rs_over_sampling;
		}
		break;
#endif
	}
}

static inline void is_resample_core(Info_Resample *rs, DATA_T *is_buf, IS_RS_DATA_T *rs_buf, int32 count)
{
	int32 i = 0;
	int32 rs_ofs = rs->offset;
#if !(defined(_MSC_VER) || defined(MSC_VER))
	int32 *ofsp1, *ofsp2;
#endif

#if (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_FLOAT) // SSE2~
	{ // offset:int32*4, resamp:float*4
	const int32 is_rs_count_mask = ~(0x3);
	int32 count2 = (count * is_rs_over_sampling) & is_rs_count_mask;
	const __m128 vec_divf = _mm_set1_ps(div_is_fraction), vec_divo = _mm_set1_pd(DIV_15BIT);	
	const __m128i vinc = _mm_set1_epi32(is_rs_increment * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128i vinc2 = _mm_set_epi32(0, is_rs_increment, is_rs_increment * 2, is_rs_increment * 3);
	__m128i vofs = _mm_sub_epi32(_mm_set1_epi32(rs_ofs), vinc);
	for(; i < count2; i += 4) {
	__m128 vfp, vv1, vv2, tmp1, tmp2, tmp3, tmp4, vec_out;	
	__m128i vofsi, vosfsf;
	vofs = _mm_add_epi32(vofs, vinc);
	vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	vosfsf = _mm_and_si128(vofs, vfmask);
	vfp = _mm_mul_ps(_mm_cvtepi32_ps(vosfsf), vec_divf); // int32 to float // calc fp
	tmp1 = _mm_loadl_pi(tmp1, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi.0)]); // L64bit ofsiとofsi+1をロード
	tmp1 = _mm_loadh_pi(tmp1, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,1)]); // H64bit 次周サンプルも同じ
	tmp3 = _mm_loadl_pi(tmp3, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,2)]); // L64bit 次周サンプルも同じ
	tmp3 = _mm_loadh_pi(tmp3, (__m64 *)&rs_buf[MM_EXTRACT_I32(vofsi,3)]); // H64bit 次周サンプルも同じ
	vv1 = _mm_shuffle_ps(tmp1, tmp3, 0x88); // v1[0,1,2,3]	// ofsiはv1に
	vv2 = _mm_shuffle_ps(tmp1, tmp3, 0xdd); // v2[0,1,2,3]	// ofsi+1はv2に移動
	vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), vfp, vv1);			
#if	defined(DATA_T_DOUBLE)
	_mm_storeu_pd(&is_buf[i], _mm_cvtps_pd(vec_out));
	_mm_storeu_pd(&is_buf[i + 2], _mm_cvtps_pd(_mm_movehl_ps(vec_out, vec_out)));
#elif defined(DATA_T_FLOAT)
	_mm_storeu_ps(&is_buf[i], vec_out);
#else // DATA_T_INT32
	vec_out = _mm_mul_ps(vec_out, _mm_set1_ps(is_output_level));
	_mm_storeu_si128((__m128i *)&is_buf[i], _mm_cvtps_epi32(vec_out));
#endif // DATA_T_INT32
	}
	rs_ofs = MM_EXTRACT_EPI32(vofs, 0x3);
	}
	
#elif (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_DOUBLE)
	{ // offset:int32*4, resamp:double*2*2
	const int32 is_rs_count_mask = ~(0x3);	
	const int32 count2 = (count * is_rs_over_sampling) & is_rs_count_mask;	
	const __m128d vec_divf = _mm_set1_pd(div_is_fraction), vec_divo = _mm_set1_pd(DIV_15BIT);	
	const __m128i vinc = _mm_set1_epi32(is_rs_increment * 4), vfmask = _mm_set1_epi32((int32)FRACTION_MASK);
	const __m128i vinc2 = _mm_set_epi32(0, is_rs_increment, is_rs_increment * 2, is_rs_increment * 3);
	__m128i vofs = _mm_sub_epi32(_mm_set1_epi32(rs_ofs), vinc2);
	for(; i < count2; i += 4) {
	__m128d vfp1, vfp2, vv11, vv12, vv21, vv22, tmp1, tmp2, tmp3, tmp4, vec_out1, vec_out2;
	__m128i vofsi, vosfsf;
	vofs = _mm_add_epi32(vofs, vinc);
	vofsi = _mm_srli_epi32(vofs, FRACTION_BITS);
	vosfsf = _mm_and_si128(vofs, vfmask);
	vfp1 = _mm_mul_pd(_mm_cvtepi32_pd(vosfsf), vec_divf); // int32 to double // calc fp
	vfp2 = _mm_mul_pd(_mm_cvtepi32_pd(_mm_shuffle_epi32(vosfsf, 0x4E)), vec_divf); // int32 to double // calc fp
#if !(defined(_MSC_VER) || defined(MSC_VER))
	ofsp1 = (int32 *)vofsi;
	tmp1 = _mm_loadu_pd(&rs_buf[ofsp1[0]]); // ofsiとofsi+1をロード
	tmp2 = _mm_loadu_pd(&rs_buf[ofsp1[1]]); // 次周サンプルも同じ
	tmp3 = _mm_loadu_pd(&rs_buf[ofsp1[2]]); // 次周サンプルも同じ
	tmp4 = _mm_loadu_pd(&rs_buf[ofsp1[3]]); // 次周サンプルも同じ	
#else
	tmp1 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,0)]); // ofsiとofsi+1をロード
	tmp2 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,1)]); // 次周サンプルも同じ
	tmp3 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,2)]); // 次周サンプルも同じ
	tmp4 = _mm_loadu_pd(&rs_buf[MM_EXTRACT_I32(vofsi,3)]); // 次周サンプルも	
#endif // !(defined(_MSC_VER) || defined(MSC_VER))	
	vv11 = _mm_shuffle_pd(tmp1, tmp2, 0x00); // v1[0,1] // ofsiはv1に
	vv21 = _mm_shuffle_pd(tmp1, tmp2, 0x03); // v2[0,1] // ofsi+1はv2に移動
	vv12 = _mm_shuffle_pd(tmp3, tmp4, 0x00); // v1[2,3] // ofsiはv1に
	vv22 = _mm_shuffle_pd(tmp3, tmp4, 0x03); // v2[2,3] // ofsi+1はv2に移動	
	vec_out1 = MM_FMA_PD(_mm_sub_pd(vv21, vv11), vfp1, vv11);
	vec_out2 = MM_FMA_PD(_mm_sub_pd(vv22, vv12), vfp2, vv12);
#if	defined(DATA_T_DOUBLE)
	_mm_storeu_pd(&is_buf[i], vec_out1);
	_mm_storeu_pd(&is_buf[i + 2], vec_out2);
#elif defined(DATA_T_FLOAT)
	_mm_storeu_ps(&is_buf[i], _mm_shuffle_ps(_mm_cvtpd_ps(vec_out1), _mm_cvtpd_ps(vec_out2), 0x44));
#else // DATA_T_INT32
	{
	__m128d vmout = _mm_set1_pd(is_output_level);
	__m128i vec_out21 = _mm_cvtpd_epi32(_mm_mul_pd(vec_out1, vmout)); // L64bit
	__m128i vec_out22 = _mm_shuffle_epi32(_mm_cvtpd_epi32(_mm_mul_pd(vec_out2, vmout)), 0x4e);// H64bit
	_mm_storeu_si128((__m128i *)&is_buf[i], _mm_or_si128(vec_out21, vec_out22));
	}
#endif // DATA_T_INT32
	}
	rs_ofs = MM_EXTRACT_EPI32(vofs, 0x3);
	}

#elif (USE_X86_EXT_INTRIN >= 2)
	{ // offset:int32*4, resamp:float*4	
	const int32 is_rs_count_mask = ~(0x3);	
	int32 count2 = (count * is_rs_over_sampling) & is_rs_count_mask;	
	__m128 vec_divf = _mm_set1_ps((float)div_is_fraction);
	for(; i < count2; i += 4) {
		int32 ofsi;		
		__m128 vec_out, vv1, vv2, vfp;
#if defined(IS_RS_DATA_T_DOUBLE)
		ALIGN float tmp1[4], tmp2[4], tmpfp[4];
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 		
		tmp1[0] = (float)rs_buf[ofsi], tmp2[0] = (float)rs_buf[++ofsi], tmpfp[0] = (float)(rs_ofs & FRACTION_MASK);
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmp1[1] = (float)rs_buf[ofsi], tmp2[1] = (float)rs_buf[++ofsi], tmpfp[1] = (float)(rs_ofs & FRACTION_MASK);
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmp1[2] = (float)rs_buf[ofsi], tmp2[2] = (float)rs_buf[++ofsi], tmpfp[2] = (float)(rs_ofs & FRACTION_MASK);
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmp1[3] = (float)rs_buf[ofsi], tmp2[3] = (float)rs_buf[++ofsi], tmpfp[3] = (float)(rs_ofs & FRACTION_MASK);
		vv1 = _mm_load_ps(tmp1);	
		vv2 = _mm_load_ps(tmp2);	
		vfp = _mm_load_ps(tmpfp);
#else // defined(IS_RS_DATA_T_FLOAT)
		ALIGN float tmpfp[4];
		__m128 tmp1, tmp2, tmp3, tmp4;
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmpfp[0] = (float)(rs_ofs & FRACTION_MASK);
		tmp1 = _mm_loadu_ps(&rs_buf[ofsi]); // L64bit ofsiとofsi+1をロード
		tmp1 = _mm_loadl_pi(tmp1, (__m64 *)&rs_buf[ofsi]); // L64bit ofsiとofsi+1をロード
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmpfp[1] = (float)(rs_ofs & FRACTION_MASK);
		tmp1 = _mm_loadh_pi(tmp1, (__m64 *)&rs_buf[ofsi]); // H64bit ofsiとofsi+1をロード
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmpfp[2] = (float)(rs_ofs & FRACTION_MASK);
		tmp3 = _mm_loadl_pi(tmp3, (__m64 *)&rs_buf[ofsi]); // L64bit ofsiとofsi+1をロード
		ofsi = (rs_ofs += is_rs_increment) >> FRACTION_BITS; 
		tmpfp[3] = (float)(rs_ofs & FRACTION_MASK);
		tmp3 = _mm_loadh_pi(tmp3, (__m64 *)&rs_buf[ofsi]); // H64bit ofsiとofsi+1をロード
		vv1 = _mm_shuffle_ps(tmp1, tmp3, 0x88); // v1[0,1,2,3]	// ofsiはv1に
		vv2 = _mm_shuffle_ps(tmp1, tmp3, 0xdd); // v2[0,1,2,3]	// ofsi+1はv2に移動	
		vfp = _mm_load_ps(tmpfp);
#endif
#if defined(DATA_T_DOUBLE)
		{
		ALIGN float out[4];
		_mm_storeu_ps(out, MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1));
		is_buf[i] = (DATA_T)out[0];
		is_buf[i + 1] = (DATA_T)out[1];
		is_buf[i + 2] = (DATA_T)out[2];
		is_buf[i + 3] = (DATA_T)out[3];
		}
#elif defined(DATA_T_FLOAT)
		_mm_storeu_ps(&is_buf[i], MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1));
#else // DATA_T_IN32
		{
		vec_out = MM_FMA_PS(_mm_sub_ps(vv2, vv1), _mm_mul_ps(vfp, vec_divf), vv1);		
		vec_out = _mm_mul_ps(vec_out, _mm_seti_ps(is_output_level));	
		is_buf[i] = _mm_cvt_ss2si(vec_out);
		is_buf[i + 1] = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xe5));
		is_buf[i + 2] = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xea));
		is_buf[i + 3] = _mm_cvt_ss2si(_mm_shuffle_ps(vec_out, vec_out, 0xff));
		}
#endif // DATA_T_INT32
	}
	}

#endif // USE_X86_EXT_INTRIN
	
	{		
		int32 ofsi, ofsf;
		FLOAT_T v1, v2, fp;
		for (; i < (count * is_rs_over_sampling); i++){
			rs_ofs += is_rs_increment;
			ofsi = rs_ofs >> FRACTION_BITS;
			ofsf = rs_ofs & FRACTION_MASK;
			fp = (FLOAT_T)ofsf * div_is_fraction;
			v1 = rs_buf[ofsi], v2 = rs_buf[ofsi + 1];
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
			is_buf[i] = v1 + (v2 - v1) * fp;
#else // DATA_T_INT32
			is_buf[i] = (v1 + (v2 - v1) * fp) * is_output_level;
#endif // DATA_T_INT32
		}

		// 
		ofsi = rs_ofs >> FRACTION_BITS;
		rs->offset = rs_ofs & FRACTION_MASK;
#if (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_DOUBLE)
		_mm_storeu_pd(rs->data, _mm_loadu_pd(&rs_buf[ofsi]));
#elif (USE_X86_EXT_INTRIN >= 3) && defined(IS_RS_DATA_T_FLOAT)
		{
		__m128 vtmp;
		_mm_storel_pi(rs->data, _mm_loadl_pi(vtmp, (__m64 *)&rs_buf[ofsi]));
		}
#else
		rs->data[0] = rs_buf[ofsi];
		rs->data[1] = rs_buf[ofsi + 1]; // interpolate		
#endif
		is_resample_down_sampling(is_buf, count);
	}
}


#endif // over sampling




//////// LA ROM
// mt32emu Code

const FLOAT_T la_base_freq = 442.0 / 440.0 * 261.626; // mt32_master_tuning / timidity_master_tuning * timidity_note60
const FLOAT_T la_sample_rate = 32000;
const LA_Ctrl_ROM_Map LA_Ctrl_ROM_Maps[7] = {
	// ID    IDc IDbytes                     PCMmap  PCMc  tmbrA   tmbrAO, tmbrAC tmbrB   tmbrBO, tmbrBC tmbrR   trC  rhythm  rhyC  rsrv    panpot  prog    rhyMax  patMax  sysMax  timMax
	{0x4014, 22, "\000 ver1.04 14 July 87 ", 0x3000,  128, 0x8000, 0x0000, 0x00, 0xC000, 0x4000, 0x00, 0x3200,  30, 0x73A6,  85,  0x57C7, 0x57E2, 0x57D0, 0x5252, 0x525E, 0x526E, 0x520A},
	{0x4014, 22, "\000 ver1.05 06 Aug, 87 ", 0x3000,  128, 0x8000, 0x0000, 0x00, 0xC000, 0x4000, 0x00, 0x3200,  30, 0x7414,  85,  0x57C7, 0x57E2, 0x57D0, 0x5252, 0x525E, 0x526E, 0x520A},
	{0x4014, 22, "\000 ver1.06 31 Aug, 87 ", 0x3000,  128, 0x8000, 0x0000, 0x00, 0xC000, 0x4000, 0x00, 0x3200,  30, 0x7414,  85,  0x57D9, 0x57F4, 0x57E2, 0x5264, 0x5270, 0x5280, 0x521C},
	{0x4010, 22, "\000 ver1.07 10 Oct, 87 ", 0x3000,  128, 0x8000, 0x0000, 0x00, 0xC000, 0x4000, 0x00, 0x3200,  30, 0x73fe,  85,  0x57B1, 0x57CC, 0x57BA, 0x523C, 0x5248, 0x5258, 0x51F4}, // MT-32 revision 1
	{0x4010, 22, "\000verX.XX  30 Sep, 88 ", 0x3000,  128, 0x8000, 0x0000, 0x00, 0xC000, 0x4000, 0x00, 0x3200,  30, 0x741C,  85,  0x57E5, 0x5800, 0x57EE, 0x5270, 0x527C, 0x528C, 0x5228}, // MT-32 Blue Ridge mod
	{0x2205, 22, "\000CM32/LAPC1.00 890404", 0x8100,  256, 0x8000, 0x8000, 0x00, 0x8080, 0x8000, 0x00, 0x8500,  64, 0x8580,  85,  0x4F65, 0x4F80, 0x4F6E, 0x48A1, 0x48A5, 0x48BE, 0x48D5},
	{0x2205, 22, "\000CM32/LAPC1.02 891205", 0x8100,  256, 0x8000, 0x8000, 0x01,  0x8080, 0x8000, 0x01,  0x8500,  64, 0x8580,  85,  0x4F93, 0x4FAE, 0x4F9C, 0x48CB, 0x48CF, 0x48E8, 0x48FF}  // CM-32L
	// (Note that all but CM-32L ROM actually have 86 entries for rhythmTemp)
};

const char *la_pcm_data_name[] = {
	"Acoustic Bass Drum",
	"Acoustic Snare Drum",
	"Electric Snare Drum",
	"Acoustic Tom-Tom",
	"Closed Hi-Hat Attack",
	"Open Hi-Hat Tail",
	"Crash Cymbal Attack",
	"Crash Cymbal Tail",
	"Ride Cymbal Attack",
	"Rim Shot",
	"Hand Clap",
	"Mute Conga",
	"High Conga",
	"Bongo",
	"Cowbell",
	"Tambourine",
	"Agogo Bell",
	"Claves",
	"Timbale",
	"Cabassa",
	"Acoustic Piano",
	"Ham & Organ Attack",
	"Trombone",
	"Trumpet",
	"Breath Noise",
	"Clarinet",
	"Flute",
	"Steamer",
	"Shaku-Hachi",
	"Alto Sax",
	"Baritone Sax",
	"Marimba",
	"Vibraphone",
	"Xylophone",
	"Windbell",
	"Fretless Bass",
	"Slap Bass Attack",
	"Slap Bass Tail",
	"Acoustic Bass",
	"Gut Guitar",
	"Steel Guitar",
	"Pizzicato Strings",
	"Harp",
	"Harpsichord",
	"Contrabass",
	"Violin",
	"Timpani",
	"Orchestra Hit",
	"Indian Flute",
	"Ham & Organ Loop",
	"Bell",
	"Telephone",
	"Ethnic",
	"Stainless Steel",
	"Loop 01 Acoustic Bass Drum",
	"Loop 02 Acoustic Snare Drum",
	"Loop 03 Electric Snare Drum",
	"Loop 04 Acoustic Tom-Tom",
	"Loop 05 Closed Hi-Hat Attack",
	"Loop 06 Crash Cymbal Attack",
	"Loop 07 Ride Cymbal Attack",
	"Loop 08 Ride Cymbal Attack short",
	"Loop 09 Rim Shot",
	"Loop 10 Hand Clap",
	"Loop 11 Mute Conga",
	"Loop 12 High Conga",
	"Loop 13 Bongo",
	"Loop 14 Cowbell",
	"Loop 15 Tambourine ",
	"Loop 16 Agogo Bell",
	"Loop 17 Claves",
	"Loop 18 Timbale ",
	"Loop 19 Cabassa",
	"Loop 20 Acoustic Piano",
	"Loop 21 Ham & Organ Attack",
	"Loop 22 Trombone",
	"Loop 23 Trumpet ",
	"Loop 24 Clarinet",
	"Loop 25 Flute",
	"Loop 26 Steamer",
	"Loop 27 Shaku-Hachi",
	"Loop 28 Alto Sax",
	"Loop 29 Baritone Sax",
	"Loop 30 Marimba",
	"Loop 31 Vibraphone",
	"Loop 32 Xylophone",
	"Loop 33 Windbell",
	"Loop 34 Fretless Bass",
	"Loop 35 Slap Bass Attack",
	"Loop 36 Acoustic Bass",
	"Loop 37 Gut Guitar",
	"Loop 38 Steel Guitar",
	"Loop 39 Pizzicato Strings",
	"Loop 40 Harp",
	"Loop 41 Contrabass",
	"Loop 42 Violin",
	"Loop 43 Timpani",
	"Loop 44 Orchestra Hit",
	"Loop 45 Indian Flute",
	"Loop 46 ",
	"Loop 47 ",
	"Loop 48 ",
	"Loop 49 ",
	"Loop 50 ",
	"Loop 51 ",
	"Loop 52 ",
	"Loop 53 ",
	"Loop 54 ",
	"Loop 55 ",
	"Loop 56 ",
	"Loop 57 ",
	"Loop 58 ",
	"Loop 59 ",
	"Loop 60 ",
	"Loop 61 ",
	"Loop 62 ",
	"Loop 63 ",
	"Loop 64 ",
	"Loop 65 ",
	"Loop 66 ",
	"Loop 67 ",
	"Loop 68 ",
	"Loop 69 ",
	"Loop 70 ",
	"Loop 71 ",
	"Loop 72 ",
	"Loop 73 ",
	"Loop 74 ",
	"Laughing",
	"Applause",
	"Windchime",
	"Crash",
	"Train",
	"Rain",
	"Birds",
	"Stream",
	"Creaking",
	"Screaming",
	"Punch",
	"Footsteps",
	"Door",
	"Engine Start",
	"Jet",
	"Pistol",
	"Horse",
	"Thunder",
	"Bubble",
	"Heartbeat",
	"Car-pass ",
	"Car-stop",
	"Siren",
	"Helicopter",
	"Dog",
	"Wave 153",
	"Wave 154",
	"Machinegun",
	"Starship",
	"Loop Laughing",
	"Loop Applause",
	"Loop Windchime",
	"Loop Crash",
	"Loop Train",
	"Loop Rain",
	"Loop Birds",
	"Loop Stream",
	"Loop Creaking",
	"Loop Screaming",
	"Loop Punch",
	"Loop Footsteps",
	"Loop Door",
	"Loop Engine Start",
	"Loop Jet",
	"Loop Pistol",
	"Loop Horse",
	"Loop Thunder",
	"Loop Bubble",
	"Loop Heartbeat",
	"Loop Engine",
	"Loop Car-stop",
	"Loop Siren",
	"Loop Helicopter",
	"Loop Dog",
	"Loop 153",
	"Loop 154",
	"Loop Machinegun",
	"Loop Starship",
	"Wave 186 Loop",
	"Wave 187 Loop",
	"Wave 188 Loop",
	"Wave 189 Loop",
	"Wave 190 Loop",
	"Wave 191 Loop",
	"Wave 192 Loop",
	"Wave 193 Loop",
	"Wave 194 Loop",
	"Wave 195 Loop",
	"Wave 196 Loop",
	"Wave 197 Loop",
	"Wave 198 Loop",
	"Wave 199 Loop",
	"Wave 200 Loop",
	"Wave 201 Loop",
	"Wave 202 Loop",
	"Wave 203 Loop",
	"Wave 204 Loop",
	"Wave 205 Loop",
	"Wave 206 Loop",
	"Wave 207 Loop",
	"Wave 208 Loop",
	"Wave 209 Loop",
	"Wave 210 Loop",
	"Wave 211 Loop",
	"Wave 212 Loop",
	"Wave 213 Loop",
	"Wave 214 Loop",
	"Wave 215 Loop",
	"Wave 216 Loop",
	"Wave 217 Loop",
	"Wave 218 Loop",
	"Wave 219 Loop",
	"Wave 220 Loop",
	"Wave 221 Loop",
	"Wave 222 Loop",
	"Wave 223 Loop",
	"Wave 224 Loop",
	"Wave 225 Loop",
	"Wave 226",
	"Wave 227",
	"Wave 228",
	"Wave 229",
	"Wave 230",
	"Wave 231",
	"Wave 232",
	"Wave 233",
	"Wave 234",
	"Wave 235",
	"Wave 236",
	"Wave 237",
	"Wave 238",
	"Wave 239",
	"Wave 240",
	"Wave 241",
	"Wave 242",
	"Wave 243",
	"Wave 244",
	"Wave 245",
	"Wave 246",
	"Wave 247",
	"Wave 248",
	"Wave 249",
	"Wave 250",
	"Wave 251",
	"Wave 252",
	"Wave 253",
	"Wave 254",
	"Wave 255",
};

static void la_ctrl_rom_conv(struct timidity_file *tf, Info_PCM *inf, int rom_size)
{
	uint8 *ctrl_rom = NULL;
	LA_Ctrl_ROM_Map *ctrl_map = NULL;
	LA_Ctrl_ROM_PCM_Struct *tps = NULL;
	int i, pitch;
	FLOAT_T frq;

	ctrl_rom = (uint8 *)safe_malloc(LA_CONTROL_ROM_SIZE);
	if(!ctrl_rom)
		return;
	memset(ctrl_rom, 0, LA_CONTROL_ROM_SIZE);
	if(!tf_read(ctrl_rom, 1, LA_CONTROL_ROM_SIZE, tf)){
		safe_free(ctrl_rom);
		return;
	}
	for (i = 0; i < sizeof(LA_Ctrl_ROM_Maps) / sizeof(LA_Ctrl_ROM_Maps[0]); i++) {
		if (memcmp(&ctrl_rom[LA_Ctrl_ROM_Maps[i].idPos], LA_Ctrl_ROM_Maps[i].idBytes, LA_Ctrl_ROM_Maps[i].idLen) == 0) {
			ctrl_map = (LA_Ctrl_ROM_Map *)&LA_Ctrl_ROM_Maps[i];
			break;
		}
	}
	memset(inf, 0, sizeof(Info_PCM) * ctrl_map->pcmCount);
	tps = (LA_Ctrl_ROM_PCM_Struct *)&ctrl_rom[ctrl_map->pcmTable];
	for (i = 0; i < ctrl_map->pcmCount; i++) {
		uint32 rAddr = tps[i].pos * 0x800;
		uint32 rLen = 0x800 << ((tps[i].len & 0x70) >> 4);
		if (rAddr + rLen > rom_size) {
			return; // Control ROM error
		}
		inf[i].loop = (tps[i].len & 0x80) != 0;
		inf[i].ofs_start = rAddr;
		inf[i].ofs_end = rAddr + rLen;
		inf[i].loop_start = rAddr;
		inf[i].loop_end = rAddr + rLen;
		inf[i].loop_length = rLen;
		pitch = (int32)((((int32)tps[i].pitchMSB) << 8) | (int32)tps[i].pitchLSB) - 20480;
		frq = pow(2.0, (FLOAT_T)(-pitch) * DIV_12BIT) * la_base_freq; // c5=20480 , +1oct=-4096 12bit
		inf[i].root_freq = frq;
		inf[i].div_root_freq = 1.0 / frq; // 1/root_freq
		inf[i].sample_rate = la_sample_rate;
	}
	safe_free(ctrl_rom);
}

static void la_pcm_rom_conv(struct timidity_file *tf, FLOAT_T *buf, int rom_size, int pcm_size)
{
	uint8 *pcm_rom = NULL;
	int i = 0, j = 0;
//	FLOAT_T maxamp = 0;

	pcm_rom = (uint8 *)safe_malloc(rom_size);
	if(!pcm_rom)
		return;
	memset(pcm_rom, 0, rom_size);
	if(!tf_read(pcm_rom, 1, rom_size, tf)){
		safe_free(pcm_rom);
		return;
	}
	for (;;) {
		int16 s = 0, c = 0; 
		int16 e = 0;
		int u, z = 15;
		int order[16] = {0, 9, 1 ,2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 8};
		FLOAT_T expval, vol;

		s = pcm_rom[j++];
		c = pcm_rom[j++];
		for(u = 0; u < 15; u++) {
			int bit;

			if((order[u] < 8) && (order[u] >= 0)) {
				bit = (s >> (7-order[u])) & 0x1;
			} else {
				if(order[u] >= 8) {
					bit = (c >> (7 - (order[u] - 8))) & 0x1;
				} else {
					bit = 0;
				}
			}
			e = e | (bit << z);
			--z;
		}
		if(e < 0) 
			expval = -32767 - e;
		else
			expval = -e;
		vol = pow(pow((expval * DIV_15BIT), 32.0), DIV_3);
		if (e > 0)
			vol = -vol;
		//expval = fabs(vol);
		//if(expval > maxamp)
		//	maxamp = expval;
		// test maxamp=0.99934915035888361		
		//buf[i] = (int)(vol * M_23BIT) * DIV_23BIT; // 
		buf[i] = vol;
		i++;
		if(i >= pcm_size)
			break; // error
		if(j >= rom_size)
			break; // error
	}
	buf[pcm_size] = 0; // for interpolation
	safe_free(pcm_rom);
}

void load_la_rom(void)
{	
	struct timidity_file *tf;

	if(la_pcm_data_load_flg)
		return;
	la_pcm_data_load_flg = 1;
	// MT32 ctrl
	if (tf = open_file("MT32_CONTROL.ROM", 1, OF_NORMAL)){
		la_ctrl_rom_conv(tf, mt32_pcm_inf, MT32_PCM_ROM_SIZE);
		close_file(tf);
	}
	// MT32 pcm
	if (tf = open_file("MT32_PCM.ROM", 1, OF_NORMAL)){
		la_pcm_rom_conv(tf, mt32_pcm_data, MT32_PCM_ROM_SIZE, MT32_PCM_SIZE);
		close_file(tf);
	}
	// CM32L ctrl
	if (tf = open_file("CM32L_CONTROL.ROM", 1, OF_NORMAL)){
		la_ctrl_rom_conv(tf, cm32l_pcm_inf, CM32L_PCM_ROM_SIZE);
		close_file(tf);
	}
	// CM32L pcm
	if (tf = open_file("CM32L_PCM.ROM", 1, OF_NORMAL)){
		la_pcm_rom_conv(tf, cm32l_pcm_data, CM32L_PCM_ROM_SIZE, CM32L_PCM_SIZE);
		close_file(tf);
	}
}




//////// load setting (int_synth.ini

//// SCC data
static void config_parse_scc_data(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_DATA_LENGTH; i++){
		set->scc_data_int[setting][i] = 0; // init param
		set->scc_data[setting][i] = 0; // init param
	}
    set->scc_data[setting][SCC_DATA_LENGTH] = 0;

	/* regist */
	for (i = 0; i < SCC_DATA_LENGTH; i++, p++) {
		if (*p == ':')
			continue;
		tmp = atoi(p);
		if(tmp >= 128)
			tmp = 128;
		else if(tmp < -128)
			tmp = -128;
		set->scc_data_int[setting][i] = (int16)tmp;
		set->scc_data[setting][i] = (double)tmp * DIV_128;
		if (! (p = strchr(p, ':')))
			break;
	}
	set->scc_data[setting][SCC_DATA_LENGTH] = set->scc_data[setting][0]; // for interpolation
}

#if 0
//// MT32 data
static void config_parse_mt32_data(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	FLOAT_T frq = 261.626; // note60
	FLOAT_T div_frq = 1.0 / frq;
	
	/* count num */
	p = cp;
	/* init */	
	// loop 0:off 1:on, ofs_start, ofs_end, loop_start, loop_end, sample_rate [Hz], root_freq [0.001Hz]
	mt32_pcm_inf[setting].loop = 0;
	mt32_pcm_inf[setting].ofs_start = 0;
	mt32_pcm_inf[setting].ofs_end = 0;
	mt32_pcm_inf[setting].loop_start = 0;
	mt32_pcm_inf[setting].loop_end = 0;
	mt32_pcm_inf[setting].sample_rate = 32000;
	mt32_pcm_inf[setting].root_freq = frq;
	mt32_pcm_inf[setting].div_root_freq = div_frq; // 1/root_freq
	/* regist */
	for (i = 0; i < 7; i++, p++) {
		if (*p == ':')
			continue;
		tmp = atoi(p);
		if(tmp < 0)
			break;
		switch(i){
		case 0:
			mt32_pcm_inf[setting].loop = tmp;
			break;
		case 1:
			mt32_pcm_inf[setting].ofs_start = tmp;
			mt32_pcm_inf[setting].loop_start = tmp;
			break;
		case 2:
			mt32_pcm_inf[setting].ofs_end = tmp;
			mt32_pcm_inf[setting].loop_end = tmp;
			break;
		case 3:
			if(tmp == 0)
				break;
			tmp *= DIV_1000;
			mt32_pcm_inf[setting].root_freq = tmp;
			mt32_pcm_inf[setting].div_root_freq = 1.0 / (double)tmp;
			break;
		default:
			break;
		}
		if (! (p = strchr(p, ':')))
			break;
	}
}

//// CM32L data
static void config_parse_cm32l_data(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	FLOAT_T frq = 261.626; // note60
	FLOAT_T div_frq = 1.0 / frq;
	
	/* count num */
	p = cp;
	/* init */	
	// loop 0:off 1:on, ofs_start, ofs_end, loop_start, loop_end, sample_rate [Hz], root_freq [0.001Hz]
	cm32l_pcm_inf[setting].loop = 0;
	cm32l_pcm_inf[setting].ofs_start = 0;
	cm32l_pcm_inf[setting].ofs_end = 0;
	cm32l_pcm_inf[setting].loop_start = 0;
	cm32l_pcm_inf[setting].loop_end = 0;
	cm32l_pcm_inf[setting].sample_rate = 32000;
	cm32l_pcm_inf[setting].root_freq = frq;
	cm32l_pcm_inf[setting].div_root_freq = div_frq; // 1/root_freq
	/* regist */
	for (i = 0; i < 7; i++, p++) {
		if (*p == ':')
			continue;
		tmp = atoi(p);
		if(tmp < 0)
			break;
		switch(i){
		case 0:
			cm32l_pcm_inf[setting].loop = tmp;
			break;
		case 1:
			cm32l_pcm_inf[setting].ofs_start = tmp;
			break;
		case 2:
			cm32l_pcm_inf[setting].ofs_end = tmp;
			break;
		case 3:
			cm32l_pcm_inf[setting].loop_start = tmp;
			break;
		case 4:
			cm32l_pcm_inf[setting].loop_end = tmp;
			break;
		case 5:
			cm32l_pcm_inf[setting].sample_rate = tmp;
			break;
		case 6:
			if(tmp == 0)
				break;
			tmp *= DIV_1000;
			cm32l_pcm_inf[setting].root_freq = tmp;
			cm32l_pcm_inf[setting].div_root_freq = 1.0 / (double)tmp;
			break;
		default:
			break;
		}
		if (! (p = strchr(p, ':')))
			break;
	}
}
#endif

//// SCC
static void config_parse_scc_mode(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_MODE_MAX; i++)
		set->scc_setting[setting]->mode[i] = 0; // init param
	/* regist */
	for (i = 0; i < SCC_MODE_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->mode[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_param(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_PARAM_MAX; i++)
		set->scc_setting[setting]->param[i] = 0; // init param
	/* regist */
	for (i = 0; i < SCC_PARAM_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->param[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_osc(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_OSC_MAX; i++)
		set->scc_setting[setting]->osc[i] = 0; // init param
	/* regist */
	for (i = 0; i < SCC_OSC_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->osc[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_amp(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_AMP_MAX; i++)
		set->scc_setting[setting]->amp[i] = 0; // init param
	/* regist */
	for (i = 0; i < SCC_AMP_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->amp[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_pitch(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_PITCH_MAX; i++)
		set->scc_setting[setting]->pitch[i] = 0; // init param
	/* regist */
	for (i = 0; i < SCC_PITCH_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->pitch[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_ampenv(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_ENV_PARAM; i++)
		set->scc_setting[setting]->ampenv[i] = 0; // init param
	set->scc_setting[setting]->ampenv[2] = 100; // init param
	/* regist */
	for (i = 0; i < SCC_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->ampenv[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_pitenv(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < SCC_ENV_PARAM; i++)
		set->scc_setting[setting]->pitenv[i] = 0; // init param
	set->scc_setting[setting]->pitenv[2] = 100; // init param
	/* regist */
	for (i = 0; i < SCC_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->pitenv[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_lfo1(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
//	for (i = 0; i < SCC_LFO_PARAM; i++)
//		set->scc_setting[setting]->lfo1[i] = 0; // init param
	set->scc_setting[setting]->lfo1[0] = 0; // init param
	set->scc_setting[setting]->lfo1[1] = 0; // init param
	set->scc_setting[setting]->lfo1[2] = 0; // init param
	set->scc_setting[setting]->lfo1[3] = 200; // init param
	/* regist */
	for (i = 0; i < SCC_LFO_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->lfo1[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_scc_lfo2(const char *cp, Preset_IS *set, int setting)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
//	for (i = 0; i < SCC_LFO_PARAM; i++)
//		set->scc_setting[setting]->lfo2[i] = 0; // init param
	set->scc_setting[setting]->lfo2[0] = 0; // init param
	set->scc_setting[setting]->lfo2[1] = 0; // init param
	set->scc_setting[setting]->lfo2[2] = 0; // init param
	set->scc_setting[setting]->lfo2[3] = 200; // init param
	/* regist */
	for (i = 0; i < SCC_LFO_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->scc_setting[setting]->lfo2[i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}


//// MMS
static void config_parse_mms_op_param(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_PARAM_MAX; i++)
		set->mms_setting[setting]->op_param[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_PARAM_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_param[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_range(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	//for (i = 0; i < MMS_OP_RANGE_MAX; i++)
	//	set->mms_setting[setting]->op_range[op_num][i] = -1; // init param
	set->mms_setting[setting]->op_range[op_num][0] = 0;
	set->mms_setting[setting]->op_range[op_num][1] = 127;
	set->mms_setting[setting]->op_range[op_num][2] = 0;
	set->mms_setting[setting]->op_range[op_num][3] = 127;
	/* regist */
	for (i = 0; i < MMS_OP_RANGE_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_range[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_connect(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_CON_MAX; i++)
		set->mms_setting[setting]->op_connect[op_num][i] = -1; // init param
	/* regist */
	for (i = 0; i < MMS_OP_CON_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_connect[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_osc(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_OSC_MAX; i++)
		set->mms_setting[setting]->op_osc[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_OSC_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_osc[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_wave(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_WAVE_MAX; i++)
		set->mms_setting[setting]->op_wave[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_WAVE_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_wave[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_sub(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_SUB_MAX; i++)
		set->mms_setting[setting]->op_sub[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_SUB_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_sub[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_amp(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_AMP_MAX; i++)
		set->mms_setting[setting]->op_amp[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_AMP_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_amp[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_pitch(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_PITCH_MAX; i++)
		set->mms_setting[setting]->op_pitch[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_PITCH_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_pitch[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_width(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_WIDTH_MAX; i++)
		set->mms_setting[setting]->op_width[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_WIDTH_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_width[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_ampenv(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_ampenv[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_ampenv[op_num][2] = 100; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_ampenv[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_modenv(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_modenv[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_modenv[op_num][2] = 100; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_modenv[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_widenv(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_widenv[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_widenv[op_num][2] = 100; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_widenv[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_pitenv(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_pitenv[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_pitenv[op_num][2] = 100; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_pitenv[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_ampenv_keyf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_ampenv_keyf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_ampenv_keyf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_modenv_keyf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_modenv_keyf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_modenv_keyf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_widenv_keyf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_widenv_keyf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_widenv_keyf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_pitenv_keyf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_pitenv_keyf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_pitenv_keyf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_ampenv_velf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_ampenv_velf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_ampenv_velf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_modenv_velf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_modenv_velf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_modenv_velf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_widenv_velf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_widenv_velf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_widenv_velf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_pitenv_velf(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++)
		set->mms_setting[setting]->op_pitenv_velf[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_ENV_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_pitenv_velf[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_lfo1(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
//	for (i = 0; i < MMS_OP_LFO_PARAM; i++)
//		set->mms_setting[setting]->op_lfo1[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_lfo1[op_num][0] = 0; // init param
	set->mms_setting[setting]->op_lfo1[op_num][1] = 0; // init param
	set->mms_setting[setting]->op_lfo1[op_num][2] = 0; // init param
	set->mms_setting[setting]->op_lfo1[op_num][3] = 200; // init param
	/* regist */
	for (i = 0; i < MMS_OP_LFO_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_lfo1[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_lfo2(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
//	for (i = 0; i < MMS_OP_LFO_PARAM; i++)
//		set->mms_setting[setting]->op_lfo2[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_lfo2[op_num][0] = 0; // init param
	set->mms_setting[setting]->op_lfo2[op_num][1] = 0; // init param
	set->mms_setting[setting]->op_lfo2[op_num][2] = 0; // init param
	set->mms_setting[setting]->op_lfo2[op_num][3] = 200; // init param
	/* regist */
	for (i = 0; i < MMS_OP_LFO_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_lfo2[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_lfo3(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
//	for (i = 0; i < MMS_OP_LFO_PARAM; i++)
//		set->mms_setting[setting]->op_lfo3[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_lfo3[op_num][0] = 0; // init param
	set->mms_setting[setting]->op_lfo3[op_num][1] = 0; // init param
	set->mms_setting[setting]->op_lfo3[op_num][2] = 0; // init param
	set->mms_setting[setting]->op_lfo3[op_num][3] = 200; // init param
	/* regist */
	for (i = 0; i < MMS_OP_LFO_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_lfo3[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_lfo4(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
//	for (i = 0; i < MMS_OP_LFO_PARAM; i++)
//		set->mms_setting[setting]->op_lfo4[op_num][i] = 0; // init param
	set->mms_setting[setting]->op_lfo4[op_num][0] = 0; // init param
	set->mms_setting[setting]->op_lfo4[op_num][1] = 0; // init param
	set->mms_setting[setting]->op_lfo4[op_num][2] = 0; // init param
	set->mms_setting[setting]->op_lfo4[op_num][3] = 200; // init param
	/* regist */
	for (i = 0; i < MMS_OP_LFO_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_lfo4[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_filter(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_FLT_PARAM; i++)
		set->mms_setting[setting]->op_filter[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_FLT_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_filter[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_cutoff(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_CUT_PARAM; i++)
		set->mms_setting[setting]->op_cutoff[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_CUT_PARAM; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_cutoff[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void config_parse_mms_op_mode(const char *cp, Preset_IS *set, int setting, int op_num)
{
	const char *p, *px;
	int i, tmp;
	
	/* count num */
	p = cp;
	/* init */
	for (i = 0; i < MMS_OP_MODE_MAX; i++)
		set->mms_setting[setting]->op_mode[op_num][i] = 0; // init param
	/* regist */
	for (i = 0; i < MMS_OP_MODE_MAX; i++, p++) {
		if (*p == ':')
			continue;
		set->mms_setting[setting]->op_mode[op_num][i] = atoi(p);
		if (! (p = strchr(p, ':')))
			break;
	}
}

static void conv_preset_envelope_param(int max, int32 *env, int16 *velf, int16 *keyf)
{
	int i;
	
	for(i = 6; i < max; i++)
		if(env[i] != 0) return; // use env_ext param
	// env1 to env2
	env[13] = env[4]; // offdelay
	env[12] = env[4]; // delay
	env[11] = 0; // release3 level
	env[10] = 0; // release3 time
	env[9] = 0; // release2 level
	env[8] = 0; // release2 time
	env[7] = 0; // release1 level
	env[6] = env[3]; // release1 time
	env[5] = env[2]; // decay level 
	env[4] = env[1]; // decay time
	env[3] = 100; // offset hold level
	env[2] = 0; // hold time
	env[1] = 100; // atk level
	env[0] = env[0]; // atk time
	// velf1 to velf2
	if(!velf) return; // scc not define velf/keyf
	velf[13] = 0;
	velf[12] = 0;
	velf[11] = 0; // release3 level
	velf[10] = 0; // release3 time
	velf[9] = 0; // release2 level
	velf[8] = 0; // release2 time
	velf[7] = 0; // release1 level
	velf[6] = velf[3]; // release1 time
	velf[5] = velf[2]; // decay level (sustain level
	velf[4] = velf[1]; // decay time
	velf[3] = 0; // hold level
	velf[2] = 0; // hold time
	velf[1] = 0; // atk level
	velf[0] = velf[0]; // atk time
	// keyf1 to keyf2
	if(!keyf) return; // scc not define velf/keyf
	keyf[13] = 0;
	keyf[12] = 0;
	keyf[11] = 0; // release3 level
	keyf[10] = 0; // release2 time
	keyf[9] = 0; // release2 level
	keyf[8] = 0; // release2 time
	keyf[7] = 0; // release1 level
	keyf[6] = keyf[3]; // release1 time
	keyf[5] = keyf[2]; // decay level (sustain level
	keyf[4] = keyf[1];
	keyf[3] = 0; // hold level
	keyf[2] = 0; // hold time
	keyf[1] = 0; // atk level
	keyf[0] = keyf[0]; // atk time
}

static void load_is_scc_data(INIDATA *ini, Preset_IS *set)
{
	LPINISEC sec = NULL;
	char tbf[258] = "";
	char *p = NULL;
	char name[30] = "";
	char name2[30] = "";
	int i;

	sec = MyIni_GetSection(ini, "SCC_DATA", 0);
	for(i = 0; i < SCC_DATA_MAX; i++){
		snprintf(name, sizeof(name), "data_%03d", i); // 桁数指定
		snprintf(name2, sizeof(name2), "name_%03d", i); // 桁数指定
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0");
		config_parse_scc_data(p, set, i);	
		p = MyIni_GetString(sec, name2, tbf, 256, "none");
		if(set->scc_data_name[i] != NULL)
			safe_free(set->scc_data_name[i]);
		set->scc_data_name[i] = safe_strdup(p);
	}
}

static void load_is_scc_preset(INIDATA *ini, Preset_IS *set, int preset, int init)
{
	LPINISEC sec = NULL;
	char tbf[258] = "";
	char *p = NULL;
	char name[30] = "";
	char name2[30] = "";
	void *mem = NULL;
	int i = preset;
	Preset_SCC *set2 = NULL;
	
	snprintf(name, sizeof(name), "SCC_%03d", i); // 桁数指定
	sec = MyIni_GetSection(ini, name, 0);
	if(sec == NULL && !init)
		return;
	mem = safe_malloc(sizeof(Preset_SCC));
	if(!mem)
		return;
	memset(mem, 0, sizeof(Preset_SCC));
	set->scc_setting[i] = (Preset_SCC *)mem;
	set2 = set->scc_setting[i];
	p = MyIni_GetString(sec, "name", tbf, 256, "SCC none");
//	snprintf(name, sizeof(name), "[SCC] %s", p);
	if(set->scc_setting[i]->inst_name != NULL)
		safe_free(set->scc_setting[i]->inst_name);
//	scc_setting[i].inst_name = safe_strdup(name);
	set->scc_setting[i]->inst_name = safe_strdup(p);
	p = MyIni_GetString(sec, "mode", tbf, 256, "0:0");
	config_parse_scc_mode(p, set, i);
	p = MyIni_GetString(sec, "param", tbf, 256, "100:0");
	config_parse_scc_param(p, set, i);
	p = MyIni_GetString(sec, "osc", tbf, 256, "0:100:0:0:0:0:0:0");
	config_parse_scc_osc(p, set, i);
	p = MyIni_GetString(sec, "amp", tbf, 256, "0:0:0");
	config_parse_scc_amp(p, set, i);
	p = MyIni_GetString(sec, "pitch", tbf, 256, "0:0:0");
	config_parse_scc_pitch(p, set, i);
#define IS_SCC_ENV_DEFAULT "0:0:100:0:0:0:0:0:0:0:0:0:0:0"
	p = MyIni_GetString(sec, "ampenv", tbf, 256, IS_SCC_ENV_DEFAULT);
	config_parse_scc_ampenv(p, set, i);
	p = MyIni_GetString(sec, "pitenv", tbf, 256, IS_SCC_ENV_DEFAULT);
	config_parse_scc_pitenv(p, set, i);
#define IS_SCC_LFO_DEFAULT "0:0:0:0"
	p = MyIni_GetString(sec, "lfo1", tbf, 256, IS_SCC_LFO_DEFAULT);
	config_parse_scc_lfo1(p, set, i);
	p = MyIni_GetString(sec, "lfo2", tbf, 256, IS_SCC_LFO_DEFAULT);
	config_parse_scc_lfo2(p, set, i);
	// conv_preset_envelope
	conv_preset_envelope_param(SCC_ENV_PARAM, set2->ampenv, NULL, NULL);
	conv_preset_envelope_param(SCC_ENV_PARAM, set2->pitenv, NULL, NULL);
}
	
static void load_is_mms_preset(INIDATA *ini, Preset_IS *set, int preset, int init)
{
	LPINISEC sec = NULL;
	char tbf[258] = "";
	char *p = NULL;
	char name[30] = "";
	char name2[30] = "";
	void *mem = NULL;
	int i = preset, j, k, param;
	Preset_MMS *set2 = NULL;
	
	snprintf(name, sizeof(name), "MMS_%03d", i); // 桁数指定
	sec = MyIni_GetSection(ini, name, 0);
	if(sec == NULL && !init)
		return;
	mem = safe_malloc(sizeof(Preset_MMS));
	if(!mem)
		return;
	memset(mem, 0, sizeof(Preset_MMS));
	set->mms_setting[i] = (Preset_MMS *)mem;
	set2 = set->mms_setting[i];
	p = MyIni_GetString(sec, "name", tbf, 256, "MMS none");
//	snprintf(name, sizeof(name), "[MMS] %s", p);
	if(set->mms_setting[i]->inst_name != NULL)
		safe_free(set->mms_setting[i]->inst_name);
//	set->mms_setting[i].inst_name = safe_strdup(name);
	set->mms_setting[i]->inst_name = safe_strdup(p);
	set->mms_setting[i]->op_max = MyIni_GetInt32(sec, "op_max", 1);
	for(j = 0; j < MMS_OP_MAX; j++){
		snprintf(name, sizeof(name), "op_%d_mode", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0");
		config_parse_mms_op_mode(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_range", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:127:0:127");
		config_parse_mms_op_range(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_param", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:100:100");
		config_parse_mms_op_param(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_connect", j);
		p = MyIni_GetString(sec, name, tbf, 256, "-1");
		config_parse_mms_op_connect(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_osc", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:100:0:0");
		config_parse_mms_op_osc(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_wave", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:100:0:0:0");
		config_parse_mms_op_wave(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_sub", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0");
		config_parse_mms_op_sub(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_amp", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0");
		config_parse_mms_op_amp(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_pitch", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0");
		config_parse_mms_op_pitch(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_width", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0");
		config_parse_mms_op_width(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_filter", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0");
		config_parse_mms_op_filter(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_cutoff", j);
		p = MyIni_GetString(sec, name, tbf, 256, "0:0:0:0");
		config_parse_mms_op_cutoff(p, set, i, j);
#define IS_MMS_ENV_DEFAULT "0:0:100:0:0:0:0:0:0:0:0:0:0:0"
		snprintf(name, sizeof(name), "op_%d_ampenv", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENV_DEFAULT);
		config_parse_mms_op_ampenv(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_modenv", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENV_DEFAULT);
		config_parse_mms_op_modenv(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_widenv", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENV_DEFAULT);
		config_parse_mms_op_widenv(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_pitenv", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENV_DEFAULT);
		config_parse_mms_op_pitenv(p, set, i, j);
#define IS_MMS_ENVF_DEFAULT "0:0:0:0:0:0:0:0:0:0:0:0:0:0"
		snprintf(name, sizeof(name), "op_%d_ampenv_keyf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_ampenv_keyf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_modenv_keyf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_modenv_keyf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_widenv_keyf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_widenv_keyf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_pitenv_keyf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_pitenv_keyf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_ampenv_velf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_ampenv_velf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_modenv_velf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_modenv_velf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_widenv_velf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_widenv_velf(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_pitenv_velf", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_ENVF_DEFAULT);
		config_parse_mms_op_pitenv_velf(p, set, i, j);
#define IS_MMS_LFO_DEFAULT "0:0:0:0"
		snprintf(name, sizeof(name), "op_%d_lfo1", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_LFO_DEFAULT);
		config_parse_mms_op_lfo1(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_lfo2", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_LFO_DEFAULT);
		config_parse_mms_op_lfo2(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_lfo3", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_LFO_DEFAULT);
		config_parse_mms_op_lfo3(p, set, i, j);
		snprintf(name, sizeof(name), "op_%d_lfo4", j);
		p = MyIni_GetString(sec, name, tbf, 256, IS_MMS_LFO_DEFAULT);
		config_parse_mms_op_lfo4(p, set, i, j);
		// conv_preset_envelop
		conv_preset_envelope_param(MMS_OP_ENV_PARAM, set2->op_ampenv[j], set2->op_ampenv_velf[j], set2->op_ampenv_keyf[j]);
		conv_preset_envelope_param(MMS_OP_ENV_PARAM, set2->op_modenv[j], set2->op_modenv_velf[j], set2->op_modenv_keyf[j]);
		conv_preset_envelope_param(MMS_OP_ENV_PARAM, set2->op_widenv[j], set2->op_widenv_velf[j], set2->op_widenv_keyf[j]);
		conv_preset_envelope_param(MMS_OP_ENV_PARAM, set2->op_pitenv[j], set2->op_pitenv_velf[j], set2->op_pitenv_keyf[j]);
	}
}

// type -1:scc/mms 0:scc 1:mms , preset -1:all 0~:num
static void load_int_synth_preset(const char *inifile, Preset_IS *set, int type, int32 preset, int init)
{
	INIDATA ini={0};
	int i;
	
	MyIni_Load_timidity(&ini, inifile, 1, OF_VERBOSE);
	if(!set->scc_data_load){
		set->scc_data_load = 1;
		load_is_scc_data(&ini, set);
	}
	if(type == IS_INI_TYPE_ALL || type == IS_INI_TYPE_SCC){
		if(preset == IS_INI_PRESET_ALL){
			for(i = 0; i < SCC_SETTING_MAX; i++){
				if(set->scc_setting[i])
					continue;
				load_is_scc_preset(&ini, set, i, init);
			}
		}else if(preset < SCC_SETTING_MAX){			
			if(!set->scc_setting[preset])
				load_is_scc_preset(&ini, set, preset, init);
		}
#ifdef IS_INI_LOAD_BLOCK
		else if(preset > 0){	 
			int32 block = (preset >> 10) - 1; // 10bit>SETTING_MAX
			int32 min = block * IS_INI_LOAD_BLOCK; // 1block=64or128preset
			int32 max = min + IS_INI_LOAD_BLOCK; // 1block=64or128preset
			if(max > SCC_SETTING_MAX) 
				max = SCC_SETTING_MAX;
			for(i = min; i < max; i++){
				if(set->scc_setting[i])
					continue;
				load_is_scc_preset(&ini, set, i, init);
			}
		}
#endif
	}
	if(type == IS_INI_TYPE_ALL || type == IS_INI_TYPE_MMS){
		if(preset == IS_INI_PRESET_ALL){
			for(i = 0; i < MMS_SETTING_MAX; i++){
				if(set->mms_setting[i])
					continue;
				load_is_mms_preset(&ini, set, i, init);
			}
		}else if(preset < MMS_SETTING_MAX){	
			if(!set->mms_setting[preset])
				load_is_mms_preset(&ini, set, preset, init);
		}
#ifdef IS_INI_LOAD_BLOCK
		else if(preset > 0){	 
			int32 block = (preset >> 10) - 1; // 10bit>SETTING_MAX
			int32 min = block * IS_INI_LOAD_BLOCK; // 1block=64or128preset
			int32 max = min + IS_INI_LOAD_BLOCK; // 1block=64or128preset
			if(max > MMS_SETTING_MAX)
				max = MMS_SETTING_MAX;
			for(i = min; i < max; i++){
				if(set->mms_setting[i])
					continue;
				load_is_mms_preset(&ini, set, i, init);
			}
		}
#endif
	}
	MyIni_SectionAllClear(&ini);
}
	



//////// IS_EDITOR
Preset_IS is_editor_preset;
int scc_data_editor_override = 0; 
static int16 scc_data_int_edit[2][SCC_DATA_LENGTH]; // 0:working 1:temp
static FLOAT_T scc_data_edit[2][SCC_DATA_LENGTH + 1]; // 0:working 1:temp
int scc_editor_override = 0; 
static Preset_SCC scc_setting_edit[2]; // 0:working 1:temp
int mms_editor_override = 0; 
static Preset_MMS mms_setting_edit[2]; // 0:working 1:temp

char is_editor_inifile[FILEPATH_MAX] = "";
static char data_name_empty[] = "error\0";


const char *scc_data_editor_load_name(int num)
{
	if(num < 0 || num >= SCC_DATA_MAX)
		return data_name_empty;
	else
		return is_editor_preset.scc_data_name[num];
}

void scc_data_editor_store_name(int num, const char *name)
{
	if(num < 0 || num >= SCC_DATA_MAX)
		return;
	if(is_editor_preset.scc_data_name[num] != NULL)
		safe_free(is_editor_preset.scc_data_name[num]);
	is_editor_preset.scc_data_name[num] = safe_strdup(name);
}

void scc_data_editor_clear_param(void)
{
	memset(scc_data_int_edit[0], 0, sizeof(scc_data_int_edit[0]));
	memset(scc_data_edit[0], 0, sizeof(scc_data_edit[0]));
}

int scc_data_editor_get_param(int num)
{
	if(num < 0 || num >= SCC_DATA_LENGTH)
		return 0; // error
	return scc_data_int_edit[0][num];
}

void scc_data_editor_set_param(int num, int val)
{
	if(num < 0 || num >= SCC_DATA_LENGTH)
		return; // error	
	if(val >= 128)
		val = 128;
	else if(val < -128)
		val = -128;
	scc_data_int_edit[0][num] = val;
	scc_data_edit[0][num] = (double)val * DIV_128;
	if(num == 0)
		scc_data_edit[0][SCC_DATA_LENGTH] = scc_data_edit[0][0]; // for interpolation
}

static void scc_data_editor_save_ini(int num)
{
	INIDATA ini={0};
	char data[258] = "";
	char key1[30] = "";
	char key2[30] = "";
	int i;

	MyIni_Load(&ini, is_editor_inifile);
	snprintf(key1, sizeof(key1), "data_%03d", num); // 桁数指定
	snprintf(key2, sizeof(key2), "name_%03d", num); // 桁数指定	
	for(i = 0; i < SCC_DATA_LENGTH; i++){
		snprintf(data, sizeof(data), "%s%d:", data, is_editor_preset.scc_data_int[num][i]);
	}
	MyIni_SetString2(&ini, "SCC_DATA", key2, is_editor_preset.scc_data_name[num]);
	MyIni_SetString2(&ini, "SCC_DATA", key1, data);
	MyIni_Save(&ini, is_editor_inifile);
	MyIni_SectionAllClear(&ini);
}

void scc_data_editor_load_preset(int num)
{
	int i;

	if(num < 0){ 
		memcpy(&scc_data_int_edit[0], &scc_data_int_edit[1], sizeof(int16) * (SCC_DATA_LENGTH));
		memcpy(&scc_data_edit[0], &scc_data_edit[1], sizeof(FLOAT_T) * (SCC_DATA_LENGTH + 1));
	}else{
		if(num > (SCC_DATA_MAX - 1))
			return; // error
		memcpy(&scc_data_int_edit[0], &is_editor_preset.scc_data_int[num], sizeof(int16) * (SCC_DATA_LENGTH));
		memcpy(&scc_data_edit[0], &is_editor_preset.scc_data[num], sizeof(FLOAT_T) * (SCC_DATA_LENGTH + 1));
	}
}

void scc_data_editor_store_preset(int num)
{
	int i;

	if(num < 0){ 
		memcpy(&scc_data_int_edit[1], &scc_data_int_edit[0], sizeof(int16) * (SCC_DATA_LENGTH));
		memcpy(&scc_data_edit[1], &scc_data_edit[0], sizeof(FLOAT_T) * (SCC_DATA_LENGTH + 1));
		return;
	}else{
		if(num > (SCC_DATA_MAX - 1))
			return; // error
		memcpy(&is_editor_preset.scc_data_int[num], &scc_data_int_edit[0], sizeof(int16) * (SCC_DATA_LENGTH));
		memcpy(&is_editor_preset.scc_data[num], &scc_data_edit[0], sizeof(FLOAT_T) * (SCC_DATA_LENGTH + 1));
		scc_data_editor_save_ini(num);
	}
}

const char *scc_editor_load_name(int num)
{
	if(num < 0 || num >= SCC_SETTING_MAX)
		return data_name_empty;
	else if(!is_editor_preset.scc_setting[num])
		return data_name_empty;
	else
		return is_editor_preset.scc_setting[num]->inst_name;
}

void scc_editor_store_name(int num, const char *name)
{
	if(num < 0 || num >= SCC_SETTING_MAX)
		return;
	else if(!is_editor_preset.scc_setting[num])
		return;
	if(is_editor_preset.scc_setting[num]->inst_name != NULL)
		safe_free(is_editor_preset.scc_setting[num]->inst_name);
	is_editor_preset.scc_setting[num]->inst_name = safe_strdup(name);
}

const char *scc_editor_load_wave_name(int fnc)
{
	int type; // osc_type
	int num; // wave_type

	switch(fnc){ // osc/lfo1/lfo2
	default:
	case 0:
		num = scc_setting_edit[0].osc[3]; // scc data
		if(num < 0){
			type = 0; // osc_type
			num = OSC_NOISE_LOWBIT;
		}else{
			type = 1; // osc_type
		}
		break;
	case 1:
		num = scc_setting_edit[0].osc[5]; // scc data2
		if(num < 0){
			type = 0; // osc_type
			num = OSC_NOISE_LOWBIT;
		}else{
			type = 1; // osc_type
		}
		break;
	case 2:
		num = scc_setting_edit[0].osc[7]; // scc data3
		if(num < 0){
			type = 0; // osc_type
			num = OSC_NOISE_LOWBIT;
		}else{
			type = 1; // osc_type
		}
		break;
	case 8:
		type = 0; // osc_type
		num = scc_setting_edit[0].lfo1[2]; // wave_type
		break;
	case 9:
		type = 0; // osc_type
		num = scc_setting_edit[0].lfo2[2]; // wave_type
		break;
	}
	switch(type){
	case 0: // wave
		if(num < 0 || num > OSC_NOISE_LOWBIT)
			return data_name_empty;
		else
			return osc_wave_name[num];
	case 1: // scc
		if(num < 0 || num >= SCC_DATA_MAX)
			return data_name_empty;
		else
			return is_editor_preset.scc_data_name[num];
	default:
		return data_name_empty;
	}
}

void scc_editor_set_default_param(int set_num)
{
	int i, num;
	Preset_SCC *set = NULL;

	if(set_num < 0)
		set = &scc_setting_edit[0];
	else if(set_num >= SCC_SETTING_MAX)
		return;
	else if(!is_editor_preset.scc_setting[set_num])
		return ;
	else
		set = is_editor_preset.scc_setting[set_num];
	
	for(i = 0; i < SCC_MODE_MAX; i++){
		switch(i){
		default:
			num = 0;
			break;
		}
		set->mode[i] = num;
	}
	for(i = 0; i < SCC_PARAM_MAX; i++){
		switch(i){
		case 0:
			num = 100;
			break;
		default:
			num = 0;
			break;
		}
		set->param[i] = num;
	}
	for(i = 0; i < SCC_OSC_MAX; i++){
		switch(i){
		case 1:
			num = 100;
			break;
		default:
			num = 0;
			break;
		}
		set->osc[i] = num;
	}
	for(i = 0; i < SCC_AMP_MAX; i++)
		set->amp[i] = 0;
	for(i = 0; i < SCC_PITCH_MAX; i++)
		set->pitch[i] = 0;
	for(i = 0; i < SCC_ENV_PARAM; i++){
		switch(i){
		case 1:
		case 3:
		case 5:
			num = 100;
			break;
		case 6:
			num = 1;
			break;
		default:
			num = 0;
			break;
		}
		set->ampenv[i] = num;
		set->pitenv[i] = num;
	}
	for(i = 0; i < SCC_LFO_PARAM; i++){
		set->lfo1[i] = 0;
		set->lfo2[i] = 0;
	}
}

int scc_editor_get_param(int type, int num)
{
	switch(type){
	case 0: return scc_setting_edit[0].param[num];
	case 1: return scc_setting_edit[0].osc[num];
	case 2: return scc_setting_edit[0].amp[num];
	case 3: return scc_setting_edit[0].pitch[num];
	case 4: return scc_setting_edit[0].ampenv[num];
	case 5: return scc_setting_edit[0].pitenv[num];
	case 6: return scc_setting_edit[0].lfo1[num];
	case 7: return scc_setting_edit[0].lfo2[num];
	case 8: return scc_setting_edit[0].mode[num];
	default: return 0;
	}
}

void scc_editor_set_param(int type, int num, int val)
{
	switch(type){
	case 0: 
		scc_setting_edit[0].param[num] = val;
		return;
	case 1: 
		scc_setting_edit[0].osc[num] = val;
		return;
	case 2: 
		scc_setting_edit[0].amp[num] = val;
		return;
	case 3: 
		scc_setting_edit[0].pitch[num] = val;
		return;
	case 4: 
		scc_setting_edit[0].ampenv[num] = val;
		return;
	case 5: 
		scc_setting_edit[0].pitenv[num] = val;
		return;
	case 6: 
		scc_setting_edit[0].lfo1[num] = val;
		return;
	case 7: 
		scc_setting_edit[0].lfo2[num] = val;
		return;
	case 8: 
		scc_setting_edit[0].mode[num] = val;
		return;
	}
}

static void scc_editor_delete_ini(int num)
{
	INIDATA ini={0};
	char sec[30] = "";
	
	MyIni_Load(&ini, is_editor_inifile);	
	snprintf(sec, sizeof(sec), "SCC_%03d", num); // 桁数指定
	MyIni_DeleteSection(&ini, sec);
	MyIni_Save(&ini, is_editor_inifile);
	MyIni_SectionAllClear(&ini);
}

static void scc_editor_save_ini(int num)
{
	Preset_SCC *setting = NULL;
	INIDATA ini={0};
	LPINISEC inisec = NULL;
	char data[258] = "";
	char sec[30] = "";
	int i, flg;
		
	if(!is_editor_preset.scc_setting[num])
		return;
	setting = is_editor_preset.scc_setting[num];
	MyIni_Load(&ini, is_editor_inifile);	
	snprintf(sec, sizeof(sec), "SCC_%03d", num); // 桁数指定
	inisec = MyIni_GetSection(&ini, sec, 1);
	MyIni_SetString(inisec, "name", setting->inst_name);
	// mode
	for(i = 0; i < SCC_MODE_MAX; i++){
		snprintf(data, sizeof(data), "%s%d:", data, setting->mode[i]);
	}
	MyIni_SetString(inisec, "mode", data);
	// param
	for(i = 0; i < SCC_PARAM_MAX; i++){
		snprintf(data, sizeof(data), "%s%d:", data, setting->param[i]);
	}
	MyIni_SetString(inisec, "param", data);
	// osc
	memset(data, 0, sizeof(data));
	for(i = 0; i < SCC_OSC_MAX; i++){
		snprintf(data, sizeof(data), "%s%d:", data, setting->osc[i]);
	}
	MyIni_SetString(inisec, "osc", data);
	// amp	
	memset(data, 0, sizeof(data));
	flg = 0;
	for(i = 0; i < SCC_AMP_MAX; i++){
		if(setting->amp[i]) flg++;
		snprintf(data, sizeof(data), "%s%d:", data, setting->amp[i]);
	}
	if(flg)
		MyIni_SetString(inisec, "amp", data);	
	else
		MyIni_DeleteKey(&ini, sec, "amp");
	// pitch
	memset(data, 0, sizeof(data));
	flg = 0;
	for(i = 0; i < SCC_PITCH_MAX; i++){
		if(setting->pitch[i]) flg++;
		snprintf(data, sizeof(data), "%s%d:", data, setting->pitch[i]);
	}
	if(flg)
		MyIni_SetString(inisec, "pitch", data);	
	else
		MyIni_DeleteKey(&ini, sec, "pitch");	
	// ampenv
	if(setting->ampenv[6] < 1) setting->ampenv[6] = 1;
	if(setting->ampenv[0] != 0
		|| setting->ampenv[1] != 100
		|| setting->ampenv[2] != 0
		|| setting->ampenv[3] != 100
		|| setting->ampenv[4] != 0
		|| setting->ampenv[5] != 100
		|| setting->ampenv[6] != 1
		|| setting->ampenv[7] != 0
		|| setting->ampenv[8] != 0
		|| setting->ampenv[9] != 0
		|| setting->ampenv[10] != 0
		|| setting->ampenv[11] != 0
		|| setting->ampenv[12] != 0
		|| setting->ampenv[13] != 0			
		){
		memset(data, 0, sizeof(data));
		for(i = 0; i < SCC_ENV_PARAM; i++){
			snprintf(data, sizeof(data), "%s%d:", data, setting->ampenv[i]);
		}
		MyIni_SetString(inisec, "ampenv", data);	
	}else
		MyIni_DeleteKey(&ini, sec, "ampenv");	
	// pitenv	
	if(setting->pitenv[6] < 1) setting->pitenv[6] = 1;
	if(setting->pitenv[0] != 0
		|| setting->pitenv[1] != 100
		|| setting->pitenv[2] != 0
		|| setting->pitenv[3] != 100
		|| setting->pitenv[4] != 0
		|| setting->pitenv[5] != 100
		|| setting->pitenv[6] != 1
		|| setting->pitenv[7] != 0
		|| setting->pitenv[8] != 0
		|| setting->pitenv[9] != 0
		|| setting->pitenv[10] != 0
		|| setting->pitenv[11] != 0
		|| setting->pitenv[12] != 0
		|| setting->pitenv[13] != 0			
		){
		memset(data, 0, sizeof(data));
		for(i = 0; i < SCC_ENV_PARAM; i++){
			snprintf(data, sizeof(data), "%s%d:", data, setting->pitenv[i]);
		}
		MyIni_SetString(inisec, "pitenv", data);	
	}else
		MyIni_DeleteKey(&ini, sec, "pitenv");	
	// lfo1
	if(setting->lfo1[0]){
		memset(data, 0, sizeof(data));
		for(i = 0; i < SCC_LFO_PARAM; i++){
			snprintf(data, sizeof(data), "%s%d:", data, setting->lfo1[i]);
		}
		MyIni_SetString(inisec, "lfo1", data);	
	}else
		MyIni_DeleteKey(&ini, sec, "lfo1");
	// lfo2
	if(setting->lfo2[0]){
		memset(data, 0, sizeof(data));
		for(i = 0; i < SCC_LFO_PARAM; i++){
			snprintf(data, sizeof(data), "%s%d:", data, setting->lfo2[i]);
		}
		MyIni_SetString(inisec, "lfo2", data);
	}else
		MyIni_DeleteKey(&ini, sec, "lfo2");
	MyIni_Save(&ini, is_editor_inifile);
	MyIni_SectionAllClear(&ini);
}

void scc_editor_delete_preset(int num)
{
	if(num < 0 || num > (SCC_SETTING_MAX - 1)) 
		return; // error
	if(!is_editor_preset.scc_setting[num])
		return;
	safe_free(is_editor_preset.scc_setting[num]->inst_name);
	memset(&is_editor_preset.scc_setting[num], 0, sizeof(Preset_SCC));
	scc_editor_delete_ini(num);
}

void scc_editor_load_preset(int num)
{
	if(num < 0){ 
		memcpy(&scc_setting_edit[0], &scc_setting_edit[1], sizeof(Preset_SCC));
	}else{
		if(num > (SCC_SETTING_MAX - 1))
			return; // error
		if(!is_editor_preset.scc_setting[num])
			return;
		memcpy(&scc_setting_edit[0], is_editor_preset.scc_setting[num], sizeof(Preset_SCC));
	}
}

void scc_editor_store_preset(int num)
{
	char *tmp;

	if(num < 0){ 
		memcpy(&scc_setting_edit[1], &scc_setting_edit[0], sizeof(Preset_SCC));
	}else{
		if(num > (SCC_SETTING_MAX - 1))
			return; // error
		if(!is_editor_preset.scc_setting[num])
			return;
		tmp = is_editor_preset.scc_setting[num]->inst_name;
		memcpy(is_editor_preset.scc_setting[num], &scc_setting_edit[0], sizeof(Preset_SCC));
		is_editor_preset.scc_setting[num]->inst_name = tmp;
		scc_editor_save_ini(num);
	}
}

int calc_random_param(int min, int max)
{
	int i, var, mod, center;

	if(min < 0){
		center = mod = max > min ? max : min;
		mod = center * 2 + 1;
		for(i = 0; i < 10000; i++){
			var = rand() % (mod) - center;
			if(var >= min && var <= max)
				return var;
		}
	}else{
		mod = max + 1;
		for(i = 0; i < 10000; i++){
			var = rand() % (mod);
			if(var >= min)
				return var;
		}
	}
	return 0; // error
}

const char *mms_editor_load_name(int num)
{
	if(num < 0 || num >= MMS_SETTING_MAX)
		return data_name_empty;
	else if(!is_editor_preset.mms_setting[num])
		return data_name_empty;
	else
		return is_editor_preset.mms_setting[num]->inst_name;
}

void mms_editor_store_name(int num, const char *name)
{
	if(num < 0 || num >= MMS_SETTING_MAX)
		return;
	if(!is_editor_preset.mms_setting[num])
		return;
	if(is_editor_preset.mms_setting[num]->inst_name != NULL)
		safe_free(is_editor_preset.mms_setting[num]->inst_name);
	is_editor_preset.mms_setting[num]->inst_name = safe_strdup(name);
}

const char *mms_editor_load_wave_name(int op, int fnc)
{
	int type; // osc_type
	int num; // wave_type

	if(fnc < 0){ // osc
		type = mms_setting_edit[0].op_wave[op][0]; // osc_type
		num = mms_setting_edit[0].op_wave[op][1]; // wave_type
	}else switch(fnc){ // lfo1~lfo4
	default:
	case 0:
		type = 0; // osc_type
		num = mms_setting_edit[0].op_lfo1[op][2]; // wave_type
		break;
	case 1:
		type = 0; // osc_type
		num = mms_setting_edit[0].op_lfo2[op][2]; // wave_type
		break;
	case 2:
		type = 0; // osc_type
		num = mms_setting_edit[0].op_lfo3[op][2]; // wave_type
		break;
	case 3:
		type = 0; // osc_type
		num = mms_setting_edit[0].op_lfo4[op][2]; // wave_type
		break;
	}
	switch(type){
	case 0: // wave
		if(num < 0 || num > OSC_NOISE_LOWBIT)
			return data_name_empty;
		else
			return osc_wave_name[num];
	case 1: // scc
		if(num < 0 || num >= SCC_DATA_MAX)
			return data_name_empty;
		else
			return is_editor_preset.scc_data_name[num];
	case 2: // mt32
		if(num < 0 || num >= MT32_DATA_MAX)
			return data_name_empty;
		else
			return la_pcm_data_name[num];
	case 3: // cm32l
		if(num < 0 || num >= CM32L_DATA_MAX)
			return data_name_empty;
		else
			return la_pcm_data_name[num];
	default:
		return data_name_empty;
	}
}

const char *mms_editor_load_filter_name(int op)
{
	int num;
	
	num = mms_setting_edit[0].op_filter[op][0]; // filter_type
	if(num < 0 || num > FILTER_HBF_L12L6)
		return data_name_empty;
	else
		return filter_name[num];
}

static void mms_editor_set_default_param_op(Preset_MMS *set, int op_num)
{
	int i, num;;
	
	for(i = 0; i < MMS_OP_PARAM_MAX; i++){
		switch(i){
		case 1:
		case 2:
			num = 100;
			break;
		default:
			num = 0;
			break;
		}
		set->op_param[op_num][i] = num;
	}
	for(i = 0; i < MMS_OP_CON_MAX; i++)
	   set->op_connect[op_num][i] = -1;	
	for(i = 0; i < MMS_OP_RANGE_MAX; i++){
		switch(i){
		case 1:
		case 3:
			num = 127;
			break;
		default:
			num = 0;
			break;
		}
		set->op_range[op_num][i] = num;
	}
	for(i = 0; i < MMS_OP_OSC_MAX; i++){
		switch(i){
		case 1:
			num = 100;
			break;
		default:
			num = 0;
			break;
		}
		set->op_osc[op_num][i] = num;
	}
	for(i = 0; i < MMS_OP_WAVE_MAX; i++){
		switch(i){
		case 2:
			num = 100;
			break;
		default:
			num = 0;
			break;
		}
		set->op_wave[op_num][i] = 0;
	}
	for(i = 0; i < MMS_OP_SUB_MAX; i++)
		set->op_sub[op_num][i] = 0;
	for(i = 0; i < MMS_OP_AMP_MAX; i++)
		set->op_amp[op_num][i] = 0;
	for(i = 0; i < MMS_OP_PITCH_MAX; i++)
		set->op_pitch[op_num][i] = 0;
	for(i = 0; i < MMS_OP_WIDTH_MAX; i++)
		set->op_width[op_num][i] = 0;
	for(i = 0; i < MMS_OP_FLT_PARAM; i++)
		set->op_filter[op_num][i] = 0;
	for(i = 0; i < MMS_OP_CUT_PARAM; i++)
		set->op_cutoff[op_num][i] = 0;	
	for(i = 0; i < MMS_OP_ENV_PARAM; i++){
		switch(i){
		case 1:
		case 3:
		case 5:
			num = 100;
			break;
		case 6:
			num = 1;
			break;
		default:
			num = 0;
			break;
		}
		set->op_ampenv[op_num][i] = num;
		set->op_modenv[op_num][i] = num;
		set->op_widenv[op_num][i] = num;
		set->op_pitenv[op_num][i] = num;
		set->op_ampenv_keyf[op_num][i] = 0;
		set->op_modenv_keyf[op_num][i] = 0;
		set->op_widenv_keyf[op_num][i] = 0;
		set->op_pitenv_keyf[op_num][i] = 0;
		set->op_ampenv_velf[op_num][i] = 0;
		set->op_modenv_velf[op_num][i] = 0;
		set->op_widenv_velf[op_num][i] = 0;
		set->op_pitenv_velf[op_num][i] = 0;
	}
	for(i = 0; i < MMS_OP_LFO_PARAM; i++){
		set->op_lfo1[op_num][i] = 0;
		set->op_lfo2[op_num][i] = 0;
		set->op_lfo3[op_num][i] = 0;
		set->op_lfo4[op_num][i] = 0;
	}
	for(i = 0; i < MMS_OP_MODE_MAX; i++){
		switch(i){
		default:
			num = 0;
			break;
		}
		set->op_mode[op_num][i] = num;
	}
}

void mms_editor_set_default_param(int set_num, int op_num)
{
	int i;
	int32 num;
	Preset_MMS *set = NULL;

	if(set_num < 0)
		set = &mms_setting_edit[0];
	else if(set_num >= MMS_SETTING_MAX)
		return;
	else if(!is_editor_preset.mms_setting[set_num])
		return;
	else
		set = is_editor_preset.mms_setting[set_num];
	if(op_num < 0){
		set->op_max = 0;
		for(i = 0; i < MMS_OP_MAX; i++)
			mms_editor_set_default_param_op(set, i);
	}else if(op_num >= MMS_OP_MAX)
		return;
	else
		mms_editor_set_default_param_op(set, op_num);
}

void mms_editor_set_magic_param(void)
{
	Preset_MMS *set = &mms_setting_edit[0];
	int i, val = 0, tmp, op_max, osc_mode;

	mms_editor_set_default_param(-1, -1);
	mms_setting_edit[0].op_max = op_max = calc_random_param(2, 6); // for 2~4op	
	switch(op_max){
	case 1:		
	case 2:		
		tmp = 200;
		osc_mode = calc_random_param(0, 1);
		set->op_connect[1][1] = !calc_random_param(0, 4) ? 0 : -1; // feedback op1 to op0 10%
		break;
	case 3:
		tmp = 100;
		osc_mode = 0;
		break;
	case 4:
		tmp = 75;
		osc_mode = 0;
		break;
	default:
	case 5:
	case 6:
		tmp = 50;
		osc_mode = 0;
		break;
	}
	for(i = 0; i < op_max; i++){
		set->op_param[i][0] = 3; // pm
		if(i == (op_max - 1)){
			set->op_connect[i][0] = -2;
			set->op_param[i][2] = 70; // lev output
			set->op_sub[i][0] = calc_random_param(20, 60); // velmod
		}else if(!calc_random_param(0, 3)){
			set->op_connect[i][0] = op_max - 1;
			set->op_param[i][2] = 50; // oplv
			set->op_sub[i][0] = calc_random_param(50, 100); // velmod
		}else{
			set->op_connect[i][0] = i + 1;
			set->op_param[i][2] = 100; // oplv
			set->op_sub[i][0] = calc_random_param(50, 100); // velmod
		}
		set->op_param[i][1] = calc_random_param(10, tmp); // modlv
	}
	// osc wave / scc	
	if(osc_mode){
		for(i = 0; i < op_max; i++){
			set->op_wave[i][0] = 1;
			set->op_wave[i][1] = calc_random_param(0, 36); // scc
			set->op_osc[i][1] = 100;
		}
	}else{
		for(i = 0; i < (op_max - 1); i++){
			set->op_wave[i][0] = 0;
			set->op_wave[i][1] = calc_random_param(0, 1); // sin / tri
		}
		switch(calc_random_param(0, 5)){
		default:
		case 0: // 1x2x3x4x5x6x7x...
			for(i = 0; i < (op_max - 1); i++){
				set->op_osc[i][1] = (calc_random_param(1, 8) * 100); // % (only %mode
			}
			break;
		case 1: // 2x4x6x8x...
			for(i = 0; i < (op_max - 1); i++){	
				set->op_osc[i][1] = calc_random_param(1, 4) * 200; // % (only %mode
			}
			break;
		case 2: // 1x3x5x7
			for(i = 0; i < (op_max - 1); i++){	
				set->op_osc[i][1] = calc_random_param(1, 4) * 200 - 100; // % (only %mode
			}
			break;
		case 3: // 0.25x
			for(i = 0; i < (op_max - 1); i++){
				set->op_osc[i][1] = calc_random_param(3, 8 * 4) * 25; // % (only %mode
			}
			break;
		}	
		set->op_osc[(op_max - 1)][1] = 100;
	}
	// ampenv	
	{
		int atk_min, atk_max, dcy_min, dcy_max;
		switch(calc_random_param(0, 3)){
		default:
		case 0: // fast atk sustain
			atk_min = 50;
			atk_max = 200;
			for(i = 0; i < (op_max - 1); i++){
				set->op_ampenv[i][0] = atk_max = calc_random_param(atk_min, atk_max);
				atk_min -= 10;
				set->op_ampenv[i][1] = 100;
				set->op_ampenv[i][2] = 0;
				set->op_ampenv[i][3] = 100;
				set->op_ampenv[i][4] = 10000;
				set->op_ampenv[i][5] = 100;
				set->op_ampenv[i][6] = 10000;
				set->op_ampenv[i][7] = 0;
			}
			break;
		case 1: // fast atk decay
			atk_min = 50;
			atk_max = 200;
			dcy_min = 300;
			dcy_max = 2500;
			for(i = 0; i < (op_max - 1); i++){
				set->op_ampenv[i][0] = atk_max = calc_random_param(atk_min, atk_max);
				atk_min -= 10;
				set->op_ampenv[i][1] = 100;
				set->op_ampenv[i][2] = 0;
				set->op_ampenv[i][3] = 100;
				set->op_ampenv[i][4] = val = dcy_min = calc_random_param(dcy_min, dcy_max);
				dcy_max += 100;
				set->op_ampenv[i][5] = calc_random_param(0, 50);
				set->op_ampenv[i][6] = val;
				set->op_ampenv[i][7] = 0;
			}
			break;
		case 2: // slow atk sustain
			atk_min = 300;
			atk_max = 800;
			for(i = 0; i < (op_max - 1); i++){
				set->op_ampenv[i][0] = atk_max = calc_random_param(atk_min, atk_max);
				atk_min -= 40;
				set->op_ampenv[i][1] = 100;
				set->op_ampenv[i][2] = 0;
				set->op_ampenv[i][3] = 100;
				set->op_ampenv[i][4] = 10000;
				set->op_ampenv[i][5] = 100;
				set->op_ampenv[i][6] = 10000;
				set->op_ampenv[i][7] = 0;
			}
			break;
		}
		set->op_ampenv[(op_max - 1)][0] = calc_random_param(10, 50);
		set->op_ampenv[(op_max - 1)][1] = 100;
		set->op_ampenv[(op_max - 1)][2] = 0;
		set->op_ampenv[(op_max - 1)][3] = 100;
		set->op_ampenv[(op_max - 1)][4] = 10000;
		set->op_ampenv[(op_max - 1)][5] = 100;
		set->op_ampenv[(op_max - 1)][6] = 10000;
		set->op_ampenv[(op_max - 1)][7] = 0;	
	}
	// widenv
	if(osc_mode == 0 && !calc_random_param(0, 4)){	
		set->op_width[0][1] = calc_random_param(50, 99);
		switch(calc_random_param(0, 1)){
		default:
		case 0: // fast down
			set->op_widenv[0][0] = 0;
			set->op_widenv[0][1] = 100;
			set->op_widenv[0][2] = 0;
			set->op_widenv[0][3] = 100;
			set->op_widenv[0][4] = val = calc_random_param(200, 1000);
			set->op_widenv[0][5] = 0;
			set->op_widenv[0][6] = val;
			set->op_widenv[0][7] = 0;
			break;
		case 1: // fast up down
			set->op_widenv[0][0] = val = calc_random_param(200, 400);
			set->op_widenv[0][1] = 100;
			set->op_widenv[0][2] = 0;
			set->op_widenv[0][3] = 100;
			set->op_widenv[0][4] = val = calc_random_param(400, 1000);
			set->op_widenv[0][5] = 0;
			set->op_widenv[0][6] = val;
			set->op_widenv[0][7] = 0;
			break;
		}
	}
}

int mms_editor_get_param(int type, int op, int num)
{
	switch(type){
	case 0: return mms_setting_edit[0].op_max;
	case 1: return mms_setting_edit[0].op_range[op][num];
	case 2: return mms_setting_edit[0].op_param[op][num];
	case 3: return mms_setting_edit[0].op_connect[op][num];
	case 4: return mms_setting_edit[0].op_osc[op][num];
	case 5: return mms_setting_edit[0].op_wave[op][num];
	case 6: return mms_setting_edit[0].op_sub[op][num];
	case 7: return mms_setting_edit[0].op_amp[op][num];
	case 8: return mms_setting_edit[0].op_pitch[op][num];		
	case 9: return mms_setting_edit[0].op_width[op][num];
	case 10: return mms_setting_edit[0].op_filter[op][num];
	case 11: return mms_setting_edit[0].op_cutoff[op][num];
	case 12: return mms_setting_edit[0].op_ampenv[op][num];
	case 13: return mms_setting_edit[0].op_pitenv[op][num];
	case 14: return mms_setting_edit[0].op_widenv[op][num];
	case 15: return mms_setting_edit[0].op_modenv[op][num];
	case 16: return mms_setting_edit[0].op_ampenv_keyf[op][num];
	case 17: return mms_setting_edit[0].op_pitenv_keyf[op][num];
	case 18: return mms_setting_edit[0].op_widenv_keyf[op][num];
	case 19: return mms_setting_edit[0].op_modenv_keyf[op][num];
	case 20: return mms_setting_edit[0].op_ampenv_velf[op][num];
	case 21: return mms_setting_edit[0].op_pitenv_velf[op][num];
	case 22: return mms_setting_edit[0].op_widenv_velf[op][num];
	case 23: return mms_setting_edit[0].op_modenv_velf[op][num];
	case 24: return mms_setting_edit[0].op_lfo1[op][num];
	case 25: return mms_setting_edit[0].op_lfo2[op][num];
	case 26: return mms_setting_edit[0].op_lfo3[op][num];
	case 27: return mms_setting_edit[0].op_lfo4[op][num];
	case 28: return mms_setting_edit[0].op_mode[op][num];
	default: return 0;
	}
}

void mms_editor_set_param(int type, int op, int num, int val)
{
	switch(type){
	case 0: mms_setting_edit[0].op_max = val > MMS_OP_MAX ? MMS_OP_MAX : val;
		return;
	case 1: mms_setting_edit[0].op_range[op][num] = val;
		return;
	case 2: mms_setting_edit[0].op_param[op][num] = val;
		return;
	case 3: mms_setting_edit[0].op_connect[op][num] = val;
		return;
	case 4: mms_setting_edit[0].op_osc[op][num] = val;
		return;
	case 5: mms_setting_edit[0].op_wave[op][num] = val;
		return;
	case 6: mms_setting_edit[0].op_sub[op][num] = val;
		return;
	case 7: mms_setting_edit[0].op_amp[op][num] = val;
		return;
	case 8: mms_setting_edit[0].op_pitch[op][num] = val;
		return;		
	case 9: mms_setting_edit[0].op_width[op][num] = val;
		return;
	case 10: mms_setting_edit[0].op_filter[op][num] = val;
		return;
	case 11: mms_setting_edit[0].op_cutoff[op][num] = val;
		return;
	case 12: mms_setting_edit[0].op_ampenv[op][num] = val;
		return;
	case 13: mms_setting_edit[0].op_pitenv[op][num] = val;
		return;
	case 14: mms_setting_edit[0].op_widenv[op][num] = val;
		return;
	case 15: mms_setting_edit[0].op_modenv[op][num] = val;
		return;
	case 16: mms_setting_edit[0].op_ampenv_keyf[op][num] = val;
		return;
	case 17: mms_setting_edit[0].op_pitenv_keyf[op][num] = val;
		return;
	case 18: mms_setting_edit[0].op_widenv_keyf[op][num] = val;
		return;
	case 19: mms_setting_edit[0].op_modenv_keyf[op][num] = val;
		return;
	case 20: mms_setting_edit[0].op_ampenv_velf[op][num] = val;
		return;
	case 21: mms_setting_edit[0].op_pitenv_velf[op][num] = val;
		return;
	case 22: mms_setting_edit[0].op_widenv_velf[op][num] = val;
		return;
	case 23: mms_setting_edit[0].op_modenv_velf[op][num] = val;
		return;
	case 24: mms_setting_edit[0].op_lfo1[op][num] = val;
		return;
	case 25: mms_setting_edit[0].op_lfo2[op][num] = val;
		return;
	case 26: mms_setting_edit[0].op_lfo3[op][num] = val;
		return;
	case 27: mms_setting_edit[0].op_lfo4[op][num] = val;
		return;
	case 28: mms_setting_edit[0].op_mode[op][num] = val;
		return;
	}
}

static void mms_editor_delete_ini(int num)
{
	INIDATA ini={0};
	char sec[30] = "";
	
	MyIni_Load(&ini, is_editor_inifile);	
	snprintf(sec, sizeof(sec), "MMS_%03d", num); // 桁数指定
	MyIni_DeleteSection(&ini, sec);
	MyIni_Save(&ini, is_editor_inifile);
	MyIni_SectionAllClear(&ini);
}

static void mms_editor_save_ini(int num)
{
	Preset_MMS *setting = NULL;
	INIDATA ini={0};
	LPINISEC inisec = NULL;
	char data[258] = "";
	char sec[30] = "";
	char key[30] = "";
	int i, j, flg;
		
	if(!is_editor_preset.mms_setting[num])
		return;
	setting = is_editor_preset.mms_setting[num];
	MyIni_Load(&ini, is_editor_inifile);	
	snprintf(sec, sizeof(sec), "MMS_%03d", num); // 桁数指定
	inisec = MyIni_GetSection(&ini, sec, 1);
	MyIni_SetString(inisec, "name", setting->inst_name);
	MyIni_SetInt32(inisec, "op_max", setting->op_max);
	for(j = 0; j < MMS_OP_MAX; j++){
		if(j >= setting->op_max){
			// 使用しないOPのキー削除
			snprintf(key, sizeof(key), "op_%d_mode", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_range", j);	
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_param", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_connect", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_osc", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_wave", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_sub", j);	
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_amp", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_pitch", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_width", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_filter", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_cutoff", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_ampenv", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_pitenv", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_widenv", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_modenv", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_ampenv_keyf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_pitenv_keyf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_widenv_keyf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_modenv_keyf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_ampenv_velf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_pitenv_velf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_widenv_velf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_modenv_velf", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_lfo1", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_lfo2", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_lfo3", j);
			MyIni_DeleteKey(&ini, sec, key);	
			snprintf(key, sizeof(key), "op_%d_lfo4", j);
			MyIni_DeleteKey(&ini, sec, key);	
			break;
		}
		// mode
		snprintf(key, sizeof(key), "op_%d_mode", j);
		if(setting->op_mode[j][0] > 0){
			memset(data, 0, sizeof(data));
			for(i = 0; i < MMS_OP_MODE_MAX; i++){
				if(setting->op_mode[j][i]) flg++;
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_mode[j][i]);
			}
			MyIni_SetString(inisec, key, data);	
		}else
			MyIni_DeleteKey(&ini, sec, key);
		// range
		snprintf(key, sizeof(key), "op_%d_range", j);	
		memset(data, 0, sizeof(data));
		for(i = 0; i < MMS_OP_RANGE_MAX; i++){
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_range[j][i]);
		}
		if(setting->op_range[j][0] != 0
			|| setting->op_range[j][1] != 127
			|| setting->op_range[j][2] != 0
			|| setting->op_range[j][3] != 127)
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// param
		snprintf(key, sizeof(key), "op_%d_param", j);
		memset(data, 0, sizeof(data));
		for(i = 0; i < MMS_OP_PARAM_MAX; i++){
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_param[j][i]);
		}
		MyIni_SetString(inisec, key, data);		
		// connect
		snprintf(key, sizeof(key), "op_%d_connect", j);
		memset(data, 0, sizeof(data));
		flg = 0;
		for(i = 0; i < MMS_OP_CON_MAX; i++){
			if(setting->op_connect[j][i] != -1) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_connect[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// osc
		snprintf(key, sizeof(key), "op_%d_osc", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_OSC_MAX; i++){
			if(i == 1){
				if(setting->op_osc[j][i] != 100) flg++;
			}else 
				if(setting->op_osc[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_osc[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// wave
		snprintf(key, sizeof(key), "op_%d_wave", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_WAVE_MAX; i++){
			if(i == 2){
				if(setting->op_wave[j][i] != 100) flg++;
			}else 
				if(setting->op_wave[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_wave[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);		
		// sub			
		snprintf(key, sizeof(key), "op_%d_sub", j);
		memset(data, 0, sizeof(data));
		flg = 0;
		for(i = 0; i < MMS_OP_SUB_MAX; i++){
			if(setting->op_sub[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_sub[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// amp			
		snprintf(key, sizeof(key), "op_%d_amp", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_AMP_MAX; i++){
			if(setting->op_amp[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_amp[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// pitch	
		snprintf(key, sizeof(key), "op_%d_pitch", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_PITCH_MAX; i++){
			if(setting->op_pitch[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_pitch[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);		
		// width	
		snprintf(key, sizeof(key), "op_%d_width", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_WIDTH_MAX; i++){
			if(setting->op_width[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_width[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// filter	
		snprintf(key, sizeof(key), "op_%d_filter", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_FLT_PARAM; i++){
			if(setting->op_filter[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_filter[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// cutoff	
		snprintf(key, sizeof(key), "op_%d_cutoff", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_CUT_PARAM; i++){
			if(setting->op_cutoff[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_cutoff[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);		
		// ampenv	
		snprintf(key, sizeof(key), "op_%d_ampenv", j);
		if(setting->op_ampenv[j][6] < 1) setting->op_ampenv[j][6] = 1;
		if(setting->op_ampenv[j][0] != 0
			|| setting->op_ampenv[j][1] != 100
			|| setting->op_ampenv[j][2] != 0
			|| setting->op_ampenv[j][3] != 100
			|| setting->op_ampenv[j][4] != 0
			|| setting->op_ampenv[j][5] != 100
			|| setting->op_ampenv[j][6] != 1
			|| setting->op_ampenv[j][7] != 0
			|| setting->op_ampenv[j][8] != 0
			|| setting->op_ampenv[j][9] != 0
			|| setting->op_ampenv[j][10] != 0
			|| setting->op_ampenv[j][11] != 0
			|| setting->op_ampenv[j][12] != 0
			|| setting->op_ampenv[j][13] != 0			
			){
			memset(data, 0, sizeof(data));
			if(setting->op_ampenv[j][6] < 1)
				setting->op_ampenv[j][6] = 1;
			for(i = 0; i < MMS_OP_ENV_PARAM; i++){
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_ampenv[j][i]);
			}
			MyIni_SetString(inisec, key, data);		
		}else
			MyIni_DeleteKey(&ini, sec, key);		
		// pitenv	
		snprintf(key, sizeof(key), "op_%d_pitenv", j);
		if(setting->op_pitenv[j][6] < 1) setting->op_pitenv[j][6] = 1;
		if(setting->op_pitenv[j][0] != 0
			|| setting->op_pitenv[j][1] != 100
			|| setting->op_pitenv[j][2] != 0
			|| setting->op_pitenv[j][3] != 100
			|| setting->op_pitenv[j][4] != 0
			|| setting->op_pitenv[j][5] != 100
			|| setting->op_pitenv[j][6] != 1
			|| setting->op_pitenv[j][7] != 0
			|| setting->op_pitenv[j][8] != 0
			|| setting->op_pitenv[j][9] != 0
			|| setting->op_pitenv[j][10] != 0
			|| setting->op_pitenv[j][11] != 0
			|| setting->op_pitenv[j][12] != 0
			|| setting->op_pitenv[j][13] != 0			
			){
			memset(data, 0, sizeof(data));
			if(setting->op_pitenv[j][6] < 1) setting->op_pitenv[j][6] = 1;
			for(i = 0; i < MMS_OP_ENV_PARAM; i++){
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_pitenv[j][i]);
			}
			MyIni_SetString(inisec, key, data);		
		}else
			MyIni_DeleteKey(&ini, sec, key);	
		// widenv	
		snprintf(key, sizeof(key), "op_%d_widenv", j);
		if(setting->op_widenv[j][6] < 1) setting->op_widenv[j][6] = 1;
		if(setting->op_widenv[j][0] != 0
			|| setting->op_widenv[j][1] != 100
			|| setting->op_widenv[j][2] != 0
			|| setting->op_widenv[j][3] != 100
			|| setting->op_widenv[j][4] != 0
			|| setting->op_widenv[j][5] != 100
			|| setting->op_widenv[j][6] != 1
			|| setting->op_widenv[j][7] != 0
			|| setting->op_widenv[j][8] != 0
			|| setting->op_widenv[j][9] != 0
			|| setting->op_widenv[j][10] != 0
			|| setting->op_widenv[j][11] != 0
			|| setting->op_widenv[j][12] != 0
			|| setting->op_widenv[j][13] != 0			
			){
			memset(data, 0, sizeof(data));
			if(setting->op_widenv[j][6] < 1) setting->op_widenv[j][6] = 1;
			for(i = 0; i < MMS_OP_ENV_PARAM; i++){
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_widenv[j][i]);
			}
			MyIni_SetString(inisec, key, data);	
		}else
			MyIni_DeleteKey(&ini, sec, key);	
		// modenv	
		snprintf(key, sizeof(key), "op_%d_modenv", j);
		if(setting->op_modenv[j][6] < 1) setting->op_modenv[j][6] = 1;
		if(setting->op_modenv[j][0] != 0
			|| setting->op_modenv[j][1] != 100
			|| setting->op_modenv[j][2] != 0
			|| setting->op_modenv[j][3] != 100
			|| setting->op_modenv[j][4] != 0
			|| setting->op_modenv[j][5] != 100
			|| setting->op_modenv[j][6] != 1
			|| setting->op_modenv[j][7] != 0
			|| setting->op_modenv[j][8] != 0
			|| setting->op_modenv[j][9] != 0
			|| setting->op_modenv[j][10] != 0
			|| setting->op_modenv[j][11] != 0
			|| setting->op_modenv[j][12] != 0
			|| setting->op_modenv[j][13] != 0			
			){
			memset(data, 0, sizeof(data));
			if(setting->op_modenv[j][6] < 1) setting->op_modenv[j][6] = 1;
			for(i = 0; i < MMS_OP_ENV_PARAM; i++){
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_modenv[j][i]);
			}
			MyIni_SetString(inisec, key, data);	
		}else
			MyIni_DeleteKey(&ini, sec, key);	
		// ampenv_keyf	
		snprintf(key, sizeof(key), "op_%d_ampenv_keyf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_ampenv_keyf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_ampenv_keyf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// pitenv_keyf
		snprintf(key, sizeof(key), "op_%d_pitenv_keyf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_pitenv_keyf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_pitenv_keyf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// widenv_keyf
		snprintf(key, sizeof(key), "op_%d_widenv_keyf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_widenv_keyf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_widenv_keyf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// modenv_keyf
		snprintf(key, sizeof(key), "op_%d_modenv_keyf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_modenv_keyf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_modenv_keyf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);	
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// ampenv_velf
		snprintf(key, sizeof(key), "op_%d_ampenv_velf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_ampenv_velf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_ampenv_velf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);	
		// pitenv_velf
		snprintf(key, sizeof(key), "op_%d_pitenv_velf", j);
		memset(data, 0, sizeof(data));
		flg = 0;	
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_pitenv_velf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_pitenv_velf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// widenv_velf
		snprintf(key, sizeof(key), "op_%d_widenv_velf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_widenv_velf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_widenv_velf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// modenv_velf
		snprintf(key, sizeof(key), "op_%d_modenv_velf", j);
		memset(data, 0, sizeof(data));
		flg = 0;		
		for(i = 0; i < MMS_OP_ENV_PARAM; i++){
			if(setting->op_modenv_velf[j][i]) flg++;
			snprintf(data, sizeof(data), "%s%d:", data, setting->op_modenv_velf[j][i]);
		}
		if(flg) 
			MyIni_SetString(inisec, key, data);		
		else
			MyIni_DeleteKey(&ini, sec, key);
		// lfo1
		snprintf(key, sizeof(key), "op_%d_lfo1", j);
		if(setting->op_lfo1[j][0] > 0){	
			memset(data, 0, sizeof(data));
			for(i = 0; i < MMS_OP_LFO_PARAM; i++){
				if(setting->op_lfo1[j][i]) flg++;
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_lfo1[j][i]);
			}
			MyIni_SetString(inisec, key, data);	
		}else
			MyIni_DeleteKey(&ini, sec, key);	
		// lfo2
		snprintf(key, sizeof(key), "op_%d_lfo2", j);
		if(setting->op_lfo2[j][0] > 0){	
			memset(data, 0, sizeof(data));
			for(i = 0; i < MMS_OP_LFO_PARAM; i++){
				if(setting->op_lfo2[j][i]) flg++;
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_lfo2[j][i]);
			}
			MyIni_SetString(inisec, key, data);		
		}else
			MyIni_DeleteKey(&ini, sec, key);
		// lfo3
		snprintf(key, sizeof(key), "op_%d_lfo3", j);
		if(setting->op_lfo3[j][0] > 0){	
			memset(data, 0, sizeof(data));
			for(i = 0; i < MMS_OP_LFO_PARAM; i++){
				if(setting->op_lfo3[j][i]) flg++;
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_lfo3[j][i]);
			}
			MyIni_SetString(inisec, key, data);	
		}else
			MyIni_DeleteKey(&ini, sec, key);	
		// lfo4
		snprintf(key, sizeof(key), "op_%d_lfo4", j);
		if(setting->op_lfo4[j][0] > 0){
			memset(data, 0, sizeof(data));
			for(i = 0; i < MMS_OP_LFO_PARAM; i++){
				if(setting->op_lfo4[j][i]) flg++;
				snprintf(data, sizeof(data), "%s%d:", data, setting->op_lfo4[j][i]);
			}
			MyIni_SetString(inisec, key, data);	
		}else
			MyIni_DeleteKey(&ini, sec, key);
	}
	MyIni_Save(&ini, is_editor_inifile);
	MyIni_SectionAllClear(&ini);
}

void mms_editor_delete_preset(int num)
{
	if(num < 0 || num > (MMS_SETTING_MAX - 1)) 
		return; // error
	if(!is_editor_preset.mms_setting[num])
		return;
	safe_free(is_editor_preset.mms_setting[num]->inst_name);
	memset(is_editor_preset.mms_setting[num], 0, sizeof(Preset_MMS));
	mms_editor_delete_ini(num);
}

void mms_editor_load_preset(int num)
{
	if(num < 0){ 
		memcpy(&mms_setting_edit[0], &mms_setting_edit[1], sizeof(Preset_MMS));
	}else{
		if(num > (MMS_SETTING_MAX - 1))
			return; // error		
		if(!is_editor_preset.mms_setting[num])
			return;
		memcpy(&mms_setting_edit[0], is_editor_preset.mms_setting[num], sizeof(Preset_MMS));
	}
}

void mms_editor_store_preset(int num)
{
	char *tmp;

	if(num < 0){ 
		memcpy(&mms_setting_edit[1], &mms_setting_edit[0], sizeof(Preset_MMS));
	}else{
		if(num > (MMS_SETTING_MAX - 1))
			return; // error	
		if(!is_editor_preset.mms_setting[num])
			return;
		tmp = is_editor_preset.mms_setting[num]->inst_name;
		memcpy(is_editor_preset.mms_setting[num], &mms_setting_edit[0], sizeof(Preset_MMS));
		is_editor_preset.mms_setting[num]->inst_name = tmp;
		mms_editor_save_ini(num);
	}
}

const char *is_editor_get_ini_path(void)
{
	return (const char *)is_editor_inifile;
}

void is_editor_set_ini_path(const char *name)
{
	strcpy(is_editor_inifile, name);
}

void free_is_editor_preset(void)
{
	int i;
	Preset_IS *set = &is_editor_preset;
	
	safe_free(set->ini_file);
	set->ini_file = NULL;
	set->scc_data_load = 0;
	for (i = 0; i < SCC_DATA_MAX; i++) {
		safe_free(set->scc_data_name[i]);
		set->scc_data_name[i] = NULL;
	}	
	for (i = 0; i < SCC_SETTING_MAX; i++) {
		if(!set->scc_setting[i])
			continue;
		safe_free(set->scc_setting[i]->inst_name);
		set->scc_setting[i]->inst_name = NULL;
		safe_free(set->scc_setting[i]);
		set->scc_setting[i] = NULL;
	}
	for (i = 0; i < MMS_SETTING_MAX; i++) {
		if(!set->mms_setting[i])
			continue;
		safe_free(set->mms_setting[i]->inst_name);
		set->mms_setting[i]->inst_name = NULL;
		safe_free(set->mms_setting[i]);
		set->mms_setting[i] = NULL;
	}
	set->next = NULL;
}

void is_editor_load_ini(void)
{
	free_is_editor_preset();
	scc_data_editor_override = 0; 
	scc_editor_override = 0; 
	mms_editor_override = 0; 	
	load_int_synth_preset(is_editor_inifile, &is_editor_preset, IS_INI_TYPE_ALL, IS_INI_PRESET_ALL, IS_INI_PRESET_INIT); // all, init
	scc_data_editor_load_preset(0);
	scc_data_editor_store_preset(-1);
	scc_editor_load_preset(0);
	scc_editor_store_preset(-1);
	mms_editor_load_preset(0);
	mms_editor_store_preset(-1);
}

void init_is_editor_param(void)
{	
	load_la_rom();
	scc_data_editor_override = 0; 
	scc_editor_override = 0; 
	mms_editor_override = 0; 	
}

void uninit_is_editor_param(void)
{
	scc_data_editor_override = 0; 
	scc_editor_override = 0; 
	mms_editor_override = 0; 
	free_is_editor_preset();	
}










//////// extract


#define IS_ENVRATE_MAX (0x3FFFFFFFL)
#define IS_ENVRATE_MIN (1L)

/* from instrum.c */
static int32 convert_envelope_rate(uint8 rate)
{
  int32 r;

  r=3-((rate>>6) & 0x3);
  r*=3;
  r = (int32)(rate & 0x3f) << r; /* 6.9 fixed point */
  /* 15.15 fixed point. */
  return (((r * 44100) / play_mode->rate) * control_ratio) << ((fast_decay) ? 10 : 9);
}

/* from instrum.c */
static int32 convert_envelope_offset(uint8 offset)
{
  /* 15.15 fixed point */
  return offset << (7+15);
}

static void init_sample_param(Sample *sample)
{		
	memset(sample, 0, sizeof(Sample));
	sample->data = NULL;
	sample->data_type = SAMPLE_TYPE_INT16;
	sample->data_alloced = 0; // use Preset_SCC Preset_MMS ptr , see instrum.c free_instrument()
	sample->loop_start = 0;
	sample->loop_end = INT_MAX;
	sample->data_length = INT_MAX;
	sample->sample_rate = play_mode->rate;
	sample->low_key = 0;
	sample->high_key = 127;
	sample->root_key = 60;
	sample->root_freq = freq_table[60];
	sample->tune = 1.0;
	sample->def_pan = 64;
	sample->sample_pan = 0.0;
	sample->note_to_use = 0;
	sample->volume = 1.0;
	sample->cfg_amp = 1.0;
	sample->modes = MODES_16BIT | MODES_LOOPING | MODES_SUSTAIN | MODES_ENVELOPE;
	sample->low_vel = 0;
	sample->high_vel = 127;
	sample->tremolo_delay = 0; // 0ms
	sample->tremolo_freq = 5000; // mHz 5Hz
	sample->tremolo_sweep = 5; // 5ms
	sample->tremolo_to_amp = 0; // 0.01%
	sample->tremolo_to_pitch = 0; // cent
	sample->tremolo_to_fc = 0; // cent
	sample->vibrato_delay = 0; // 0ms
	sample->vibrato_freq = 5000; // mHz 5Hz
	sample->vibrato_sweep = 5; // 5ms
	sample->vibrato_to_amp = 0; // 0.01%
	sample->vibrato_to_pitch = 0; // cent
	sample->vibrato_to_fc = 0; // cent
	sample->cutoff_freq = 20000;
	sample->cutoff_low_limit = -1;
	sample->cutoff_low_keyf = 0; // cent
	sample->resonance =  0;
	sample->modenv_to_pitch = 0;
	sample->modenv_to_fc =0;
	sample->vel_to_fc = 0;
	sample->key_to_fc = 0;
	sample->vel_to_resonance = 0;
	sample->envelope_velf_bpo = 64;
	sample->modenv_velf_bpo = 64;
	sample->envelope_keyf_bpo = 60;
	sample->modenv_keyf_bpo = 60;
	sample->vel_to_fc_threshold = 0;
	sample->key_to_fc_bpo = 60;
	sample->scale_freq = 60;
	sample->scale_factor = 1024;
	sample->sample_type = SF_SAMPLETYPE_MONO;
	sample->root_freq_org = 60;
	sample->sample_rate_org = play_mode->rate;
	sample->sf_sample_link = -1;
	sample->sf_sample_index = 0;
	sample->lpf_type = -1;
	sample->keep_voice = 0;
	sample->hpf[0] = -1; // opt_hpf_def
	sample->hpf[1] = 10;
	sample->hpf[2] = 0;
	sample->root_freq_detected = 0;
	sample->transpose_detected = 0;
	sample->chord = -1;
	sample->pitch_envelope[0] = 0; // 0cent init
	sample->pitch_envelope[1] = 0; // 0cent atk
	sample->pitch_envelope[2] = 0; // 125ms atk
	sample->pitch_envelope[3] = 0; // 0cent dcy1
	sample->pitch_envelope[4] = 0; // 125ms dcy1
	sample->pitch_envelope[5] = 0; // 0cent dcy2
	sample->pitch_envelope[6] = 0; // 125ms dcy3
	sample->pitch_envelope[7] = 0; // 0cent rls
	sample->pitch_envelope[8] = 0; // 125ms rls
	sample->envelope_delay = 0;
	sample->envelope_offset[0] = convert_envelope_offset(255);
	sample->envelope_rate[0] = IS_ENVRATE_MAX; // instant
	sample->envelope_offset[1] = convert_envelope_offset(255);
	sample->envelope_rate[1] = IS_ENVRATE_MAX; // instant
	sample->envelope_offset[2] = convert_envelope_offset(255);
	sample->envelope_rate[2] = IS_ENVRATE_MAX; // instant
	sample->envelope_offset[3] = 0;
	sample->envelope_rate[3] = convert_envelope_rate(250);
	sample->envelope_offset[4] = 0;
	sample->envelope_rate[4] = convert_envelope_rate(250);
	sample->envelope_offset[5] = 0;
	sample->envelope_rate[5] = convert_envelope_rate(250);
	sample->modenv_delay = 0;
	sample->modenv_offset[0] = convert_envelope_offset(255);
	sample->modenv_rate[0] = IS_ENVRATE_MAX; // instant
	sample->modenv_offset[1] = convert_envelope_offset(255);
	sample->modenv_rate[1] = IS_ENVRATE_MAX; // instant
	sample->modenv_offset[2] = convert_envelope_offset(255);
	sample->modenv_rate[2] = IS_ENVRATE_MAX; // instant
	sample->modenv_offset[3] = 0;
	sample->modenv_rate[3] = convert_envelope_rate(1);
	sample->modenv_offset[4] = 0;
	sample->modenv_rate[4] = convert_envelope_rate(1);
	sample->modenv_offset[5] = 0;
	sample->modenv_rate[5] = convert_envelope_rate(1);
	sample->envelope_keyf[0] = 0;
	sample->envelope_keyf[1] = 0;
	sample->envelope_keyf[2] = 0;
	sample->envelope_keyf[3] = 0;
	sample->envelope_keyf[4] = 0;
	sample->envelope_keyf[5] = 0;
	sample->envelope_velf[0] = 0;
	sample->envelope_velf[1] = 0;
	sample->envelope_velf[2] = 0;
	sample->envelope_velf[3] = 0;
	sample->envelope_velf[4] = 0;
	sample->envelope_velf[5] = 0;
	sample->modenv_keyf[0] = 0;
	sample->modenv_keyf[1] = 0;
	sample->modenv_keyf[2] = 0;
	sample->modenv_keyf[3] = 0;
	sample->modenv_keyf[4] = 0;
	sample->modenv_keyf[5] = 0;
	sample->modenv_velf[0] = 0;
	sample->modenv_velf[1] = 0;
	sample->modenv_velf[2] = 0;
	sample->modenv_velf[3] = 0;
	sample->modenv_velf[4] = 0;
	sample->modenv_velf[5] = 0;
}


static Preset_IS *is_preset = NULL;

static Preset_IS *load_ini_file(char *ini_file, int type, int preset)
{
	int i;
	Preset_IS *set = NULL;
	Preset_IS *newset = NULL;

#if defined(IS_INI_LOAD_BLOCK)
#if (IS_INI_LOAD_BLOCK == 32)
	uint32 block = preset >> 5; // 1block=64preset 32block
#elif (IS_INI_LOAD_BLOCK == 64)
	uint32 block = preset >> 6; // 1block=64preset 16block
#elif (IS_INI_LOAD_BLOCK == 128)
	uint32 block = preset >> 7; // 1block=128preset 8block
#endif
	uint32 bit = 1L << block; // < 32bit
	block = (block + 1) << 10; // 10bit > max(SCC_SETTING_MAX MMS_SETTING_MAX)
#endif

    for (set = is_preset; set; set = set->next){
		if (set->ini_file && !strcmp(set->ini_file, ini_file)){
#if defined(IS_INI_LOAD_TYPE)
			if(type == IS_INI_TYPE_SCC && !set->scc_load)){
				load_int_synth_preset(ini_file, set, type, IS_INI_PRESET_ALL, IS_INI_PRESET_NONE);
				set->scc_load = 1;
			}else (type == IS_INI_TYPE_MMS && !set->mms_load){
				load_int_synth_preset(ini_file, set, type, IS_INI_PRESET_ALL, IS_INI_PRESET_NONE);
				set->mms_load = 1;
			}
#elif defined(IS_INI_LOAD_BLOCK) // block単位ロード
			if(type == IS_INI_TYPE_SCC && !(set->scc_load & bit)){
				load_int_synth_preset(ini_file, set, type, block, IS_INI_PRESET_NONE);
				set->scc_load |= bit;
			}else if(type == IS_INI_TYPE_MMS && !(set->mms_load & bit)){
				load_int_synth_preset(ini_file, set, type, block, IS_INI_PRESET_NONE);
				set->mms_load |= bit;
			}
#elif defined(IS_INI_LOAD_PRESET) // preset単位ロード
			load_int_synth_preset(ini_file, set, type, preset, IS_INI_PRESET_NONE);
#endif
			return set;
		}
	}
	newset = (Preset_IS *)safe_malloc(sizeof(Preset_IS));
	memset(newset, 0, sizeof(Preset_IS));
	if(!is_preset){
		is_preset = newset;
	}else{
		for (set = is_preset; set; set = set->next){
			if(!set->next){
				set->next = newset;
			}
		}
	}
	newset->ini_file = safe_strdup(ini_file);
#if defined(IS_INI_LOAD_ALL) // 全部ロード
	load_int_synth_preset(ini_file, newset, IS_INI_TYPE_ALL, IS_INI_PRESET_ALL, IS_INI_PRESET_NONE); // 全部ロード
	newset->scc_load = 1;
	newset->mms_load = 1;
#elif defined(IS_INI_LOAD_TYPE)  // type単位ロード
	load_int_synth_preset(ini_file, newset, type, IS_INI_PRESET_ALL, IS_INI_PRESET_NONE); // type単位ロード
	if(type == IS_INI_TYPE_SCC)
		newset->scc_load = 1;
	else if(type == IS_INI_TYPE_MMS)
		newset->mms_load = 1;
#elif defined(IS_INI_LOAD_BLOCK) // block単位ロード
	load_int_synth_preset(ini_file, newset, type, block, IS_INI_PRESET_NONE); // preset単位ロード
	if(type == IS_INI_TYPE_SCC)
		newset->scc_load |= bit;
	else if(type == IS_INI_TYPE_MMS)
		newset->mms_load |= bit;
#elif defined(IS_INI_LOAD_PRESET) // preset単位ロード
	load_int_synth_preset(ini_file, newset, type, preset, IS_INI_PRESET_NONE); // preset単位ロード
#endif
	return newset;
}

Instrument *extract_scc_file(char *ini_file, int preset)
{
	Instrument *inst;
	Sample *sample;
	Preset_IS *set = NULL;

	set = load_ini_file(ini_file, IS_INI_TYPE_SCC, preset);
	inst = (Instrument *)safe_malloc(sizeof(Instrument));
	memset(inst, 0, sizeof(Instrument));
	inst->instname = (char *)safe_malloc(256);
	if(!set->scc_setting[preset])
		snprintf(inst->instname, 256, "[SCC] %d: ----", preset);
	else
		snprintf(inst->instname, 256, "[SCC] %d: %s", preset, (const char *)set->scc_setting[preset]->inst_name);
	inst->type = INST_SCC;
	inst->samples = 1;
	inst->sample = (Sample *)safe_malloc(sizeof(Sample));
	memset(inst->sample, 0, sizeof(Sample));
	sample = inst->sample;
	init_sample_param(sample);
	sample->inst_type = INST_SCC;
	sample->sf_sample_index = preset;
	sample->data = (sample_t *)set;
	return inst;
}

Instrument *extract_mms_file(char *ini_file, int preset)
{
	Instrument *inst;
	Sample *sample;
	Preset_IS *set = NULL;
		
	load_la_rom();
	set = load_ini_file(ini_file, IS_INI_TYPE_MMS, preset);
	inst = (Instrument *)safe_malloc(sizeof(Instrument));
	memset(inst, 0, sizeof(Instrument));
	inst->instname = (char *)safe_malloc(256);
	if(!set->mms_setting[preset])
		snprintf(inst->instname, 256, "[MMS] %d: ----", preset);
	else
		snprintf(inst->instname, 256, "[MMS] %d: %s", preset, (const char *)set->mms_setting[preset]->inst_name);
	inst->type = INST_MMS;
	inst->samples = 1;
	inst->sample = (Sample *)safe_malloc(sizeof(Sample));
	memset(inst->sample, 0, sizeof(Sample));
	sample = inst->sample;
	init_sample_param(sample);
	sample->inst_type = INST_MMS;
	sample->sf_sample_index = preset;
	sample->data = (sample_t *)set;
	return inst;
}

void free_int_synth_file(Instrument *ip)
{
    safe_free(ip->instname);
    ip->instname = NULL;
}






//////// common


// mix 0.0~1.0 (mix=0.0 param1, mix=0.5 param1*0.5+param2*0.5, mix=1.0 param2
static inline double mix_double(double mix, double param1, double param2)
{
	return param1 * (1.0 - mix) + param2 * mix;
}


/* OSC */
/*
in  : 0.0 ~ 1.0
out : -1.0 ~ 1.0

zero point offset
sine:+0.25
tria:+0.25
saw1:+0.5
saw2:+0.5
squa:+0.0
*/

const char *osc_wave_name[] = 
{
    "SINE",
    "TRIANGULAR",
    "SAW1",
    "SAW2",
    "SQUARE",
    "NOISE",
    "NOISE_LOWBIT",
    "SINE_TABLE",
};

FLOAT_T compute_osc_sine(FLOAT_T in, int32 var)
{
	return sin((FLOAT_T)M_PI2 * (in + 0.75));	
}
	
FLOAT_T compute_osc_triangular(FLOAT_T in, int32 var)
{
	if(in < 0.5)
		return -1.0 + in * 4.0; // -1.0 + (in - 0.0) * 4.0;
	else  //  if(in < 1.0)
		return 1.0 - (in - 0.5) * 4.0;
}

FLOAT_T compute_osc_saw1(FLOAT_T in, int32 var)
{
	if(in < 0.5)
		return in * 2.0; // (in - 0.0) * 2.0;
	else  //  if(in < 1.0)
		return (in - 1.0) * 2.0;
}

FLOAT_T compute_osc_saw2(FLOAT_T in, int32 var)
{
	if(in < 0.5)
		return in * -2.0; // (in - 0.0) * -2.0;
	else  //  if(in < 1.0)
		return (in - 1.0) * -2.0;
}

FLOAT_T compute_osc_square(FLOAT_T in, int32 var)
{
	if(in < 0.5)
		return 1.0;
	else  //  if(in < 1.0)
		return -1.0;
}

FLOAT_T compute_osc_noise(FLOAT_T in, int32 var)
{	
	return (FLOAT_T)((int32)(rand() & 0x7FFFUL) - M_14BIT) * DIV_14BIT;
}

FLOAT_T compute_osc_noise_lowbit(FLOAT_T in, int32 var)
{	
	return lookup_noise_lowbit(in, var);
}

FLOAT_T compute_osc_sine_table(FLOAT_T in, int32 var)
{
	return lookup2_sine(in + 0.75);	
}

FLOAT_T compute_osc_sine_table_linear(FLOAT_T in, int32 var)
{
	return lookup2_sine_linear(in + 0.75);	
}

compute_osc_t compute_osc[] = {
// cfg sort
	compute_osc_sine,
	compute_osc_triangular,
	compute_osc_saw1,
	compute_osc_saw2,
	compute_osc_square,
	compute_osc_noise,
	compute_osc_noise_lowbit,
	compute_osc_sine_table,
	compute_osc_sine_table_linear,
};

static int check_is_osc_wave_type(int tmpi, int limit)
{
	if(tmpi < 0 || tmpi > limit)
		tmpi = 0; // sine
	// use table
	switch(tmpi){	
	default:
		return tmpi;	
	case OSC_SINE:
		switch(opt_int_synth_sine){
		default:
		case 0:
			return OSC_SINE;
		case 1:
			return OSC_SINE_TABLE;
		case 2:
			return OSC_SINE_TABLE_LINEAR;
		}
	}
}



/* SCC */
/*
in  : 0.0 ~ 1.0
out : -1.0 ~ 1.0
*/

FLOAT_T compute_osc_scc_none(FLOAT_T in, FLOAT_T *data)
{	
	return data[((int)(in * 32)) & 0x1F]; // 0~31<32
}

FLOAT_T compute_osc_scc_linear(FLOAT_T in, FLOAT_T *data)
{	
	int32 ofsi;
    FLOAT_T v1, v2, fp;
		
	in *= 32;
	fp = floor(in);
	ofsi = fp;
	ofsi &= 0x1F;
    v1 = data[ofsi];
    v2 = data[ofsi + 1];
	return v1 + (v2 - v1) * (in - fp);
}

FLOAT_T compute_osc_scc_sine(FLOAT_T in, FLOAT_T *data)
{	
	int32 ofsi;
    FLOAT_T v1, v2, fp;

	in *= 32;
	fp = floor(in);
	ofsi = fp;
//	fp = 0.5 + sin((in - ofsi - 0.5) * M_PI) * DIV_2;
//	fp = 0.5 + sine_table[(int32)((in - ofsi - 0.5) * M_12BIT) & table_size_mask] * DIV_2;
	fp = lookup2_sine_p((in - fp) * DIV_2);
	ofsi &= 0x1F;
    v1 = data[ofsi];
    v2 = data[ofsi + 1];
    return v1 + (v2 - v1) * fp;
}




/* PCM */
/*
rate : 基準になるサンプルカウント
pcm_rate : 変調用のサンプルカウント
pcm_rate = rate + mod_offset (compute_op_pm()ではrtに相当
変調しない場合は mod_offset = 0 なので pcm_rate = rate をセット
ついでにrateの値域チェック
out : -1.0 ~ 1.0
*/

static inline FLOAT_T compute_pcm_none(Info_OP *info)
{
	FLOAT_T *pcm_data = info->data_ptr;
	int32 index = (int32)info->pcm_rate;
	
	if(info->loop_length){ // loop
		if(info->rate >= info->ofs_end) // recompute base rate
			info->rate -= info->loop_length;	
		if(index >= info->ofs_end)
			index -= info->loop_length;	
		else if(index < info->ofs_start)
			index += info->loop_length;
		return pcm_data[index];
	}else{ // non loop
		if(index >= info->ofs_end){
			info->op_flag = 0; // pcm end	
			return 0;
		}
		return pcm_data[index];
	}
}

static inline FLOAT_T compute_pcm_linear(Info_OP *info)
{
	FLOAT_T *pcm_data = info->data_ptr;
	int32 index = (int32)info->pcm_rate;
	FLOAT_T fp = info->pcm_rate - index;
	FLOAT_T v1, v2;
	
	if(info->loop_length){ // loop
		if(info->rate >= info->ofs_end) // recompute base rate
			info->rate -= info->loop_length;	
		if(index >= info->ofs_end)
			index -= info->loop_length;	
		else if(index < info->ofs_start)
			index += info->loop_length;
		v1 = pcm_data[index];
		if(++index < info->ofs_end)
			v2 = pcm_data[index];
		else // return loop_start
			v2 = pcm_data[info->ofs_start];
	}else{ // non loop
		if(index >= info->ofs_end){
			info->op_flag = 0; // pcm end	
			return 0;
		}
		v1 = pcm_data[index];
		if(++index < info->ofs_end)
			v2 = pcm_data[index]; // linear interpolation
		else // pcm end // v2 = 0;
			v2 = 0; // linear interpolation
	}
	return v1 + (v2 - v1) * fp; // linear interpolation
}



/* LFO */
/*
out mode0 = -1.0 ~ 1.0
out mode1 = 0.0 ~ 1.0
*/

static void init_lfo(Info_LFO *info, FLOAT_T freq, FLOAT_T delay_ms, int type, FLOAT_T attack_ms, int mode)
{
	if(freq <= 0.0){
		info->out = 0.0;
		return;
	}
	if(freq < 0.001)
		freq = 0.001;
	else if(freq > 100.0)
		freq = 40.0; // safe sr>22050,cnt<256 
	info->freq = freq * div_is_sample_rate;
	if(delay_ms < 0)
		delay_ms = 0;
	info->delay = delay_ms * is_sample_rate_ms ;
	if(attack_ms < 0)
		attack_ms = 0;
	init_envelope3(&info->env, 0.0, attack_ms * is_sample_rate_ms);
	reset_envelope3(&info->env, 1.0, ENVELOPE_KEEP);
	info->osc_ptr = compute_osc[check_is_osc_wave_type(type, OSC_NOISE_LOWBIT)]; // check OSC_LIST
	info->mode = mode > 0 ? 1 : 0;
	info->rate = 0;
	info->cycle = 0;
	info->out = 0.0;
}

static void compute_lfo(Info_LFO *info, int32 count)
{
	FLOAT_T ov;
	if(info->freq <= 0.0)
		return;	
	if(info->delay > 0){
		info->delay -= count;
		return;
	}
	info->rate += info->freq * count; // +1 = 1Hz
	if(info->rate >= 1.0){
		FLOAT_T ov = floor(info->rate);
		info->rate -= ov; // reset count
		info->cycle += (int32)ov;
		info->cycle &= 0xFFFF;
	}
	compute_envelope3(&info->env, count);
	if(info->mode)
		info->out = (info->osc_ptr(info->rate, info->cycle) + 1.0) * DIV_2 * info->env.vol; // 0.0~<1.0
	else
		info->out = info->osc_ptr(info->rate, info->cycle) * info->env.vol; // -1.0~1.0
}



/* OP */
/*

in = -1.0 ~ 1.0
out = -1.0 ~ 1.0
*/

static inline void op_filter(FilterCoefficients *fc, FLOAT_T *inout)
{
	DATA_T flt = *inout;
	sample_filter(fc, &flt);
	*inout = flt;
}

#define RESET_OP_RATE \
	if(info->sync) { \
		info->sync = 0; \
		info->rate = 0.0; \
		info->cycle = 0; \
		if(info->update_width == 1) info->update_width = 2; \
	} else { \
		FLOAT_T ov = floor(info->rate); \
		if(ov != 0.0){ \
			info->rate -= ov; \
			info->cycle += (int32)ov; \
			info->cycle &= 0xFFFF; \
			if(info->update_width == 1) info->update_width = 2; \
		} \
	} \

static inline FLOAT_T calc_op_width(Info_OP *info, FLOAT_T rate)
{
	if(info->update_width == 2){
		info->update_width = 0;
		info->wave_width1 =	info->req_wave_width1; 
		info->rate_width1 =	info->req_rate_width1; 
		info->rate_width2 =	info->req_rate_width2;
	}
	if(info->wave_width1 == 0.5)
		return rate;
	else if(rate < info->wave_width1)
		return rate * info->rate_width1;
	else
		return 0.5 + (rate - info->wave_width1) * info->rate_width2;
}

static inline void compute_op_output(Info_OP *info, FLOAT_T out)
{
	// ptr_connect = out , op->in , dummy
#if (MMS_OP_CON_MAX == 4)
	*info->ptr_connect[0] += out;
	*info->ptr_connect[1] += out;
	*info->ptr_connect[2] += out;
	*info->ptr_connect[3] += out;
#else
	int j;
	for(j = 0; j < MMS_OP_CON_MAX; j++)
		*info->ptr_connect[j] += out;	
#endif
}

static inline void compute_op_sync(Info_OP *info)
{
	info->rate += info->freq; // +1/sr = 1Hz
	if(info->rate < 1.0)
		return;
	info->rate -= floor(info->rate); // reset count
	// ptr_connect = op->sync , dummy
#if (MMS_OP_CON_MAX == 4)
	*info->ptr_connect[0] = 1.0;
	*info->ptr_connect[1] = 1.0;
	*info->ptr_connect[2] = 1.0;
	*info->ptr_connect[3] = 1.0;
#else
	int j;
	for(j = 0; j < MMS_OP_CON_MAX; j++)
		*info->ptr_connect[j] = 1.0;
#endif
}

static inline void compute_op_clip(Info_OP *info)
{
	FLOAT_T in = info->in;	 

	info->in = 0.0; // clear
	if(in > info->efx_var1)
		in = info->efx_var1;
	else if(in < -info->efx_var1)
		in = -info->efx_var1;
	op_filter(&info->fc, &in); 
	compute_op_output(info, in * info->amp_vol); // include info->op_level
}

static inline void compute_op_lowbit(Info_OP *info)
{
	FLOAT_T in = info->in;
	int32 tmp;

	info->in = 0.0; // clear
	tmp = in * info->efx_var1;
	in = (FLOAT_T)tmp * info->efx_var2;
	op_filter(&info->fc, &in); 
	compute_op_output(info, in * info->amp_vol); // include info->op_level
}




static inline void compute_op_null(Info_OP *info){}

static inline void compute_op_wave_none(Info_OP *info)
{
	FLOAT_T osc, lfo1, lfo2;
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	osc = info->osc_ptr(calc_op_width(info, info->rate), info->cycle);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_wave_fm(Info_OP *info)
{
	FLOAT_T osc, rt;
	FLOAT_T in = info->in;
	
	info->in = 0.0; // clear
	info->rate += info->freq * (1.0 + (FLOAT_T)in * info->mod_level); // +1/sr = 1Hz;
	RESET_OP_RATE
	osc = info->osc_ptr(calc_op_width(info, info->rate), info->cycle);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_wave_am(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in;
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	osc = info->osc_ptr(calc_op_width(info, info->rate), info->cycle);
	osc *= (1.0 - ((FLOAT_T)in * DIV_2 + 0.5) * info->mod_level);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_wave_pm(Info_OP *info)
{
	FLOAT_T osc, rt;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	rt = info->rate + ((FLOAT_T)in * info->mod_level); // mod level;
	rt -= floor(rt);
	osc = info->osc_ptr(calc_op_width(info, rt), info->cycle);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_wave_ampm(Info_OP *info)
{
	FLOAT_T osc, rt;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	rt = info->rate + ((FLOAT_T)in * info->mod_level); // mod level;
	rt -= floor(rt);
	osc = info->osc_ptr(calc_op_width(info, rt), info->cycle);
	osc *= (1.0 - ((FLOAT_T)in * DIV_2 + 0.5) * info->mod_level);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_wave_rm(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in;
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	osc = info->osc_ptr(calc_op_width(info, info->rate), info->cycle);
	op_filter(&info->fc, &osc);
	osc *= 1.0 + (in - 1.0) * info->mod_level; // rm
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_wave_sync(Info_OP *info)
{
	compute_op_sync(info);
}

static inline void compute_op_wave_clip(Info_OP *info)
{
	compute_op_clip(info);
}

static inline void compute_op_wave_lowbit(Info_OP *info)
{
	compute_op_lowbit(info);
}

static inline void compute_op_scc_none(Info_OP *info)
{
	FLOAT_T osc, lfo1, lfo2;
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	osc = info->scc_ptr(calc_op_width(info, info->rate), info->data_ptr);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_scc_fm(Info_OP *info)
{
	FLOAT_T osc, rt;
	FLOAT_T in = info->in;
	
	info->in = 0.0; // clear
	info->rate += info->freq * (1.0 + (FLOAT_T)in * info->mod_level); // +1/sr = 1Hz;
	RESET_OP_RATE
	osc = info->scc_ptr(calc_op_width(info, info->rate), info->data_ptr);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_scc_am(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in;
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	osc = info->scc_ptr(calc_op_width(info, info->rate), info->data_ptr);
	osc *= (1.0 - ((FLOAT_T)in * DIV_2 + 0.5) * info->mod_level);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_scc_pm(Info_OP *info)
{
	FLOAT_T osc, rt;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	rt = info->rate + ((FLOAT_T)in * info->mod_level); // mod level;
	rt -= floor(rt);
	osc = info->scc_ptr(calc_op_width(info, rt), info->data_ptr);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_scc_ampm(Info_OP *info)
{
	FLOAT_T osc, rt;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	rt = info->rate + in * info->mod_level; // mod level;
	if(rt >= 1.0)
		rt -= floor(rt);
	else if(rt < 0.0)
		rt += floor(rt);
	osc = info->scc_ptr(calc_op_width(info, rt), info->data_ptr);
	osc *= (1.0 - ((FLOAT_T)in * DIV_2 + 0.5) * info->mod_level);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_scc_rm(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in;
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr = 1Hz
	RESET_OP_RATE
	osc = info->scc_ptr(calc_op_width(info, info->rate), info->data_ptr);
	op_filter(&info->fc, &osc);
	osc *= 1.0 + (in - 1.0) * info->mod_level; // rm
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_scc_sync(Info_OP *info)
{
	compute_op_sync(info);
}

static inline void compute_op_scc_clip(Info_OP *info)
{
	compute_op_clip(info);
}

static inline void compute_op_scc_lowbit(Info_OP *info)
{
	compute_op_lowbit(info);
}

static inline void compute_op_pcm_none(Info_OP *info)
{
	FLOAT_T osc;
	
	info->in = 0.0; // clear
	info->pcm_rate = info->rate += info->freq; // +1/sr*pcm_rate/root_freq = 1Hz
	osc = compute_pcm_linear(info);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_pcm_fm(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq * (1.0 + (FLOAT_T)in * info->mod_level); // +1/sr*pcm_rate/root_freq = 1Hz
	osc = compute_pcm_linear(info);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_pcm_am(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->pcm_rate = info->rate += info->freq; // +1/sr*pcm_rate/root_freq = 1Hz
	osc = compute_pcm_linear(info);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_pcm_pm(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr*pcm_rate/root_freq = 1Hz
	info->pcm_rate = info->rate + in * info->mod_level * info->pcm_cycle; // mod level;
	osc = compute_pcm_linear(info);
	osc *= (1.0 - ((FLOAT_T)in * DIV_2 + 0.5) * info->mod_level);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_pcm_ampm(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->rate += info->freq; // +1/sr*pcm_rate/root_freq = 1Hz
	info->pcm_rate = info->rate + in * info->mod_level * info->pcm_cycle; // mod level;
	osc = compute_pcm_linear(info);
	osc *= (1.0 - ((FLOAT_T)in * DIV_2 + 0.5) * info->mod_level);
	op_filter(&info->fc, &osc);
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_pcm_rm(Info_OP *info)
{
	FLOAT_T osc;
	FLOAT_T in = info->in; // 
	
	info->in = 0.0; // clear
	info->pcm_rate = info->rate += info->freq; // +1/sr*pcm_rate/root_freq = 1Hz
	osc = compute_pcm_linear(info);
	op_filter(&info->fc, &osc);
	osc *= 1.0 + (in - 1.0) * info->mod_level; // rm
	compute_op_output(info, osc * info->amp_vol); // include info->op_level
}

static inline void compute_op_pcm_sync(Info_OP *info)
{
	compute_op_sync(info);
}

static inline void compute_op_pcm_clip(Info_OP *info)
{
	compute_op_clip(info);
}

static inline void compute_op_pcm_lowbit(Info_OP *info)
{
	compute_op_lowbit(info);
}


compute_op_t compute_op[4][OP_LIST_MAX] = {
// cfg sort
	{// WAVE
		compute_op_wave_none, 
		compute_op_wave_fm, 
		compute_op_wave_am, 
		compute_op_wave_pm, 
		compute_op_wave_ampm, 
		compute_op_wave_rm, 
		compute_op_wave_sync, 
		compute_op_wave_clip, 
		compute_op_wave_lowbit, 
	},{ // SCC
		compute_op_scc_none, 
		compute_op_scc_fm, 
		compute_op_scc_am, 
		compute_op_scc_pm, 
		compute_op_scc_ampm, 
		compute_op_scc_rm, 
		compute_op_scc_sync, 
		compute_op_scc_clip, 
		compute_op_scc_lowbit, 
	},{ // MT32
		compute_op_pcm_none, 
		compute_op_pcm_fm, 
		compute_op_pcm_am, 
		compute_op_pcm_pm, 
		compute_op_pcm_ampm, 
		compute_op_pcm_rm, 
		compute_op_pcm_sync, 
		compute_op_pcm_clip, 
		compute_op_pcm_lowbit, 
	},{ // CM32L
		compute_op_pcm_none, 
		compute_op_pcm_fm, 
		compute_op_pcm_am, 
		compute_op_pcm_pm, 
		compute_op_pcm_ampm, 
		compute_op_pcm_rm, 
		compute_op_pcm_sync, 
		compute_op_pcm_clip, 
		compute_op_pcm_lowbit, 
	},
};


// compute_op_num

#define COMP_OP_NUM (*com_ptr++)(inf_ptr++)

static inline FLOAT_T compute_op_num0(InfoIS_MMS *info)
{	
	return 0;
}

static inline FLOAT_T compute_op_num1(InfoIS_MMS *info)
{	
	info->out = 0.0;
	info->op_ptr[0](&info->op[0]);
	return info->out;
}

static inline FLOAT_T compute_op_num2(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num3(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	return info->out;
}

static inline FLOAT_T compute_op_num4(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	return info->out;
}

static inline FLOAT_T compute_op_num5(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num6(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num7(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num8(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	return info->out;
}

static inline FLOAT_T compute_op_num9(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num10(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num11(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num12(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num13(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num14(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num15(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM;
	return info->out;
}

static inline FLOAT_T compute_op_num16(InfoIS_MMS *info)
{	
	compute_op_t *com_ptr = &info->op_ptr[0];
	Info_OP *inf_ptr = &info->op[0];

	info->out = 0.0;
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; COMP_OP_NUM; 
	return info->out;
}

compute_op_num_t compute_op_num[] = {
	compute_op_num0,
	compute_op_num1,
	compute_op_num2,
	compute_op_num3,
	compute_op_num4,
	compute_op_num5,
	compute_op_num6,
	compute_op_num7,
	compute_op_num8,
	compute_op_num9,
	compute_op_num10,
	compute_op_num11,
	compute_op_num12,
	compute_op_num13,
	compute_op_num14,
	compute_op_num15,
	compute_op_num16,
};




//////// synth



//// SCC
static void set_envelope_param_scc(Envelope0 *env, int32 *envp, int up, int down, FLOAT_T atk_delay)
{
	FLOAT_T ofs, cnt, tmpf;

	// init
	init_envelope0(env);
	// ENV0_END_STAGE		
	env->rate[ENV0_END_STAGE] = (FLOAT_T)ENV0_OFFSET_MAX;
	env->offset[ENV0_END_STAGE] = 0;	
	// ENV0_ATTACK_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[1] * DIV_100; // env level
	cnt = (FLOAT_T)envp[0] * is_sample_rate_ms; // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 1) {ofs = 1;}
	env->offset[ENV0_ATTACK_STAGE] = ofs;
	env->curve[ENV0_ATTACK_STAGE] = up;
	env->rate[ENV0_ATTACK_STAGE] = ofs / cnt;
	// ENV0_HOLD_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[3] * DIV_100; // env level
	cnt = (FLOAT_T)envp[2] * is_sample_rate_ms; // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_HOLD_STAGE] = ofs;
	ofs -= env->offset[ENV0_ATTACK_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_HOLD_STAGE] = cnt;}
	env->curve[ENV0_HOLD_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_HOLD_STAGE] = fabs(ofs) / cnt;
	// ENV0_DECAY_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[5] * DIV_100; // env level
	cnt = (FLOAT_T)envp[4] * is_sample_rate_ms; // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_DECAY_STAGE] = ofs;
	ofs -= env->offset[ENV0_HOLD_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_DECAY_STAGE] = cnt;}
	env->curve[ENV0_DECAY_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_DECAY_STAGE] = fabs(ofs) / cnt;
	// ENV0_SUSTAIN_STAGE
	env->offset[ENV0_SUSTAIN_STAGE] = 0; // env level	
	ofs = env->offset[ENV0_HOLD_STAGE];
	cnt = (FLOAT_T)600000.0 * is_sample_rate_ms; // env time
	env->count[ENV0_SUSTAIN_STAGE] = cnt;
	env->curve[ENV0_SUSTAIN_STAGE] = down;
	env->rate[ENV0_SUSTAIN_STAGE] = ofs / cnt;
	env->follow[ENV0_SUSTAIN_STAGE] = 1.0;		
	// ENV0_RELEASE1_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[7] * DIV_100; // env level
	cnt = (FLOAT_T)envp[6] * is_sample_rate_ms; // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_RELEASE1_STAGE] = ofs;
	ofs = (FLOAT_T)ENV0_OFFSET_MAX; // release1
	env->curve[ENV0_RELEASE1_STAGE] = down;
	env->rate[ENV0_RELEASE1_STAGE] = fabs(ofs) / cnt;
	// ENV0_RELEASE2_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[9] * DIV_100; // env level
	cnt = (FLOAT_T)envp[8] * is_sample_rate_ms; // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_RELEASE2_STAGE] = ofs;
	ofs -= env->offset[ENV0_RELEASE1_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_RELEASE2_STAGE] = cnt;}
	env->curve[ENV0_RELEASE2_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_RELEASE2_STAGE] = fabs(ofs) / cnt;
	// ENV0_RELEASE3_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[11] * DIV_100; // env level
	cnt = (FLOAT_T)envp[10] * is_sample_rate_ms; // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_RELEASE3_STAGE] = ofs;
	ofs -= env->offset[ENV0_RELEASE2_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_RELEASE3_STAGE] = cnt;}
	env->curve[ENV0_RELEASE3_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_RELEASE3_STAGE] = fabs(ofs) / cnt;
	// ENV0_RELEASE4_STAGE
	env->offset[ENV0_RELEASE4_STAGE] = ofs = 0; // env level
	cnt = (FLOAT_T)10.0 * is_sample_rate_ms; // env time
	ofs -= env->offset[ENV0_RELEASE3_STAGE];
	if(ofs == 0.0) {ofs = -1.0;}
	env->curve[ENV0_RELEASE4_STAGE] = down;
	env->rate[ENV0_RELEASE4_STAGE] = fabs(ofs) / cnt;
	// set delay
	env->delay = (atk_delay + (FLOAT_T)envp[12]) * is_sample_rate_ms;
	env->offdelay = (atk_delay + (FLOAT_T)envp[13]) * is_sample_rate_ms;
	// apply
	apply_envelope0_param(env);
}

static inline void init_scc_preset_osc(InfoIS_SCC *info, int v)
{	
	Preset_IS *is_set = info->is_set;
	Preset_SCC *set = info->set;
	int tmpi;
	FLOAT_T tmpf;

	// param
	tmpi = set->param[0]; // param0= output_level
	if(tmpi < 1)
		tmpi = 100;
	info->output_level = (FLOAT_T)tmpi * DIV_100;
	// osc
	tmpi = set->osc[1]; // osc1= freq / mlt
	tmpf = set->osc[2]; // osc2=tune cent
	tmpf += (FLOAT_T)calc_random_param(-400, 400) * DIV_100; // auto detune
	switch(set->osc[0]){ // osc0= mode 0:OSC(%) 1:OSC(ppm) 2:OSC(mHz) 3:OSC(note)
	default:
	case 0: // var cent
		info->mode = 0;
		if(tmpi < 1)
			tmpi = 100;
		info->freq_mlt = (FLOAT_T)tmpi * DIV_100 * voice[v].sample->tune; // % to mlt
		info->freq_mlt *= POW2(tmpf * DIV_1200); // osc2=tune  // cent to mlt
		info->freq = (FLOAT_T)voice[v].frequency * DIV_1000 * info->freq_mlt;
		break;
	case 1: // var ppm
		info->mode = 0;
		if(tmpi < 1)
			tmpi = 1000000;
		info->freq_mlt = (FLOAT_T)tmpi * DIV_1000000 * voice[v].sample->tune; // ppm to mlt
		info->freq_mlt *= POW2(tmpf * DIV_1200); // osc2=tune  // cent to mlt
		info->freq = (FLOAT_T)voice[v].frequency * DIV_1000 * info->freq_mlt;
		break;
	case 2: // fix mHz
		info->mode = 1;
		if(tmpi < 1)
			tmpi = 1;
		info->freq_mlt = (FLOAT_T)tmpi * DIV_1000 * voice[v].sample->tune; // mHz to Hz
		info->freq_mlt *= POW2(tmpf * DIV_1200); // osc2=tune  // cent to mlt
		info->freq = info->freq_mlt;
		break;
	case 3: // fix note
		info->mode = 1;
		if(tmpi < 0)
			tmpi = 0;
		else if(tmpi > 127)
			tmpi = 127;
		info->freq_mlt = (FLOAT_T)freq_table[tmpi] * DIV_1000 * voice[v].sample->tune; // note to Hz
		info->freq_mlt *= POW2(tmpf * DIV_1200); // osc2=tune  // cent to mlt
		info->freq = info->freq_mlt;
		break;
	}
	tmpi = set->osc[3]; // osc3= SCC_DATA
	if(scc_data_editor_override){ // IS_EDITOR
		info->data_ptr1 = scc_data_edit[0];
	}else if(tmpi < 0){ // noise
		info->data_ptr1 = NULL;
	}else{
		if(tmpi >= SCC_DATA_MAX)
			tmpi = 0;
		info->data_ptr1 = is_set->scc_data[tmpi];
	}	
	tmpi = set->osc[4]; // osc4= time2
	if(tmpi < 1 || tmpi > 100000)
		tmpi = 0;
	info->data2_count = tmpi * is_sample_rate_ms;
	tmpi = set->osc[5]; // osc5= SCC_DATA2
	if(tmpi < 0){ // noise
		info->data_ptr2 = NULL;
	}else{
		if(tmpi >= SCC_DATA_MAX)
			tmpi = 0;
		info->data_ptr2 = is_set->scc_data[tmpi];
	}
	tmpi = set->osc[6]; // osc6= flag3
	tmpi = tmpi ? 1 : 0;
	info->data3_flag = tmpi;
	tmpi = set->osc[7]; // osc7= SCC_DATA3
	if(tmpi < 0){ // noise
		info->data_ptr3 = NULL;
	}else{
		if(tmpi >= SCC_DATA_MAX)
			tmpi = 0;
		info->data_ptr3 = is_set->scc_data[tmpi];
	}	
	if(info->data2_count){
		info->data_ptr = info->data_ptr2;
	}else{
		info->data_ptr = info->data_ptr1;
	}
	info->rate = 0;
	info->cycle = 0;
	// amp
	tmpi = set->amp[0]; // amp0= lfo1 to amp
	if(tmpi < 0)
		tmpi = 0;
	else if(tmpi > 100)
		tmpi = 100;
	info->lfo_amp = (FLOAT_T)tmpi * DIV_100;
	// pitch
	info->lfo_pitch = (FLOAT_T)set->pitch[0] * DIV_1200; // pitch0= lfo2 to freq cent
	info->env_pitch = (FLOAT_T)set->pitch[1] * DIV_1200; // pitch1= env to freq cent
	info->pitch = 1.0;
	// ampenv
	tmpf = (FLOAT_T)calc_random_param(0, 400) * DIV_100; // attack delay
	set_envelope_param_scc(&info->amp_env, set->ampenv, LINEAR_CURVE, DEF_VOL_CURVE, tmpf);
	// pitenv
	set_envelope_param_scc(&info->pit_env, set->pitenv, LINEAR_CURVE, LINEAR_CURVE, tmpf);
	// lfo1
	if(info->lfo_amp != 0.0)
		init_lfo(&info->lfo1, (FLOAT_T)set->lfo1[0] * DIV_100, set->lfo1[1], set->lfo1[2], set->lfo1[3], 1);
	// lfo2
	if(info->lfo_pitch != 0.0)
		init_lfo(&info->lfo2, (FLOAT_T)set->lfo2[0] * DIV_100, set->lfo2[1], set->lfo2[2], set->lfo2[3], 0);
	// mode
	tmpi = set->mode[0]; // release
	tmpi = tmpi ? 1 : 0;
	info->skip_flag = tmpi;
	tmpi = set->mode[1]; // loop ms
	if(tmpi <= 0 || tmpi > 100000) // 100000ms 100s
		tmpi = 0;
	info->loop_count = tmpi * is_sample_rate_ms;
	// other
	info->scc_flag = 1;
}

static inline void init_scc_preset(int v, InfoIS_SCC *info, Preset_IS *is_set, int preset)
{
	Preset_SCC *set = scc_editor_override ? &scc_setting_edit[0] : is_set->scc_setting[preset];
	int tmpi;
	FLOAT_T tmpf;
	
	if(set == NULL){
		info->init = 0;
		return;
	}
	info->init = 1;
	info->set = set;
	info->is_set = is_set;
	
	init_scc_preset_osc(info, v);	
	// resample
	is_resample_init(&info->rs);

	info->thru_count = -1; // init

}

static void noteoff_scc(InfoIS_SCC *info)
{
	if(!info->init) return;
	if(info->loop_count) return;
	if(info->skip_flag){
		info->skip_flag = -1;
		return;
	}
	reset_envelope0_release(&info->amp_env, ENV0_KEEP);
	reset_envelope0_release(&info->pit_env, ENV0_KEEP);
}

static void damper_scc(InfoIS_SCC *info, int8 damper)
{
	if(!info->init) return;
	if(info->loop_count) return;	
	if(info->skip_flag) return;
	reset_envelope0_damper(&info->amp_env, damper);
	reset_envelope0_damper(&info->pit_env, damper);
}

static void pre_compute_scc(InfoIS_SCC *info, int v, int32 count)
{
	Voice *vp = voice + v;
	FLOAT_T amp = 1.0, pitch = 0.0;
	int scount, thru_flg;
	
	if(info->scc_flag == 0 && !info->loop_count){ // scc off	
		vp->finish_voice = 1;
		return;
	}
	// release
	if(info->skip_flag == -1){
		init_scc_preset_osc(info, v);
		info->skip_flag = 0;
	}else if(info->skip_flag > 0){
		return;
	}
	// loop
	if(info->loop_count == -1){
		init_scc_preset_osc(info, v);
	}else if(info->loop_count > 0){
		info->loop_count -= count;
		if(info->loop_count <= 0)
			info->loop_count = -1; // reset flag
	}
	// SCC_DATA2
	if(info->data2_count){
		if(info->data2_count == -1){
			info->data_ptr = info->data_ptr1;
			info->data2_count = 0;
		}else{
			info->data2_count -= count;
			if(info->data2_count <= 0)
				info->data2_count = -1;
		}
	}
	// SCC_DATA3
	if(info->data3_flag){
		if(check_envelope0(&info->amp_env) >= ENV0_RELEASE1_STAGE){
			info->data_ptr = info->data_ptr3;
			info->data2_count = 0;
			info->data3_flag = 0;
		}
	}
	// amp
	compute_envelope0(&info->amp_env, count);	
	if(!check_envelope0(&info->amp_env)){ // amp_env end
		info->amp_vol = 0.0;
		if(!info->loop_count)
			info->scc_flag = 0; // scc off
		return;
	}
	// thru count
	if(thru_count_mlt > 1){
		scount = count * thru_count_mlt;
		if((++info->thru_count) >= thru_count_mlt)
			info->thru_count = 0;
		thru_flg = info->thru_count;
	}else{
		scount = count;
		thru_flg = 0;
	}	
	// amp
	amp *= info->amp_env.volume;
	if(info->lfo_amp > 0.0){
		if(!thru_flg)
			compute_lfo(&info->lfo1, scount);
		amp *= 1.0 - info->lfo1.out * info->lfo_amp;
	}	
	info->amp_vol = floor(amp * 15.0) * DIV_15; // 4bit amp
	info->amp_vol *= info->output_level;
	if(thru_flg)
		return;

	// pitch
	if(info->env_pitch != 0.0){
		compute_envelope0(&info->pit_env, scount);
		pitch += info->env_pitch * info->pit_env.volume;
	}	
	if(info->lfo_pitch != 0.0){
		compute_lfo(&info->lfo2, scount);
		pitch += info->lfo_pitch * info->lfo2.out;
	}
	if(!info->mode)
		info->freq = (FLOAT_T)vp->frequency * DIV_1000 * info->freq_mlt;
	else {
		if(vp->pitchfactor)
			info->freq = (FLOAT_T)info->freq_mlt * vp->pitchfactor;
		else
			info->freq = (FLOAT_T)info->freq_mlt;
		info->freq += channel[vp->channel].pitch_offset_fine;
	}
	info->freq *= POW2(pitch) * div_is_sample_rate; // 1/sr = 1Hz
}

static inline FLOAT_T compute_scc(InfoIS_SCC *info)
{
	if(info->skip_flag)
		return 0;
	info->rate += info->freq; // +1/sr = 1Hz
	if(info->data_ptr){
		info->rate -= floor(info->rate);
		return compute_osc_scc_none(info->rate, info->data_ptr) * info->amp_vol;
	}else{ // noise
		FLOAT_T ov = floor(info->rate);
		info->rate -= ov;
		info->cycle += (int32)ov;
		info->cycle &= 0xFFFF;
		return lookup_noise_lowbit(info->rate, info->cycle) * info->amp_vol;
	}
}

static inline void compute_voice_scc_switch(int v, int32 count, DATA_T *is_buf, IS_RS_DATA_T *rs_buf)
{
	Voice *vp = voice + v;
	InfoIS_SCC *info = vp->scc;
	int32 i;
	
	if(!info->init){
		memset(is_buf, 0, count * sizeof(DATA_T));		
	}else if(!is_rs_mode){
		pre_compute_scc(info, v, count);
		for (i = 0; i < count; i++)
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
			is_buf[i] = (DATA_T)compute_scc(info);
#else
			is_buf[i] = (DATA_T)(compute_scc(info) * is_output_level);
#endif
	}else{
		IS_RS_DATA_T *rs_buf2 = rs_buf + is_rs_buff_offset;
		Info_Resample *rs = &info->rs;
	
		is_resample_pre(rs, rs_buf, count);
		pre_compute_scc(info, v, rs->rs_count);
		for (i = 0; i < rs->rs_count; i++)
			rs_buf2[i] = (IS_RS_DATA_T)compute_scc(info);
		is_resample_core(rs, is_buf, rs_buf, count);
	}  
}



//// MMS
static void set_envelope_param_mms(Envelope0 *env, int32 *envp, int16 *velf, int16 *keyf, int up, int down, FLOAT_T sub_velo, FLOAT_T sub_note)
{
	FLOAT_T ofs, cnt;

	init_envelope0(env);
	env->delay = envp[12] * is_sample_rate_ms;
	env->offdelay = envp[13] * is_sample_rate_ms;
	// ENV0_END_STAGE		
	env->rate[ENV0_END_STAGE] = (FLOAT_T)ENV0_OFFSET_MAX;
	env->offset[ENV0_END_STAGE] = 0;	
	// ENV0_ATTACK_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[1] * DIV_100
		* POW2(((FLOAT_T)sub_velo * velf[1] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[1] * DIV_1200)); // env level
	cnt = (FLOAT_T)envp[0] * is_sample_rate_ms
		* POW2(((FLOAT_T)sub_velo * velf[0] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[0] * DIV_1200)); // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 1) {ofs = 1;}
	env->offset[ENV0_ATTACK_STAGE] = ofs;
	env->curve[ENV0_ATTACK_STAGE] = up;
	env->rate[ENV0_ATTACK_STAGE] = ofs / cnt;
	// ENV0_HOLD_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[3] * DIV_100
		* POW2(((FLOAT_T)sub_velo * velf[3] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[3] * DIV_1200)); // env level
	cnt = (FLOAT_T)envp[2] * is_sample_rate_ms
		* POW2(((FLOAT_T)sub_velo * velf[2] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[2] * DIV_1200)); // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_HOLD_STAGE] = ofs;
	ofs -= env->offset[ENV0_ATTACK_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_HOLD_STAGE] = cnt;}
	env->curve[ENV0_HOLD_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_HOLD_STAGE] = fabs(ofs) / cnt;
	// ENV0_DECAY_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[5] * DIV_100
		* POW2(((FLOAT_T)sub_velo * velf[5] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[5] * DIV_1200)); // env level
	cnt = (FLOAT_T)envp[4] * is_sample_rate_ms
		* POW2(((FLOAT_T)sub_velo * velf[4] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[4] * DIV_1200)); // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_DECAY_STAGE] = ofs;
	ofs -= env->offset[ENV0_HOLD_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_DECAY_STAGE] = cnt;}
	env->curve[ENV0_DECAY_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_DECAY_STAGE] = fabs(ofs) / cnt;
	// ENV0_SUSTAIN_STAGE
	env->offset[ENV0_SUSTAIN_STAGE] = 0; // env level	
	ofs = env->offset[ENV0_HOLD_STAGE];
	cnt = (FLOAT_T)600000.0 * is_sample_rate_ms; // env time
	env->count[ENV0_SUSTAIN_STAGE] = cnt;
	env->curve[ENV0_SUSTAIN_STAGE] = down;
	env->rate[ENV0_SUSTAIN_STAGE] = ofs / cnt;
	env->follow[ENV0_SUSTAIN_STAGE] = 1.0;		
	// ENV0_RELEASE1_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[7] * DIV_100
		* POW2(((FLOAT_T)sub_velo * velf[7] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[7] * DIV_1200)); // env level
	cnt = (FLOAT_T)envp[6] * is_sample_rate_ms
		* POW2(((FLOAT_T)sub_velo * velf[6] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[6] * DIV_1200)); // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_RELEASE1_STAGE] = ofs;
	ofs = (FLOAT_T)ENV0_OFFSET_MAX; // release1
	env->curve[ENV0_RELEASE1_STAGE] = down;
	env->rate[ENV0_RELEASE1_STAGE] = fabs(ofs) / cnt;
	// ENV0_RELEASE2_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[9] * DIV_100
		* POW2(((FLOAT_T)sub_velo * velf[9] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[9] * DIV_1200)); // env level
	cnt = (FLOAT_T)envp[8] * is_sample_rate_ms
		* POW2(((FLOAT_T)sub_velo * velf[8] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[8] * DIV_1200)); // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_RELEASE2_STAGE] = ofs;
	ofs -= env->offset[ENV0_RELEASE1_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_RELEASE2_STAGE] = cnt;}
	env->curve[ENV0_RELEASE2_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_RELEASE2_STAGE] = fabs(ofs) / cnt;
	// ENV0_RELEASE3_STAGE
	ofs = (FLOAT_T)ENV0_OFFSET_MAX * envp[11] * DIV_100
		* POW2(((FLOAT_T)sub_velo * velf[11] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[11] * DIV_1200)); // env level
	cnt = (FLOAT_T)envp[10] * is_sample_rate_ms
		* POW2(((FLOAT_T)sub_velo * velf[10] * DIV_127 * DIV_100) + ((FLOAT_T)sub_note * keyf[10] * DIV_1200)); // env time
	if(cnt == 0.0) {cnt = is_sample_rate_ms;}
	if(ofs > ENV0_OFFSET_MAX) {ofs = ENV0_OFFSET_MAX;}
	else if(ofs < 0) {ofs = 0;}
	env->offset[ENV0_RELEASE3_STAGE] = ofs;
	ofs -= env->offset[ENV0_RELEASE2_STAGE];
	if(ofs == 0.0) {ofs = -1.0; env->count[ENV0_RELEASE3_STAGE] = cnt;}
	env->curve[ENV0_RELEASE3_STAGE] = (ofs > 0.0) ? up : down;
	env->rate[ENV0_RELEASE3_STAGE] = fabs(ofs) / cnt;
	// ENV0_RELEASE4_STAGE
	env->offset[ENV0_RELEASE4_STAGE] = ofs = 0; // env level
	cnt = (FLOAT_T)10.0 * is_sample_rate_ms; // env time
	ofs -= env->offset[ENV0_RELEASE3_STAGE];
	if(ofs == 0.0) {ofs = -1.0;}
	env->curve[ENV0_RELEASE4_STAGE] = down;
	env->rate[ENV0_RELEASE4_STAGE] = fabs(ofs) / cnt;

	apply_envelope0_param(env);
}

static inline void init_mms_preset_op(InfoIS_MMS *info, int v, int i)
{	
	Preset_IS *is_set = info->is_set;
	Preset_MMS *set = info->set;
	double delay_cnt, attack_cnt, hold_cnt, decay_cnt, sustain_vol, sustain_cnt, release_cnt;
	int note = voice[v].note, velo = voice[v].velocity;
	FLOAT_T sub_note = note - 60, sub_velo = 127 - velo, div_velo = (FLOAT_T)velo * DIV_127;
	int j, tmpi;	
	Info_OP *info2 = &info->op[i];

	// param
	tmpi = set->op_param[i][0]; // param0 = op mode 0:OSC 1:FM 2:AM 3:PM 4:AMPM 5:RM 6:SYNC (7:CLIP 8:LOWBIT
	if(tmpi < 0 || tmpi >= OP_LIST_MAX)
		tmpi = 0;
	info2->mod_type = tmpi;		
	switch(info2->mod_type){
	default:
		tmpi = set->op_param[i][1]; // param1 = mod level %
		if(tmpi < 1)
			tmpi = 100;		
		info2->mod_level = (FLOAT_T)tmpi * DIV_100;
		break;
	case OP_CLIP:
		info2->mod_level = 1.0;
		tmpi = set->op_param[i][1]; // param1 = clip level %
		if(tmpi < 1)
			tmpi = 100;		
		info2->efx_var1 = (FLOAT_T)tmpi * DIV_100;
		break;
	case OP_LOWBIT:
		info2->mod_level = 1.0;
		tmpi = set->op_param[i][1]; // param1 = multiply
		if(tmpi < 2)
			tmpi = 2;		
		info2->efx_var1 = (FLOAT_T)tmpi;
		info2->efx_var2 = 1.0 / info2->efx_var1;
		break;
	}
	tmpi = set->op_param[i][2]; // param2 = op level %
	if(tmpi < 1)
		tmpi = 100;		
	info2->op_level = (FLOAT_T)tmpi * DIV_100;
	// range
	if(note < set->op_range[i][0] // range0 = key low
	|| note > set->op_range[i][1] // range1 = key hi
	|| velo < set->op_range[i][2] // range2 = velo low
	|| velo > set->op_range[i][3] // range3 = velo hi
	)
		info2->op_flag = 0; // off
	else
		info2->op_flag = 1; // on
	// connect
	if(info2->mod_type == OP_SYNC){
		for(j = 0; j < MMS_OP_CON_MAX; j++){ // connect0 ~ connect3
			tmpi = set->op_connect[i][j]; // connect0 = -1:none(def) -2:output 0~7:op num
			if(tmpi >= 0 && tmpi <= info->op_max)
				info2->ptr_connect[j] = &info->op[tmpi].sync; 
			else
				info2->ptr_connect[j] = &info->dummy;	// no connect
		}
	}else{
		for(j = 0; j < MMS_OP_CON_MAX; j++){ // connect0 ~ connect3
			tmpi = set->op_connect[i][j]; // connect0 = -1:none(def) -2:output 0~7:op num
			if(tmpi == -2)
				info2->ptr_connect[j] = &info->out;	// output
			else if(tmpi <= -1 || tmpi >= info->op_max)
				info2->ptr_connect[j] = &info->dummy;	// no connect
			else
				info2->ptr_connect[j] = &info->op[tmpi].in; 
		}
	}
	// osc
	switch(set->op_osc[i][0]){ // osc0= mode 0:OSC(%) 0:OSC(ppm) 1:OSC(mHz) 2:OSC(note)
	default:
	case 0: // var %
		info2->mode = 0;
		tmpi = set->op_osc[i][1]; // osc1= freq / mlt
		if(tmpi < 1)
			tmpi = 100;
		info2->freq_mlt = (FLOAT_T)tmpi * DIV_100 * voice[v].sample->tune; // % to mlt
		info2->freq_mlt *= POW2((FLOAT_T)set->op_osc[i][2] * DIV_1200); // osc2=tune  // cent to mlt
		info2->freq = (FLOAT_T)voice[v].frequency * DIV_1000 * info2->freq_mlt;
		break;
	case 1: // var ppm
		info2->mode = 0;
		tmpi = set->op_osc[i][1]; // osc1= freq / mlt
		if(tmpi < 1)
			tmpi = 1000000;
		info2->freq_mlt = (FLOAT_T)tmpi * DIV_1000000 * voice[v].sample->tune; // ppm to mlt
		info2->freq_mlt *= POW2((FLOAT_T)set->op_osc[i][2] * DIV_1200); // osc2=tune  // cent to mlt
		info2->freq = (FLOAT_T)voice[v].frequency * DIV_1000 * info2->freq_mlt;
		break;
	case 2: // fix mHz 
		info2->mode = 1;
		tmpi = set->op_osc[i][1]; // osc1= freq / mlt
		if(tmpi < 1)
			tmpi = 1;
		info2->freq_mlt = (FLOAT_T)tmpi * DIV_1000 * voice[v].sample->tune; // mHz to Hz
		info2->freq_mlt *= POW2((FLOAT_T)set->op_osc[i][2] * DIV_1200); // osc2=tune  // cent to mlt
		info2->freq = info2->freq_mlt;
		break;
	case 3: // fix note 
		info2->mode = 1;
		tmpi = set->op_osc[i][1]; // osc1= freq / mlt
		if(tmpi < 0)
			tmpi = 0;
		else if(tmpi > 127)
			tmpi = 127;
		info2->freq_mlt = (FLOAT_T)freq_table[tmpi] * DIV_1000 * voice[v].sample->tune; // note to Hz
		info2->freq_mlt *= POW2((FLOAT_T)set->op_osc[i][2] * DIV_1200); // osc2=tune  // cent to mlt
		info2->freq = info2->freq_mlt;
		break;
	}
	// wave
	switch(set->op_wave[i][0]){ // wave0 = osc type 0:wave 1:SCC 2:MT32 3:CM32
	default:
	case MMS_OSC_WAVE:
		info2->osc_type = 0;
		tmpi = set->op_wave[i][1]; // wave1 = wave type  wave 0:sine ~ 
		info2->wave_type = check_is_osc_wave_type(tmpi, OSC_NOISE_LOWBIT);
		info2->osc_ptr = compute_osc[info2->wave_type];
		tmpi = set->op_wave[i][2]; // wave2 = osc width %
		if(tmpi < 0)
			tmpi = 0;
		else if(tmpi > 200)
			tmpi = 200;
		info2->wave_width = (FLOAT_T)tmpi * DIV_200; // width=100%  ratio=50:50=(width/2):(1-width/2)	
		tmpi = set->op_wave[i][3]; // wave3 = phase offset % (start rate
		if(tmpi < 0 || tmpi > 99)
			tmpi = 0;
		info2->rate = (FLOAT_T)tmpi * DIV_100;	
		info2->freq_coef = div_is_sample_rate; // +1/sr = 1Hz
		break;
	case MMS_OSC_SCC:
		info2->osc_type = 1;
		tmpi = set->op_wave[i][1]; // wave1 = scc type  , scc number 0~511
		if(tmpi < 0 || tmpi > SCC_DATA_MAX)
			tmpi = 0;
		info2->wave_type = tmpi;
		info2->data_ptr = is_set->scc_data[tmpi];			
		if(scc_data_editor_override) // IS_EDITOR
			info2->data_ptr = scc_data_edit[0];
		tmpi = set->op_wave[i][2]; // wave2 = wave width %
		if(tmpi < 0 || tmpi > 200)
			tmpi = 0;
		info2->wave_width = (FLOAT_T)tmpi * DIV_200; // width=100%  ratio=50:50=(width/2):(1-width/2)		
		tmpi = set->op_wave[i][3]; // wave3 = phase offset % (start rate
		if(tmpi < 0 || tmpi > 99)
			tmpi = 0;
		info2->rate = (FLOAT_T)tmpi * DIV_100;
		info2->freq_coef = div_is_sample_rate; // +1/sr = 1Hz
		info2->scc_ptr = compute_osc_scc_linear;
	//	info2->scc_ptr = compute_osc_scc_sine;
		if(!set->op_wave[i][4]) // wave4 拡張フラグ
			break;
		switch(set->op_wave[i][5]){ // wave5 = interpoation
		default:
		case 0:
			info2->scc_ptr = compute_osc_scc_none;
			break;
		case 1:
			info2->scc_ptr = compute_osc_scc_linear;
			break;
		case 2:
			info2->scc_ptr = compute_osc_scc_sine;
			break;
		}
		break;
	case MMS_OSC_MT32:
		{
		Info_PCM *pcm_inf; 
		info2->osc_type = 2;
		tmpi = set->op_wave[i][1]; // wave1 = pcm type , pcm number 0~	
		if(tmpi < 0 || tmpi >= MT32_DATA_MAX)
			tmpi = 0;
		pcm_inf = &mt32_pcm_inf[tmpi];
		info2->wave_type = tmpi;
		info2->data_ptr = mt32_pcm_data;
		if(pcm_inf->loop){
			info2->ofs_start = pcm_inf->loop_start;
			info2->ofs_end = pcm_inf->loop_end;
			info2->loop_length = pcm_inf->loop_length;
		}else{
			info2->ofs_start = pcm_inf->ofs_start;
			info2->ofs_end = pcm_inf->ofs_end;
			info2->loop_length = 0;
		}
		info2->pcm_cycle = pcm_inf->sample_rate * pcm_inf->div_root_freq; // PCMの1周期分相当
		info2->freq_coef = div_is_sample_rate * info2->pcm_cycle; // +1/sr*pcm_rate/root_freq = 1Hz
		info2->rate = pcm_inf->ofs_start;
		}
		break;
	case MMS_OSC_CM32L:
		{
		Info_PCM *pcm_inf; 
		info2->osc_type = 3;
		tmpi = set->op_wave[i][1]; // wave1 = pcm type , pcm number 0~	
		if(tmpi < 0 || tmpi >= CM32L_DATA_MAX)
			tmpi = 0;
		pcm_inf = &cm32l_pcm_inf[tmpi];
		info2->wave_type = tmpi;
		info2->data_ptr = cm32l_pcm_data;
		if(pcm_inf->loop){
			info2->ofs_start = pcm_inf->loop_start;
			info2->ofs_end = pcm_inf->loop_end;
			info2->loop_length = pcm_inf->loop_length;
		}else{
			info2->ofs_start = pcm_inf->ofs_start;
			info2->ofs_end = pcm_inf->ofs_end;
			info2->loop_length = 0;
		}
		info2->pcm_cycle = pcm_inf->sample_rate * pcm_inf->div_root_freq; // PCMの1周期分相当
		info2->freq_coef = div_is_sample_rate * info2->pcm_cycle; // +1/sr*pcm_rate/root_freq = 1Hz
		info2->rate = pcm_inf->ofs_start;
		}
		break;
	}
	// sub
	tmpi = set->op_sub[i][0]; // sub0= velo to mod_level
	if(tmpi < 1)
		tmpi = 100;	
	info2->mod_level *= mix_double(div_velo, tmpi * DIV_100, 1.0); // vel=127 modlv=100% , vel=0 modlv=tmpi%
	tmpi = set->op_sub[i][1]; // sub1= velo to op_level
	if(tmpi < 1)
		tmpi = 100;	
	info2->op_level *= mix_double(div_velo, tmpi * DIV_100, 1.0); // vel=127 modlv=100% , vel=0 modlv=tmpi%
	tmpi = set->op_sub[i][2]; // sub2= key to mod_level
	info2->mod_level *= POW2((FLOAT_T)sub_note * tmpi * DIV_1200);
	tmpi = set->op_sub[i][3]; // sub3= key to op_level
	info2->op_level *= POW2((FLOAT_T)sub_note * tmpi * DIV_1200);
	// amp
	tmpi = set->op_amp[i][0]; // amp0= lfo1 to amp
	if(tmpi < 0)
		tmpi = 0;
	else if(tmpi > 100)
		tmpi = 100;
	info2->lfo_amp = (FLOAT_T)tmpi * DIV_100;
	info2->amp_vol = 0.0;
	// pitch
	info2->lfo_pitch = (FLOAT_T)set->op_pitch[i][0] * DIV_1200; // pitch0= lfo2 to freq 
	info2->env_pitch = (FLOAT_T)set->op_pitch[i][1] * DIV_1200; // pitch1= pitenv to freq cent
	// width
	if(info2->osc_type <= MMS_OSC_SCC){ 	
		tmpi = set->op_width[i][0]; // width0= lfo3 to wave_width %
		if(tmpi < -200)
			tmpi = -200;
		else if(tmpi > 200)
			tmpi = 200;	
		info2->lfo_width = (FLOAT_T)tmpi * DIV_200;
		tmpi = set->op_width[i][1]; // width1= widenv to wave_width %
		if(tmpi < -200)
			tmpi = -200;
		else if(tmpi > 200)
			tmpi = 200;
		info2->env_width = (FLOAT_T)tmpi * DIV_200;		
		tmpi = set->op_width[i][2]; // width2= vel to wave_width %
		if(tmpi < -200)
			tmpi = -200;
		else if(tmpi > 200)
			tmpi = 200;	
		info2->wave_width += div_velo * (FLOAT_T)tmpi * DIV_200;
		if(info2->wave_width >= 1.0){
			info2->wave_width = 1.0;
			info2->req_wave_width1 = info2->wave_width1 = 1.0;
			info2->req_rate_width1 = info2->rate_width1 = 0.5;
			info2->req_rate_width2 = info2->rate_width2 = 0.0;
		}else if(info2->wave_width <= 0.0){
			info2->wave_width = 0.0;
			info2->req_wave_width1 = info2->wave_width1 = 0.0;
			info2->req_rate_width1 = info2->rate_width1 = 0.0;
			info2->req_rate_width2 = info2->rate_width2 = 0.5;
		}else{
			info2->req_wave_width1 = info2->wave_width1 = info2->wave_width;
			info2->req_rate_width1 = info2->rate_width1 = 0.5 / (info2->wave_width1);
			info2->req_rate_width2 = info2->rate_width2 = 0.5 / (1.0 - info2->wave_width1);
		}
	}else{
		info2->lfo_width = 0.0;
		info2->env_width = 0.0;
		info2->req_wave_width1 = info2->wave_width1 = 0.5;
		info2->req_rate_width1 = info2->rate_width1 = 1.0;
		info2->req_rate_width2 = info2->rate_width2 = 1.0;
	}
	// filter
	set_sample_filter_type(&info2->fc, set->op_filter[i][0]); // filter 0 = type
	if(info2->fc.type){
		info2->flt_freq = (FLOAT_T)set->op_filter[i][1]; // filter 1 = freq	
		info2->flt_reso = (FLOAT_T)set->op_filter[i][2]; // filter 2 = reso
	// cutoff
		info2->lfo_cutoff = (FLOAT_T)set->op_cutoff[i][0] * DIV_1200; // cutoff 0 = lfo4 to cutoff
		info2->env_cutoff = (FLOAT_T)set->op_cutoff[i][1] * DIV_1200; // cutoff 1 = env to cutoff
		info2->flt_freq *= POW2(((FLOAT_T)sub_velo * set->op_cutoff[i][2] * DIV_127 * DIV_100) // cutoff 2 = vel to cutoff
			+ ((FLOAT_T)sub_note * set->op_cutoff[i][3] * DIV_1200)); // cutoff 3 = key to cutoff
		set_sample_filter_ext_rate(&info2->fc, is_sample_rate);
		set_sample_filter_freq(&info2->fc, info2->flt_freq);
		set_sample_filter_reso(&info2->fc, info2->flt_reso);
	}else{ // filter off
		info2->flt_freq = 0;
		info2->flt_reso = 0;
		info2->env_cutoff = 0.0;
		info2->lfo_cutoff = 0.0;
	}		
	// amp_env
	if(info2->mod_type < OP_SYNC){
		info2->amp_env_flg = 1;
		set_envelope_param_mms(&info2->amp_env, 
			set->op_ampenv[i], set->op_ampenv_velf[i], set->op_ampenv_keyf[i], 
			LINEAR_CURVE, DEF_VOL_CURVE, sub_velo, sub_note);
	}else{
		info2->amp_env_flg = 0;
		info2->amp_env.volume = 0.0;
	}
	// pit_env
	if(info2->env_pitch != 0.0){
		info2->pit_env_flg = 1;
		set_envelope_param_mms(&info2->pit_env, 
			set->op_pitenv[i], set->op_pitenv_velf[i], set->op_pitenv_keyf[i], 
			LINEAR_CURVE, LINEAR_CURVE, sub_velo, sub_note);
	}else{
		info2->pit_env_flg = 0;
		info2->pit_env.volume = 0.0;
	}			
	// wid_env
	if(info2->env_width != 0.0){			
		info2->wid_env_flg = 1;
		set_envelope_param_mms(&info2->wid_env, 
			set->op_widenv[i], set->op_widenv_velf[i], set->op_widenv_keyf[i], 
			LINEAR_CURVE, LINEAR_CURVE, sub_velo, sub_note);
	}else{
		info2->wid_env_flg = 0;
		info2->wid_env.volume = 0.0;
	}
	// mod_env
	if(info2->env_cutoff != 0.0){
		info2->mod_env_flg = 1;
		set_envelope_param_mms(&info2->mod_env, 
			set->op_modenv[i], set->op_modenv_velf[i], set->op_modenv_keyf[i], 
			SF2_CONVEX, SF2_CONCAVE, sub_velo, sub_note);
	}else{
		info2->mod_env_flg = 0;
		info2->mod_env.volume = 0.0;
	}
	// lfo1
	if(info2->lfo_amp != 0.0){
		info2->lfo1_flg = 1;
		init_lfo(&info2->lfo1, (FLOAT_T)set->op_lfo1[i][0] * DIV_100, set->op_lfo1[i][1], set->op_lfo1[i][2], set->op_lfo1[i][3], 1);
	}else{
		info2->lfo1_flg = 0;
		info2->lfo1.out = 0.0;
	}
	// lfo2
	if(info2->lfo_pitch != 0.0){			
		info2->lfo2_flg = 1;
		init_lfo(&info2->lfo2, (FLOAT_T)set->op_lfo2[i][0] * DIV_100, set->op_lfo2[i][1], set->op_lfo2[i][2], set->op_lfo2[i][3], 0);
	}else{
		info2->lfo2_flg = 0;
		info2->lfo2.out = 0.0;
	}
	// lfo3
	if(info2->lfo_width != 0.0){			
		info2->lfo3_flg = 1;
		init_lfo(&info2->lfo3, (FLOAT_T)set->op_lfo3[i][0] * DIV_100, set->op_lfo3[i][1], set->op_lfo3[i][2], set->op_lfo3[i][3], 1);
	}else{
		info2->lfo3_flg = 0;
		info2->lfo3.out = 0.0;
	}
	// lfo4
	if(info2->lfo_cutoff != 0.0){			
		info2->lfo4_flg = 1;
		init_lfo(&info2->lfo4, (FLOAT_T)set->op_lfo4[i][0] * DIV_100, set->op_lfo4[i][1], set->op_lfo4[i][2], set->op_lfo4[i][3], 1);
	}else{
		info2->lfo4_flg = 0;
		info2->lfo4.out = 0.0;
	}
	// mode
	tmpi = set->op_mode[i][0]; // release
	tmpi = tmpi ? 1 : 0;
	info2->skip_flag = tmpi;
	tmpi = set->op_mode[i][1]; // loop ms
	if(tmpi <= 0 || tmpi > 100000) // 100000ms 100s
		tmpi = 0;
	info2->loop_count = tmpi * is_sample_rate_ms;
	// other
	info2->in = info2->sync = 0.0;
	info2->cycle = 0;
	// op_ptr
	if(info2->op_flag)
		info->op_ptr[i] = compute_op[info2->osc_type][info2->mod_type];
	else
		info->op_ptr[i] = compute_op_null;
}

static inline void init_mms_preset(int v, InfoIS_MMS *info, Preset_IS *is_set, int preset)
{
	Preset_MMS *set = mms_editor_override ? &mms_setting_edit[0] : is_set->mms_setting[preset];
	int i, j, tmpi;

	if(set == NULL){
		info->init = 0;
		return;
	}
	info->init = 1;	
	info->set = set;
	info->is_set = is_set;

	tmpi = set->op_max; // op max
	if(tmpi < 1 || tmpi > MMS_OP_MAX)
		tmpi = 0;
	info->op_max = tmpi;
	// init op
	for(i = 0; i < info->op_max; i++){
		init_mms_preset_op(info, v, i);
		// release
		if(info->op[i].skip_flag)
			info->op_ptr[i] = compute_op_null;
	}	
	// resample
	is_resample_init(&info->rs);
	// mms_ptr
	info->op_num_ptr = compute_op_num[info->op_max];

	info->thru_count = -1; // init
	
}

static void noteoff_mms(InfoIS_MMS *info)
{
	int i;
	
	if(!info->init) return;
	for(i = 0; i < info->op_max; i++){
		Info_OP *info2 = &info->op[i];
		
		if(info2->op_flag == 0) // op off
			continue;
		if(info2->loop_count)
			continue;
		if(info2->skip_flag){
			info2->skip_flag = -1;
			return;
		}
		if(info2->amp_env_flg)
			reset_envelope0_release(&info2->amp_env, ENV0_KEEP);
		if(info2->pit_env_flg)
			reset_envelope0_release(&info2->pit_env, ENV0_KEEP);
		if(info2->wid_env_flg)
			reset_envelope0_release(&info2->wid_env, ENV0_KEEP);
		if(info2->mod_env_flg)
			reset_envelope0_release(&info2->mod_env, ENV0_KEEP);
	}
}

static void damper_mms(InfoIS_MMS *info, int8 damper)
{
	int i;
	
	if(!info->init) return;
	for(i = 0; i < info->op_max; i++){
		Info_OP *info2 = &info->op[i];
		
		if(info2->op_flag == 0) // op off
			continue;
		if(info2->loop_count)
			continue;
		if(info2->skip_flag)
			return;
		if(damper){
			if(info2->amp_env_flg)
				reset_envelope0_damper(&info2->amp_env, damper);
			if(info2->pit_env_flg)
				reset_envelope0_damper(&info2->pit_env, damper);
			if(info2->wid_env_flg)
				reset_envelope0_damper(&info2->wid_env, damper);
			if(info2->mod_env_flg)
				reset_envelope0_damper(&info2->mod_env, damper);
		}
	}
}

static void pre_compute_mms(InfoIS_MMS *info, int v, int32 count)
{
	Voice *vp = voice + v;
	int i, scount, thru_flg, op_count = 0;

	// thru count
	if(thru_count_mlt > 1){
		scount = count * thru_count_mlt;
		if((++info->thru_count) >= thru_count_mlt)
			info->thru_count = 0;
		thru_flg = info->thru_count;
	}else{
		scount = count;
		thru_flg = 0;
	}	
	for(i = 0; i < info->op_max; i++){
		Info_OP *info2 = &info->op[i];
		FLOAT_T amp = 1.0, pitch = 0.0, cutoff = 0.0, width = 0.0;
		
		if(info2->op_flag == 0) // op off
			continue;
		// release
		if(info2->skip_flag == -1){
			init_mms_preset_op(info, v, i);
			info2->skip_flag = 0;
		}else if(info2->skip_flag > 0){
			++op_count;
			continue;
		}
		// loop
		if(info2->loop_count == -1){
			init_mms_preset_op(info, v, i);
		}else if(info2->loop_count > 0){
			++op_count;
			info2->loop_count -= count;
			if(info2->loop_count <= 0)
				info2->loop_count = -1; // reset flag
		}
		// amp
		if(info2->amp_env_flg){
			if(!check_envelope0(&info2->amp_env)){ // amp_env end
				info2->amp_vol = 0.0;
				if(!info2->loop_count)
					info2->op_flag = 0; // operator turn off
				info->op_ptr[i] = compute_op_null;
				continue;
			}
			compute_envelope0(&info2->amp_env, count);
			amp *= info2->amp_env.volume;
		}
		++op_count;
		if(info2->lfo1_flg){
			if(!thru_flg)
				compute_lfo(&info2->lfo1, scount);
			amp *= 1.0 - info2->lfo1.out * info2->lfo_amp;	
		}
		info2->amp_vol = info2->op_level * amp;
		
		if(thru_flg)
			continue;

		// pitch
		if(info2->pit_env_flg){
			compute_envelope0(&info2->pit_env, scount);
			pitch += info2->env_pitch * info2->pit_env.volume;
		}
		if(info2->lfo2_flg){	
			compute_lfo(&info2->lfo2, scount);
			pitch += info2->lfo_pitch * info2->lfo2.out;
		}
		if(!info2->mode)
			info2->freq = (FLOAT_T)vp->frequency * DIV_1000 * info2->freq_mlt;
		else {
			if(vp->pitchfactor)
				info2->freq = info2->freq_mlt * vp->pitchfactor;
			else
				info2->freq = info2->freq_mlt;			
			info2->freq += channel[vp->channel].pitch_offset_fine;
		}
		info2->freq *= POW2(pitch) * info2->freq_coef;
		
		// width
		if(info2->wid_env_flg){
			compute_envelope0(&info2->wid_env, scount);
			width += info2->env_width * info2->wid_env.volume;
		}	
		if(info2->lfo3_flg){	
			compute_lfo(&info2->lfo3, scount);	
			width += info2->lfo_width * info2->lfo3.out;
		}
		if(width != 0.0){
			width += info2->wave_width;
			if(info2->wave_width1 != width){
				info2->update_width = 1;			
				if(width >= 1.0) {
					info2->req_wave_width1 = 1.0;
					info2->req_rate_width1 = 0.5;
					info2->req_rate_width2 = 0.0;
				}else if(width <= 0.0) {
					info2->req_wave_width1 = 0.0;
					info2->req_rate_width1 = 0.0;
					info2->req_rate_width2 = 0.5;
				}else{
					info2->req_wave_width1 = width;
					info2->req_rate_width1 = 0.5 / (width);
					info2->req_rate_width2 = 0.5 / (1.0 - width);
				}
			}
		}
		
		// filter // cutoff
		if(info2->mod_env_flg){
			compute_envelope0(&info2->mod_env, scount);
			cutoff += info2->env_cutoff * info2->mod_env.volume;
		}
		if(info2->lfo4_flg){		
			compute_lfo(&info2->lfo4, scount);
			cutoff += info2->lfo_cutoff * (1.0 - info2->lfo4.out);
		}
		if(info2->fc.type){
			set_sample_filter_freq(&info2->fc, info2->flt_freq * POW2(cutoff));
			recalc_filter(&info2->fc);
		}
	}
	if(!op_count) vp->finish_voice = 1;
}

static inline void compute_voice_mms_switch(int v, int32 count, DATA_T *is_buf, IS_RS_DATA_T *rs_buf)
{
	Voice *vp = voice + v;
	InfoIS_MMS *info = vp->mms;
	int32 i;
	
	if(!info->init){
		memset(is_buf, 0, count * sizeof(DATA_T));		
	}else if(!is_rs_mode){
		pre_compute_mms(info, v, count);
		for (i = 0; i < count; i++)
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
			is_buf[i] = (DATA_T)(info->op_num_ptr(info));
#else
			is_buf[i] = (DATA_T)(info->op_num_ptr(info) * is_output_level);
#endif
	}else{
		IS_RS_DATA_T *rs_buf2 = rs_buf + is_rs_buff_offset;
		Info_Resample *rs = &info->rs;

		is_resample_pre(rs, rs_buf, count);
		pre_compute_mms(info, v, rs->rs_count);
		for (i = 0; i < rs->rs_count; i++)
			rs_buf2[i] = (IS_RS_DATA_T)info->op_num_ptr(info);
		is_resample_core(rs, is_buf, rs_buf, count);
	}  
}



/**************** interface function SCC MMS ****************/

void init_int_synth_scc(int v)
{
	Voice *vp = voice + v;	
		
	if(!vp->scc)
		return;
	init_scc_preset(v, vp->scc, (Preset_IS *)vp->sample->data, vp->sample->sf_sample_index);
}

void noteoff_int_synth_scc(int v)
{	
	Voice *vp = voice + v;	
	
	if(!vp->scc)
		return;
	noteoff_scc(vp->scc);
}

void damper_int_synth_scc(int v, int8 damper)
{
	Voice *vp = voice + v;	
	
	if(!vp->scc)
		return;
	damper_scc(vp->scc, damper);
}

void compute_voice_scc(int v, DATA_T *ptr, int32 count)
{
	Voice *vp = voice + v;	
	
	if(!vp->scc)
		return;
	compute_voice_scc_switch(v, count, ptr, is_resample_buffer);
}

void init_int_synth_mms(int v)
{
	Voice *vp = voice + v;	
	
	if(!vp->mms)
		return;
	init_mms_preset(v, vp->mms, (Preset_IS *)vp->sample->data, vp->sample->sf_sample_index);
}

void noteoff_int_synth_mms(int v)
{
	Voice *vp = voice + v;	
	
	if(!vp->mms)
		return;
	noteoff_mms(vp->mms);
}

void damper_int_synth_mms(int v, int8 damper)
{
	Voice *vp = voice + v;
	
	if(!vp->mms)
		return;
	damper_mms(vp->mms, damper);
}

void compute_voice_mms(int v, DATA_T *ptr, int32 count)
{
	Voice *vp = voice + v;	
	
	if(!vp->mms)
		return;
	compute_voice_mms_switch(v, count, ptr, is_resample_buffer);
}


///// init / free

void free_int_synth_preset(void)
{
	int i;
	Preset_IS *set = is_preset;

	while(set){
		Preset_IS *next = set->next;
		safe_free(set->ini_file);
		set->ini_file = NULL;
		set->scc_data_load = 0;
		for (i = 0; i < SCC_DATA_MAX; i++) {
			safe_free(set->scc_data_name[i]);
			set->scc_data_name[i] = NULL;
		}	
		for (i = 0; i < SCC_SETTING_MAX; i++) {
			if(!set->scc_setting[i])
				continue;
			safe_free(set->scc_setting[i]->inst_name);
			set->scc_setting[i]->inst_name = NULL;
			safe_free(set->scc_setting[i]);
			set->scc_setting[i] = NULL;
		}
		for (i = 0; i < MMS_SETTING_MAX; i++) {
			if(!set->mms_setting[i])
				continue;
			safe_free(set->mms_setting[i]->inst_name);
			set->mms_setting[i]->inst_name = NULL;
			safe_free(set->mms_setting[i]);
			set->mms_setting[i] = NULL;
		}
		safe_free(set);
		set = next;
	}
	is_preset = NULL;
}

void free_int_synth(void)
{
	free_int_synth_preset();
	free_is_editor_preset();
	la_pcm_data_load_flg = 0;	
}

void init_int_synth(void)
{
	set_sample_rate();
	init_int_synth_lite();
}




#ifdef MULTI_THREAD_COMPUTE
#include "thread_int_synth.c"
#endif


#undef POW2 

#endif // INT_SYNTH