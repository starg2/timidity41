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
#include "tables.h"
#include "output.h"

#include "oscillator.h"




static FLOAT_T compute_osc_sine(FLOAT_T in)
{
#ifdef LOOKUP_SINE
	return lookup2_sine_linear(in);	
#else
	return sin((FLOAT_T)M_PI2 * in);	
#endif
}
	
static FLOAT_T compute_osc_triangular(FLOAT_T in)
{
	if(in < 0.5)
		return -1.0 + in * 4.0; // -1.0 + (in - 0.0) * 4.0;
	else  //  if(in < 1.0)
		return 1.0 - (in - 0.5) * 4.0;
}

static FLOAT_T compute_osc_saw1(FLOAT_T in)
{
	if(in < 0.5)
		return in * 2.0; // (in - 0.0) * 2.0;
	else  //  if(in < 1.0)
		return (in - 1.0) * 2.0;
}

static FLOAT_T compute_osc_saw2(FLOAT_T in)
{
	if(in < 0.5)
		return in * -2.0; // (in - 0.0) * -2.0;
	else  //  if(in < 1.0)
		return (in - 1.0) * -2.0;
}

static FLOAT_T compute_osc_square(FLOAT_T in)
{
	if(in < 0.5)
		return 1.0;
	else  //  if(in < 1.0)
		return -1.0;
}

static FLOAT_T compute_osc_noise(FLOAT_T in)
{	
	const uint32 mask = 0x7FFF;
	return (FLOAT_T)((int32)(rand() & mask) - M_14BIT) * DIV_14BIT;
}

static osc_type_t compute_osc[] = {
// cfg sort
	compute_osc_sine,
	compute_osc_triangular,
	compute_osc_saw1,
	compute_osc_saw2,
	compute_osc_square,
	compute_osc_noise,
};

int check_osc_wave_type(int tmpi, int limit)
{
	if(tmpi < 0 || tmpi > limit)
		tmpi = 0; // sine
	return tmpi;	
}

osc_type_t get_osc_ptr(int tmpi)
{
	if(tmpi < 0 || tmpi > 5)
		tmpi = 0; // sine
	return compute_osc[tmpi];
}


///////////////// Oscillator /////////////////
/*
オシレータの初期化 (必須
freq[Hz] : オシレータ周波数
delay_count[samples] : 発振開始までの時間 (その間出力 0.0
attack_count[samples] : 発振開始から振幅最大(1.0)になるまでの時間
wave_type : 波形タイプ see oscillator.h // OSC_TYPE
init_phase : 初期位相 0.0 ~ 1.0
*/
void init_oscillator(Oscillator *osc, FLOAT_T freq, int32 delay_count, int32 attack_count, int wave_type, FLOAT_T init_phase)
{	
	osc->mode = freq > 0.0 ? 1: 0; // on/off	
	if(wave_type < 0 || wave_type >= OSC_TYPE_LIST_MAX)
		wave_type = 0; // sine
	osc->wave_type = wave_type;
	osc->osc_ptr = compute_osc[wave_type];
	osc->delay = delay_count;
	osc->freq = div_playmode_rate * freq; // 1/sr = 1Hz
	osc->rate = init_phase;
	osc->out = 0;
	init_envelope3(&osc->env, 0.0, attack_count);
	reset_envelope3(&osc->env, 1.0, ENVELOPE_KEEP);
}

/*
オシレータ周波数を変更するときに使用
freq[Hz] : オシレータ周波数
*/
void reset_oscillator(Oscillator *osc, FLOAT_T freq)
{
	if(freq > 0.0)
		osc->freq = div_playmode_rate * freq; // 1/sr = 1Hz
	else{
		osc->mode = 2; // 停止フラグ
		reset_envelope3(&osc->env, 0.0, playmode_rate_ms * 100.0); // 100ms
	}
}

void compute_oscillator(Oscillator *osc, int32 count)
{
	if(!osc->mode){
		osc->out = 0.0;
		return;
	}
	if(osc->delay > 0){
		if(osc->delay >= count){		
			osc->delay -= count;
			return;
		}else{
			count -= osc->delay;
			osc->delay = 0;
		}
	}
	if((osc->rate += osc->freq * count) >= 1.0){
		if(osc->mode == 2){
			osc->mode = 0;
			osc->out = 0.0;
			return;
		}
		osc->rate -= floor(osc->rate); // reset count	
	}
	compute_envelope3(&osc->env, count);
//	osc->out = lookup2_sine(osc->rate) * osc->env.vol;
//	osc->out = lookup2_sine_linear(osc->rate) * osc->env.vol;
	osc->out = osc->osc_ptr(osc->rate) * osc->env.vol;
	if(osc->mode == 2 && !check_envelope3(&osc->env))
		osc->mode = 0;
}





///////////////// Oscillator2 /////////////////
/*
2phase用 (freq,wave_type共通で位相だけ違う場合
phase_diff : phase1とphase2の位相差 (phase2rate = phase1rate + phase_diff
*/
void init_oscillator2(Oscillator2 *osc, FLOAT_T freq, int wave_type, FLOAT_T init_phase, FLOAT_T phase_diff)
{	
	osc->mode = freq > 0.0 ? 1: 0; // on/off	
	if(wave_type < 0 || wave_type >= OSC_TYPE_LIST_MAX)
		wave_type = 0; // sine
	osc->wave_type = wave_type;
	osc->osc_ptr = compute_osc[wave_type];
	osc->freq = div_playmode_rate * freq; // 1/sr = 1Hz
	osc->rate = init_phase;
	osc->phase_diff = phase_diff;
	osc->out[0] = 0;
	osc->out[1] = 0;
}

void reset_oscillator2(Oscillator2 *osc, FLOAT_T freq)
{
	if(freq > 0.0)
		osc->freq = div_playmode_rate * freq; // 1/sr = 1Hz
	else
		osc->mode = 2; // 停止
}

void compute_oscillator2(Oscillator2 *osc, int32 count)
{
	FLOAT_T rate2;
	if(!osc->mode){
		return;
	}
	osc->rate += osc->freq * count;
	if(osc->rate >= 1.0){
		if(osc->mode == 2){
			osc->mode = 0;
			osc->rate = 0;
		}else
			osc->rate -= floor(osc->rate); // reset count	
	}
	rate2 = osc->rate + osc->phase_diff;
	if(rate2 >= 1.0){
		rate2 -= floor(rate2); // reset count	
	}
	osc->out[0] = osc->osc_ptr(osc->rate);
	osc->out[1] = osc->osc_ptr(rate2);
}


///////////////// OscillatorMulti /////////////////
/*
multi_phase用 (freq共通で位相だけ違う場合
sinのみ 
初期位相0のみ  (phase_diffで代替可能
sin SIMDとかあれば・・?
未使用
*/
#if 0
void init_oscillator_multi(Oscillator *osc, int phase_num, FLOAT_T freq, FLOAT_T *phase_diff)
{	
	int i;

	if(phase_num > OSC_MULTI_PHASE_MAX)
		phase_num = OSC_MULTI_PHASE_MAX;
	osc->phase_num = phase_num;
	osc->mode = freq > 0.0 ? 1: 0; // on/off	
	osc->freq = div_playmode_rate * freq; // 1/sr = 1Hz
	osc->rate = 0;
	for(i = 0; i < OSC_MULTI_PHASE_MAX; i++){
		osc->phase_diff = phase_diff[i];
		osc->out[i] = 0;
	}
}

void reset_oscillator_multi(Oscillator *osc, FLOAT_T freq)
{
	if(freq > 0.0)
		osc->freq = div_playmode_rate * freq; // 1/sr = 1Hz
	else
		osc->mode = 0; // 停止
}

void compute_oscillator_multi(Oscillator *osc, int32 count)
{
	int i;
	FLOAT_T rate[OSC_MULTI_PHASE_MAX];
	if(!osc->mode){
		for(i = 0; i < OSC_MULTI_PHASE_MAX; i++)
			osc->out[i] = 0;
		return;
	}
	osc->rate += osc->freq * count;
	if(osc->rate >= 1.0){
		osc->rate -= floor(osc->rate); // reset count	
	}
	for(i = 0; i < OSC_MULTI_PHASE_MAX; i++){
		FLOAT_T tmp_rate = osc->rate + osc->phase_diff[i];
		if(tmp_rate >= 1.0)
			tmp_rate -= floor(tmp_rate); // reset count	
		osc->out[i] = lookup2_sine_linear(tmp_rate);
	}
}
#endif