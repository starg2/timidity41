/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    In case you haven't heard, this program is free software;
    you can redistribute it and/or modify it under the terms of the
    GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef ___VOICE_EFFECT_H_
#define ___VOICE_EFFECT_H_

#include "envelope.h"
#include "filter.h"
#include "oscillator.h"

#define VOICE_EFFECT_NUM (8)
#define VOICE_EFFECT_PARAM_NUM (16)



enum {
	VFX_NONE = 0, // 0 // cfg sort
	VFX_MATH,
	VFX_DISTORTION,
	VFX_EQUALIZER,
	VFX_FILTER,
	VFX_FORMANT,
	VFX_DELAY,
	VFX_COMPRESSOR,
	VFX_PITCH_SHIFTER,
	VFX_FEEDBACKER,
	VFX_REVERB,
	VFX_ENVELOPE_REVERB,
	VFX_ENVELOPE,
	VFX_ENVELOPE_FILTER,
	VFX_LOFI,
	VFX_DOWN_SAMPLE,
	VFX_CHORUS,
	VFX_ALLPASS,
	VFX_COMB,
	VFX_REVERB2,
	VFX_RING_MODULATOR,
	VFX_WAH,
	VFX_TREMOLO,
	VFX_SYMPHONIC,
	VFX_PHASER,
	VFX_ENHANCER,
	VFX_TEST,
	VFX_LIST_MAX, // last
};

typedef struct {
	int param[VOICE_EFFECT_PARAM_NUM]; // param[0] = effect type
	void *info;
	struct _VFX_Engine *engine;
} VoiceEffect; // Voive Effect

struct _VFX_Engine {
	int type;
	void (*init_vfx)(int, VoiceEffect *vfx);
	void (*uninit_vfx)(int, VoiceEffect *vfx);
	void (*noteoff_vfx)(int, VoiceEffect *vfx);
	void (*damper_vfx)(int, VoiceEffect *vfx, int8 damper);
	void (*do_vfx)(int, VoiceEffect *vfx, DATA_T *, int32);
	int info_size;
};



/*   effect info     */
#define VFX_DELAY_BUFFER_SIZE 192000 // 500ms sr max 384kHz 

typedef struct {
	int32 delay, count;
	FLOAT_T ptr[VFX_DELAY_BUFFER_SIZE + 1]; // data stream pointer
} Info_Delay;

#define VFX_PS_CF_PHASE 3
#define VFX_PS_CF_DELAY (30) // *2 ms < 85ms
#define VFX_PS_BUFFER_SIZE (32768) // = 1<<15 (2^n)  // 85ms sr max 384kHz 

typedef struct {
	int32 wcount, wcycle;
	FLOAT_T rcount[VFX_PS_CF_PHASE], rscount[VFX_PS_CF_PHASE], rsdelay, rate, div_cf;
	FLOAT_T ptr[VFX_PS_BUFFER_SIZE + 1];
} Info_PitchShifter;

#define VFX_ALLPASS_BUFFER_SIZE 192000 // 500ms sr max 384kHz 

typedef struct {
	int32 delay, count;
	FLOAT_T ptr[VFX_ALLPASS_BUFFER_SIZE + 1];
	FLOAT_T feedback;
} Info_Allpass;

#define VFX_COMB_BUFFER_SIZE 192000 // 500ms sr max 384kHz 

typedef struct {
	// comb
	int32 delay, count;
	FLOAT_T ptr[VFX_COMB_BUFFER_SIZE + 1];
	FLOAT_T feedback;
	// filter
	FilterCoefficients fc;
} Info_Comb2;


/*   voice effect info     */

typedef struct {
	int dummy;
} InfoVFX_None;

typedef struct {
	int type;
	FLOAT_T var;
} InfoVFX_Math;

typedef struct {
	int type;
	FLOAT_T gain, level, velgain, div_gain;
} InfoVFX_Distortion;

typedef struct {
	FilterCoefficients eq;
} InfoVFX_Equalizer;

typedef struct {
	FilterCoefficients fc;
	FLOAT_T wet, dry;
} InfoVFX_Filter;

typedef struct {
	int type;
	FLOAT_T db[17]; // feedback param (avx *2+1
} InfoVFX_Formant;

typedef struct {
	FLOAT_T wet, dry, feedback;
	Info_Delay dly;
} InfoVFX_Delay;

#define VFX_COMP_BUFFER_SIZE1 400 // 1ms sr max 384kHz 
#define VFX_COMP_BUFFER_SIZE2 2000 // 5ms sr max 384kHz 
typedef struct {
	int32 lhsmp, nrms; // count 
	FLOAT_T threshold, ratio, attack, release, div_nrms; // float coef
	FLOAT_T env;
	int32 delay1, count1, delay2, count2;
	FLOAT_T ptr1[VFX_COMP_BUFFER_SIZE1 + 1];
	FLOAT_T ptr2[VFX_COMP_BUFFER_SIZE2 + 1];
} InfoVFX_Compressor;

typedef struct {
	Info_PitchShifter ps;
	Info_Delay dly;
	FLOAT_T wet, dry, sfeedback;
	int init_flg;
} InfoVFX_PitchShifter;

typedef struct {
	FilterCoefficients fc;
	Envelope3 env;
	FLOAT_T feedback, fb_data;
	FLOAT_T wet, dry;
} InfoVFX_Feedbacker;

typedef struct {
	Info_Delay dly1, dly2, dly3, dly4;
	FLOAT_T feedback, fb_data;
	FLOAT_T wet, dry;
	FilterCoefficients fc;
} InfoVFX_Reverb;

typedef struct {
	Info_Delay dly1, dly2, dly3, dly4;
	FLOAT_T feedback, fb_data;
	FLOAT_T wet, dry;
	FilterCoefficients fc;
	Envelope1 env;
} InfoVFX_Envelope_Reverb;

typedef struct {
	Envelope1 env;
	FLOAT_T wet, dry;
} InfoVFX_Envelope;

typedef struct {
	FilterCoefficients fc;
	FLOAT_T freq, reso;
	Envelope1 env;
	FLOAT_T cent, vel_freq, key_freq, vel_reso;
} InfoVFX_Envelope_Filter;

typedef struct {
	FLOAT_T mult, div;
} InfoVFX_Lofi;

typedef struct {
	int8 model;
	int32 ratio, index;
	FLOAT_T div_ratio;
	FLOAT_T buf[16];
} InfoVFX_DownSample;

#define VFX_CHORUS_PHASE_MAX 8
#define VFX_CHORUS_BUFFER_SIZE 9600 // 20ms sr max 384kHz 
typedef struct {
	int32 phase, delay_size, delay_count;
	FLOAT_T wet, dry, feedback,  delayc, depthc, div_out, hist;
	FLOAT_T lfo_count, lfo_rate, lfo_phase[VFX_CHORUS_PHASE_MAX]; 
	FLOAT_T buf[VFX_CHORUS_BUFFER_SIZE + 2]; // + 1 for linear interpolation
} InfoVFX_Chorus;

#define VFX_ALLPASS_NUM_MAX 4
typedef struct {
	FLOAT_T level, wet, dry;
	Info_Allpass ap[VFX_ALLPASS_NUM_MAX];
	FLOAT_T feedback;
	int init_flg, ap_num;
} InfoVFX_Allpass;

#define VFX_COMB_NUM_MAX 8
typedef struct {
	FLOAT_T level, wet, dry;
	Info_Comb2 comb[VFX_COMB_NUM_MAX];
	int init_flg, comb_num;
} InfoVFX_Comb;

typedef struct {
	FLOAT_T level, wet, dry;
	Info_Allpass ap[VFX_ALLPASS_NUM_MAX];
	Info_Comb2 comb[VFX_COMB_NUM_MAX];
	int init_flg, flt_num1, flt_num2;
} InfoVFX_Reverb2;

typedef struct {
	FLOAT_T level, wet, dry;
	int mode;//, wave_type;
	FLOAT_T wave_width1, rate_width1, rate_width2, pow;
	FLOAT_T freq_mlt, freq, mod_level, cent, rate;
	Envelope1 env;
//	compute_osc_t osc_ptr;
	osc_type_t osc_ptr;
} InfoVFX_RingModulator;

#define VFX_WAH_PHASE_MAX 16
typedef struct {
	FLOAT_T level, wet, dry;
	int phase;
	FLOAT_T freq, multi, lfo_count, lfo_rate, lfo_phase[VFX_WAH_PHASE_MAX]; 
	// filter
	FilterCoefficients fc[VFX_WAH_PHASE_MAX];
} InfoVFX_Wah;

typedef struct {
	FLOAT_T mod_level, lfo_count, lfo_rate; 
//	compute_osc_t osc_ptr;
	osc_type_t osc_ptr;
} InfoVFX_Tremolo;

#define VFX_SYMPHONIC_PHASE_MAX 16
#define VFX_SYMPHONIC_BUFFER_SIZE 9600 // 20ms sr max 384kHz 
typedef struct {
	int32 phase, delay_size, delay_count;
	FLOAT_T wet, dry, feedback, div_out, hist;
	FLOAT_T delayc[VFX_SYMPHONIC_PHASE_MAX], depthc[VFX_SYMPHONIC_PHASE_MAX];
	FLOAT_T lfo_count[VFX_SYMPHONIC_PHASE_MAX], lfo_rate[VFX_SYMPHONIC_PHASE_MAX]; 
	FLOAT_T buf[VFX_SYMPHONIC_BUFFER_SIZE + 2]; // + 1 for linear interpolation
} InfoVFX_Symphonic;

#define VFX_PHASER_PHASE_MAX 8
#define VFX_PHASER_BUFFER_SIZE 9600 // 20ms sr max 384kHz 
typedef struct {
	int32 phase, delay_size, delay_count;
	FLOAT_T wet, dry, feedback,  delayc, depthc, div_out
		, hist;
	FLOAT_T lfo_count, lfo_rate, lfo_phase[VFX_PHASER_PHASE_MAX]; 
	FLOAT_T buf[VFX_PHASER_BUFFER_SIZE + 2]; // + 1 for linear interpolation
	// filter
	FilterCoefficients fc;
} InfoVFX_Phaser;

#define VFX_ENHANCER_BUFFER_SIZE 1920 // 5ms sr max 384kHz 
typedef struct {
	FLOAT_T mix, feedback;
	int32 delay_size, delay_count;
	FLOAT_T buf[VFX_ENHANCER_BUFFER_SIZE];
	// filter
	FilterCoefficients fc, eq;
} InfoVFX_Enhancer;


// test
#define VFX_TEST_BUFFER_SIZE 192000 // 500ms sr max 384kHz 

typedef struct {
	FLOAT_T mix, feedback, thres, norm;
	FLOAT_T buf[VFX_TEST_BUFFER_SIZE];
		
	int32 size, delay_count, delay1, delay2;
	FLOAT_T rate, freq;

	// filter
//	FLOAT_T rate, freq;
	FilterCoefficients fc;
} InfoVFX_Test;


#ifdef VOICE_EFFECT

extern void free_voice_effect(int v);
extern void init_voice_effect(int v);
extern void uninit_voice_effect(int v);
extern void noteoff_voice_effect(int v);
extern void damper_voice_effect(int v, int8 damper);
extern void voice_effect(int v, DATA_T *sp, int32 count);

#endif


#endif /* ___VOICE_EFFECT_H_ */