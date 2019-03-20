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
	
    effect.h
*/
/*
 * REVERB EFFECT FOR TIMIDITY++-1.X (Version 0.06e  1999/1/28)
 * Copyright (C) 1997,1998,1999  Masaki Kiryu <mkiryu@usa.net>
 *                           (http://w3mb.kcom.ne.jp/~mkiryu/)
 * reverb.h
 */
#ifndef ___EFFECT_H_
#define ___EFFECT_H_


#ifndef EFFECT_PRIVATE
#define EXTERN extern
#else 
#define EXTERN
#endif


#include "envelope.h"
#include "oscillator.h"
#include "filter.h"



/******************************** define ********************************/


#define DEFAULT_REVERB_SEND_LEVEL 40

#define MAGIC_INIT_EFFECT_INFO -1
#define MAGIC_FREE_EFFECT_INFO -2

enum {
	CH_NONE, 
	CH_MIX_MONO, // mix in mono out
	CH_MIX_STEREO, // mix in stereo out		
	CH_MONO_STEREO, // mono in stereo out (left in stereo out
	CH_MONO, // mono in mono out (left in mono out
	CH_STEREO, // stereo in stereo out
	CH_LEFT, // only left (mono in mono out)
	CH_RIGHT, // only right 
}; // effect mode

enum {
	XG_CONN_INSERTION = 0,
	XG_CONN_SYSTEM = 1,
	XG_CONN_SYSTEM_CHORUS,
	XG_CONN_SYSTEM_REVERB,
};

enum {
	EFFECT_NONE,
	
	EFFECT_GS_POST_EQ,
	EFFECT_GS_STEREO_EQ,
	EFFECT_GS_SPECTRUM,
	EFFECT_GS_ENHANCER,
	EFFECT_GS_HUMANIZER,
	EFFECT_GS_OVERDRIVE1,
	EFFECT_GS_DISTORTION1,
	EFFECT_GS_PHASER,
	EFFECT_GS_AUTO_WAH,
	EFFECT_GS_ROTARY,
	EFFECT_GS_STEREO_FLANGER,
	EFFECT_GS_STEP_FLANGER,
	EFFECT_GS_TREMOLO,
	EFFECT_GS_AUTO_PAN,
	EFFECT_GS_COMPRESSOR,
	EFFECT_GS_LIMITER,
	EFFECT_GS_HEXA_CHORUS,
	EFFECT_GS_TREMOLO_CHORUS,
	EFFECT_GS_STEREO_CHORUS,
	EFFECT_GS_SPACE_D,
	EFFECT_GS_3D_CHORUS,
	EFFECT_GS_STEREO_DELAY,
	EFFECT_GS_MOD_DELAY,
	EFFECT_GS_3TAP_DELAY,
	EFFECT_GS_4TAP_DELAY,
	EFFECT_GS_TM_CTRL_DELAY,
	EFFECT_GS_REVERB,
	EFFECT_GS_GATE_REVERB,
	EFFECT_GS_3D_DELAY,
	EFFECT_GS_2PITCH_SHIFTER,
	EFFECT_GS_FB_P_SHIFTER,
	EFFECT_GS_3D_AUTO,
	EFFECT_GS_3D_MANUAL,
	EFFECT_GS_LOFI1,
	EFFECT_GS_LOFI2,
	EFFECT_GS_S_OD_CHORUS,
	EFFECT_GS_S_OD_FLANGER,
	EFFECT_GS_S_OD_DELAY,
	EFFECT_GS_S_DS_CHORUS,
	EFFECT_GS_S_DS_FLANGER,
	EFFECT_GS_S_DS_DELAY,
	EFFECT_GS_S_EH_CHORUS,
	EFFECT_GS_S_EH_FLANGER,
	EFFECT_GS_S_EH_DELAY,
	EFFECT_GS_S_CHO_DELAY,
	EFFECT_GS_S_FL_DELAY,
	EFFECT_GS_S_CHO_FLANGER,
	EFFECT_GS_S_ROTARY_MULTI,
	EFFECT_GS_S_GTR_MULTI1,
	EFFECT_GS_S_GTR_MULTI2,
	EFFECT_GS_S_GTR_MULTI3,
	EFFECT_GS_S_CLEAN_GT_MULTI1,
	EFFECT_GS_S_CLEAN_GT_MULTI2,
	EFFECT_GS_S_BASE_MULTI,
	EFFECT_GS_S_RHODES_MULTI,
	EFFECT_GS_S_KEYBOARD_MULTI,
	EFFECT_GS_P_CHO_DELAY,
	EFFECT_GS_P_FL_DELAY,
	EFFECT_GS_P_CHO_FLANGER,
	EFFECT_GS_P_OD1_OD2,
	EFFECT_GS_P_OD_ROTARY,
	EFFECT_GS_P_OD_PHASER,
	EFFECT_GS_P_OD_AUTOWAH,
	EFFECT_GS_P_PH_ROTARY,
	EFFECT_GS_P_PH_AUTOWAH,
	
	EFFECT_XG_HALL1,
	EFFECT_XG_HALL2,
	EFFECT_XG_HALL_M,
	EFFECT_XG_HALL_L,
	EFFECT_XG_ROOM1,
	EFFECT_XG_ROOM2,
	EFFECT_XG_ROOM3,
	EFFECT_XG_ROOM_S,
	EFFECT_XG_ROOM_M,
	EFFECT_XG_ROOM_L,
	EFFECT_XG_STAGE1,
	EFFECT_XG_STAGE2,
	EFFECT_XG_PLATE,
	EFFECT_XG_GM_PLATE,
	EFFECT_XG_DELAY_LCR,
	EFFECT_XG_DELAY_LR,
	EFFECT_XG_ECHO,
	EFFECT_XG_CROSS_DELAY,
	EFFECT_XG_EARLY_REF1,
	EFFECT_XG_EARLY_REF2,
	EFFECT_XG_GATE_REVERB,
	EFFECT_XG_REVERSE_GATE,
	EFFECT_XG_WHITE_ROOM,
	EFFECT_XG_TUNNEL,
	EFFECT_XG_CANYON,
	EFFECT_XG_BASEMENT,
	EFFECT_XG_KARAOKE1,
	EFFECT_XG_KARAOKE2,
	EFFECT_XG_KARAOKE3,
	EFFECT_XG_TEMPO_DELAY,
	EFFECT_XG_TEMPO_ECHO,
	EFFECT_XG_TEMPO_CROSS,
	EFFECT_XG_CHORUS1,
	EFFECT_XG_CHORUS2,
	EFFECT_XG_CHORUS3,
	EFFECT_XG_GM_CHORUS1,
	EFFECT_XG_GM_CHORUS2,
	EFFECT_XG_GM_CHORUS3,
	EFFECT_XG_GM_CHORUS4,
	EFFECT_XG_FB_CHORUS,
	EFFECT_XG_CHORUS4,
	EFFECT_XG_CELESTE1,
	EFFECT_XG_CELESTE2,
	EFFECT_XG_CELESTE3,
	EFFECT_XG_CELESTE4,
	EFFECT_XG_FLANGER1,
	EFFECT_XG_FLANGER2,
	EFFECT_XG_GM_FLANGER,
	EFFECT_XG_FLANGER3,
	EFFECT_XG_SYMPHONIC,
	EFFECT_XG_ROTARY_SPEAKER,
	EFFECT_XG_DS_ROTARY_SPEAKER,
	EFFECT_XG_OD_ROTARY_SPEAKER,
	EFFECT_XG_AMP_ROTARY_SPEAKER,
	EFFECT_XG_TREMOLO,
	EFFECT_XG_AUTO_PAN,
	EFFECT_XG_PHASER1,
	EFFECT_XG_PHASER2,
	EFFECT_XG_DISTORTION,
	EFFECT_XG_COMP_DISTORTION,
	EFFECT_XG_STEREO_DISTORTION,
	EFFECT_XG_OVERDRIVE,
	EFFECT_XG_STEREO_OVERDRIVE,
	EFFECT_XG_AMP_SIMULATOR,
	EFFECT_XG_AMP_SIMULATOR2,
	EFFECT_XG_STEREO_AMP_SIMULATOR,
	EFFECT_XG_EQ3,
	EFFECT_XG_EQ2,
	EFFECT_XG_AUTO_WAH,
	EFFECT_XG_AUTO_WAH_DS,
	EFFECT_XG_AUTO_WAH_OD,
	EFFECT_XG_PITCH_CHANGE1,
	EFFECT_XG_PITCH_CHANGE2,
	EFFECT_XG_HARMONIC_ENHANCER,
	EFFECT_XG_TOUCH_WAH1,
	EFFECT_XG_TOUCH_WAH_DS,
	EFFECT_XG_TOUCH_WAH_OD,
	EFFECT_XG_TOUCH_WAH2,
	EFFECT_XG_COMPRESSOR,
	EFFECT_XG_NOISE_GATE,
	EFFECT_XG_2WAY_ROTARY_SPEAKER,
	EFFECT_XG_DS_2WAY_ROTARY_SPEAKER,
	EFFECT_XG_OD_2WAY_ROTARY_SPEAKER,
	EFFECT_XG_AMP_2WAY_ROTARY_SPEAKER,
	EFFECT_XG_ENSEMBLE_DETUNE,
	EFFECT_XG_AMBIENCE,
	EFFECT_XG_TALKING_MODULATOR,
	EFFECT_XG_LOFI,
	EFFECT_XG_DS_DELAY,
	EFFECT_XG_OD_DELAY,
	EFFECT_XG_COMP_DS_DELAY,
	EFFECT_XG_COMP_OD_DELAY,
	EFFECT_XG_WAH_DS_DELAY,
	EFFECT_XG_WAH_OD_DELAY,
	EFFECT_XG_V_DIST_HARD,
	EFFECT_XG_V_DIST_HARD_DELAY,
	EFFECT_XG_V_DIST_SOFT,
	EFFECT_XG_V_DIST_SOFT_DELAY,
	EFFECT_XG_DUAL_ROTAR_SPEAKER1,
	EFFECT_XG_DUAL_ROTAR_SPEAKER2,
	EFFECT_XG_DS_TEMPO_DELAY,
	EFFECT_XG_OD_TEMPO_DELAY,
	EFFECT_XG_COMP_DS_TEMPO_DELAY,
	EFFECT_XG_COMP_OD_TEMPO_DELAY,
	EFFECT_XG_WAH_DS_TEMPO_DELAY,
	EFFECT_XG_WAH_OD_TEMPO_DELAY,
	EFFECT_XG_V_DIST_HARD_TEMPO_DELAY,
	EFFECT_XG_V_DIST_SOFT_TEMPO_DELAY,
	EFFECT_XG_V_FLANGER,
	EFFECT_XG_TEMPO_FLANGER,
	EFFECT_XG_TEMPO_PHASER,
	EFFECT_XG_RING_MODULATOR,
	EFFECT_XG_3D_MANUAL,
	EFFECT_XG_3D_AUTO,
	
	// SD Chorus
	EFFECT_SD_CHO_CHORUS,
	EFFECT_SD_CHO_DELAY,
	EFFECT_SD_CHO_CHORUS1, // GM2_CHORUS
	EFFECT_SD_CHO_CHORUS2, // GM2_CHORUS
	EFFECT_SD_CHO_CHORUS3, // GM2_CHORUS
	EFFECT_SD_CHO_CHORUS4, // GM2_CHORUS
	EFFECT_SD_CHO_FB_CHORUS, // GM2_CHORUS
	EFFECT_SD_CHO_FLANGER, // GM2_CHORUS
	// SD Reverb
	EFFECT_SD_REV_ROOM1, // REVERB
	EFFECT_SD_REV_ROOM2, // REVERB
	EFFECT_SD_REV_STAGE1, // REVERB
	EFFECT_SD_REV_STAGE2, // REVERB
	EFFECT_SD_REV_HALL1, // REVERB
	EFFECT_SD_REV_HALL2, // REVERB
	EFFECT_SD_REV_DELAY, // REVERB
	EFFECT_SD_REV_PANDELAY, // REVERB
	EFFECT_SD_REV_SRV_ROOM,
	EFFECT_SD_REV_SRV_HALL,
	EFFECT_SD_REV_SRV_PLATE,
	EFFECT_SD_REV_SMALL_ROOM, // GM2_REVERB
	EFFECT_SD_REV_MEDIUM_ROOM, // GM2_REVERB
	EFFECT_SD_REV_LARGE_ROOM, // GM2_REVERB
	EFFECT_SD_REV_MEDIUM_HALL, // GM2_REVERB
	EFFECT_SD_REV_LARGE_HALL, // GM2_REVERB
	EFFECT_SD_REV_PLATE, // GM2_REVERB
	// SD MFX
	EFFECT_SD_STEREO_EQ,
	EFFECT_SD_OVERDRIVE,
	EFFECT_SD_DISTORTION,
	EFFECT_SD_PHASER,
	EFFECT_SD_SPECTRUM,
	EFFECT_SD_ENHANCER,
	EFFECT_SD_AUTO_WAH,
	EFFECT_SD_ROTARY,
	EFFECT_SD_COMPRESSOR,
	EFFECT_SD_LIMITER,
	EFFECT_SD_HEXA_CHORUS,
	EFFECT_SD_TREMOLO_CHORUS,
	EFFECT_SD_SPACE_D,
	EFFECT_SD_STEREO_CHORUS,
	EFFECT_SD_STEREO_FLANGER,
	EFFECT_SD_STEP_FLANGER,
	EFFECT_SD_STEREO_DELAY,
	EFFECT_SD_MOD_DELAY,
	EFFECT_SD_3TAP_DELAY,
	EFFECT_SD_4TAP_DELAY,
	EFFECT_SD_TM_CTRL_DELAY,
	EFFECT_SD_2PITCH_SHIFTER,
	EFFECT_SD_FB_P_SHIFTER,
	EFFECT_SD_REVERB,
	EFFECT_SD_GATE_REVERB,
	EFFECT_SD_S_OD_CHORUS,
	EFFECT_SD_S_OD_FLANGER,
	EFFECT_SD_S_OD_DELAY,
	EFFECT_SD_S_DS_CHORUS,
	EFFECT_SD_S_DS_FLANGER,
	EFFECT_SD_S_DS_DELAY,
	EFFECT_SD_S_EH_CHORUS,
	EFFECT_SD_S_EH_FLANGER,
	EFFECT_SD_S_EH_DELAY,
	EFFECT_SD_S_CHO_DELAY,
	EFFECT_SD_S_FL_DELAY,
	EFFECT_SD_S_CHO_FLANGER,
	EFFECT_SD_P_CHO_DELAY,
	EFFECT_SD_P_FL_DELAY,
	EFFECT_SD_P_CHO_FLANGER,
	EFFECT_SD_STEREO_PHASER,
	EFFECT_SD_KEYSYNC_FLANGER,
	EFFECT_SD_FORMANT_FILTER,
	EFFECT_SD_RING_MODULATOR,
	EFFECT_SD_MULTITAP_DELAY,
	EFFECT_SD_REVERSE_DELAY,
	EFFECT_SD_SHUFFLE_DELAY,
	EFFECT_SD_3D_DELAY,
	EFFECT_SD_3PITCH_SHIFTER,
	EFFECT_SD_LOFI_COMPRESS,
	EFFECT_SD_LOFI_NOISE,
	EFFECT_SD_SPEAKER_SIMULATOR,
	EFFECT_SD_OVERDRIVE2,
	EFFECT_SD_DISTORTION2,
	EFFECT_SD_STEREO_COMPRESSOR,
	EFFECT_SD_STEREO_LIMITER,
	EFFECT_SD_GATE,
	EFFECT_SD_SLICER,
	EFFECT_SD_ISOLATOR,
	EFFECT_SD_3D_CHORUS,
	EFFECT_SD_3D_FLANGER,
	EFFECT_SD_TREMOLO,
	EFFECT_SD_AUTO_PAN,
	EFFECT_SD_STEREO_PHASER2,
	EFFECT_SD_STEREO_AUTO_WAH,
	EFFECT_SD_STEREO_FORMANT_FILTER,
	EFFECT_SD_MULTITAP_DELAY2,
	EFFECT_SD_REVERSE_DELAY2,
	EFFECT_SD_SHUFFLE_DELAY2,
	EFFECT_SD_3D_DELAY2,
	EFFECT_SD_ROTARY2,
	EFFECT_SD_S_ROTARY_MULTI,
	EFFECT_SD_S_KEYBOARD_MULTI,
	EFFECT_SD_S_RHODES_MULTI,
	EFFECT_SD_S_JD_MULTI,
	EFFECT_SD_STEREO_LOFI_COMPRESS,
	EFFECT_SD_STEREO_LOFI_NOISE,
	EFFECT_SD_GUITAR_AMP_SIMULATOR,
	EFFECT_SD_STEREO_OVERDRIVE,
	EFFECT_SD_STEREO_DISTORTION,
	EFFECT_SD_S_GUITAR_MULTI_A,
	EFFECT_SD_S_GUITAR_MULTI_B,
	EFFECT_SD_S_GUITAR_MULTI_C,
	EFFECT_SD_S_CLEAN_GUITAR_MULTI_A,
	EFFECT_SD_S_CLEAN_GUITAR_MULTI_B,
	EFFECT_SD_S_BASE_MULTI,
	EFFECT_SD_ISOLATOR2,
	EFFECT_SD_STEREO_SPECTRUM,
	EFFECT_SD_3D_AUTO_SPIN,
	EFFECT_SD_3D_MANUAL,
	// for effect test
	EFFECT_SD_TEST1,
	EFFECT_SD_TEST2,

};



/******************************** EFFECT Utilities ********************************/

/*! simple delay */
typedef struct {
	DATA_T *buf;
	int32 size, index;
} simple_delay;

/*! Pink Noise Generator */
typedef struct {
	float b0, b1, b2, b3, b4, b5, b6;
} pink_noise;


#ifndef SINE_CYCLE_LENGTH
#define SINE_CYCLE_LENGTH 1024
#endif

/*! LFO */
typedef struct {
	int32 buf[SINE_CYCLE_LENGTH];
	int32 count, cycle;	/* in samples */
	int32 icycle;	/* proportional to (SINE_CYCLE_LENGTH / cycle) */
	int type;	/* current content of its buffer */
	double freq;	/* in Hz */
} lfo;

///r
enum {
	LFO_NONE = 0,
	LFO_SINE,
	LFO_TRIANGULAR,
	LFO_SAW1,
	LFO_SAW2,
	LFO_SQUARE,
};
///r
/*! modulated delay with allpass interpolation */
typedef struct {
	DATA_T *buf, hist, hist2; // hist2 for read_mod_delay
	int32 size, rindex, windex, rindex2; // index2 for read_mod_delay
	int32 ndelay, depth;	/* in samples */
} mod_delay;

/*! modulated allpass filter with allpass interpolation */
typedef struct {
	DATA_T *buf, hist;
	int32 size, rindex, windex;
	int32 ndelay, depth;	/* in samples */
	double feedback;
	int32 feedbacki;
} mod_allpass;


/*! allpass filter */
typedef struct _allpass {
	DATA_T *buf;
	int32 size, index;
	double feedback;
	int32 feedbacki;
} allpass;

/*! comb filter */
typedef struct _comb {
	DATA_T *buf;
	int32 filterstore, size, index;
	FLOAT_T filterstoref;
	FLOAT_T feedback, damp1, damp2;
	int32 feedbacki, damp1i, damp2i;
} comb;


/*! allpass filter */
typedef struct _allpass2 {
	int8 alloc;
	DATA_T *buf;
	int32 size, index, delay;
	double feedback;
	FLOAT_T lfo_count, lfo_cycle, lfo_rate, lfo_depth, lfo_phase, offset;
} allpass2;

/*! comb filter 2*/
#define NET_COMB_CON 4 // xnet 2,4,8
#define NET_COMB_LFO 2 // log2(NET_COMB_CON)
typedef struct _comb2 {
	int8 alloc;
	DATA_T *buf;
	int32 size, index, delay;
	FLOAT_T feedback_dry, feedback_wet, feedback_in, feedback_out, return_in;
	FilterCoefficients fc;
	// net_comb
	FLOAT_T net_dry, net_wet;
	DATA_T net_in, net_out, dummy, *net_ptr[NET_COMB_CON];
	// net_comb // xnet
	FLOAT_T lfo_count[NET_COMB_LFO], lfo_rate[NET_COMB_LFO], cf[NET_COMB_CON];
	FLOAT_T lfo_cycle, lfo_depth, lfo_phase, offset;
} comb2;

typedef struct {
	DATA_T *buf;
	FLOAT_T feedback;
	int32 size, index;
} fb_delay;

typedef struct {
	int8 init, mode, env_mode;
	double attack_ms, release_ms, threshold;
	double peak_rate, peak_count, peak_level, gate_coef, attack_cnt, release_cnt;
	Envelope3 gate_env;
} InfoGate;

typedef struct {
	int8 init, out, prev_out;
	FLOAT_T azimuth;
	simple_delay delay;
	FilterCoefficients side, back;
} Info3DLocate;

/*! drive */
/*   
float/double
in: 0.0 ~ 8.0 (max DRIVE_SCALE_MAX
out: 0.0 ~ 8.0 * clip_level
int32
in: 0.0 ~ 8.0 (1.0: 1<<(DRIVE_INPUT_BIT) , DRIVE_SCALE_BIT+DRIVE_BASE_BIT+FRACTION_BITS < 30bit
out: 0.0 ~ 8.0 * clip_level
*/
#define DRIVE_SCALE_BIT (3) // 1.0 * 2^MATH_SCALE_BIT
#define DRIVE_SCALE_MAX (1 << DRIVE_SCALE_BIT) // table max 1.0 * MATH_SCALE_MAX
#define DRIVE_BASE_BIT (6) // 0.0~1.0 table size
#define DRIVE_BASE_LENGTH (1 << (DRIVE_BASE_BIT)) // 0.0~1.0:table size
#define DRIVE_TABLE_LENGTH (1 << (DRIVE_BASE_BIT + DRIVE_SCALE_BIT)) // 0.0~1.0 * MATH_SCALE_MAX table size
#define DRIVE_FRAC_BIT (14) // for int32
#define DRIVE_FRAC_MASK ((1 << DRIVE_FRAC_BIT) - 1) // for int32
#define DRIVE_INPUT_BIT (DRIVE_BASE_BIT + DRIVE_FRAC_BIT) // for int32

typedef struct {
	FLOAT_T cnv;
	int32 cnvi;
	DATA_T dc[DRIVE_TABLE_LENGTH + 1];
} Drive;

/********************************  EFFECT core  ********************************/

typedef struct {
	double level, levell, levelr, panll, panlr, panrl, panrr;
} Info_P_Mix;

typedef struct {
	FLOAT_T freqL, freqH;
	FilterCoefficients fcL;
	FilterCoefficients fcH;
} InfoPreFilter;

typedef struct {
	int8 model;
	int32 ratio, index;
	FLOAT_T div_ratio;
	FLOAT_T *buf;
} InfoDownSample;

/*! 2-Band EQ */
typedef struct {	
	int8 mode;
    int16 low_freq, high_freq;		/* in Hz */
	int16 low_gain, high_gain;		/* in dB */
	FilterCoefficients hsf, lsf;
} InfoEQ2;

/*! 3-Band EQ */
typedef struct {
	int8 mode;
    int16 low_freq, high_freq, mid_freq;		/* in Hz */
	int16 low_gain, high_gain, mid_gain;		/* in dB */
	double mid_width;
	FilterCoefficients hsf, lsf, peak;
} InfoEQ3;

/*! Plate Reverb */
typedef struct {
	int8 mode;
	FLOAT_T rev_time_sec, rev_damp, rev_level, rev_feedback, rev_wet, rev_diff;
	FLOAT_T er_time_ms, er_level;
	FLOAT_T feedback, levelrv, leveler;
	int32 feedbacki, levelrvi, leveleri;
	simple_delay pd, pdR, od1l, od2l, od3l, od4l, od5l, od6l, od7l,
		od1r, od2r, od3r, od4r, od5r, od6r, od7r,
		td1, td2, td1d, td2d;
	lfo lfo1, lfo1d;
	allpass ap1, ap2, ap3, ap4, ap1R, ap2R, ap3R, ap4R, ap6, ap6d;
	mod_allpass ap5, ap5d;
	FilterCoefficients lpf1, lpf2;
	int32 t1, t1d;
	FLOAT_T dt1, dt1d;
	FLOAT_T decay, ddif1, ddif2, idif1, idif2, dry, wet;
	int32 decayi, ddif1i, ddif2i, idif1i, idif2i, dryi, weti;
	DATA_T histL, histR;
} InfoPlateReverb;

/*! Standard Reverb */
typedef struct {
	int8 mode;
	double rev_time_sec, rev_rt, rev_roomsize, rev_width, rev_damp, rev_level, rev_feedback, rev_wet, rev_diff;
	double er_time_ms, er_level;
	int32 spt0, spt1, spt2, spt3, rpt0, rpt1, rpt2, rpt3;
	int32 ta, tb, HPFL, HPFR, LPFL, LPFR, EPFL, EPFR;
	FLOAT_T tad, tbd, HPFLd, HPFRd, LPFLd, LPFRd, EPFLd, EPFRd;
	simple_delay buf0_L, buf0_R, buf1_L, buf1_R, buf2_L, buf2_R, buf3_L, buf3_R;
	FLOAT_T fbklev, nmixlev, cmixlev, monolev, hpflev, lpflev, lpfinp, epflev, epfinp, width, wet;
	int32 fbklevi, nmixlevi, cmixlevi, monolevi, hpflevi, lpflevi, lpfinpi, epflevi, epfinpi, widthi, weti;
} InfoStandardReverb;

/*! early reflection */
#define ER_APF_NUM 2
typedef struct {
	int8 mode;
	double roomsize, dif, feedback, feedbackd;
	double er_time_ms, damp_freq;
	simple_delay pdelayL, pdelayR;
	DATA_T histL, histR;	
	FilterCoefficients lpf1;
	allpass apL[ER_APF_NUM], apR[ER_APF_NUM];
	FLOAT_T ddif[ER_APF_NUM];
} InfoEarlyReflection;

/*! Freeverb */
///r
#define FREEVERV_RV 64 // def 8, ~64
#define FREEVERV_AP 4
enum{
	FREEVERV_L = 0, // rv/ap L
	FREEVERV_R,     // rv/ap R
	FREEVERV_CH, // rv/ap ch type
};
enum{
	FREEVERV_PD = 0, // pre delay
	FREEVERV_RD,     // rv delay 
	FREEVERV_RSV1,
	FREEVERV_RSV2,
	FREEVERV_DELAY1, // other delay type
};
typedef struct _InfoFreeverb{
	int8 mode;
	double rev_dly_ms, rev_time_sec, rev_rt, rev_roomsize, rev_width, rev_damp, rev_level, rev_feedback, rev_wet, rev_dif;
	double er_time_ms, er_level, er_damp_freq, er_roomsize;
	FLOAT_T feedback_rv[FREEVERV_RV], damp1, damp2, feedback_ap, wet1, wet2, in_level, feedback, levelrv, leveler ;
	int32 feedback_rvi[FREEVERV_RV], damp1i, damp2i, feedback_api, wet1i, wet2i, in_leveli, feedbacki, levelrvi, leveleri;
	int8 init, unit_num, alloc1[FREEVERV_DELAY1], alloc_ap[FREEVERV_AP][2], alloc_rv[FREEVERV_RV][2];
	int32 size1[FREEVERV_DELAY1], index1[FREEVERV_DELAY1], size_ap[FREEVERV_AP][2], index_ap[FREEVERV_AP][2], 
		size_rv[FREEVERV_RV][2], index_rv[FREEVERV_RV][2];
	DATA_T hist[2], *buf1[FREEVERV_DELAY1], *buf_ap[FREEVERV_AP][2], *buf_rv[FREEVERV_RV][2], fb_rv[FREEVERV_RV][2];	
	FilterCoefficients lpf1;
	void (*do_reverb_mode)(DATA_T *buf, int32 count, struct _InfoFreeverb *info);
} InfoFreeverb;


// reverb_ex
#define REV_EX_UNITS 64 // ER,RV共通
#define REV_EX_AP_MAX 8
enum{
	REV_EX_ER_L1 = 0, // er left
	REV_EX_ER_R1,     // er right
	REV_EX_RV_L1,     // rv left
	REV_EX_RV_R1,     // rv right
	REV_EX_DELAY,     // unit delay type
};
enum{
	REV_EX_UNIT = 0, // er/rv index,size, (use mod
	REV_EX_RD,     // rv delay 
	REV_EX_AP1,    // allpass1 (er
	REV_EX_AP2,    // allpass2 (rv
	REV_EX_DELAY2, // other delay type
};

typedef struct _InfoReverbEX{
	int8 mode, flt_type;
	double rev_dly_ms, rev_time_sec, rev_width, rev_damp, rev_level, rev_feedback, rev_wet;	
	double height, width, depth, rev_damp_freq, rev_damp_type, rev_damp_bal, density;
	double er_time_ms, er_level, level, er_damp_freq, er_roomsize;
	FLOAT_T levelrv, leveler, feedback, flt_dry, flt_wet, rv_feedback[REV_EX_UNITS], st_sprd, in_level, levelap, fbap;
	int32 levelrvi, leveleri, feedbacki, flt_dryi, flt_weti, rv_feedbacki[REV_EX_UNITS], st_sprdi, in_leveli, levelapi, fbapi;
	int8 init, unit_num, ap_num, alloc[REV_EX_UNITS][REV_EX_DELAY], alloc2[2], aalloc[REV_EX_AP_MAX][REV_EX_DELAY];
	DATA_T *buf[REV_EX_UNITS][REV_EX_DELAY], rv_out[REV_EX_UNITS][2], *rv_in[REV_EX_UNITS][2], 
		hist[2], *buf2[2], *abuf[REV_EX_AP_MAX][REV_EX_DELAY];
	int32 size[REV_EX_UNITS][REV_EX_DELAY], index[REV_EX_UNITS][REV_EX_DELAY];
	int32 size2[REV_EX_DELAY2], index2[REV_EX_DELAY2], delaya[REV_EX_AP_MAX][REV_EX_DELAY];
	FilterCoefficients er_fc, rv_fc1[REV_EX_UNITS], hpf;
	void (*do_reverb_mode)(DATA_T *buf, int32 count, struct _InfoReverbEX *info);
	// MOD
	FLOAT_T mcount[REV_EX_UNITS][REV_EX_DELAY], mrate[REV_EX_UNITS][REV_EX_DELAY], mphase[REV_EX_UNITS][REV_EX_DELAY]
		, mdelay[REV_EX_UNITS][REV_EX_DELAY], mdepth[REV_EX_UNITS][REV_EX_DELAY];
	// thread
	int8 thread;
	int32 tcount;
	DATA_T *tibuf; // in
	DATA_T *tobuf; // out
	int32 index2t[REV_EX_DELAY2];
} InfoReverbEX;


typedef struct _InfoReverbEX2{
	int8 mode;
	int32 revtype;
	double er_time_ms, er_damp_freq, rev_dly_ms, rev_time_sec, rev_feedback, level;
	FLOAT_T levelrv;
	int8 init, thread, ithread, rsmode, fftmode, fm_p;
	int32 frame, srate, count, tcount, bsize, bcount, tbcount, pmr_p, rt_p;	
	float *irdata[2], *buf[2], *tbuf[2]; // buf:delay(in)*2 , tbuf:out*2
	FLOAT_T rsfb[2];
	DATA_T *ptr;
	int32 fnum, scount[2], bdcount[2];
	float *fs[2], *ss[2], *rvs[2], *rs[2], *is[2], *os[2], *fi[2], *bd[2], *ios[2];
	float *fftw[2];
	int *ffti[2];
} InfoReverbEX2;


/*! 3-Tap Stereo Delay Effect */
typedef struct _InfoDelay3 {
	uint8 delay_type, opt_mode;
	double time_ms[3];
	simple_delay delayL, delayR;
	int32 size[3], index[3];
	double level[3], feedback, feedbackd, send_reverb, delay_level;
	int32 leveli[3], feedbacki, send_reverbi;
	DATA_T *ptrL, *ptrR;
} InfoDelay3;


#define CHORUS_PHASE_MAX 8
/*! Stereo Chorus Effect */
typedef struct _InfoStereoChorus{
	uint8 init, mode, filter_type, depth_dev, pdelay_dev, pan_dev;
	FLOAT_T filter_cutoff;
	simple_delay delayL, delayR;
	lfo lfoL, lfoR;
	int32 sptL0, sptL1, sptL2, sptL3, sptL4, sptL5, sptR0, sptR1, sptR2, sptR3, sptR4, sptR5;
	DATA_T histL, histR;
	int32 depth0, depth1, depth2, depth3, depth4, depth5,
		pdelay0, pdelay1, pdelay2, pdelay3, pdelay4, pdelay5;
	int32 pdelayc, depthc;	/* in samples */
	int32 pan0, pan1, pan2, pan3, pan4, pan5;
	FLOAT_T rate, pdelay_ms, depth_ms, depth_cent, phase_diff;
	FLOAT_T level, feedback, feedbackd, send_reverb, send_delay;
	FLOAT_T dry, dryd, wet, wetd;
	FilterCoefficients fc;

	int32 leveli, feedbacki, send_reverbi, send_delayi;

// chorus_ex
	uint8 phase, depth_type, stage;
	int32 pan[CHORUS_PHASE_MAX];
	FLOAT_T lfo_count, lfo_rate, div_out;
	FLOAT_T delay[CHORUS_PHASE_MAX], depth[CHORUS_PHASE_MAX], lfo_phase[CHORUS_PHASE_MAX][2]; 
	DATA_T *buf, hist[2];
	void (*do_chorus_mode)(DATA_T *buf, int32 count, struct _InfoStereoChorus *info);
	struct _InfoStereoChorus *next_stage;
} InfoStereoChorus;

typedef struct {
	uint8 init, mode, filter_type, depth_dev, pdelay_dev, pan_dev, out;
	FLOAT_T filter_cutoff;
	simple_delay delayL, delayR;
	int32 delay_count, size;
	FLOAT_T rate, pdelay_ms, depth_ms, depth_cent, phase_diff;
	FLOAT_T level, feedback, feedbackd;
	FLOAT_T dry, dryd, wet, wetd;
	FilterCoefficients fc;
	int32 leveli, feedbacki;
// chorus_ex
	uint8 phase, depth_type, stage;
	FLOAT_T lfo_count, lfo_rate, div_out;
	FLOAT_T delay[CHORUS_PHASE_MAX], depth[CHORUS_PHASE_MAX], lfo_phase[CHORUS_PHASE_MAX][2]; 
	DATA_T *buf, hist;
	FLOAT_T azimuth[CHORUS_PHASE_MAX];
	Info3DLocate locate[CHORUS_PHASE_MAX];
} Info3DChorus;

/*! Stereo Overdrive / Distortion */
typedef struct {
	int8 mode, od_type, amp_type, cab_type;
	double drive, cutoff;
	double wetd;
	int32 weti;
	Drive drv1, drv2, drv3;
	FilterCoefficients bw1l, bw2l, bw3l, bw4l, bw1r, bw2r, bw3r, bw4r;
	FilterCoefficients bq;
} InfoStereoOD;

/*! Delay L,R */
typedef struct {
	int8 init, mode, phaser, phasel, fb_mode;
	simple_delay delayL, delayR;
	int32 index[4], offset[4];	/* L,R, fbL, fbR */
	double rdelay, ldelay, fdelay1, fdelay2;	/* in ms */
	double feedback, high_damp;
	FLOAT_T feedbackd;
	int32 feedbacki;
	double psignL, psignR;
	DATA_T *fb_bufL, *fb_bufR;
	int32 *fb_indexL, *fb_indexR;
	FilterCoefficients lpf;
} InfoDelayLR;

/*! Echo */
typedef struct {
	simple_delay delayL, delayR;
	int32 index[2], size[2];	/* L2,R2 */
	double rdelay1, ldelay1, rdelay2, ldelay2;	/* in ms */
	double delay2_level, lfeedback, rfeedback, high_damp;
	int32 delay2_leveli, lfeedbacki, rfeedbacki;
	FilterCoefficients lpf;
} InfoEcho;

/*! LO-FI */
typedef struct {
	int8 mode, pre_fil_type, post_fil_type, nz_gen, wp_sel, disc_type, hum_type;
	FLOAT_T bit_length, sr_rate, pre_fil_freq, pre_fil_reso, post_fil_freq, post_fil_reso, wp_freq, disc_freq, hum_freq;	
	FLOAT_T level_in, level_out, level_down, level_up;
	FLOAT_T wp_level, rnz_lev, discnz_lev, hum_level, rdetune;
//	FLOAT_T wp_wet, wp_dry, rnz_wet, rnz_dry, discnz_wet, discnz_dry, hum_wet, hum_dry, rdetune;
	pink_noise pnzl, pnzr;
	FilterCoefficients sr_fil, pre_fil, post_fil, wp_lpf, hum_lpf, disc_lpf;
} InfoLoFi;

typedef struct {
	int8 init, out, clockwize, turn;
	double azimuth, speed, elevation, distance; // Hz?
	FLOAT_T count, rate, stop;
	Info3DLocate locate;
} Info3DAuto;

typedef struct {
	int8 init, out;
	double azimuth, elevation, distance; // Hz?
	Info3DLocate locate;
} Info3DManual;

typedef struct {
	FLOAT_T leveld;
	int8 mode, wave, lfo_sw;
	double freq, rate, depth;
	FLOAT_T osc_rate, osc_freq, lfo_rate, lfo_freq;	
	FLOAT_T (*do_lfo)(FLOAT_T);
} InfoRingModulator;

#define PS_CF_PHASE 3
#define PS_CF_DELAY (24) // ms

typedef struct {
	int8 init_flg, fade_flg;
	FLOAT_T leveld;
	double pitch_cent, feedback, pre_delay_ms, ps_delay_ms;
	double pitch_cent_p, pre_delay_ms_p, ps_delay_ms_p;
	int32 wdelay, wcount, wcycle;
	FLOAT_T rcount[PS_CF_PHASE], rsdelay, rscount[PS_CF_PHASE], rate, pdelay, div_cf;
	FLOAT_T *ptr;
	Envelope3 env;
} InfoPitchShifter_core;

typedef struct {
	int8 mode;
	InfoPitchShifter_core psl, psr;
	int init_flg;
} InfoPitchShifter;


/*! Stereo EQ */
typedef struct {
    int16 low_freq, high_freq, m1_freq, m2_freq;		/* in Hz */
	int16 low_gain, high_gain, m1_gain, m2_gain;		/* in dB */
	double m1_q, m2_q;
	FilterCoefficients hsf, lsf, m1, m2;
} InfoStereoEQ;

/*! 8-Band EQ */
typedef struct {
	int8 init, mode;
    int16 freq[8];		/* in Hz */
	int16 gain[8];		/* in dB */
	FLOAT_T width;
	FilterCoefficients peak[8];
} InfoSpectrum;

typedef struct {
	int8 init, mode;
	FLOAT_T sens, hpf_cutoff, wetd, feedbackd;
//	filter_shelving hpfl1, hpfl2, hpfl3, hpfl4,	hpfr1, hpfr2, hpfr3, hpfr4;
	simple_delay delayL, delayR;
	FilterCoefficients fc, hsh;
} InfoEnhancer;

#define HUMANIZER_PHASE 10
typedef struct {
	int8 init, mode, od_sw, vowel, p_vowel;
	double drive, accel;
	double leveld, p_accel, p_ac, inleveld;
	int32 leveli, acceli, inleveli;
	Drive drv;
	Envelope3 env[HUMANIZER_PHASE], env2;
	FilterCoefficients fc[HUMANIZER_PHASE], fc2;
} InfoHumanizer;

typedef struct InfoDistortion_t{
	int8 mode, od_sw, od_type, bass_od;
	FLOAT_T level, drive, tone, edge;
	FLOAT_T drived, leveld, curve1, curve2;
	FilterCoefficients od_fc1, od_fc2, od_fc3;
	void (*do_od1)(DATA_T *buf, FLOAT_T gain, FLOAT_T curve);
	void (*do_od2)(DATA_T *buf, FLOAT_T gain, FLOAT_T curve);
} InfoDistortion;

typedef struct {
	int8 init, mode, amp_sw, cab_sw, amp_type, cab_type;
	FLOAT_T level, gain1, gain2, tone, bright, mic_pos, mic_level, mic_direct, ch_delay;
	FLOAT_T eq1_gain, eq2_gain, eq3_gain, eq4_gain;
	FLOAT_T leveld, gain1d, gain2d, curve1, curve2, curve3;
	void (*do_amp1)(DATA_T *buf, FLOAT_T gain, FLOAT_T curve);
	void (*do_amp2)(DATA_T *buf, FLOAT_T gain, FLOAT_T curve);
	void (*do_amp3)(DATA_T *buf, FLOAT_T gain, FLOAT_T curve);
	FilterCoefficients amp_fc1, amp_fc2, amp_fc3, amp_fc4;
	FilterCoefficients eq_fc1, eq_fc2, eq_fc3, eq_fc4;
	FilterCoefficients cab_fc1, cab_fc2, cab_fc3;
	int8 speaker_num, stack_num;
	FLOAT_T mwet, mdry, flevel, blevel;
	DATA_T *ptr1, *ptr2, *ptr3, *ptr4;
	int32 index, offset1, offset2[3], offset3[2];
} InfoAmpSimulator;

/*! Overdrive 1 / Distortion 1 */
typedef struct {
	double level, leveld;
	int32 leveli; /* in fixed-point */
	int8 drive, amp_sw, amp_type, type, mode;
	Drive drv1, drv2, drv3;
	FilterCoefficients bw1, bw2, bw3, bw4, bq;
	InfoAmpSimulator amp;
	simple_delay dly;
} InfoOverdrive;

#define PHASER_PHASE 12

// phaser_ex
typedef struct {
	int8 init, mode, depth_type, phase;
	simple_delay delayL, delayR;
	DATA_T hist[2];
	double rate, pdelay_ms, depth_ms, depth_cent, phase_diff;
	double wetd, feedback, feedbackd, div_out;
	double manual, reso, offset;
	int32 delay_size, delay_count;
	FLOAT_T lfo_count, lfo_rate;
	FLOAT_T delaycd, depthcd, lfo_phaseL[PHASER_PHASE], lfo_phaseR[PHASER_PHASE]; 
	FilterCoefficients fc;
} InfoPhaser;

typedef struct {
	int8 init, pol, mode, type, sens_sw, lfo_sw, env_mode, ptype;
	double manual, peak, sens, rate, depth, release, in_leveld, leveld;
	double lfo_rate, lfo_freq, peak_rate, peak_count, peak_level;
	double sens_level, sens_coef, sens_mult, depth_mult, attack_cnt, release_cnt;
	Envelope3 man_env, sens_env, flt_env;
	FilterCoefficients fc;
} InfoAutoWah;

typedef struct {
	int8 init, mode, speed, speedp;
	FLOAT_T low_slow, low_fast, low_accl, low_level,	hi_slow, hi_fast, hi_accl, hi_level, separate;
	FLOAT_T low_rate, hi_rate, low_slow_freq, low_fast_freq, hi_slow_freq, hi_fast_freq;
	FLOAT_T low_leveld, hi_leveld;
	int32 low_cnt, hi_cnt;
	Envelope3 low_env, hi_env;
	FilterCoefficients fc_rotL, fc_rotH, fc_lpfL, fc_lpfH;
} InfoRotary;

typedef struct {
	int8 init, type, wave, mode, pre_type, pre_wave, pm_mode, pm_init;
	FLOAT_T rate, depth, phase, pre_phase, depth_cent, depth_cnt;
	int32 count, size;
	DATA_T *buf;
	Oscillator2 lfo;
	Envelope2 env;
	Envelope3 pm_env;
} InfoTremolo; // inc AutoPan

typedef struct {
	int8 mode;
	FLOAT_T div_level_0db;
	FLOAT_T attak, sustain, pre_gain, post_gain, threshold, slope, lookahead, window;
	FLOAT_T att, rel, div_nrms, prgain, pogain, env;
	int32 lhsmp, nrms;
	int32 delay1, count1, delay2, count2;
	FLOAT_T ptr1[400]; // 1ms sr max 384kHz 
	DATA_T ptr2[4000]; // stereo 5ms sr max 384kHz 
} InfoCompressor;

typedef struct {
	int8 init, mode, dir;
	FLOAT_T leveld, pre_gain, post_gain, ahead, release, threshold, ratio;
	FLOAT_T amp, pre, post, max, max2, peak_tmp, peak_hold, env, att, rel;
	int32 buffer_size, write_cnt, read_cnt, wnd_cnt, wnd_size, hold_len, hold_cnt;
	DATA_T *ptr;
} InfoLimiter;

// mod_delay_ex
typedef struct {
	int8 init, mode, fb_mode, depth_type;
	simple_delay delayL, delayR;
	DATA_T hist[2], *fb_ptrL, *fb_ptrR;
	double rate, ldelay_ms, rdelay_ms, depth_ms, depth_cent, phase_diff;
	double wetd, feedback, feedbackd;
	double high_damp, phase, offset;
	int32 delay_size, delay_countL, delay_countR;
	FLOAT_T lfo_count, lfo_rate;
	FLOAT_T delaycdL, delaycdR, depthcd, lfo_phaseL, lfo_phaseR; 
	FilterCoefficients fc;
} InfoModDelay;

#define MT_DELAY_TAP_MAX 5 // > 2 , +1 feedback_delay
typedef struct {
	int8 init, mode, tap, pan[MT_DELAY_TAP_MAX];
	simple_delay delay;
	int32 index[MT_DELAY_TAP_MAX], size[MT_DELAY_TAP_MAX];	/* tap + feedback */
	double dtime[MT_DELAY_TAP_MAX], dlevel[MT_DELAY_TAP_MAX];	/* in ms */
	double feedback, high_damp;
	double outL[MT_DELAY_TAP_MAX], outR[MT_DELAY_TAP_MAX], feedbackd;
	int32 dleveli[MT_DELAY_TAP_MAX];
	int32 feedbacki;
	FilterCoefficients fc;
} InfoMultiTapDelay;
	
typedef struct {
	int8 init, mode, tap, out, pan[MT_DELAY_TAP_MAX];
	simple_delay delay;
	int32 index[MT_DELAY_TAP_MAX], size[MT_DELAY_TAP_MAX];	/* tap + feedback */
	double dtime[MT_DELAY_TAP_MAX], dlevel[MT_DELAY_TAP_MAX];	/* in ms */
	double feedback, high_damp;
	double feedbackd;
	int32 dleveli[MT_DELAY_TAP_MAX];
	int32 feedbacki;
	FilterCoefficients fc;
	FLOAT_T azimuth[MT_DELAY_TAP_MAX];
	Info3DLocate locate[MT_DELAY_TAP_MAX];
} Info3DMultiTapDelay;

typedef struct {
	int8 init, mode, accel, lshift, rshift;
	simple_delay delayL, delayR;
	int32 index[2], size[2];	/* L,R */
	FLOAT_T index2[2];
	double rdelay, ldelay;
	FLOAT_T rdelay_set, ldelay_set;	/* in ms */
	double feedback, high_damp;
	FLOAT_T feedbackd;
	FLOAT_T laccel, raccel;
	FilterCoefficients fc;
} InfoDelayShifter;

// gate_reverb
#define GATE_REV_ER 8
#define GATE_REV_AP 2
#define GATE_REV_RV 8
typedef struct {
	int8 init, mode, alloc_flag, type, flt_type, env_mode;
	double gate_time_ms, rev_time_sec, rev_width, rev_damp, rev_level, rev_feedback, rev_wet;	
	double room_size, rev_damp_freq, rev_damp_type, rev_damp_bal, density;
	double er_time_ms, er_level, er_damp_freq, er_roomsize;
	double feedback, levelrv, leveler, level, flt_dry, flt_wet, in_level;	
	double er_norm, rv_norm, er_cross, rv_cross;
	fb_delay erL[GATE_REV_ER], erR[GATE_REV_ER];
	allpass2 apL[GATE_REV_AP], apR[GATE_REV_AP];
	comb2 rvL[GATE_REV_RV], rvR[GATE_REV_RV];
	FilterCoefficients fc, hpf;
	DATA_T histL, histR;	
	InfoGate gate;
} InfoGateReverb;

typedef struct {
	int8 init, mode, env_mode;
	double attack_ms, release_ms, threshold;
	InfoGate gate;
} InfoNoiseGate;




typedef struct {
	int8 init, mode, phase;
	double delay_ms;
	FLOAT_T wetd, feedbackd, sign;
	int32 delay_cnt;
	simple_delay delay;
} InfoAmbience;



/******************************** GS EFFECT ********************************/


typedef struct {
    InfoEQ2 eq;
} Info_GS_PostEQ;

typedef struct {
	double level;
    InfoStereoEQ eq;
} Info_GS_StereoEQ;

typedef struct {
	double level, panl, panr;
	InfoSpectrum eq;
} Info_GS_Spectrum;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoEnhancer eh;
} Info_GS_Enhancer;

typedef struct {
	double level, panl, panr;
	InfoHumanizer hm;
} Info_GS_Humanizer;

typedef struct {
	double level, panl, panr;
	InfoOverdrive od;
} Info_GS_Overdrive;

typedef struct {
	double level, panl, panr;
	InfoOverdrive od;
} Info_GS_Distortion;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPhaser ph;
} Info_GS_Phaser;

typedef struct {
	double level, panl, panr;
	int32 leveli;
	InfoAutoWah aw;
} Info_GS_AutoWah;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
} Info_GS_Rotary;

typedef struct {
	double level, dry, wet;
	int32 leveli;	
	InfoStereoChorus fl;
} Info_GS_StereoFlanger;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus fl;
} Info_GS_StepFlanger;

typedef struct {
	double level;
	int32 leveli;
	InfoTremolo trm;
} Info_GS_Tremolo; 

typedef struct {
	double level;
	int32 leveli;
	InfoTremolo trm;
} Info_GS_AutoPan; 

typedef struct {
	double level, panl, panr;
	int32 leveli;
	InfoCompressor cmp;
} Info_GS_Compressor;

typedef struct {
	double level, panl, panr;
	int32 leveli;
	InfoLimiter lmt;
} Info_GS_Limiter;	

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
} Info_GS_HexaChorus;

typedef struct {
	double level, dry, wet, pre_dly, cho_rate, cho_depth, trm_phase, trm_rate, trm_sep;
	int32 leveli;
	InfoStereoChorus cho;
	InfoTremolo trm;
} Info_GS_TremoloChorus;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
} Info_GS_StereoChorus;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
} Info_GS_SpaceD;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	Info3DChorus cho;
} Info_GS_3DChorus;
	
typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoDelayLR dly;
} Info_GS_StereoDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoModDelay dly;
} Info_GS_ModDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
//	InfoDelayLCR dly;
//	Info3TapDelay dly;
	InfoMultiTapDelay dly;
} Info_GS_3TapDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
//	Info4TapDelay dly;
	InfoMultiTapDelay dly;
} Info_GS_4TapDelay;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoDelayShifter dly;
} Info_GS_TmCtrlDelay;

typedef struct {
	double rev_level, rev_wet, rev_dry;
	int32 leveli;
//	InfoReverb rvb;
	InfoFreeverb rvb;
	InfoReverbEX rvb2;
	int rev_type;
} Info_GS_Reverb;

typedef struct {
	double gr_wet, gr_dry, gr_level;
	InfoGateReverb gr;
} Info_GS_GateReverb;

typedef struct {
	double level, dry, wet;
	int32 leveli;
//	InfoDelayLCR dly;
//	Info3TapDelay dly;
//	InfoMultiTapDelay dly;
	Info3DMultiTapDelay dly;
} Info_GS_3DDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	double bal1, bal2, panll, panlr, panrl, panrr;
	InfoPitchShifter ps;
} Info_GS_2PitchShifter;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoPitchShifter ps;
} Info_GS_FbPitchShifter;

typedef struct {
	double level;
	int32 leveli;
	Info3DAuto locate;
} Info_GS_3DAuto;

typedef struct {
	double level;
	int32 leveli;
	Info3DManual locate;
} Info_GS_3DManual;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoLoFi lf;
} Info_GS_LoFi1;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoLoFi lf;
} Info_GS_LoFi2;

typedef struct {
	double level, od_panl, od_panr, cho_wet, cho_dry;
	int32 leveli;
	InfoOverdrive info_od;
	InfoStereoChorus info_cho;
} Info_GS_S_OD_Chorus;

typedef struct {
	double level, od_panl, od_panr, fl_wet, fl_dry;
	int32 leveli;
	InfoOverdrive info_od;
	InfoStereoChorus info_fl;
} Info_GS_S_OD_Flanger;

typedef struct {
	double level, od_panl, od_panr, dly_wet, dly_dry;
	int32 leveli;
	InfoOverdrive info_od;
	InfoDelayLR info_dly;
} Info_GS_S_OD_Delay;

typedef struct {
	double level, od_panl, od_panr, cho_wet, cho_dry;
	int32 leveli;
	InfoOverdrive info_od;
	InfoStereoChorus info_cho;
} Info_GS_S_DS_Chorus;

typedef struct {
	double level, od_panl, od_panr, fl_wet, fl_dry;
	int32 leveli;
	InfoOverdrive info_od;
	InfoStereoChorus info_fl;
} Info_GS_S_DS_Flanger;

typedef struct {
	double level, od_panl, od_panr, dly_wet, dly_dry;
	int32 leveli;
	InfoOverdrive info_od;
	InfoDelayLR info_dly;
} Info_GS_S_DS_Delay;

typedef struct {
	double level, eh_wet, eh_dry, cho_wet, cho_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoStereoChorus info_cho;
} Info_GS_S_EH_Chorus;

typedef struct {
	double level, eh_wet, eh_dry, fl_wet, fl_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoStereoChorus info_fl;
} Info_GS_S_EH_Flanger;

typedef struct {
	double level, eh_wet, eh_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoDelayLR info_dly;
} Info_GS_S_EH_Delay;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_GS_S_Cho_Delay;

typedef struct {
	double level, fl_wet, fl_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoChorus info_fl;
	InfoDelayLR info_dly;
} Info_GS_S_FL_Delay;

typedef struct {
	double level, cho_wet, cho_dry, fl_wet, fl_dry;
	int32 leveli;
	InfoStereoChorus info_cho;
	InfoStereoChorus info_fl;
} Info_GS_S_Cho_Flanger;

typedef struct {
	double level;
	int32 leveli;
	InfoOverdrive info_od;
	InfoEQ3 info_eq;
	InfoRotary info_rt;
} Info_GS_S_RotaryMulti;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoCompressor info_cmp;
	InfoOverdrive info_od;
	InfoEQ2 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_GS_S_GtMulti1;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry;
	int32 leveli;
	InfoCompressor info_cmp;
	InfoOverdrive info_od;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
} Info_GS_S_GtMulti2;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry;
	int32 leveli;	
//	InfoWah info_wah;
	InfoAutoWah info_aw;
	InfoOverdrive info_od;
	InfoEQ2 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_GS_S_GtMulti3;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoCompressor info_cmp;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_GS_S_CleanGtMulti1;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoAutoWah info_aw;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_GS_S_CleanGtMulti2;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry;
	int32 leveli;
	InfoCompressor info_cmp;
	InfoOverdrive info_od;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
} Info_GS_S_BaseMulti;

typedef struct {
	double level, eh_wet, eh_dry, ph_wet, ph_dry, cho_wet, cho_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoPhaser info_ph;
	InfoStereoChorus info_cho;
	InfoTremolo info_trm;
} Info_GS_S_RhodesMulti;

typedef struct {
	double level, rm_wet, rm_dry, pswet, psdry, ph_wet, ph_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoRingModulator info_rm;
	InfoEQ3 info_eq;
	InfoPitchShifter info_ps;
	InfoPhaser info_ph;
	InfoDelayLR info_dly;
} Info_GS_S_KeyboardMulti;

typedef struct {
	Info_P_Mix info_mix;
	double cho_wet, cho_dry, dly_wet, dly_dry;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_GS_P_Cho_Delay;

typedef struct {
	Info_P_Mix info_mix;
	double fl_wet, fl_dry, dly_wet, dly_dry;
	InfoStereoChorus info_fl;
	InfoDelayLR info_dly;
} Info_GS_P_FL_Delay;

typedef struct {
	Info_P_Mix info_mix;
	double cho_wet, cho_dry, fl_wet, fl_dry;
	InfoStereoChorus info_cho;
	InfoStereoChorus info_fl;
} Info_GS_P_Cho_Flanger;

typedef struct {
	Info_P_Mix info_mix;
	InfoOverdrive info_od1, info_od2;
} Info_GS_P_OD1_OD2;

typedef struct {
	Info_P_Mix info_mix;
	InfoOverdrive info_od;
	InfoRotary info_rt;
} Info_GS_P_OD_Rotary;

typedef struct {
	Info_P_Mix info_mix;
	double ph_wet, ph_dry;
	InfoOverdrive info_od;
	InfoPhaser info_ph;
} Info_GS_P_OD_Phaser;

typedef struct {
	Info_P_Mix info_mix;
	InfoOverdrive info_od;
	InfoAutoWah info_aw;
} Info_GS_P_OD_AutoWah;

typedef struct {
	Info_P_Mix info_mix;
	double ph_wet, ph_dry;
	InfoPhaser info_ph;
	InfoRotary info_rt;
} Info_GS_P_PH_Rotary;

typedef struct {
	Info_P_Mix info_mix;
	double ph_wet, ph_dry;
	InfoPhaser info_ph;
	InfoAutoWah info_aw;
} Info_GS_P_PH_AutoWah;



/******************************** XG EFFECT ********************************/

typedef struct {
	double rev_level, rev_wet, rev_dry;
	int32 leveli;
	InfoPreFilter bpf;
	InfoStandardReverb rvb0;
	InfoFreeverb rvb;
	InfoReverbEX rvb2;
	InfoReverbEX2 rvb3;
	int rev_type;
} Info_XG_Reverb1;

typedef struct {
	double rev_level, rev_wet, rev_dry;
	int32 leveli;
	InfoPreFilter bpf;
//	InfoReverb rvb;
	InfoPlateReverb rvb;
	InfoReverbEX2 rvb3;
	int rev_type;
} Info_XG_Plate;

typedef struct {
	double level, wet, dry;
	int32 leveli;
//	InfoDelayLCR dly;
//	Info3TapDelay dly;
	InfoMultiTapDelay dly;
	InfoEQ2 eq;
} Info_XG_DelayLCR;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoDelayLR dly;
	InfoEQ2 eq;
} Info_XG_DelayLR;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoEcho dly;
	InfoEQ2 eq;
} Info_XG_Echo;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoDelayLR dly;
	InfoEQ2 eq;
} Info_XG_CrossDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPreFilter bpf;
	InfoEarlyReflection er;
} Info_XG_EarlyRef1;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPreFilter bpf;
	InfoEarlyReflection er;
} Info_XG_EarlyRef2;

typedef struct {
	double rev_level, rev_wet, rev_dry;
	InfoPreFilter bpf;
	InfoGateReverb gr;
} Info_XG_GateReverb;

typedef struct {
	double rev_level, rev_wet, rev_dry;
	int32 leveli;	
	InfoPreFilter bpf;
	InfoStandardReverb rvb0;
	InfoFreeverb rvb;
	InfoReverbEX rvb2;
	InfoReverbEX2 rvb3;
	int rev_type;
} Info_XG_Reverb2;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoEcho dly;
	InfoPreFilter bpf;
} Info_XG_Karaoke;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoDelayLR dly;
	InfoEQ2 eq;
} Info_XG_TempoDelay;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoEcho dly;
	InfoEQ2 eq;
} Info_XG_TempoEcho;

typedef struct {
	double dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoDelayLR dly;
	InfoEQ2 eq;
} Info_XG_TempoCross;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoStereoChorus cho;
	InfoEQ3 eq;
} Info_XG_Chorus;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoStereoChorus cho;
	InfoEQ3 eq;
} Info_XG_Celeste;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoStereoChorus fl;
	InfoEQ3 eq;
} Info_XG_Flanger;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoStereoChorus cho1, cho2;
	InfoEQ3 eq;
} Info_XG_Symphonic;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoEQ3 eq;	
} Info_XG_RotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_DS_RotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_OD_RotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_AMP_RotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoTremolo trm;
	InfoEQ3 eq;	
} Info_XG_Tremolo;

typedef struct {
	double level;
	int32 leveli;
	InfoTremolo ap;
	InfoEQ3 eq;	
} Info_XG_AutoPan;

typedef struct {
	double level, ph_wet, ph_dry;
	int32 leveli;
	InfoPhaser ph;
	InfoEQ2 eq;	
} Info_XG_Phaser1;

typedef struct {
	double level, ph_wet, ph_dry;
	int32 leveli;
	InfoPhaser ph;
	InfoEQ2 eq;	
} Info_XG_Phaser2;

typedef struct {
	double od_level, od_wet, od_dry;
	InfoStereoOD od;
	InfoEQ3 eq;	
} Info_XG_Distortion;

typedef struct {
	double od_level, od_wet, od_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoEQ3 eq;	
	InfoCompressor cmp1, cmp2;
} Info_XG_Comp_Distortion;

typedef struct {
	double od_level, od_wet, od_dry;
	InfoStereoOD od;
	InfoEQ3 eq;	
} Info_XG_StDistortion;

typedef struct {
	double od_level, od_wet, od_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoEQ3 eq;	
} Info_XG_Overdrive;

typedef struct {
	double od_level, od_wet, od_dry;
	InfoStereoOD od;
	InfoEQ3 eq;	
} Info_XG_StOverdrive;

typedef struct {
	double od_level, od_wet, od_dry;
	InfoStereoOD od;
	InfoEQ3 eq;	
} Info_XG_AmpSimulator;

typedef struct {
	double od_level, od_wet, od_dry;
	InfoStereoOD od;
	InfoEQ3 eq;	
} Info_XG_StAmpSimulator;

typedef struct {
	InfoEQ3 eq;	
} Info_XG_EQ3;

typedef struct {
	InfoEQ2 eq;	
} Info_XG_EQ2;

typedef struct {
	double level, dry, wet;
	int32 leveli;
//	InfoXGAutoWah aw;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_AutoWah;

typedef struct {
	double level, dry, wet;
	int32 leveli;
//	InfoXGAutoWah aw;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
	InfoEQ3 eq3;	
} Info_XG_AutoWah_DS;

typedef struct {
	double level, dry, wet;
	int32 leveli;
//	InfoXGAutoWah aw;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
	InfoEQ3 eq3;	
} Info_XG_AutoWah_OD;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPitchShifter ps;
	Info_P_Mix pmix;
} Info_XG_PitchChange1;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPitchShifter ps;
	Info_P_Mix pmix;
} Info_XG_PitchChange2;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoEnhancer eh;
} Info_XG_HarmonicEnhancer;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_TouchWah1;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_TouchWah_DS;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
	InfoEQ3 eq3;	
} Info_XG_TouchWah_OD;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoEQ2 eq;	
	InfoEQ3 eq3;	
} Info_XG_TouchWah2;

typedef struct {
	double level;
	int32 leveli;
	InfoCompressor cmp;
} Info_XG_Compressor;

typedef struct {
	double level;
	int32 leveli;
	InfoNoiseGate ng;
} Info_XG_NoiseGate;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPitchShifter ps;
	InfoEQ2 eq;	
} Info_XG_EnsembleDetune;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoEQ2 eq;	
	InfoAmbience amb;
} Info_XG_Ambience;

typedef struct {
	double level, wet, dry;
	InfoHumanizer hm;
} Info_XG_TalkingModulator;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoLoFi lf;
} Info_XG_LoFi;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoEQ3 eq;	
} Info_XG_2WayRotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_DS_2WayRotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_OD_2WayRotarySpeaker;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoStereoOD od;
	InfoEQ2 eq;	
} Info_XG_AMP_2WayRotarySpeaker;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_DS_Delay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_OD_Delay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoCompressor cmp;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Comp_DS_Delay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoCompressor cmp;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Comp_OD_Delay;

typedef struct {
	double wah_wet, wah_dry, od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Wah_DS_Delay;

typedef struct {
	double wah_wet, wah_dry, od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Wah_OD_Delay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
} Info_XG_VDistortionHard;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
} Info_XG_VDistortionHard_Delay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
} Info_XG_VDistortionSoft;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
} Info_XG_VDistortionSoft_Delay;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
	InfoEQ2 eq;	
} Info_XG_DualRotarSpeaker;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_DS_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_OD_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoCompressor cmp;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Comp_DS_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoCompressor cmp;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Comp_OD_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Wah_DS_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoAutoWah aw;
	InfoStereoOD od;
	InfoDelayLR dly;
	InfoEQ3 eq;	
} Info_XG_Wah_OD_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
} Info_XG_VDistortionHard_TempoDelay;

typedef struct {
	double od_level, od_wet, od_dry, dly_level, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoOD od;
	InfoDelayLR dly;
} Info_XG_VDistortionSoft_TempoDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoStereoChorus fl;
	InfoEQ3 eq;
} Info_XG_VFlanger;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoStereoChorus fl;
	InfoEQ3 eq;
} Info_XG_TempoFlanger;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoPhaser ph;
	InfoEQ3 eq;	
} Info_XG_TempoPhaser;

typedef struct {
	double rm_level, rm_wet, rm_dry;
	int32 leveli;
	InfoRingModulator rm;
	InfoEQ2 eq;
	FilterCoefficients lpf, hpf;
} Info_XG_RingModulator;

typedef struct {
	double level;
	int32 leveli;
	Info3DManual locate;
} Info_XG_3DManual;

typedef struct {
	double level;
	int32 leveli;
	Info3DAuto locate;
} Info_XG_3DAuto;


/******************************** SD EFFECT ********************************/

/******** SD Chorus Reverb **********/

typedef struct {
	int8 rev_type;
	double level;
	int32 leveli;
	InfoStandardReverb rvb0;
	InfoFreeverb rvb;
	InfoReverbEX rvb2;
	InfoReverbEX2 rvb3;
} Info_SD_Rev_Reverb;

typedef struct {
	double level;
	int32 leveli;
	InfoPlateReverb rvb;
	InfoReverbEX2 rvb3;
	int rev_type;
} Info_SD_Rev_Plate;

typedef struct {
	double level;
	int32 leveli;
	InfoDelayLR dly;
} Info_SD_Rev_Delay;

typedef struct {
	double level;
	int32 leveli;
	InfoStereoChorus cho;
} Info_SD_Cho_Chorus;

typedef struct {
	double level;
	int32 leveli;
	InfoMultiTapDelay dly;
} Info_SD_Cho_Delay;


/******** SD MFX ********/

typedef struct {
	double level;
    InfoStereoEQ eq;
} Info_SD_StereoEQ;

typedef struct {
	double level, panl, panr;
	int amp_sw;
	InfoDistortion od;
	InfoAmpSimulator amp;
    InfoEQ2 eq;
} Info_SD_Overdrive;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoPhaser ph;
    InfoEQ2 eq;
} Info_SD_Phaser;

typedef struct {
	double level, panl, panr;
	InfoSpectrum eq;
} Info_SD_Spectrum;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoEnhancer eh;
    InfoEQ2 eq;
} Info_SD_Enhancer;

typedef struct {
	double level, panl, panr;
	int32 leveli;
	InfoAutoWah aw;
    InfoEQ2 eq;
} Info_SD_AutoWah;

typedef struct {
	double level;
	int32 leveli;
	InfoRotary rt;
    InfoEQ2 eq;
} Info_SD_Rotary;

typedef struct {
	double level, panl, panr;
	int32 leveli;
	InfoCompressor cmp;
    InfoEQ2 eq;
} Info_SD_Compressor;

typedef struct {
	double level, panl, panr;
	int32 leveli;
	InfoLimiter lmt;
    InfoEQ2 eq;
} Info_SD_Limiter;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
} Info_SD_HexaChorus;

typedef struct {
	double level, dry, wet, pre_dly, cho_rate, cho_depth, trm_phase, trm_rate, trm_sep;
	int32 leveli;
	InfoStereoChorus cho;
	InfoTremolo trm;
} Info_SD_TremoloChorus;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
    InfoEQ2 eq;
} Info_SD_SpaceD;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
    InfoEQ2 eq;
} Info_SD_StereoChorus;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoStereoChorus cho;
    InfoEQ2 eq;
} Info_SD_StepFlanger;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	InfoDelayLR dly;
    InfoEQ2 eq;
} Info_SD_StereoDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoModDelay dly;
    InfoEQ2 eq;
} Info_SD_ModDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoMultiTapDelay dly;
    InfoEQ2 eq;
} Info_SD_3TapDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoMultiTapDelay dly;
} Info_SD_4TapDelay;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoDelayShifter dly;
    InfoEQ2 eq;
} Info_SD_TmCtrlDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	double bal1, bal2, panll, panlr, panrl, panrr;
	InfoPitchShifter ps;
} Info_SD_2PitchShifter;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoPitchShifter ps;
    InfoEQ2 eq;
} Info_SD_FbPitchShifter;

typedef struct {
	double rev_level, rev_wet, rev_dry;
	int32 leveli;
//	InfoReverb rvb;
	InfoFreeverb rvb;
	InfoReverbEX rvb2;
	int rev_type;
    InfoEQ2 eq;
} Info_SD_Reverb;

typedef struct {
	double gr_wet, gr_dry, gr_level;
	InfoGateReverb gr;
    InfoEQ2 eq;
} Info_SD_GateReverb;

typedef struct {
	double level, od_panl, od_panr, cho_wet, cho_dry;
	int32 leveli;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoStereoChorus info_cho;
} Info_SD_S_OD_Chorus;

typedef struct {
	double level, od_panl, od_panr, fl_wet, fl_dry;
	int32 leveli;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoStereoChorus info_fl;
} Info_SD_S_OD_Flanger;

typedef struct {
	double level, od_panl, od_panr, dly_wet, dly_dry;
	int32 leveli;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoDelayLR info_dly;
} Info_SD_S_OD_Delay;

typedef struct {
	double level, od_panl, od_panr, cho_wet, cho_dry;
	int32 leveli;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoStereoChorus info_cho;
} Info_SD_S_DS_Chorus;

typedef struct {
	double level, od_panl, od_panr, fl_wet, fl_dry;
	int32 leveli;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoStereoChorus info_fl;
} Info_SD_S_DS_Flanger;

typedef struct {
	double level, od_panl, od_panr, dly_wet, dly_dry;
	int32 leveli;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoDelayLR info_dly;
} Info_SD_S_DS_Delay;

typedef struct {
	double level, eh_wet, eh_dry, cho_wet, cho_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoStereoChorus info_cho;
} Info_SD_S_EH_Chorus;

typedef struct {
	double level, eh_wet, eh_dry, fl_wet, fl_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoStereoChorus info_fl;
} Info_SD_S_EH_Flanger;

typedef struct {
	double level, eh_wet, eh_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoEnhancer info_eh;
	InfoDelayLR info_dly;
} Info_SD_S_EH_Delay;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_SD_S_Cho_Delay;

typedef struct {
	double level, fl_wet, fl_dry, dly_wet, dly_dry;
	int32 leveli;
	InfoStereoChorus info_fl;
	InfoDelayLR info_dly;
} Info_SD_S_FL_Delay;

typedef struct {
	double level, cho_wet, cho_dry, fl_wet, fl_dry;
	int32 leveli;
	InfoStereoChorus info_cho;
	InfoStereoChorus info_fl;
} Info_SD_S_Cho_Flanger;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_SD_P_Cho_Delay;

typedef struct {
	double level, fl_wet, fl_dry, dly_wet, dly_dry;
	InfoStereoChorus info_fl;
	InfoDelayLR info_dly;
} Info_SD_P_FL_Delay;

typedef struct {
	double level, cho_wet, cho_dry, fl_wet, fl_dry;
	InfoStereoChorus info_cho;
	InfoStereoChorus info_fl;
} Info_SD_P_Cho_Flanger;

typedef struct {
	double level, dry, wet;
	int32 leveli;	
	InfoStereoChorus fl;
    InfoEQ2 eq;
} Info_SD_KeysyncFlanger;

typedef struct {
	double level, panl, panr;
	InfoHumanizer hm;
    InfoEQ2 eq;
} Info_SD_FormantFilter;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoRingModulator rm;
	InfoEQ2 eq;
	FilterCoefficients lpf, hpf;
} Info_SD_RingModulator;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	InfoMultiTapDelay dly;
    InfoEQ2 eq;
} Info_SD_MultiTapDelay;

typedef struct {
	double level;
	int32 leveli;
    InfoEQ2 eq;
} Info_SD_ReverseDelay;

typedef struct {
	double level;
	int32 leveli;
    InfoEQ2 eq;
} Info_SD_ShuffleDelay;

typedef struct {
	double level, dry, wet;
	int32 leveli;
//	InfoMultiTapDelay dly;
	Info3DMultiTapDelay dly;
    InfoEQ2 eq;
} Info_SD_3DDelay;

typedef struct {
	double level, wet, dry;
	int32 leveli;
	double bal1, bal2, bal3, pan1l, pan1r, pan2l, pan2r, pan3l, pan3r;
	int init_flg;
	InfoPitchShifter_core ps1, ps2, ps3;
} Info_SD_3VoicePitchShifter;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoLoFi lf;
    InfoEQ2 eq;
} Info_SD_LoFi_Compress;

typedef struct {
	double level, wet, dry, panl, panr;
	int32 leveli;
	InfoLoFi lf;
    InfoEQ2 eq;
} Info_SD_LoFi_Noise;

typedef struct {
	double level;
	int32 leveli;
	InfoAmpSimulator amp;
} Info_SD_SpeakerSimulator;

typedef struct {
	int8 init, mode, env_mode;
	double attack_ms, release_ms, threshold;
//	InfoGate gate;
	InfoNoiseGate ng;
} Info_SD_Gate;

typedef struct {
	double level;
	int32 leveli;
} Info_SD_Slicer;

typedef struct {
	double level;
	int32 leveli;
} Info_SD_Isolator;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	Info3DChorus cho;
    InfoEQ2 eq;
} Info_SD_3DChorus;

typedef struct {
	double level, dry, wet;
	int32 leveli;
	Info3DChorus cho;
    InfoEQ2 eq;
} Info_SD_3DFlanger;

typedef struct {
	double level;
	int32 leveli;
	InfoTremolo trm;
    InfoEQ2 eq;
} Info_SD_Tremolo;

typedef struct {
	double level, panl, panr;
	int32 leveli;
	int rt_sw, eq_sw, od_sw, amp_sw;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoEQ3 info_eq;
	InfoRotary info_rt;
} Info_SD_S_RotaryMulti;

typedef struct {
	double level, rm_wet, rm_dry, pswet, psdry, ph_wet, ph_dry, dly_wet, dly_dry;
	int32 leveli;
	int rm_sw, eq_sw, ps_sw, ph_sw, dly_sw;
	InfoRingModulator info_rm;
	InfoEQ3 info_eq;
	InfoPitchShifter info_ps;
	InfoPhaser info_ph;
	InfoDelayLR info_dly;
} Info_SD_S_KeyboardMulti;

typedef struct {
	double level, eh_wet, eh_dry, ph_wet, ph_dry, cho_wet, cho_dry;
	int32 leveli;
	int eh_sw, ph_sw, cf_sw, trm_sw;
	InfoEnhancer info_eh;
	InfoPhaser info_ph;
	InfoStereoChorus info_cho;
	InfoTremolo info_trm;
} Info_SD_S_RhodesMulti;

struct _EffectList;

typedef struct {
	double level, eh_wet, eh_dry, ph_wet, ph_dry, panl, panr;
	int32 leveli;
	int eh_sw, ph_sw, eq_sw, od_sw;
	InfoOverdrive info_od;
	//InfoDistortion info_od;
	//InfoAmpSimulator info_amp;
	InfoPhaser info_ph;
	InfoSpectrum info_eq;
	InfoEnhancer info_eh;
	void (*efx1)(DATA_T *, int32, struct _EffectList *);
	void (*efx2)(DATA_T *, int32, struct _EffectList *);
	void (*efx3)(DATA_T *, int32, struct _EffectList *);
	void (*efx4)(DATA_T *, int32, struct _EffectList *);
} Info_SD_S_JD_Multi;

typedef struct {
	double level, panl, panr;
//	InfoOverdrive od;
	InfoAmpSimulator amp;
	InfoEQ2 eq; 
} Info_SD_GuitarAmpSimulator;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry, dly_wet, dly_dry, panl, panr;
	int32 leveli;
	int cmp_sw, od_sw, cf_sw, dly_sw, amp_sw;
	InfoCompressor info_cmp;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_SD_S_GuitarMultiA;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry, panl, panr;
	int32 leveli;
	int cmp_sw, od_sw, eq_sw, cf_sw, amp_sw;
	InfoCompressor info_cmp;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
} Info_SD_S_GuitarMultiB;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry, panl, panr;
	int32 leveli;	
	int aw_sw, od_sw, cf_sw, eq_sw, dly_sw, amp_sw;
	InfoAutoWah info_aw;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoEQ2 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_SD_S_GuitarMultiC;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry, dly_wet, dly_dry, panl, panr;
	int32 leveli;
	int cmp_sw, eq_sw, cf_sw, dly_sw;
	InfoCompressor info_cmp;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_SD_S_CleanGuitarMultiA;

typedef struct {
	double level, cho_wet, cho_dry, dly_wet, dly_dry, panl, panr;
	int32 leveli;
	int aw_sw, eq_sw, cf_sw, dly_sw;
	InfoAutoWah info_aw;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
	InfoDelayLR info_dly;
} Info_SD_S_CleanGuitarMultiB;

typedef struct {
	double level, level_cmp, cho_wet, cho_dry, panl, panr;
	int32 leveli;
	int cmp_sw, eq_sw, cf_sw, od_sw, amp_sw;
	InfoCompressor info_cmp;
//	InfoOverdrive info_od;
	InfoDistortion info_od;
	InfoAmpSimulator info_amp;
	InfoEQ3 info_eq;
	InfoStereoChorus info_cho;
} Info_SD_S_BaseMulti;

typedef struct {
	double level;
	int32 leveli;
} Info_SD_Isolator2;

typedef struct {
	double level;
	int32 leveli;
	Info3DAuto locate;
} Info_SD_3DAutoSpin;

typedef struct {
	double level;
	int32 leveli;
	Info3DManual locate;
} Info_SD_3DManual;


/****************** SD test ***************/
typedef struct {
	double level;
	int32 leveli;
	InfoDistortion od;
	InfoAmpSimulator amp;
} Info_SD_Test1;

typedef struct {
	double level;
	int32 leveli;
	InfoDistortion od;
	InfoAmpSimulator amp;
} Info_SD_Test2;







/******************************** SYSTEM EFFECT struct ********************************/

/* GS parameters of reverb effect */
EXTERN struct reverb_status_gs_t
{
	int8 system_mode; // 0:GS 1:GM2 3:SD
	/* GS parameters */
	int8 character, pre_lpf, level, time, delay_feedback, pre_delay_time;	
	///* SD parameters */
	//int8 type;
	//int16 param[20];

	InfoStandardReverb info_standard_reverb;
	InfoPlateReverb info_plate_reverb;
	InfoFreeverb info_freeverb;
	InfoReverbEX info_reverb_ex;
	InfoReverbEX2 info_reverb_ex2;
	InfoDelay3 info_reverb_delay;
	InfoPreFilter lpf;
} reverb_status_gs;

struct chorus_text_gs_t
{
    int status;
    uint8 voice_reserve[18], macro[3], pre_lpf[3], level[3], feed_back[3],
		delay[3], rate[3], depth[3], send_level[3];
};

/* GS parameters of chorus effect */
EXTERN struct chorus_status_gs_t
{
	int8 system_mode; // 0:GS 1:GM2 2:SD
	/* GS/GM2 parameters */
	int8 macro, pre_lpf, level, feedback, delay, rate, depth, send_reverb, send_delay;
	///* SD parameters */
	//int8 type;
	//int16 param[20];

	struct chorus_text_gs_t text;

	InfoStereoChorus info_stereo_chorus;
	InfoPreFilter lpf;
	//InfoDelay3 info_chorus_delay; // for SD
} chorus_status_gs;

/* GS parameters of delay effect */
EXTERN struct delay_status_gs_t
{
	/* GS parameters */
	int8 type, level, level_center, level_left, level_right,
		feedback, pre_lpf, send_reverb, time_c, time_l, time_r;
    //double time_center;			/* in ms */
    //double time_ratio_left, time_ratio_right;		/* in pct */

	/* for pre-calculation */
	//int32 sample[3];	/* center, left, right */
	//double level_ratio[3];	/* center, left, right */
	//double feedback_ratio, send_reverb_ratio;
	
	InfoDelay3 info_delay;
	InfoPreFilter lpf;
} delay_status_gs;


/* GS parameters of channel EQ */
EXTERN struct eq_status_gs_t
{
	/* GS parameters */
    int8 low_freq, high_freq, low_gain, high_gain;
	//filter_shelving hsf, lsf;
	FilterCoefficients hsf, lsf;
} eq_status_gs;


/* GS parameters of insertion effect */
EXTERN struct insertion_effect_gs_t {
	int32 type;
	int8 type_lsb, type_msb, set_param[20], parameter[20], 
		send_reverb, send_chorus, send_delay, send_eq_switch,
		control_source1, control_source2, 
		control_depth1, control_depth2, 
		control_param1, control_param2;
	struct _EffectList *ef;
	DATA_T *efx_buf;
} insertion_effect_gs;

struct effect_parameter_gs_t {
	int8 type_msb, type_lsb;
	char *name;
	int8 param[20];
	int8 control1, control2;
};

EXTERN struct effect_parameter_gs_t effect_parameter_gs[];


/* XG parameters of Multi EQ */
EXTERN struct multi_eq_xg_t
{
	/* XG parameters */
	int8 type, gain1, gain2, gain3, gain4, gain5,
		freq1, freq2, freq3, freq4, freq5,
		q1, q2, q3, q4, q5, shape1, shape5;

	int8 valid, valid1, valid2, valid3, valid4, valid5;
	//filter_shelving eq1s, eq5s;
	//filter_peaking eq1p, eq2p, eq3p, eq4p, eq5p;
	FilterCoefficients eq1, eq2, eq3, eq4, eq5;
} multi_eq_xg;


/* XG parameters of SYS/VAR/INS effect */
struct effect_xg_t {
	int32 type;
	int8 use_msb, type_msb, type_lsb, 
		param_lsb[16], param_msb[10], set_param_lsb[16], set_param_msb[10], control_param,
		ret, pan, send_reverb, send_chorus, connection, part;
	int8 control_depth[7]; // type = 0:mw, 1:bend, 2:cat, 3:ac1, 4:ac2, 5:cbc1, 6:cbc2 (ac1~cbc2 map to cc1~cc4
	FLOAT_T return_level, reverb_level, chorus_level, pan_level[2];
//	FLOAT_T efx_level;
	struct _EffectList *ef;
	DATA_T *efx_buf;
};

#define XG_INSERTION_EFFECT_NUM 4 // variable
#define XG_VARIATION_EFFECT_NUM 1 // fixed

EXTERN struct effect_xg_t insertion_effect_xg[XG_INSERTION_EFFECT_NUM],
	variation_effect_xg[XG_VARIATION_EFFECT_NUM], reverb_status_xg, chorus_status_xg;

struct effect_parameter_xg_t {
	int8 type_msb, type_lsb;
	char *name;
	int8 param_msb[10], param_lsb[16];
	int8 control;
};

EXTERN struct effect_parameter_xg_t effect_parameter_xg[];


/* SD(GM2) parameters of Multi EQ */
EXTERN struct multi_eq_sd_t
{
	/* SD(GM2) parameters */
	int8 sw, gain_ll, gain_lr, gain_hl, gain_hr,
		freq_ll, freq_lr, freq_hl, freq_hr;
	int8 valid;
	FilterCoefficients eq_ll, eq_lr, eq_hl, eq_hr;
} multi_eq_sd;

/* SD parameters of MFX/chorus/reverb effect */
struct mfx_effect_sd_t {
	int8 efx_source, ctrl_channel, ctrl_port;

	int8 common_dry_send, common_send_reverb, common_send_chorus;
	int8 common_type, common_ctrl_source[4], common_ctrl_sens[4], common_ctrl_assign[4];
	int16 common_param[32];
	int8 common_efx_level; // for chorus reverb
	int8 common_output_select; // for chorus

	int8 *type, *ctrl_source, *ctrl_sens, *ctrl_assign;
	int16 *set_param;
	int8 *output_select;

	int16 parameter[32];
	int8 ctrl_param[8];
	
	FLOAT_T dry_level, reverb_level, chorus_level;
	int32 dry_leveli, reverb_leveli, chorus_leveli;
	struct _EffectList *ef;
	DATA_T *efx_buf;
};

#define SD_MFX_EFFECT_NUM 3 // fixed
EXTERN struct mfx_effect_sd_t mfx_effect_sd[SD_MFX_EFFECT_NUM];
EXTERN struct mfx_effect_sd_t reverb_status_sd, chorus_status_sd;

struct effect_parameter_sd_t {
	int8 type;
	char *name;
	int16 param[32];
	int8 control[8]; // 0=<:param num, -1;undefine, -2>=:special control
};

EXTERN struct effect_parameter_sd_t effect_parameter_sd[];


/* GS/XG/SD parameters effect */
struct _EffectEngine {
	int type;
	char *name;
	void (*do_effect)(DATA_T *, int32, struct _EffectList *);
	void (*conv_gs)(struct insertion_effect_gs_t *, struct _EffectList *);
	void (*conv_xg)(struct effect_xg_t *, struct _EffectList *);
	void (*conv_sd)(struct mfx_effect_sd_t *, struct _EffectList *);
	int info_size;
};

typedef struct _EffectList {
	int type;
	void *info;
	struct _EffectEngine *engine;
	struct _EffectList *next_ef;
	DATA_T *efx_buf;
} EffectList;

EXTERN struct _EffectEngine effect_engine[];




/******************************** extern  ********************************/

/* Reverb Effect */
extern void do_ch_reverb(DATA_T *, int32);
extern void do_mono_reverb(DATA_T *, int32);
extern void set_ch_reverb(DATA_T *, int32, int32);
extern void init_ch_reverb(void);
extern void reverb_rc_event(int, int32);
extern void do_ch_reverb_xg(DATA_T *, int32);
extern void do_ch_reverb_sd(DATA_T *, int32);
extern void calc_reverb_level_sd(struct mfx_effect_sd_t *st);

/* Chorus Effect */
extern void do_ch_chorus(DATA_T *, int32);
extern void set_ch_chorus(DATA_T *, int32, int32);
extern void init_ch_chorus(void);
extern void do_ch_chorus_xg(DATA_T *, int32);
extern void do_ch_chorus_sd(DATA_T *, int32);
extern void calc_chorus_level_sd(struct mfx_effect_sd_t *st);

/* Delay (Celeste) Effect */
extern void do_ch_delay(DATA_T *, int32);
extern void set_ch_delay(DATA_T *, int32, int32);
extern void init_ch_delay(void);

/* EQ */
extern void init_eq_gs(void);
extern void set_ch_eq_gs(DATA_T *, int32);
extern void do_ch_eq_gs(DATA_T *, int32);
extern void do_ch_eq_xg(DATA_T *, int32, struct part_eq_xg *); 
extern void do_multi_eq_xg(DATA_T *, int32);
extern void do_multi_eq_sd(DATA_T *, int32);

/* SD MFX Effect */
extern DATA_T* get_mfx_buffer(int mfx_select);
extern void set_ch_mfx_sd(DATA_T *sbuffer, int32 n, int mfx_select, int32 level);
extern void do_mfx_effect_sd(DATA_T *buf, int32 count, int mfx_select);
extern void calc_mfx_send_level_sd(struct mfx_effect_sd_t *st);
extern void init_ch_effect_sd(void);

/* GS INS/VAR Effect */
extern DATA_T* get_insertion_buffer(void);
extern void set_ch_insertion_gs(DATA_T *, int32);
extern void do_insertion_effect_gs(DATA_T*, int32, int);

/* XG INS/VAR Effect */
extern void do_insertion_effect_xg(DATA_T*, int32, struct effect_xg_t *);
extern void do_variation_effect1_xg(DATA_T*, int32);
extern void init_ch_effect_xg(void);
extern void calc_send_return_xg(struct effect_xg_t *st);

/* Filter */
extern void init_pre_lpf(InfoPreFilter *info);

/* ALL Effect */
extern EffectList *push_effect(EffectList *, int);
extern void do_effect_list(DATA_T *, int32, EffectList *);
extern void free_effect_list(EffectList *);

/* effect.c */
extern void free_effect_buffers(void);
extern void init_effect(void);
extern void do_effect(DATA_T *buf, int32 count);

/* effect util */
extern void set_dry_signal_int32(int32 *, int32);
extern void set_dry_signal(DATA_T *, int32);
extern void set_dry_signal_xg(DATA_T *, int32, int32);
extern void mix_dry_signal(DATA_T *, int32);

/* effect control */
extern double voice_filter_reso;
extern double voice_filter_gain;

extern FLOAT_T reverb_predelay_factor;
extern double scalewet;
extern double freeverb_scaleroom;
extern double freeverb_offsetroom;
extern double fixedgain;
extern double combfbk;
extern double time_rt_diff;
extern int numcombs;

extern double ext_reverb_ex_time;
extern double ext_reverb_ex_level;
extern double ext_reverb_ex_er_level;
extern double ext_reverb_ex_rv_level;
extern int ext_reverb_ex_rv_num;
extern int ext_reverb_ex_ap_num;
//extern int ext_reverb_ex_lite;
//extern int ext_reverb_ex_mode;
//extern int ext_reverb_ex_er_num;
//extern int ext_reverb_ex_rv_type;
//extern int ext_reverb_ex_ap_type;
extern int ext_reverb_ex_mod;

extern double ext_reverb_ex2_level;
extern int ext_reverb_ex2_rsmode;
extern int ext_reverb_ex2_fftmode;

extern double ext_plate_reverb_level;
extern double ext_plate_reverb_time;

extern double ext_chorus_level;
extern double ext_chorus_feedback;
extern double ext_chorus_depth;
extern int ext_chorus_ex_phase;
//extern int ext_chorus_ex_lite;
extern int ext_chorus_ex_ov;

extern double ext_filter_shelving_gain;
extern double ext_filter_shelving_reduce;
extern double ext_filter_shelving_q;
extern double ext_filter_peaking_gain;
extern double ext_filter_peaking_reduce;
extern double ext_filter_peaking_q;

//extern double xg_system_return_level;
extern double xg_reverb_return_level;
extern double xg_chorus_return_level;
extern double xg_variation_return_level;
extern double xg_chorus_send_reverb;
extern double xg_variation_send_reverb;
extern double xg_variation_send_chorus;


/* other */
extern float get_pink_noise_light(pink_noise *);
EXTERN pink_noise global_pink_noise_light;


/* timidity effect */
extern int opt_limiter;


/* VST */
extern void do_channel_vst(DATA_T *out_buf, DATA_T **in_buf, int32 nsamples, int ch);




#undef EXTERN

#endif /* ___EFFECT_H_ */
