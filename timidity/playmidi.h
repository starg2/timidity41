/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2018 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    playmidi.h
*/

#ifndef ___PLAYMIDI_H_
#define ___PLAYMIDI_H_

///r
#include "envelope.h"
#include "oscillator.h"
#include "filter.h"
//#include "effect.h"
#include "voice_effect.h"
#include "int_synth.h"
#include "resample.h"

typedef struct {
  int32 time;
  uint8 type, channel, a, b;
} MidiEvent;

#define REVERB_MAX_DELAY_OUT (4 * play_mode->rate) // 4sec ?

#define MIDI_EVENT_NOTE(ep) (ISDRUMCHANNEL((ep)->channel) ? (ep)->a : \
			     (((int)(ep)->a + note_key_offset + \
			       channel[ep->channel].key_shift) & 0x7f))

#define MIDI_EVENT_TIME(ep) ((int32)((ep)->time * midi_time_ratio + 0.5))

#define SYSEX_TAG 0xFF


/* Midi events */
enum midi_event_t
{
	ME_NONE,
	
	/* MIDI events */
	ME_NOTEOFF,
	ME_NOTEON,
	ME_KEYPRESSURE,
	ME_PROGRAM,
	ME_CHANNEL_PRESSURE,
	ME_PITCHWHEEL,
	
	/* Controls */
	/* begin cc*/
	ME_TONE_BANK_MSB,
	ME_TONE_BANK_LSB,
	ME_MODULATION_WHEEL,
	ME_BREATH,
	ME_FOOT,
	ME_MAINVOLUME,
	ME_BALANCE,
	ME_PAN,
	ME_EXPRESSION,
	ME_SUSTAIN,
	ME_PORTAMENTO_TIME_MSB,
	ME_PORTAMENTO_TIME_LSB,
	ME_PORTAMENTO,
	ME_PORTAMENTO_CONTROL,
	ME_DATA_ENTRY_MSB,
	ME_DATA_ENTRY_LSB,
	ME_SOSTENUTO,
	ME_SOFT_PEDAL,
	ME_LEGATO_FOOTSWITCH,
	ME_HOLD2,
	ME_HARMONIC_CONTENT,
	ME_RELEASE_TIME,
	ME_DECAY_TIME,
	ME_ATTACK_TIME,
	ME_BRIGHTNESS,
	ME_VIBRATO_RATE,
	ME_VIBRATO_DEPTH,
	ME_VIBRATO_DELAY,
	ME_REVERB_EFFECT,
	ME_TREMOLO_EFFECT,
	ME_CHORUS_EFFECT,
	ME_CELESTE_EFFECT,
	ME_PHASER_EFFECT,
	ME_RPN_INC,
	ME_RPN_DEC,
	ME_NRPN_LSB,
	ME_NRPN_MSB,
	ME_RPN_LSB,
	ME_RPN_MSB,
	ME_LOOP_START,
	ME_ALL_SOUNDS_OFF,
	ME_RESET_CONTROLLERS,
	ME_ALL_NOTES_OFF,
	ME_MONO,
	ME_POLY,
	ME_UNDEF_CTRL_CHNG,
	/* end cc*/
	
	/* TiMidity Extensionals */
#if 0
	ME_VOLUME_ONOFF,		/* Not supported */
#endif
	ME_MASTER_TUNING,		/* Master tuning */
	ME_SCALE_TUNING,		/* Scale tuning */
	ME_BULK_TUNING_DUMP,	/* Bulk tuning dump */
	ME_SINGLE_NOTE_TUNING,	/* Single-note tuning */
	ME_RANDOM_PAN,
	ME_SET_PATCH,			/* Install special instrument */
	ME_DRUMPART,
	ME_KEYSHIFT,
	ME_PATCH_OFFS,			/* Change special instrument sample position
							 * Channel, LSB, MSB
							 */
	
	/* Global channel events */
	ME_TEMPO,
	ME_CHORUS_TEXT,
	ME_LYRIC,
	ME_GSLCD,				/* GS L.C.D. Exclusive message event */
	ME_MARKER,
	ME_INSERT_TEXT,			/* for SC */
	ME_TEXT,
	ME_KARAOKE_LYRIC,		/* for KAR format */
	ME_MASTER_VOLUME,
	ME_RESET,				/* Reset and change system mode */
	ME_NOTE_STEP,
	ME_CUEPOINT,			/* skip to time segment */
	
	ME_TIMESIG,				/* Time signature */
	ME_KEYSIG,				/* Key signature */
	ME_TEMPER_KEYSIG,		/* Temperament key signature */
	ME_TEMPER_TYPE,			/* Temperament type */
	ME_MASTER_TEMPER_TYPE,	/* Master temperament type */
	ME_USER_TEMPER_ENTRY,	/* User-defined temperament entry */
	
	ME_SYSEX_LSB,			/* Universal system exclusive message (LSB) */
	ME_SYSEX_MSB,			/* Universal system exclusive message (MSB) */
	ME_SYSEX_GS_LSB,		/* GS system exclusive message (LSB) */
	ME_SYSEX_GS_MSB,		/* GS system exclusive message (MSB) */
	ME_SYSEX_XG_LSB,		/* XG system exclusive message (LSB) */
	ME_SYSEX_XG_MSB,		/* XG system exclusive message (MSB) */
	ME_SYSEX_SD_LSB,		/* SD system exclusive message (LSB) */
	ME_SYSEX_SD_MSB,		/* SD system exclusive message (MSB) */
	ME_SYSEX_SD_HSB,		/* SD system exclusive message (HSB) */
	
	ME_WRD,					/* for MIMPI WRD tracer */
	ME_SHERRY,				/* for Sherry WRD tracer */
	ME_BARMARKER,
	ME_STEP,				/* for Metronome */
	
	ME_LAST = 254,			/* Last sequence of MIDI list.
							 * This event is reserved for realtime player.
							 */
	ME_EOT = 255			/* End of MIDI.  Finish to play */
};

#define GLOBAL_CHANNEL_EVENT_TYPE(type)	\
	((type) == ME_NONE || (type) >= ME_TEMPO)

enum rpn_data_address_t /* NRPN/RPN */
{
    NRPN_ADDR_0108,
    NRPN_ADDR_0109,
    NRPN_ADDR_010A,
    NRPN_ADDR_0120,
    NRPN_ADDR_0121,
    NRPN_ADDR_0124, // hpf
    NRPN_ADDR_0125, // hpf
	NRPN_ADDR_0130,
	NRPN_ADDR_0131,
	NRPN_ADDR_0132,
	NRPN_ADDR_0133,
	NRPN_ADDR_0134,
	NRPN_ADDR_0135,
	NRPN_ADDR_0136,
	NRPN_ADDR_0137,
	NRPN_ADDR_0138,
	NRPN_ADDR_0139,
	NRPN_ADDR_013A,
	NRPN_ADDR_013B,
	NRPN_ADDR_013C,
	NRPN_ADDR_013D,
    NRPN_ADDR_0163,
    NRPN_ADDR_0164,
    NRPN_ADDR_0166,
    NRPN_ADDR_1400,
    NRPN_ADDR_1500,
    NRPN_ADDR_1600,
    NRPN_ADDR_1700,
    NRPN_ADDR_1800,
    NRPN_ADDR_1900,
    NRPN_ADDR_1A00,
    NRPN_ADDR_1C00,
    NRPN_ADDR_1D00,
    NRPN_ADDR_1E00,
    NRPN_ADDR_1F00,
    NRPN_ADDR_2400, // hpf
    NRPN_ADDR_2500, // hpf
    NRPN_ADDR_3000,
    NRPN_ADDR_3100,
    NRPN_ADDR_3200,
    NRPN_ADDR_3300,
    NRPN_ADDR_3400,
    NRPN_ADDR_3500,
    NRPN_ADDR_3600,
    NRPN_ADDR_3700,
    NRPN_ADDR_3800,
    NRPN_ADDR_3900,
    NRPN_ADDR_3A00,
    NRPN_ADDR_3B00,
    NRPN_ADDR_3C00,
    NRPN_ADDR_3D00,
    RPN_ADDR_0000,
    RPN_ADDR_0001,
    RPN_ADDR_0002,
    RPN_ADDR_0003,
    RPN_ADDR_0004,
	RPN_ADDR_0005,
	RPN_ADDR_0040, // bend pitch low control 
    RPN_ADDR_7F7F,
    RPN_ADDR_FFFF,
    RPN_MAX_DATA_ADDR
};

#define RX_PITCH_BEND (1<<0)
#define RX_CH_PRESSURE (1<<1)
#define RX_PROGRAM_CHANGE (1<<2)
#define RX_CONTROL_CHANGE (1<<3)
#define RX_POLY_PRESSURE (1<<4)
#define RX_NOTE_MESSAGE (1<<5)
#define RX_RPN (1<<6)
#define RX_NRPN (1<<7)
#define RX_MODULATION (1<<8)
#define RX_VOLUME (1<<9)
#define RX_PANPOT (1<<10)
#define RX_EXPRESSION (1<<11)
#define RX_HOLD1 (1<<12)
#define RX_PORTAMENTO (1<<13)
#define RX_SOSTENUTO (1<<14)
#define RX_SOFT (1<<15)
#define RX_NOTE_ON (1<<16)
#define RX_NOTE_OFF (1<<17)
#define RX_BANK_SELECT (1<<18)
#define RX_BANK_SELECT_LSB (1<<19)

enum {
	EG_ATTACK = 0,
	EG_DECAY = 2,
	EG_DECAY1 = 1,
	EG_DECAY2 = 2,
	EG_RELEASE = 3,
	EG_NULL = 5,
	EG_GUS_ATTACK = 0,
	EG_GUS_DECAY = 1,
	EG_GUS_SUSTAIN = 2,
	EG_GUS_RELEASE1 = 3,
	EG_GUS_RELEASE2 = 4,
	EG_GUS_RELEASE3 = 5,
	EG_SF_ATTACK = 0,
	EG_SF_HOLD = 1,
	EG_SF_DECAY = 2,
	EG_SF_RELEASE = 3,
};

//#ifndef PART_EQ_XG
//#define PART_EQ_XG
///*! shelving filter */
//typedef struct {
//	double freq, gain, q;
//	int32 x1l, x2l, y1l, y2l, x1r, x2r, y1r, y2r;
//	int32 a1, a2, b0, b1, b2;
//} filter_shelving;
//
///*! Part EQ (XG) */
//struct part_eq_xg {
//	int8 bass, treble, bass_freq, treble_freq;
//	filter_shelving basss, trebles;
//	int8 valid;
//};
//#endif /* PART_EQ_XG */

typedef struct {
	int8 mode; // 0:normal 1:center_based
	int8 num; // control cc number for cc1~cc4
	int16 val;
	int16 pitch;	/* in +-semitones [-24, 24] */
	float cutoff;	/* in +-cents [-9600, 9600] */
	float amp;	/* [-1.0, 1.0] */
	/* in GS, LFO1 means LFO for voice 1, LFO2 means LFO for voice2.
		LFO2 is not supported. */
	float lfo1_rate, lfo2_rate;	/* in +-Hz [-10.0, 10.0] */
	float lfo1_pitch_depth, lfo2_pitch_depth;	/* in cents [0, 600] */
	float lfo1_tvf_depth, lfo2_tvf_depth;	/* in cents [0, 2400] */
	float lfo1_tva_depth, lfo2_tva_depth;	/* [0, 1.0] */
} midi_controller;

///r
struct part_eq_xg {
	int8 bass, treble, bass_freq, treble_freq;
	int8 mid_bass, mid_treble, mid_bass_freq, mid_treble_freq;
	int8 bass_q, treble_q, mid_bass_q, mid_treble_q;
	int8 bass_shape, treble_shape;
	FilterCoefficients basss, trebles, mid_basss, mid_trebles;
	int8 valid;
};

struct DrumPartEffect
{
	DATA_T *buf, *buf_ptr[4]; // buf2 for thread
	int8 note, reverb_send, chorus_send, delay_send;
	struct part_eq_xg *eq_xg;
};

struct DrumParts
{
    int8 drum_panning;
    int32 drum_envelope_rate[6]; /* drum instrument envelope */
    int8 pan_random;    /* flag for drum random pan */
	float drum_level;

	int8 chorus_level, reverb_level, delay_level, coarse, fine,
		play_note, drum_cutoff_freq, drum_resonance;
	int32 rx;
	int8 rx_note_off;
///r
	int8 drum_hpf_cutoff_freq, drum_hpf_resonance;
	int8 drum_velo_pitch_sens, drum_velo_cutoff_sens;
	struct part_eq_xg eq_xg;
};

typedef struct {
  int8	bank_msb, bank_lsb, bank, program, volume,
	expression, sustain, panning, mono,
	key_shift, loop_timeout;
  
  int8 program_flag; // check program change for recompute_bank_parameter_tone

  /* chorus, reverb... Coming soon to a 300-MHz, eight-way superscalar
     processor near you */
  int8 chorus_level;	/* Chorus level */
  int8 reverb_level;	/* Reverb level */
  int8 delay_level;		/* Delay Send Level */
  int8 eq_gs;	/* EQ ON/OFF (GS) */
  int8 insertion_effect;
  
  int8 sd_output_assign, sd_output_mfx_select, sd_dry_send_level;
   int8 mfx_part_dry_send, mfx_part_send_reverb, mfx_part_send_chorus, 
		mfx_part_ctrl_source[4], mfx_part_ctrl_sens[4], mfx_part_ctrl_assign[4];
  int8 mfx_part_type;
  int16 mfx_part_param[32];
  int8 chorus_part_type;
  int16 chorus_part_param[32];
  int8 chorus_part_efx_level, chorus_part_send_reverb;
  int8 chorus_part_output_select;
  int8 reverb_part_type;
  int16 reverb_part_param[32];
  int8 reverb_part_efx_level;
  int8 gm2_inst;

  /* Special sample ID. (0 means Normal sample) */
  uint8 special_sample;

  int pitchbend;
  
  /* For portamento */
  int8	portamento, portamento_control;
  uint8 portamento_time_msb, portamento_time_lsb;
  int porta_status;
  int32 porta_last_note_fine;
  FLOAT_T porta_dpb; // share portamento & legato
  int32 porta_pb, porta_next_pb, porta_note_fine; // share portamento & legato
  int8 legato;	/* legato footswitch */
  int legato_status;
  int8 legato_flag;	/* note-on flag for legato */  
  uint8 legato_note, legato_last_note, legato_velo, legato_hist[128];
  int32 legato_note_time;

  /* For Drum part */
  struct DrumParts *drums[128];

  /* For NRPN Vibrato */
  int8 param_vibrato_delay, param_vibrato_depth, param_vibrato_rate;
  int32 vibrato_delay;

  /* For RPN */
  uint8 rpnmap[RPN_MAX_DATA_ADDR]; /* pseudo RPN address map */
  uint8 rpnmap_lsb[RPN_MAX_DATA_ADDR];
  uint8 lastlrpn, lastmrpn;
  int8  nrpn; /* 0:RPN, 1:NRPN, -1:Undefined */
  int rpn_7f7f_flag;		/* Boolean flag used for RPN 7F/7F */

  /* For channel envelope */
  int32 envelope_rate[6]; /* for Envelope Generator in mix.c
			   * 0: value for attack rate
			   * 2: value for decay rate
			   * 3: value for release rate
			   */

  int mapID;			/* Program map ID */
  AlternateAssign *altassign;	/* Alternate assign patch table */
  int32 lasttime;     /* Last sample time of computed voice on this channel */

  /* flag for random pan */
  int pan_random;

///r
  /* for lfo rate */  
  FLOAT_T lfo_rate[2];
  FLOAT_T lfo_amp_depth[2];
  FLOAT_T lfo_cutoff_depth[2];
  FLOAT_T lfo_pitch_depth[2];
  
  /* for Voice pitch */  
  int32 pitch;
   int32 *freq_table;

  /* for Voice amp (exclude pan) */  
  FLOAT_T amp;

  /* for XG channel mixer (include pan) */  
  FLOAT_T ch_amp[2]; // 0:left 1:right
  Envelope2 vol_env, mix_env;

  /* for Voice LPF/HPF / Resonance */
  int8 param_resonance, param_cutoff_freq;	/* -64 ~ 63 */
  int8 hpf_param_resonance, hpf_param_cutoff_freq;	/* -64 ~ 63 */
  FLOAT_T modenv_depth;
  FLOAT_T cutoff_cent, resonance_dB;
  FLOAT_T hpf_cutoff_cent, hpf_resonance_dB;

  /* for XG pitch envelope */
  int8 pit_env_level[5], pit_env_time[4];

  int8 velocity_sense_depth, velocity_sense_offset;
  
  int8 scale_tuning[12], prev_scale_tuning;
  int8 temper_type;

  int8 soft_pedal;
  int8 sostenuto;
  int8 damper_mode;

  int8 tone_map0_number;
  int8 assign_mode;
  
  midi_controller mod, bend, caf, paf, cc1, cc2, cc3, cc4;

  ChannelBitMask channel_layer;
  int port_select;

  struct part_eq_xg eq_xg;

  int8 dry_level;
  int8 note_limit_high, note_limit_low;	/* Note Limit (Keyboard Range) */
  int8 vel_limit_high, vel_limit_low;	/* Velocity Limit */
  int32 rx;	/* Rx. ~ (Rcv ~) */

  int drum_effect_num;
  int8 drum_effect_flag;
  struct DrumPartEffect *drum_effect;

  int8 sysex_gs_msb_addr, sysex_gs_msb_val,
		sysex_xg_msb_addr, sysex_xg_msb_val,
		sysex_sd_msb_addr, sysex_sd_msb_val, sysex_sd_hsb_addr, sysex_sd_hsb_val,
		sysex_msb_addr, sysex_msb_val;
  /* for GS Pitch Offset Fine */
  FLOAT_T pitch_offset_fine;	/* in Hz */
  /* for XG Detune */
  uint8 detune_param;
  FLOAT_T detune; /* in Hz */

} Channel;

/* Causes the instrument's default panning to be used. */
#define NO_PANNING -1


///r
typedef struct {
  /* for voice status */
  int vid;
  uint8 status;
  uint8 channel, note, velocity;
  int temper_instant;
  int8 sostenuto;  
  int8 paf_ctrl;
  int32 orig_frequency, frequency;
  Sample *sample;
    
  /* for pan */
  int8 panned;
  int8 pan_ctrl;
  FLOAT_T pan_amp[2];
  
  /* for voice control */
  int32 overlap_count; 
  int32 channel_voice_count; 
  int8 init_voice, update_voice, finish_voice;
  
  /* for resample (move resample.h)*/
  splen_t reserve_offset;
  resample_rec_t resrc;
  
  /* for pitch control */
  int32 init_tuning;
  int32 prev_tuning;
  FLOAT_T pitchfactor, orig_pitchfactor; /* precomputed pitch bend factor to save some fdiv's */

  /* for portamento */
  int porta_status;
  FLOAT_T porta_dpb;
  int32 porta_pb, porta_next_pb, porta_out, porta_note_fine;
  
  /* for cache */
  struct cache_hash *cache;

  /* for envelope */
  int delay; /* Note ON delay samples */
  int32 mod_update_count;
  int add_delay_cnt;
  Envelope0 amp_env, mod_env;
  Envelope4 pit_env;
  
  /* for amp/mix  */
  FLOAT_T init_amp;
  final_volume_t left_mix, right_mix;
  FLOAT_T left_amp, right_amp;
  Envelope2 vol_env, mix_env;  
  
  /* for lfo */
  FLOAT_T tremolo_volume;
  FLOAT_T lfo_rate[2];
  FLOAT_T lfo_amp_depth[2];
  Oscillator lfo1, lfo2;
  
  /* for voive filter */
  int init_lpf_val;
  FLOAT_T init_lpf_cent, init_lpf_reso;
  int init_hpf_val;
  FLOAT_T init_hpf_cent, init_hpf_reso;
  FLOAT_T lpf_gain;
  FLOAT_T lpf_orig_freq, lpf_orig_reso;
  FLOAT_T hpf_orig_freq, hpf_orig_reso;
  FLOAT_T lpf_freq_low_limit;
  FilterCoefficients fc;
  FilterCoefficients fc2; // hpf XG ext filter
  FLOAT_T rs_sample_rate_root;
  FilterCoefficients rf_fc; // resample_filter
  
  /* for voive effect */
#ifdef VOICE_EFFECT
  int vfxe_num;
  VoiceEffect *vfx[VOICE_EFFECT_NUM]; // Voive Effect
#endif
  
  /* for int synth */
#ifdef INT_SYNTH
  InfoIS_SCC *scc;
  InfoIS_MMS *mms;
#endif
} Voice;

/* Voice status options: */
#define VOICE_FREE	(1<<0)
#define VOICE_ON	(1<<1)
#define VOICE_SUSTAINED	(1<<2)
#define VOICE_OFF	(1<<3)
#define VOICE_DIE	(1<<4)

/* Voice panned options: */
#define PANNED_MYSTERY 0
#define PANNED_LEFT 1
#define PANNED_RIGHT 2
#define PANNED_CENTER 3
/* Anything but PANNED_MYSTERY only uses the left volume */

/* update_voice options: */
#define UPDATE_VOICE_FINISH_NOTE (1<<0)
#define UPDATE_VOICE_CUT_NOTE	(1<<1)
#define UPDATE_VOICE_CUT_NOTE2	(1<<2)
#define UPDATE_VOICE_KILL_NOTE	(1<<3)

#define ISDRUMCHANNEL(c)  IS_SET_CHANNELMASK(drumchannels, c)

extern Channel channel[];
extern Voice *voice;

/* --module */
extern int opt_default_module;
extern int opt_preserve_silence;

///r
enum { // module_id
	MODULE_TIMIDITY_DEFAULT = 0x0,
	/* GS modules */ // 0x
	MODULE_SC55 = 0x1,
	MODULE_SC88 = 0x2,
	MODULE_SC88PRO = 0x3,
	MODULE_SC8850 = 0x4,
	/* GM2 modules */
	MODULE_SD20 = 0x60,
	MODULE_SD80 = 0x61,
	MODULE_SD90 = 0x62,
	MODULE_SD50 = 0x63,
	/* LA PCM modules */ //38~
	MODULE_MT32 = 0x38,
	MODULE_CM32L = 0x39,	
	MODULE_CM32P = 0x3a,
	MODULE_CM64 = 0x3b,
	MODULE_CM500A = 0x3c,
	MODULE_CM500D = 0x3d,
	MODULE_CM64_SN01 = 0x41,
	MODULE_CM64_SN02 = 0x42,
	MODULE_CM64_SN03 = 0x43,
	MODULE_CM64_SN04 = 0x44,
	MODULE_CM64_SN05 = 0x45,
	MODULE_CM64_SN06 = 0x46,
	MODULE_CM64_SN07 = 0x47,
	MODULE_CM64_SN08 = 0x48,
	MODULE_CM64_SN09 = 0x49,
	MODULE_CM64_SN10 = 0x4a,
	MODULE_CM64_SN11 = 0x4b,
	MODULE_CM64_SN12 = 0x4c,
	MODULE_CM64_SN13 = 0x4d,
	MODULE_CM64_SN14 = 0x4e,
	MODULE_CM64_SN15 = 0x4f,
	/* XG modules */ // 1x
	MODULE_MU50 = 0x10,
	MODULE_MU80 = 0x11,
	MODULE_MU90 = 0x12,
	MODULE_MU100 = 0x13,
	MODULE_MU128 = 0x14,
	MODULE_MU500 = 0x15,
	MODULE_MU1000 = 0x16,
	MODULE_MU2000 = 0x17,
	/* KORG modules */
	MODULE_AG10 = 0x50,
	MODULE_05RW = 0x52,
	MODULE_NX5R = 0x54,
	/* GM modules */ // 2x
	MODULE_SBLIVE = 0x20,
	MODULE_SBAUDIGY = 0x21,
	/* Special modules */
	MODULE_TIMIDITY_SPECIAL1 = 0x70,
	MODULE_TIMIDITY_DEBUG = 0x7f,
};

struct _ModuleList {
	int num;
	const char *name;
};
static struct _ModuleList module_list[] = {
	{MODULE_TIMIDITY_DEFAULT,	"TiMidity++ Default"},
	{MODULE_SC55,				"SC-55 CM-300 CM-500C"},
	{MODULE_SC88,				"SC-88"},
	{MODULE_SC88PRO,				"SC-88Pro"},
	{MODULE_SC8850,				"SC-8850 SC-8820 SC-D70"},
	{MODULE_SD20,				"SD-20"},
	{MODULE_SD80,				"SD-80"},
	{MODULE_SD90,				"SD-90"},
	{MODULE_SD50,				"SD-50"},
	{MODULE_MU50,				"MU-50"},
	{MODULE_MU80,				"MU-80"},
	{MODULE_MU90,				"MU-90"},
	{MODULE_MU100,				"MU-100"},
	{MODULE_MU128,				"MU-128"},
	{MODULE_MU500,				"MU-500"},
	{MODULE_MU1000,				"MU-1000"},
	{MODULE_MU2000,				"MU-2000"},
	{MODULE_AG10,				"AG-10"},
	{MODULE_05RW,				"05R/W"},
	{MODULE_NX5R,				"NX5R NS5R"},
	{MODULE_MT32,				"MT-32"},
	{MODULE_CM32L,				"CM-32L"},
	{MODULE_CM32P,				"CM-32P"},
	{MODULE_CM64,				"CM-64 (CM32L+CM32P)"},
	{MODULE_CM500D,				"CM-500 mode D"},
	{MODULE_CM64_SN01,			"CM64+SN01 P.Orgn+Harpsi"},
	{MODULE_CM64_SN02,			"CM64+SN02 Latin+FXPerc"},
	{MODULE_CM64_SN03,			"CM64+SN03 Ethic"},
	{MODULE_CM64_SN04,			"CM64+SN04 E.Grnd+Clavi"},
	{MODULE_CM64_SN05,			"CM64+SN05 Orch Strings"},
	{MODULE_CM64_SN06,			"CM64+SN06 Orch Winds"},
	{MODULE_CM64_SN07,			"CM64+SN07 Elec Guitar"},
	{MODULE_CM64_SN08,			"CM64+SN08 Synthesizer"},
	{MODULE_CM64_SN09,			"CM64+SN09 Guitar+KB"},
	{MODULE_CM64_SN10,			"CM64+SN10 Rock Drums"},
	{MODULE_CM64_SN11,			"CM64+SN11 Sound Effect"},
	{MODULE_CM64_SN12,			"CM64+SN12 Sax+Trombone"},
	{MODULE_CM64_SN13,			"CM64+SN13 Super Strings"},
	{MODULE_CM64_SN14,			"CM64+SN14 Super AcGuitar"},
	{MODULE_CM64_SN15,			"CM64+SN15 Super Brass"},
	{MODULE_SBLIVE,				"Sound Blaster Live!"},
	{MODULE_SBAUDIGY,			"Sound Blaster Audigy"},
	{MODULE_TIMIDITY_SPECIAL1,	"TiMidity++ Special 1"},
	{MODULE_TIMIDITY_DEBUG,		"TiMidity++ Debug"},
};
#define module_list_num (sizeof(module_list) / sizeof(struct _ModuleList))


static inline int get_module(void) {return opt_default_module;}
///r
static inline int is_gs_module(void)
{
	int module = get_module();
    return (MODULE_SC55 <= module && module <= MODULE_SC8850);
}

static inline int is_cm_module(void)
{
	int module = get_module();
    return (MODULE_MT32 <= module && module <= MODULE_CM64_SN15);
}

static inline int is_xg_module(void)
{
	int module = get_module();
    return (MODULE_MU50 <= module && module <= MODULE_MU2000);
}

static inline int is_sd_module(void)
{
	int module = get_module();
//    return (module == MODULE_SDMODE);
    return (MODULE_SD20 <= module && module <= MODULE_SD90);
}

static inline int is_gm2_module(void)
{
	int module = get_module();
//    return (module == MODULE_SDMODE);
    return (MODULE_SD20 <= module && module <= MODULE_SD90);
}

static inline int is_kg_module(void)
{
	int module = get_module();
    return (MODULE_AG10 <= module && module <= MODULE_NX5R);
}
///r
extern int32 control_ratio, amp_with_poly, amplification, output_amplification;
extern double div_control_ratio;
extern ChannelBitMask default_drumchannel_mask;
extern ChannelBitMask drumchannel_mask;
extern ChannelBitMask default_drumchannels;
extern ChannelBitMask drumchannels;



extern int adjust_panning_immediately;
extern int max_voices;
extern int voices, upper_voices;
extern int note_key_offset;
extern FLOAT_T midi_time_ratio;
extern int opt_modulation_wheel;
extern int opt_portamento;
extern int opt_nrpn_vibrato;
extern int opt_reverb_control;
extern int opt_chorus_control;
extern int opt_surround_chorus;
extern int opt_channel_pressure;
extern int opt_lpf_def;
extern int opt_hpf_def;
extern int opt_overlap_voice_allow;
///r
extern int opt_overlap_voice_count;
extern int opt_max_channel_voices;
extern int opt_temper_control;
extern int opt_tva_attack;
extern int opt_tva_decay;
extern int opt_tva_release;
extern int opt_delay_control;
extern int opt_eq_control;
extern int opt_insertion_effect;
extern int opt_drum_effect;
extern int opt_env_attack;
extern int opt_modulation_envelope;
extern int noise_sharp_type;
extern int32 current_play_tempo;
extern int opt_realtime_playing;
extern int volatile stream_max_compute;
extern uint32 opt_rtsyn_latency;
extern int opt_rtsyn_skip_aq;

extern int reduce_voice_threshold; /* msec */
///r
extern int reduce_quality_threshold;
extern int reduce_polyphony_threshold;

extern int check_eot_flag;
extern int special_tonebank;
extern int default_tonebank;
extern int playmidi_seek_flag;
extern int auto_reduce_polyphony;
extern int play_pause_flag;
extern int reduce_quality_flag;
///r
extern int reduce_voice_flag;
extern int reduce_polyphony_flag;
extern int no_4point_interpolation;
extern int add_play_time;
extern int add_silent_time;
extern int emu_delay_time;

extern int compute_buffer_bits;
extern int compute_buffer_size;
extern int synth_buffer_size;
extern int opt_mix_envelope;
extern int mix_env_mask;
extern int opt_modulation_update;
extern int ramp_out_count;
extern int opt_cut_short_time;
extern double voice_filter_reso;
extern double voice_filter_gain;

extern double gs_env_attack_calc;
extern double gs_env_decay_calc;
extern double gs_env_release_calc;
extern double xg_env_attack_calc;
extern double xg_env_decay_calc;
extern double xg_env_release_calc;
extern double gm2_env_attack_calc;
extern double gm2_env_decay_calc;
extern double gm2_env_release_calc;
extern double gm_env_attack_calc;
extern double gm_env_decay_calc;
extern double gm_env_release_calc;
extern double env_attack_calc;
extern double env_decay_calc;
extern double env_release_calc;
extern double env_add_offdelay_time;

/* time (ms) for full vol note to sustain */
extern int min_sustain_time; // ms
extern int min_sustain_count;

extern ChannelBitMask channel_mute;
extern int temper_type_mute;
extern int8 current_keysig;
extern int8 current_temper_keysig;
extern int temper_adj;
extern int8 opt_init_keysig;
extern int8 opt_force_keysig;
extern int key_adjust;
extern FLOAT_T tempo_adjust;
extern int opt_pure_intonation;
extern int current_freq_table;
extern int32 opt_drum_power;
extern int opt_amp_compensation;
extern int opt_realtime_priority;	/* interface/alsaseq_c.c */
extern int opt_sequencer_ports;		/* interface/alsaseq_c.c */
///r
extern double opt_user_volume_curve;

extern int play_midi_file(char *fn);
extern int dumb_pass_playing_list(int number_of_files, char *list_of_files[]);
extern void default_ctl_lyric(int lyricid);
extern int check_apply_control(void);
extern void recompute_freq(int v);
extern int midi_drumpart_change(int ch, int isdrum);
///r
//extern void ctl_note_event(int noteID);
extern void ctl_mode_event(int type, int trace, ptr_size_t arg1, ptr_size_t arg2);
extern char *channel_instrum_name(int ch);
extern int get_reverb_level(int ch);
extern int get_chorus_level(int ch);
extern void playmidi_output_changed(int play_state);
extern Instrument *play_midi_load_instrument(int dr, int bk, int prog, int elm, int *elm_max);
extern void midi_program_change(int ch, int prog);
extern void free_voice(int v);
//extern void free_reverb_buffer(void);
extern void play_midi_setup_drums(int ch,int note);

/* For stream player */
extern void playmidi_stream_init(void);
extern void playmidi_tmr_reset(void);


extern int play_event(MidiEvent *ev);

extern void recompute_voice_lfo(int);
extern void recompute_voice_amp(int);
extern void recompute_voice_filter(int);
extern void recompute_resample_filter(int v);
extern void recompute_voice_pitch(int);
///r
extern int32 get_note_freq(Sample *, int);

extern void free_drum_effect(int);

///r
extern void playmidi_stream_free(void);
extern void init_playmidi(void);
extern void free_playmidi(void);
extern int32 get_current_play_tempo(void);
extern void init_voice(int i);
extern void update_voice(int i);

extern int calc_bend_val(int val);
extern void kill_all_voices(void);


#ifdef VOICE_EFFECT
extern int cfg_flg_vfx;
#endif
#ifdef INT_SYNTH
extern int cfg_flg_int_synth_scc;
extern int cfg_flg_int_synth_mms;
#endif
extern void free_voice_pointer(void);
extern void init_voice_pointer(void);

#endif /* ___PLAYMIDI_H_ */
