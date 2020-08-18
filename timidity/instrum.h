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

   instrum.h

   */

#ifndef ___INSTRUM_H_
#define ___INSTRUM_H_


///r
#include "voice_effect.h"

#define HPF_PARAM_NUM 3

///r
typedef struct _Sample {
  splen_t offset, loop_start, loop_end, data_length;
  int32 sample_rate, root_freq; /* root_freqはroot_key+tuneの機能 */
  int8 low_key, high_key, root_key; /* root_keyは表示用 */
  FLOAT_T tune;
  int8 note_to_use;
  FLOAT_T volume, cfg_amp;
  sample_t *data;
  int data_type;
  uint32 modes;
  uint8 data_alloced, low_vel, high_vel;
  int32 cutoff_freq, cutoff_low_limit, cutoff_low_keyf;	/* in Hz, [1, 20000] */
  int16 resonance;	/* in centibels, [0, 960] */
  int16 vel_to_fc, key_to_fc; /* in cents, [-12000, 12000] */
  int16 vel_to_resonance;	/* in centibels, [-960, 960] */
  // env
  int32 envelope_rate[6], envelope_offset[6],
	modenv_rate[6], modenv_offset[6];
  int32 envelope_delay, modenv_delay;	/* in samples */
  int16 modenv_to_pitch, modenv_to_fc,
	  envelope_keyf[6], envelope_velf[6], modenv_keyf[6], modenv_velf[6];
  int8 envelope_velf_bpo, modenv_velf_bpo, envelope_keyf_bpo, modenv_keyf_bpo,
	  key_to_fc_bpo, vel_to_fc_threshold;	/* in notes */
  int32 pitch_envelope[9];
  // lfo
  int16 tremolo_delay, tremolo_sweep, tremolo_freq;
  int16 vibrato_delay, vibrato_sweep, vibrato_freq;
  int16 tremolo_to_amp, tremolo_to_pitch, tremolo_to_fc;
  int16 vibrato_to_amp, vibrato_to_pitch, vibrato_to_fc;
  // other
  int16 scale_freq;	/* in notes */
  int16 scale_factor;	/* in 1024divs/key */
  int8 inst_type;
  int32 sf_sample_index, sf_sample_link;	/* for stereo SoundFont */
  uint16 sample_type;	/* 1 = Mono, 2 = Right, 4 = Left, 8 = Linked, $8000 = ROM */
  FLOAT_T root_freq_detected;	/* root freq from pitch detection */
  int transpose_detected;	/* note offset from detected root */
  int chord;			/* type of chord for detected pitch */
  
  int lpf_type;
  int32 root_freq_org, sample_rate_org;
  int hpf[HPF_PARAM_NUM];
  int vfxe_num;
  int vfx[VOICE_EFFECT_NUM][VOICE_EFFECT_PARAM_NUM];
  int8 keep_voice;
  int8 def_pan;
  FLOAT_T sample_pan;

  int32 seq_length;	/* length of the round robin, 0 == disabled */
  int32 seq_position;	/* 1-based position within the round robin, 0 == disabled */
  FLOAT_T lorand;
  FLOAT_T hirand;
} Sample;

///r
// data_type = resampler_data_type
enum {
	SAMPLE_TYPE_INT16 = 0, // def sample_t int16 int8
//	SAMPLE_TYPE_INT8,
	SAMPLE_TYPE_INT32,
//	SAMPLE_TYPE_INT64,
	SAMPLE_TYPE_FLOAT,
	SAMPLE_TYPE_DOUBLE,
};

///r
/* Bits in modes: */
/* GUS-compatible flags */
#define MODES_16BIT	    (1<<0)
#define MODES_UNSIGNED	(1<<1)
#define MODES_LOOPING	(1<<2)
#define MODES_PINGPONG	(1<<3)
#define MODES_REVERSE	(1<<4)
#define MODES_SUSTAIN	(1<<5)
#define MODES_ENVELOPE	(1<<6)
#define MODES_CLAMPED	(1<<7) /* ?? (for last envelope??) */
/* Flags not defined by GUS */
#define MODES_RELEASE   (1<<8)
#define MODES_TRIGGER_RANDOM      (1<<9)
#define MODES_NO_NOTEOFF          (1<<10)
#define MODES_TRIGGER_RELEASE     (1<<11)

#define INST_GUS	0
#define INST_SF2	1
#define INST_MOD	2
#define INST_PCM	3	/* %sample */
///r
#define INST_MMS	4	/* %mms */
#define INST_SCC	5	/* %scc */
#define INST_SFZ	6	/* %sfz */
#define INST_DLS	7	/* %dls */

/* sfSampleType */
#define SF_SAMPLETYPE_MONO 1
#define SF_SAMPLETYPE_RIGHT 2
#define SF_SAMPLETYPE_LEFT 4
#define SF_SAMPLETYPE_LINKED 8
#define SF_SAMPLETYPE_COMPRESSED 0x10
#define SF_SAMPLETYPE_ROM 0x8000

typedef struct {
  int type;
  int samples;
  Sample *sample;
  char *instname;
} Instrument;

#define MAX_TONEBANK_COMM_LEN 64

///r
typedef struct {
	char *name;
	char *comment;
	Instrument *instrument;
	int8 note, strip_loop, strip_envelope, strip_tail, loop_timeout,
	font_preset, font_keynote, legato, tva_level, play_note, damper_mode;
	uint8 font_bank;
	uint8 instype;
/*
	0: Normal // pat
	1: %font // sf2
	2: %sample // wav,aiff
	3: %mms
	4: %scc
	5: %sfz
	6: %dls
	7-255: reserved
*/
	int16 amp;
	int8 amp_normalize;
	int8 lokey, hikey, lovel, hivel;
	int sample_lokeynum, sample_hikeynum, sample_lovelnum, sample_hivelnum;
	int16 *sample_lokey, *sample_hikey, *sample_lovel, *sample_hivel;
	int16 rnddelay;
	int tunenum;
	float *tune;
	int sclnotenum;
	int16 *sclnote;
	int scltunenum;
	int16 *scltune;
	// filter
	int fcnum;
	int16 *fc;
	int resonum;
	int16 *reso;	
	int lpf_type;
	int hpfnum;
	int **hpf;
	int fclownum, fclowkeyfnum, fcmulnum, fcaddnum;
	int16 *fclow, *fclowkeyf, *fcmul, *fcadd;
	int16 vel_to_fc, key_to_fc, vel_to_resonance;
	// env
	int envratenum, envofsnum;
	int **envrate, **envofs;
	int modenvratenum, modenvofsnum;
	int **modenvrate, **modenvofs;
	int envvelfnum, envkeyfnum;
	int **envvelf, **envkeyf;
	int modenvvelfnum, modenvkeyfnum;
	int **modenvvelf, **modenvkeyf;
	int modpitchnum, modfcnum;
	int16 *modpitch, *modfc;
	int pitenvnum;
	int **pitenv;
	// lfo
	int tremnum, vibnum;
	struct Quantity_ **trem, **vib;	
	int trempitchnum, tremfcnum, tremdelaynum, tremfreqnum, tremsweepnum, tremampnum;
	int16 *trempitch, *tremfc, *tremdelay, *tremfreq, *tremsweep, *tremamp;
	int vibfcnum, vibampnum, vibdelaynum, vibfreqnum, vibsweepnum, vibpitchnum;
	int16 *vibfc, *vibamp, *vibdelay, *vibfreq, *vibsweep, *vibpitch;
	// other
	int8 reverb_send, chorus_send, delay_send;
	int vfxe_num;
	int vfxnum[VOICE_EFFECT_NUM];
	int **vfx[VOICE_EFFECT_NUM];
	int is_preset;
	int8 keep_voice;
	int8 rx_note_off;
	int8 element_num;
	int8 def_pan;
	int sample_pan, sample_width;
	int32 seq_length;
	int32 seq_position;
	//FLOAT_T lorand;
	//FLOAT_T hirand;
} ToneBankElement;

#define MAGIC_ERROR_INSTRUMENT ((Instrument *)(-1))
#define IS_MAGIC_INSTRUMENT(ip) ((ip) == MAGIC_ERROR_INSTRUMENT)

#define DYNAMIC_INSTRUMENT_NAME ""

typedef struct _AlternateAssign {
    /* 128 bit vector:
     * bits[(note >> 5) & 0x3] & (1 << (note & 0x1F))
     */
    uint32 bits[4];
    struct _AlternateAssign* next;
} AlternateAssign;

///r
#define MAX_ELEMENT 8

typedef struct {
  ToneBankElement *tone[128][MAX_ELEMENT];
  AlternateAssign *alt;
} ToneBank;

typedef struct _SpecialPatch /* To be used MIDI Module play mode */
{
    int type;
    int samples;
    Sample *sample;
    char *name;
    int32 sample_offset;
} SpecialPatch;
///r
enum instrument_mapID
{
    INST_NO_MAP = 0,
    SC_55_TONE_MAP,
    SC_55_DRUM_MAP,
    SC_88_TONE_MAP,
    SC_88_DRUM_MAP,
    SC_88PRO_TONE_MAP,
    SC_88PRO_DRUM_MAP,
    SC_8850_TONE_MAP,
    SC_8850_DRUM_MAP,
    XG_NORMAL_MAP, // MSB0
    XG_SFX_KIT_MAP, // MSB126
    XG_DRUM_KIT_MAP, // MSB127
    XG_FREE_MAP, // MSB63
	XG_MU100EXC_MAP, // MSB48
	XG_SAMPLING16_MAP, // MSB16
	XG_SAMPLING126_MAP, // MSB126	
    XG_PCM_USER_MAP, // MSB32
    XG_PCM_SFX_MAP, // MSB64
    XG_PCM_A_MAP, // MSB80
    XG_PCM_B_MAP, // MSB96
    XG_VA_USER_MAP, // MSB33
    XG_VA_SFX_MAP, // MSB65
    XG_VA_A_MAP, // MSB81
    XG_VA_B_MAP, // MSB97	
    XG_SG_USER_MAP, // MSB34
    XG_SG_SFX_MAP, // MSB66
    XG_SG_A_MAP, // MSB82
    XG_SG_B_MAP, // MSB98
    XG_FM_USER_MAP, // MSB35
    XG_FM_SFX_MAP, // MSB67
    XG_FM_A_MAP, // MSB83
    XG_FM_B_MAP, // MSB99
    GM2_TONE_MAP,
    GM2_DRUM_MAP,
	MT32_TONE_MAP,
	MT32_DRUM_MAP,
	CM32L_TONE_MAP,
	CM32P_TONE_MAP,
	CM32_DRUM_MAP,
	SN01_TONE_MAP,
	SN02_TONE_MAP,
	SN03_TONE_MAP,
	SN04_TONE_MAP,
	SN05_TONE_MAP,
	SN06_TONE_MAP,
	SN07_TONE_MAP,
	SN08_TONE_MAP,
	SN09_TONE_MAP,
	SN10_TONE_MAP,
	SN11_TONE_MAP,
	SN12_TONE_MAP,
	SN13_TONE_MAP,
	SN14_TONE_MAP,
	SN15_TONE_MAP,
	SN01_DRUM_MAP,
	SN02_DRUM_MAP,
	SN03_DRUM_MAP,
	SN04_DRUM_MAP,
	SN05_DRUM_MAP,
	SN06_DRUM_MAP,
	SN07_DRUM_MAP,
	SN08_DRUM_MAP,
	SN09_DRUM_MAP,
	SN10_DRUM_MAP,
	SN11_DRUM_MAP,
	SN12_DRUM_MAP,
	SN13_DRUM_MAP,
	SN14_DRUM_MAP,
	SN15_DRUM_MAP,
	SDXX_TONE80_MAP, 
	SDXX_TONE81_MAP, 
	SDXX_TONE87_MAP,
	SDXX_TONE89_MAP,
	SDXX_TONE96_MAP, 
	SDXX_TONE97_MAP, 
	SDXX_TONE98_MAP, 
	SDXX_TONE99_MAP, 
	SDXX_DRUM86_MAP,
	SDXX_DRUM104_MAP,
	SDXX_DRUM105_MAP,
	SDXX_DRUM106_MAP,
	SDXX_DRUM107_MAP,
	K05RW_TONE0_MAP,
	K05RW_TONE56_MAP,
	K05RW_TONE57_MAP,
	K05RW_DRUM62_MAP,
	NX5R_TONE0_MAP,
	NX5R_TONE1_MAP,
	NX5R_TONE2_MAP,
	NX5R_TONE3_MAP,
	NX5R_TONE4_MAP,
	NX5R_TONE5_MAP,
	NX5R_TONE6_MAP,
	NX5R_TONE7_MAP,
	NX5R_TONE8_MAP,
	NX5R_TONE9_MAP,
	NX5R_TONE10_MAP,
	NX5R_TONE11_MAP,
	NX5R_TONE16_MAP,
	NX5R_TONE17_MAP,
	NX5R_TONE18_MAP,
	NX5R_TONE19_MAP,
	NX5R_TONE24_MAP,
	NX5R_TONE25_MAP,
	NX5R_TONE26_MAP,
	NX5R_TONE32_MAP,
	NX5R_TONE33_MAP,
	NX5R_TONE40_MAP,
	NX5R_TONE56_MAP,
	NX5R_TONE57_MAP,
	NX5R_TONE64_MAP,
	NX5R_TONE80_MAP,
	NX5R_TONE81_MAP,
	NX5R_TONE82_MAP,
	NX5R_TONE83_MAP,
	NX5R_TONE88_MAP,
	NX5R_TONE89_MAP,
	NX5R_TONE90_MAP,
	NX5R_TONE91_MAP,
	NX5R_TONE125_MAP,
	NX5R_DRUM61_MAP,
	NX5R_DRUM62_MAP,
	NX5R_DRUM126_MAP,
	NX5R_DRUM127_MAP,
    NUM_INST_MAP
};
///r
#define MAP_BANK_COUNT 768
extern ToneBank *tonebank[], *drumset[];

extern Instrument *default_instrument;
#define NSPECIAL_PATCH 256
extern SpecialPatch *special_patch[ /* NSPECIAL_PATCH */ ];
///r
extern int default_program[MAX_CHANNELS];
extern int special_program[MAX_CHANNELS];

extern int antialiasing_allowed;
extern int fast_decay;
extern int opt_print_fontname;
extern int free_instruments_afterwards;
extern int cutoff_allowed;

#define SPECIAL_PROGRAM -1

/* sndfont.c */
extern int opt_sf_close_each_file;
extern void add_soundfont(char *sf_file, int sf_order,
			  int cutoff_allowed, int resonance_allowed,
			  int amp);
extern void remove_soundfont(char *sf_file);
extern void init_load_soundfont(void);
extern Instrument *load_soundfont_inst(int order, int bank, int preset,
				       int keynote);
extern Instrument *extract_soundfont(char *sf_file, int bank, int preset,
				     int keynote);
extern int exclude_soundfont(int bank, int preset, int keynote);
extern int order_soundfont(int bank, int preset, int keynote, int order);
extern char *soundfont_preset_name(int bank, int preset, int keynote,
				   char **sndfile);
extern void free_soundfonts(void);

/* instrum.c */
extern void free_instruments(int reload_default_inst);
extern void free_special_patch(int id);
extern int set_default_instrument(char *name);
extern void clear_magic_instruments(void);
extern Instrument *load_instrument(int dr, int b, int prog, int elm);
extern int find_instrument_map_bank(int dr, int map, int bk);
extern int alloc_instrument_map_bank(int dr, int map, int bk);
extern void alloc_instrument_bank(int dr, int bankset);
extern int instrument_map_no_mapped(int mapID, int *set_in_out, int *elem_in_out);
extern int instrument_map(int mapID, int *set_in_out, int *elem_in_out);
extern void set_instrument_map(int mapID,
			       int set_from, int elem_from,
			       int set_to, int elem_to);
extern void free_instrument_map(void);
extern AlternateAssign *add_altassign_string(AlternateAssign *old,
					     char **params, int n);
extern AlternateAssign *find_altassign(AlternateAssign *altassign, int note);
extern void copy_tone_bank_element(ToneBankElement *elm, const ToneBankElement *src);
extern void free_tone_bank_element(ToneBankElement *elm);
extern void free_tone_bank(void);
extern void free_instrument(Instrument *ip);
extern void squash_sample_16to8(Sample *sp);
///r
extern void reinit_tone_bank_element(ToneBankElement *tone);
extern int alloc_tone_bank_element(ToneBankElement **tone_ptr);

extern char *default_instrument_name;
extern int progbase;

extern int32 modify_release;
#define MAX_MREL 5000
#define DEFAULT_MREL 800

extern int opt_load_all_instrument;
extern void load_all_instrument(void);

#endif /* ___INSTRUM_H_ */
