/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2009 Masanao Izumo <iz@onicos.co.jp>
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

    playmidi.c -- random stuff in need of rearrangement
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __POCC__
#include <sys/types.h>
#endif // for off_t
#ifdef __W32__

#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#ifdef __W32__
#include <windows.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "mix.h"
#include "controls.h"
#include "miditrace.h"
#include "recache.h"
#include "arc.h"
#include "effect.h"
#include "wrd.h"
#include "aq.h"
#include "freq.h"
#include "quantity.h"
///r
#include "resample.h"
#include "thread.h"
#include "sndfontini.h"
#include "mt19937ar.h"

extern int convert_mod_to_midi_file(MidiEvent * ev);

#define ABORT_AT_FATAL 1 /*#################*/
#define MYCHECK(s) do { if(s == 0) { printf("## L %d\n", __LINE__); abort(); } } while(0)

extern VOLATILE int intr;

/* #define SUPPRESS_CHANNEL_LAYER */

#ifdef SOLARIS
/* shut gcc warning up */
int usleep(unsigned int useconds);
#endif

#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */

#include "tables.h"

///r
#define ADD_PLAY_TIME   (1)// sec
#define ADD_SILENT_TIME	(1) // sec
#define EMU_DELAY_TIME  (5) // 0.1msec

#define PORTAMENTO_TIME_TUNING		(0.0002) //  1.0 / 5000.0
#define PORTAMENTO_CONTROL_BIT      (13) // 13bit == tuning (see recompute_voice_pitch() // def 8bit
#define PORTAMENTO_CONTROL_RATIO	(1<<PORTAMENTO_CONTROL_BIT) // def 256(8bit)	/* controls per sec */
#define DEFAULT_CHORUS_DELAY1		(0.02)
#define DEFAULT_CHORUS_DELAY2		(0.003)
#define CHORUS_OPPOSITE_THRESHOLD	(32)
#define EOT_PRESEARCH_LEN		(32)
#define SPEED_CHANGE_RATE		(1.0594630943592952645618252949461)  /* 2^(1/12) */



/* Undefine if you don't want to use auto voice reduce implementation */
///r to timidity.h
//#define REDUCE_VOICE_TIME_TUNING	(play_mode->rate/5) /* 0.2 sec */
//#define REDUCE_VOICE_TIME_TUNING	(play_mode->rate/200)
#ifdef REDUCE_VOICE_TIME_TUNING
static int max_good_nv = 1;
static int min_bad_nv = 256;
static int32 ok_nv_total = 32;
static int32 ok_nv_counts = 1;
static int32 ok_nv_sample = 0;
static int ok_nv = 32;
static int old_rate = -1;
#endif

static int midi_streaming = 0;
static int prescanning_flag;
static int32 midi_restart_time = 0;
Channel channel[MAX_CHANNELS];
int max_voices = DEFAULT_VOICES;
Voice *voice = NULL;
int8 current_keysig = 0;
int8 current_temper_keysig = 0;
int temper_adj = 0;
int8 opt_init_keysig = 8;
int8 opt_force_keysig = 8;
int32 current_play_tempo = 500000;
int opt_realtime_playing = 0;
///r
int volatile stream_max_compute = 500; /* compute time limit (in msec) when streaming */
uint32 opt_rtsyn_latency = 200; 
int opt_rtsyn_skip_aq = 0;
int reduce_voice_threshold = -1;
///r
int reduce_quality_threshold = -1;
int reduce_polyphony_threshold = -1;
int add_play_time = ADD_PLAY_TIME;
static int32 add_play_count = ADD_PLAY_TIME * 48000;
int add_silent_time = ADD_SILENT_TIME;
static int32 add_silent_count = ADD_SILENT_TIME * 48000;
int emu_delay_time = EMU_DELAY_TIME;

static MBlockList playmidi_pool;
int check_eot_flag;
int special_tonebank = -1;
int default_tonebank = 0;
int playmidi_seek_flag = 0;
int play_pause_flag = 0;
static int file_from_stdin;
int key_adjust = 0;
FLOAT_T tempo_adjust = 1.0;
int opt_pure_intonation = 0;
int current_freq_table = 0;
int current_temper_freq_table = 0;
static int master_tuning = 0;

static void set_reverb_level(int ch, int level);
static int make_rvid_flag = 0; /* For reverb optimization */

/* Ring voice id for each notes.  This ID enables duplicated note. */
static uint8 vidq_head[128 * MAX_CHANNELS], vidq_tail[128 * MAX_CHANNELS];

#ifdef MODULATION_WHEEL_ALLOW
int opt_modulation_wheel = 1;
#else
int opt_modulation_wheel = 0;
#endif /* MODULATION_WHEEL_ALLOW */

#ifdef PORTAMENTO_ALLOW
int opt_portamento = 1;
#else
int opt_portamento = 0;
#endif /* PORTAMENTO_ALLOW */

#ifdef NRPN_VIBRATO_ALLOW
int opt_nrpn_vibrato = 1;
#else
int opt_nrpn_vibrato = 0;
#endif /* NRPN_VIBRATO_ALLOW */

#ifdef REVERB_CONTROL_ALLOW
int opt_reverb_control = 1;
#else
#ifdef FREEVERB_CONTROL_ALLOW
int opt_reverb_control = 3;
#else
int opt_reverb_control = 0;
#endif /* FREEVERB_CONTROL_ALLOW */
#endif /* REVERB_CONTROL_ALLOW */

#ifdef CHORUS_CONTROL_ALLOW
int opt_chorus_control = 1;
#else
int opt_chorus_control = 0;
#endif /* CHORUS_CONTROL_ALLOW */

#ifdef SURROUND_CHORUS_ALLOW
int opt_surround_chorus = 1;
#else
int opt_surround_chorus = 0;
#endif /* SURROUND_CHORUS_ALLOW */

#ifdef GM_CHANNEL_PRESSURE_ALLOW
int opt_channel_pressure = 1;
#else
int opt_channel_pressure = 0;
#endif /* GM_CHANNEL_PRESSURE_ALLOW */

#ifdef VOICE_CHAMBERLIN_LPF_ALLOW
#define VOICE_LPF_DEFAULT 1
#elif defined(VOICE_MOOG_LPF_ALLOW)
#define VOICE_LPF_DEFAULT 2
#elif defined(VOICE_BW_LPF_ALLOW)
#define VOICE_LPF_DEFAULT 
#elif defined(VOICE_LPF12_2_ALLOW)
#define VOICE_LPF_DEFAULT 4
#else
#define VOICE_LPF_DEFAULT 0
#endif
int opt_lpf_def = VOICE_LPF_DEFAULT;
#undef VOICE_LPF_DEFAULT

int opt_hpf_def = 0;

#ifdef OVERLAP_VOICE_ALLOW
int opt_overlap_voice_allow = 1;
#else
int opt_overlap_voice_allow = 0;
#endif /* OVERLAP_VOICE_ALLOW */

///r
#ifdef OVERLAP_VOICE_COUNT 
int opt_overlap_voice_count = OVERLAP_VOICE_COUNT;
#else
int opt_overlap_voice_count = 0;
#endif /* OVERLAP_VOICE_COUNT */

int opt_max_channel_voices = MAX_CHANNEL_VOICES;

#ifdef TEMPER_CONTROL_ALLOW
int opt_temper_control = 1;
#else
int opt_temper_control = 0;
#endif /* TEMPER_CONTROL_ALLOW */

int opt_tva_attack = 1;	/* attack envelope control */
int opt_tva_decay = 1;	/* decay envelope control */
int opt_tva_release = 1;	/* release envelope control */
///r
double gs_env_attack_calc = 1.0;
double gs_env_decay_calc = 1.0;
double gs_env_release_calc = 1.0;
double xg_env_attack_calc = 1.0;
double xg_env_decay_calc = 1.0;
double xg_env_release_calc = 1.0;
double gm2_env_attack_calc = 1.0;
double gm2_env_decay_calc = 1.0;
double gm2_env_release_calc = 1.0;
double gm_env_attack_calc = 1.0;
double gm_env_decay_calc = 1.0;
double gm_env_release_calc = 1.0;
double env_attack_calc = 1.0; 
double env_decay_calc = 1.0;
double env_release_calc = 1.0; 
double env_add_offdelay_time = 0.0; 
static int env_add_offdelay_count = 0;


int opt_delay_control = 1;	/* CC#94 delay(celeste) effect control */
int opt_eq_control = 1;		/* channel equalizer control */
int opt_insertion_effect = 1;	/* insertion effect control */
int opt_drum_effect = 1;	/* drumpart effect control */
int32 opt_drum_power = 100;		/* coef. of drum amplitude */
int opt_amp_compensation = 0;
int opt_modulation_envelope = 1;
///r
double opt_user_volume_curve = 0;
int opt_default_module = MODULE_SC88PRO; //MODULE_TIMIDITY_DEFAULT;
int opt_preserve_silence = 0;

int voices=DEFAULT_VOICES, upper_voices;

int32 control_ratio=0;
double div_control_ratio;
int32 amplification=DEFAULT_AMPLIFICATION;
///r
int32 output_amplification=DEFAULT_OUTPUT_AMPLIFICATION;
static FLOAT_T input_volume;
static int32 master_volume_ratio = 0xFFFF;
ChannelBitMask default_drumchannel_mask;
ChannelBitMask default_drumchannels;
ChannelBitMask drumchannel_mask;
ChannelBitMask drumchannels;
int adjust_panning_immediately = 1;
int auto_reduce_polyphony = 1;
double envelope_modify_rate = 1.0;
int reduce_quality_flag = 0;
int reduce_voice_flag = 0;
int reduce_polyphony_flag = 0;
int no_4point_interpolation = 0;
char* pcm_alternate_file = NULL; /* NULL or "none": Nothing (default)
				  * "auto": Auto select
				  * filename: Use it
				  */

static int32 lost_notes, cut_notes;
///r
static ALIGN DATA_T common_buffer[AUDIO_BUFFER_SIZE*2], /* stereo samples */
             *buffer_pointer;
static int16 wav_buffer[AUDIO_BUFFER_SIZE*2];
static int32 buffered_count;

///r
int compute_buffer_bits	= DEFAULT_COMPUTE_BUFFER_BITS;
int compute_buffer_size;
int synth_buffer_size; // stereo size
//int synth_buffer_size_mono;

static int vol_env_count, ch_vol_env_count;
int opt_mix_envelope = 1; // ON
int mix_env_mask = 0;
int opt_modulation_update = 1;
static int mix_env_count;
int ramp_out_count;
int opt_cut_short_time = CUT_SHORT_TIME2;

double ramp_out_rate;
double cut_short_rate;
double cut_short_rate2;

int min_sustain_time = 0; // ms
int min_sustain_count; // ms

#define TREMOLO_RATE_MAX (10.0) // Hz
#define TREMOLO_RATE_MIN (0.001) // Hz	
#define VIBRATO_RATE_MAX (10.0) // Hz
#define VIBRATO_RATE_MIN (0.001) // Hz
#define NRPN_VIBRATO_RATE (10.0) // Hz
#define NRPN_VIBRATO_DELAY (5000.0) // ms
#define NRPN_VIBRATO_DEPTH (50.0) // cent

static FLOAT_T nrpn_vib_depth_cent = NRPN_VIBRATO_DEPTH;
static FLOAT_T nrpn_vib_rate = NRPN_VIBRATO_RATE;
static FLOAT_T nrpn_vib_delay = NRPN_VIBRATO_DELAY * 48.0; // count def48kHz 
static int8 nrpn_vib_delay_mode = NRPN_PARAM_GM_DELAY;
static int8 nrpn_vib_rate_mode = NRPN_PARAM_GM_RATE;

#define NRPN_FILTER_FREQ_MAX (9600.0) // MOD TVF CUTOFF CONTROL と同じ変化幅
#define NRPN_FILTER_RESO_MAX (36.0) // 仕様不明 CUTOFF同様の変化幅
static FLOAT_T nrpn_filter_freq = NRPN_FILTER_FREQ_MAX;
static FLOAT_T nrpn_filter_reso = NRPN_FILTER_RESO_MAX;
static int8 nrpn_filter_freq_mode = NRPN_PARAM_GM_CUTOFF;
//static int8 nrpn_filter_reso_mode = NRPN_PARAM_GM_RESO;
static int8 nrpn_filter_hpf_freq_mode = NRPN_PARAM_GM_CUTOFF_HPF;
static int8 nrpn_env_attack_mode = NRPN_PARAM_GM_ATTACK;
static int8 nrpn_env_decay_mode = NRPN_PARAM_GM_DECAY;
static int8 nrpn_env_release_mode = NRPN_PARAM_GM_RELEASE;

static MidiEvent *event_list;
static MidiEvent *current_event;
static int32 sample_count;	/* Length of event_list */
int32 current_sample;		/* Number of calclated samples */

int note_key_offset = 0;		/* For key up/down */
FLOAT_T midi_time_ratio = 1.0;	/* For speed up/down */
ChannelBitMask channel_mute;	/* For channel mute */
int temper_type_mute;			/* For temperament type mute */

/* for auto amplitude compensation */
static int mainvolume_max; /* maximum value of mainvolume */
static double compensation_ratio = 1.0; /* compensation ratio */

static int32 compute_skip_count = -1; // def use do_compute_data_midi()

static int find_samples(MidiEvent *, int *);
static int select_play_sample(Sample *, int, int *, int *, MidiEvent *, int, int);
static double get_play_note_ratio(int, int);
static int find_voice(MidiEvent *);
static void finish_note(int i);
static void update_portamento_controls(int ch);
static void update_legato_status(int ch);
static void update_rpn_map(int ch, int addr, int update_now);
static void ctl_prog_event(int ch);
///r
static void ctl_note_event(int noteID);
static void ctl_note_event2(uint8 channel, uint8 note, uint8 status, uint8 velocity);
static void ctl_timestamp(void);
static void ctl_updatetime(int32 samples);
static void ctl_pause_event(int pause, int32 samples);
static void update_legato_controls(int ch);
static void set_master_tuning(int);
static void set_single_note_tuning(int, int, int, int);
static void set_user_temper_entry(int, int, int);
static void set_cuepoint(int, int, int);

static void init_voice_filter(int);
/* XG Part EQ */
void init_part_eq_xg(struct part_eq_xg *);
void recompute_part_eq_xg(struct part_eq_xg *);
/* MIDI controllers (MW, Bend, CAf, PAf,...) */
static void init_midi_controller(midi_controller *);
static float get_midi_controller_amp(midi_controller *);
static float get_midi_controller_filter_cutoff(midi_controller *);
static int32 get_midi_controller_pitch(midi_controller *);
static float get_midi_controller_lfo1_pitch_depth(midi_controller *);
static float get_midi_controller_lfo1_amp_depth(midi_controller *);
static float get_midi_controller_lfo1_filter_depth(midi_controller *);
static float get_midi_controller_lfo2_pitch_depth(midi_controller *);
static float get_midi_controller_lfo2_amp_depth(midi_controller *);
static float get_midi_controller_lfo2_filter_depth(midi_controller *);
/* Rx. ~ (Rcv ~) */
static void init_rx(int);
static void set_rx(int, int32, int);
static int32 get_rx(int ch, int32 rx);
static void init_rx_drum(struct DrumParts *);
static void set_rx_drum(struct DrumParts *, int32, int);
static int32 get_rx_drum(struct DrumParts *, int32);

static int is_insertion_effect_xg(int ch);
static int is_mfx_effect_sd(int ch);

// 
#define OFFSET_MAX (0x3FFFFFFFL)
#define OFFSET_MIN (1L)

///r
const int32 delay_out_time_ms[] = {
	10,	// 0 : buffer mix / effect thru / no feedback
	300,	// 1 : filter (
	600,	// 2 : multi filter + any
	1500,	// 3 : pre_delay + feedback (chorus/flanger/phaser
	6000,	// 4 : delay // long delay + max feedbackだと足りないかも
	12000,	// 5 : reverb
	30000,	// 6 : system reverb
};
static int32 delay_out_count[7];

///r
// channel mixer
#if 0 // dim ch buffer
static ALIGN DATA_T ch_buffer_s[MAX_CHANNELS][AUDIO_BUFFER_SIZE * 2]; // max:4MB
#else // malloc ch buffer
#endif // malloc ch buffer
DATA_T *ch_buffer[MAX_CHANNELS];

// VST channel use flag
#ifdef VST_LOADER_ENABLE
int use_vst_channel = 0;
#endif



#ifndef POW2
#if 1 // lite
#define POW2(val) exp((float)(M_LN2 * val))
#else // precision
#define POW2(val) pow(2.0, val)
#endif
#endif /* POW2 */


static inline int clip_int(int val, int min, int max)
{
	return ((val > max) ? max : (val < min) ? min : val);
}



#ifdef VOICE_EFFECT
int cfg_flg_vfx = 0;
#endif
#ifdef INT_SYNTH
int cfg_flg_int_synth_scc = 0;
int cfg_flg_int_synth_mms = 0;
#endif

// free voice[]
void free_voice_pointer(void)
{
	int i, j;
	
	if (!voice)
		return;
	
	for(i = 0; i < max_voices; i++) {			
		Voice *vp = voice + i;	

		if(!vp)
			continue;
#ifdef VOICE_EFFECT
		for(j = 0; j < VOICE_EFFECT_NUM; j++){
			if(vp->vfx[j]){
				free(vp->vfx[j]);
				vp->vfx[j] = NULL;
			}	
		}
#endif
#ifdef INT_SYNTH
		if(vp->scc){
			free(vp->scc);
			vp->scc = NULL;
		}
		if(vp->mms){
			free(vp->mms);
			vp->mms = NULL;
		}
#endif
	}

    safe_free(voice);
	voice = NULL;
}

// Allocate voice[]
void init_voice_pointer(void)
{	
	int i, j, error = 0;

    voice = (Voice*) safe_calloc(max_voices, sizeof(Voice));
	if (!voice)
		return;
	for(i = 0; i < max_voices; i++) {			
		Voice *vp = voice + i;	

		if(!vp){
			error++;
			break;
		}
#ifdef VOICE_EFFECT
		if(cfg_flg_vfx){
			for(j = 0; j < VOICE_EFFECT_NUM; j++){
				if(!vp->vfx[j]){
					vp->vfx[j] = (VoiceEffect *)safe_malloc(sizeof(VoiceEffect));
					if(!vp->vfx[j]){
						error++;
						break;
					}
					memset(vp->vfx[j], 0, sizeof(VoiceEffect));
				}	
			}
		}
#endif
#ifdef INT_SYNTH
		if(cfg_flg_int_synth_scc && !vp->scc){
			vp->scc = (InfoIS_SCC *)safe_malloc(sizeof(InfoIS_SCC));
			if(!vp->scc){
				error++;
				break;
			}
			memset(vp->scc, 0, sizeof(InfoIS_SCC));
		}	
		if(cfg_flg_int_synth_mms && !vp->mms){
			vp->mms = (InfoIS_MMS *)safe_malloc(sizeof(InfoIS_MMS));
			if(!vp->mms){
				error++;
				break;
			}
			memset(vp->mms, 0, sizeof(InfoIS_MMS));
		}
#endif
	}

	if(error)
		free_voice_pointer();
}




void init_playmidi(void){
	int i, tmp, tmp2;
#ifdef ALIGN_SIZE
	const int min_compute_sample = 8;
	uint32 min_compute_sample_mask = ~(min_compute_sample - 1);
#endif
	
	if(compute_buffer_bits < 0){ // auto
		tmp = tmp2 = playmode_rate_ms * abs(compute_buffer_bits);
#ifdef ALIGN_SIZE
		tmp &= min_compute_sample_mask;
		if(tmp < tmp2)
			tmp += min_compute_sample; // >=1ms
#endif
		compute_buffer_size = tmp;
	}else{ // ! auto
		compute_buffer_size = 1U << compute_buffer_bits;
#ifdef ALIGN_SIZE
		if(compute_buffer_size < min_compute_sample)
			compute_buffer_size = min_compute_sample; // AVX 256bit / float 32bit 
#endif
	}		
	if((compute_buffer_bits < 0) && (compute_buffer_size < (1<<audio_buffer_bits))){
		double cb_ms = compute_buffer_size * div_playmode_rate * 1000.0;
		ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Auto compute_buffer_size : %d : %.3f ms",
			compute_buffer_size, cb_ms);
	}else if(compute_buffer_size < (1<<audio_buffer_bits)){
		double cb_ms = compute_buffer_size * div_playmode_rate * 1000.0;
		ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "compute_buffer_size : %d : %.3f ms",
			compute_buffer_size, cb_ms);
	}else{
		compute_buffer_bits = DEFAULT_COMPUTE_BUFFER_BITS;
		compute_buffer_size = 1U << compute_buffer_bits;
		audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR! : Auto Compute Buffer Size");
		ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "compute_buffer_bits Set default : %d", compute_buffer_bits);
		ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "audio_buffer_bits Set default : %d", audio_buffer_bits);
	}

#ifdef ALIGN_SIZE	
	synth_buffer_size = (compute_buffer_size + min_compute_sample) * 2 * sizeof(DATA_T); // 2ch // compute_buffer_size < AUDIO_BUFFER_SIZE
	// +8 終端付近でのSIMD store対策
#else
//	synth_buffer_size_mono = compute_buffer_size * sizeof(DATA_T); // 1ch // compute_buffer_size < AUDIO_BUFFER_SIZE	
	synth_buffer_size = compute_buffer_size * 2 * sizeof(DATA_T); // 2ch // compute_buffer_size < AUDIO_BUFFER_SIZE
#endif
		
	control_ratio = compute_buffer_size;
	div_control_ratio = 1.0 / (double)control_ratio;
	
	add_play_count = add_play_time > 0 ? (add_play_time * play_mode->rate) : 1;
	add_silent_count = add_silent_time > 0 ? (add_silent_time * play_mode->rate) : 1;
	ch_vol_env_count = playmode_rate_ms * CH_VOL_ENV_TIME; // per ms channel
	vol_env_count = playmode_rate_ms * VOL_ENV_TIME; // per ms voice
	mix_env_count = playmode_rate_ms * VOL_ENV_TIME; // per ms
	if(compute_buffer_size == 1)
		opt_mix_envelope = 0; // OFF
	if(opt_mix_envelope > 0){
		if(control_ratio < compute_buffer_size)
			mix_env_count = control_ratio;
		else			
			mix_env_count = compute_buffer_size; // per 0.1ms
		if(opt_mix_envelope >= compute_buffer_size)
			opt_mix_envelope = 1;
		if(!(opt_mix_envelope == 1 || opt_mix_envelope == 2 || opt_mix_envelope == 4
		 || opt_mix_envelope == 8 || opt_mix_envelope == 16 || opt_mix_envelope == 32
		 || opt_mix_envelope == 64 || opt_mix_envelope == 128 || opt_mix_envelope == 256))
			opt_mix_envelope = 1;
		mix_env_mask = opt_mix_envelope - 1;		

	}else{
		opt_mix_envelope = 0;
		mix_env_mask = 0;
	}
	if(opt_modulation_update > 100){
		opt_modulation_update = 100;
	}else if(opt_modulation_update < 1)	{
		opt_modulation_update = 1;
	}

	ramp_out_count = playmode_rate_dms * MAX_DIE_TIME;
	ramp_out_rate = (double)OFFSET_MAX * div_playmode_rate * control_ratio * 1000.0 * 10.0 / MAX_DIE_TIME;
	cut_short_rate = (double)OFFSET_MAX * div_playmode_rate * control_ratio * 1000.0 / CUT_SHORT_TIME;
	if(opt_cut_short_time > 0){
		cut_short_rate2 = (double)OFFSET_MAX * div_playmode_rate * control_ratio * 1000.0 / opt_cut_short_time;
		if(cut_short_rate2 > OFFSET_MAX)
			cut_short_rate2 = OFFSET_MAX;
		else if(cut_short_rate2 < OFFSET_MIN)
			cut_short_rate2 = OFFSET_MIN;
	}else
		cut_short_rate2 = (double)OFFSET_MAX * div_playmode_rate * control_ratio * 1000.0 / CUT_SHORT_TIME2;

	ramp_out_rate *= div_control_ratio; // rate / sample
	cut_short_rate *= div_control_ratio; // rate / sample
	cut_short_rate2 *= div_control_ratio; // rate / sample
	env_add_offdelay_count = playmode_rate_ms * env_add_offdelay_time;


	if(min_sustain_time > 0)
		min_sustain_count = playmode_rate_ms * min_sustain_time; // per ms	
	for(i = 0; i < 7; i++)
		delay_out_count[i] = playmode_rate_ms * delay_out_time_ms[i]; // init delay_out count
	
	for(i = 0; i < MAX_CHANNELS; i++){
#if 0 // dim ch buffer
		ch_buffer[i] = ch_buffer_s[i];
#else // malloc ch buffer
#ifdef ALIGN_SIZE
		if(ch_buffer[i]){
			aligned_free(ch_buffer[i]);
			ch_buffer[i] = NULL;
		}
		ch_buffer[i] = (DATA_T *)aligned_malloc(synth_buffer_size, ALIGN_SIZE);
#else
		if(ch_buffer[i]){
			safe_free(ch_buffer[i]);
			ch_buffer[i] = NULL;
		}
		ch_buffer[i] = (DATA_T *)safe_malloc(synth_buffer_size);
#endif
#endif // malloc ch buffer
		memset(ch_buffer[i], 0, synth_buffer_size);
	}
	
// VST channel use flag
#ifdef VST_LOADER_ENABLE
	use_vst_channel = opt_reverb_control == 11;
#endif
}


void free_playmidi(void)
{
	int i, j;

#if 0 // dim ch buffer
#else // malloc ch buffer
	for(i = 0; i < MAX_CHANNELS; i++){ 
		if(ch_buffer[i]){
#ifdef ALIGN_SIZE
			aligned_free(ch_buffer[i]);
#else
			safe_free(ch_buffer[i]);
#endif
			ch_buffer[i] = NULL;
		}
	}
#endif
	
	for(i = 0; i < MAX_CHANNELS; i++){
		for (j = 0; j < MAX_ELEMENT; j++) {
			safe_free(channel[i].seq_counters[j]);
			channel[i].seq_counters[j] = NULL;
			channel[i].seq_num_counters[j] = 0;
		}
	}
	
#ifdef VOICE_EFFECT
	if (!voice)
		return;	
	for(i = 0; i < max_voices; i++) {	
		free_voice_effect(i);
	}
#endif
}



#define IS_SYSEX_EVENT_TYPE(event) ((event)->type == ME_NONE || (event)->type >= ME_RANDOM_PAN || (event)->b == SYSEX_TAG)

static char *event_name(int type)
{
#define EVENT_NAME(X) case X: return #X
	switch (type) {
	EVENT_NAME(ME_NONE);
	EVENT_NAME(ME_NOTEOFF);
	EVENT_NAME(ME_NOTEON);
	EVENT_NAME(ME_KEYPRESSURE);
	EVENT_NAME(ME_PROGRAM);
	EVENT_NAME(ME_CHANNEL_PRESSURE);
	EVENT_NAME(ME_PITCHWHEEL);
	EVENT_NAME(ME_TONE_BANK_MSB);
	EVENT_NAME(ME_TONE_BANK_LSB);
	EVENT_NAME(ME_MODULATION_WHEEL);
	EVENT_NAME(ME_BREATH);
	EVENT_NAME(ME_FOOT);
	EVENT_NAME(ME_MAINVOLUME);
	EVENT_NAME(ME_BALANCE);
	EVENT_NAME(ME_PAN);
	EVENT_NAME(ME_EXPRESSION);	
	EVENT_NAME(ME_SUSTAIN);
	EVENT_NAME(ME_PORTAMENTO_TIME_MSB);
	EVENT_NAME(ME_PORTAMENTO_TIME_LSB);
	EVENT_NAME(ME_PORTAMENTO);
	EVENT_NAME(ME_PORTAMENTO_CONTROL);
	EVENT_NAME(ME_DATA_ENTRY_MSB);
	EVENT_NAME(ME_DATA_ENTRY_LSB);
	EVENT_NAME(ME_SOSTENUTO);
	EVENT_NAME(ME_SOFT_PEDAL);
	EVENT_NAME(ME_LEGATO_FOOTSWITCH);
	EVENT_NAME(ME_HOLD2);
	EVENT_NAME(ME_HARMONIC_CONTENT);
	EVENT_NAME(ME_RELEASE_TIME);
	EVENT_NAME(ME_DECAY_TIME);
	EVENT_NAME(ME_ATTACK_TIME);
	EVENT_NAME(ME_BRIGHTNESS);
	EVENT_NAME(ME_VIBRATO_RATE);
	EVENT_NAME(ME_VIBRATO_DEPTH);
	EVENT_NAME(ME_VIBRATO_DELAY);
	EVENT_NAME(ME_REVERB_EFFECT);
	EVENT_NAME(ME_TREMOLO_EFFECT);
	EVENT_NAME(ME_CHORUS_EFFECT);
	EVENT_NAME(ME_CELESTE_EFFECT);
	EVENT_NAME(ME_PHASER_EFFECT);
	EVENT_NAME(ME_RPN_INC);
	EVENT_NAME(ME_RPN_DEC);
	EVENT_NAME(ME_NRPN_LSB);
	EVENT_NAME(ME_NRPN_MSB);
	EVENT_NAME(ME_RPN_LSB);
	EVENT_NAME(ME_RPN_MSB);
	EVENT_NAME(ME_ALL_SOUNDS_OFF);
	EVENT_NAME(ME_RESET_CONTROLLERS);
	EVENT_NAME(ME_ALL_NOTES_OFF);
	EVENT_NAME(ME_MONO);
	EVENT_NAME(ME_POLY);
	EVENT_NAME(ME_UNDEF_CTRL_CHNG);
#if 0
	EVENT_NAME(ME_VOLUME_ONOFF);		/* Not supported */
#endif
	EVENT_NAME(ME_MASTER_TUNING);
	EVENT_NAME(ME_SCALE_TUNING);
	EVENT_NAME(ME_BULK_TUNING_DUMP);
	EVENT_NAME(ME_SINGLE_NOTE_TUNING);
	EVENT_NAME(ME_RANDOM_PAN);
	EVENT_NAME(ME_SET_PATCH);
	EVENT_NAME(ME_DRUMPART);
	EVENT_NAME(ME_KEYSHIFT);
	EVENT_NAME(ME_PATCH_OFFS);
	EVENT_NAME(ME_TEMPO);
	EVENT_NAME(ME_CHORUS_TEXT);
	EVENT_NAME(ME_LYRIC);
	EVENT_NAME(ME_GSLCD);
	EVENT_NAME(ME_MARKER);
	EVENT_NAME(ME_INSERT_TEXT);
	EVENT_NAME(ME_TEXT);
	EVENT_NAME(ME_KARAOKE_LYRIC);
	EVENT_NAME(ME_MASTER_VOLUME);
	EVENT_NAME(ME_RESET);
	EVENT_NAME(ME_NOTE_STEP);
	EVENT_NAME(ME_CUEPOINT);
	EVENT_NAME(ME_TIMESIG);
	EVENT_NAME(ME_KEYSIG);
	EVENT_NAME(ME_TEMPER_KEYSIG);
	EVENT_NAME(ME_TEMPER_TYPE);
	EVENT_NAME(ME_MASTER_TEMPER_TYPE);
	EVENT_NAME(ME_USER_TEMPER_ENTRY);
	EVENT_NAME(ME_SYSEX_LSB);
	EVENT_NAME(ME_SYSEX_MSB);
	EVENT_NAME(ME_SYSEX_GS_LSB);
	EVENT_NAME(ME_SYSEX_GS_MSB);
	EVENT_NAME(ME_SYSEX_XG_LSB);
	EVENT_NAME(ME_SYSEX_XG_MSB);
	EVENT_NAME(ME_SYSEX_SD_LSB);
	EVENT_NAME(ME_SYSEX_SD_MSB);
	EVENT_NAME(ME_SYSEX_SD_HSB);
	EVENT_NAME(ME_WRD);
	EVENT_NAME(ME_SHERRY);
	EVENT_NAME(ME_BARMARKER);
	EVENT_NAME(ME_STEP);
	EVENT_NAME(ME_LOOP_EXPANSION_START);
	EVENT_NAME(ME_LOOP_EXPANSION_END);
	EVENT_NAME(ME_LAST);
	EVENT_NAME(ME_EOT);
	}
	return "Unknown";
#undef EVENT_NAME
}

///r
#define DIV_COMPENSATION_RATIO_MAX ((double)(1.5259021896696421759365224689097e-5)) // 1/(0xFFFF)

static void adjust_amplification(void)
{
    /* input volume */
//    input_volume = (double)(amplification) * DIV_100 * ((double)master_volume_ratio * (compensation_ratio/0xFFFF));
	if (opt_user_volume_curve != 0) {
		input_volume = (double)(amplification) * DIV_100
			* user_vol_table[(master_volume_ratio >> 9) & 0x7F] * DIV_127;
	} else if (play_system_mode == SD_SYSTEM_MODE) {
		input_volume = (double)(amplification) * DIV_100
			* gm2_vol_table[(master_volume_ratio >> 9) & 0x7F] * DIV_127;
	} else if (play_system_mode == GM2_SYSTEM_MODE) {
		input_volume = (double)(amplification) * DIV_100
			* gm2_vol_table[(master_volume_ratio >> 9) & 0x7F] * DIV_127;
	} else if(play_system_mode == GS_SYSTEM_MODE) {	/* use measured curve */ 
		input_volume = (double)(amplification) * DIV_100
			* sc_vol_table[(master_volume_ratio >> 9) & 0x7F] * DIV_127;
	} else if(play_system_mode == CM_SYSTEM_MODE) {	/* use measured curve */ 
		input_volume = (double)(amplification) * DIV_100
			* sc_vol_table[(master_volume_ratio >> 9) & 0x7F] * DIV_127;
	} else if (IS_CURRENT_MOD_FILE) {	/* use linear curve */
		input_volume = (double)(amplification) * DIV_100
			* (double)master_volume_ratio * DIV_COMPENSATION_RATIO_MAX;
	} else {	/* use generic exponential curve */
		input_volume = (double)(amplification) * DIV_100
			* perceived_vol_table[(master_volume_ratio >> 9) & 0x7F] * DIV_127;
	}	
	if(opt_amp_compensation)	
		input_volume *= compensation_ratio;
}

static int new_vidq(int ch, int note)
{
    int i;

    if(opt_overlap_voice_allow)
    {
	i = ch * 128 + note;
	return vidq_head[i]++;
    }
    return 0;
}

static int last_vidq(int ch, int note)
{
    int i;

    if(opt_overlap_voice_allow)
    {
	i = ch * 128 + note;
	if(vidq_head[i] == vidq_tail[i])
	{
	    ctl->cmsg(CMSG_WARNING, VERB_DEBUG_SILLY,
		      "channel=%d, note=%d: Voice is already OFF", ch, note);
	    return -1;
	}
	return vidq_tail[i]++;
    }
    return 0;
}

static void swap_voices(Voice *a, Voice *b)
{
    Voice swap;
    memcpy(&swap, a, sizeof(Voice));
    memmove(a, b, sizeof(Voice));
    memcpy(b, &swap, sizeof(Voice));
}

static void reset_voices(void)
{
    int i;
    for(i = 0; i < max_voices; i++)
    {
	voice[i].status = VOICE_FREE;
	voice[i].temper_instant = 0;
    }
    upper_voices = 0;
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

///r
static void kill_note(int i)
{
	if(voice[i].status & (VOICE_FREE | VOICE_DIE))
		return;
    voice[i].status = VOICE_DIE;
    if (voice[i].sample->modes & MODES_ENVELOPE)
		if(voice[i].update_voice < UPDATE_VOICE_KILL_NOTE)
			voice[i].update_voice = UPDATE_VOICE_KILL_NOTE; // kill_note
    if(!prescanning_flag)
	ctl_note_event(i);
}

static void cut_note(int i)
{
	if(voice[i].status & (VOICE_FREE))
		return;
    voice[i].status = VOICE_DIE;
    if (voice[i].sample->modes & MODES_ENVELOPE)
		if(voice[i].update_voice < UPDATE_VOICE_CUT_NOTE)
			voice[i].update_voice = UPDATE_VOICE_CUT_NOTE; // cut_note
    if(!prescanning_flag)
	ctl_note_event(i);
}

static void cut_note2(int i)
{
	if(voice[i].status & (VOICE_FREE))
		return;
    voice[i].status = VOICE_DIE;
    if (voice[i].sample->modes & MODES_ENVELOPE)
		if(voice[i].update_voice < UPDATE_VOICE_CUT_NOTE2)
			voice[i].update_voice = UPDATE_VOICE_CUT_NOTE2; // cut_note2
    if(!prescanning_flag)
	ctl_note_event(i);
}


void kill_all_voices(void)
{
    int i, uv = upper_voices;

    for(i = 0; i < uv; i++)
	if(voice[i].status & ~(VOICE_FREE | VOICE_DIE))
	    kill_note(i);
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

static void reset_drum_controllers(struct DrumParts *d[], int note)
{
    int i,j;


    if(note == -1)
    {
	for(i = 0; i < 128; i++)
	    if(d[i] != NULL)
	    {
		d[i]->drum_panning = NO_PANNING;
		for(j=0;j<6;j++) {d[i]->drum_envelope_rate[j] = -1;}
		d[i]->pan_random = 0;
		d[i]->drum_level = 1.0f;
		d[i]->coarse = 0;
		d[i]->fine = 0;
		d[i]->delay_level = -1;
		d[i]->chorus_level = -1;
		d[i]->reverb_level = -1;
		d[i]->play_note = -1;
		d[i]->drum_cutoff_freq = 0;
		d[i]->drum_resonance = 0;
///r
		d[i]->drum_hpf_cutoff_freq = 0;
		d[i]->drum_hpf_resonance = 0;
		d[i]->drum_velo_pitch_sens = 0;
		d[i]->drum_velo_cutoff_sens = 0;
		if(opt_eq_control && opt_drum_effect)
			init_part_eq_xg(&d[i]->eq_xg);
		init_rx_drum(d[i]);	
		d[i]->rx_note_off = -1;
	    }
    }
    else
    {
	d[note]->drum_panning = NO_PANNING;
	for(j = 0; j < 6; j++) {d[note]->drum_envelope_rate[j] = -1;}
	d[note]->pan_random = 0;
	d[note]->drum_level = 1.0f;
	d[note]->coarse = 0;
	d[note]->fine = 0;
	d[note]->delay_level = -1;
	d[note]->chorus_level = -1;
	d[note]->reverb_level = -1;
	d[note]->play_note = -1;
	d[note]->drum_cutoff_freq = 0;
	d[note]->drum_resonance = 0;
///r
	d[note]->drum_hpf_cutoff_freq = 0;
	d[note]->drum_hpf_resonance = 0;
	d[note]->drum_velo_pitch_sens = 0;
	d[note]->drum_velo_cutoff_sens = 0;
	if(opt_eq_control && opt_drum_effect)
		init_part_eq_xg(&d[note]->eq_xg);
	init_rx_drum(d[note]);
	d[note]->rx_note_off = -1;
    }
}

///r
static void initialize_controllers(int c)
{
	/* Reset Controllers other CC#121 */
	int i, module = get_module();
	
	switch(module) {	/* TONE MAP-0 NUMBER */
	case MODULE_SC55:
		channel[c].tone_map0_number = 1;
		break;
	case MODULE_SC88:
		channel[c].tone_map0_number = 2;
		break;
	case MODULE_SC88PRO:
		channel[c].tone_map0_number = 3;
		break;
	case MODULE_SC8850:
		channel[c].tone_map0_number = 4;
		break;
	//	case MODULE_SDMODE:
	case MODULE_SD20:
	case MODULE_SD80:
	case MODULE_SD90:
	case MODULE_SD50:
		channel[c].tone_map0_number = 120;
		break;
		break;
	default:
		channel[c].tone_map0_number = 0;
		break;
	}	
	
	channel[c].program_flag = 0;

	channel[c].damper_mode = 0;
	channel[c].loop_timeout = 0;		
	if(play_system_mode == XG_SYSTEM_MODE)
		channel[c].volume = 100;
	else
		channel[c].volume = 90;
	if (prescanning_flag) {
		if (channel[c].volume > mainvolume_max) {	/* pick maximum value of mainvolume */
			mainvolume_max = channel[c].volume;
			ctl->cmsg(CMSG_INFO,VERB_DEBUG,"ME_MAINVOLUME/max (CH:%d VAL:%#x)",c,mainvolume_max);
		}
	}
	channel[c].mono = 0;
	channel[c].portamento_time_lsb = 0;
	channel[c].portamento_time_msb = 0;
	channel[c].porta_status = 0;
	channel[c].porta_last_note_fine = -1;
	channel[c].legato_status = 0;
	channel[c].legato_flag = 0;
	channel[c].legato_note = 255;
	channel[c].legato_note_time = 0;

	set_reverb_level(c, -1);
	if(opt_chorus_control == 1)
		channel[c].chorus_level = 0;
	else
		channel[c].chorus_level = -opt_chorus_control;
	channel[c].delay_level = 0;

	/* NRPN */
	reset_drum_controllers(channel[c].drums, -1);
	for(i = 0; i < 6; i++) {channel[c].envelope_rate[i] = -1;}
	channel[c].param_vibrato_rate = 0;
	channel[c].param_vibrato_depth = 0;
	channel[c].param_vibrato_delay = 0;	
	channel[c].vibrato_delay = 0;	
	channel[c].amp = 1.0;
	channel[c].ch_amp[0] = 1.0;
	channel[c].ch_amp[1] = 1.0;
	channel[c].lfo_amp_depth[0] = 0;
	channel[c].lfo_amp_depth[1] = 0;	
	channel[c].modenv_depth = 0;
	channel[c].param_cutoff_freq = 0;
	channel[c].param_resonance = 0;
	channel[c].cutoff_cent = 0;
	channel[c].resonance_dB = 0;
	channel[c].hpf_param_cutoff_freq = 0;
	channel[c].hpf_param_resonance = 0;
	channel[c].hpf_cutoff_cent = 0;
	channel[c].hpf_resonance_dB = 0;
	channel[c].lfo_cutoff_depth[0] = 0;
	channel[c].lfo_cutoff_depth[1] = 0;
	channel[c].pitch = 0;
	channel[c].lfo_pitch_depth[0] = 0;
	channel[c].lfo_pitch_depth[1] = 0;
	channel[c].freq_table = freq_table;
		
	/* System Exclusive */
	channel[c].sysex_gs_msb_addr = channel[c].sysex_gs_msb_val =
	channel[c].sysex_xg_msb_addr = channel[c].sysex_xg_msb_val =
	channel[c].sysex_msb_addr = channel[c].sysex_msb_val = 0;
	channel[c].dry_level = 127;
	channel[c].eq_gs = 1;
	channel[c].insertion_effect = 0;
	channel[c].velocity_sense_depth = 0x40;
	channel[c].velocity_sense_offset = 0x40;
	channel[c].pit_env_level[0] = 0x40;
	channel[c].pit_env_level[1] = 0x40;
	channel[c].pit_env_level[2] = 0x40;
	channel[c].pit_env_level[3] = 0x40;
	channel[c].pit_env_level[4] = 0x40;
	channel[c].pit_env_time[0] = 0x40;
	channel[c].pit_env_time[1] = 0x40;
	channel[c].pit_env_time[2] = 0x40;
	channel[c].pit_env_time[3] = 0x40;
	channel[c].detune_param = 0x80;
	channel[c].detune = 1.0;
	channel[c].pitch_offset_fine = 0.0;
	
#if 1 // fix limited multi
	channel[c].assign_mode = 1;
#else // GS XG spec
	if(ISDRUMCHANNEL(c) && play_system_mode == GS_SYSTEM_MODE && module == MODULE_SC55 )
		channel[c].assign_mode = 0;
	else
		channel[c].assign_mode = 1;
#endif

	for (i = 0; i < 12; i++)
		channel[c].scale_tuning[i] = 0;
	channel[c].prev_scale_tuning = 0;
	channel[c].temper_type = 0;

	init_channel_layer(c);
	init_part_eq_xg(&(channel[c].eq_xg));

	/* channel pressure & polyphonic key pressure control */
	init_midi_controller(&(channel[c].mod));
	init_midi_controller(&(channel[c].bend)); 
	init_midi_controller(&(channel[c].caf)); 
	init_midi_controller(&(channel[c].paf)); 
	init_midi_controller(&(channel[c].cc1)); 
	init_midi_controller(&(channel[c].cc2)); 
	channel[c].mod.mode = 0;
	channel[c].bend.mode = 0;
	channel[c].caf.mode = 0;
	channel[c].paf.mode = 0;  
	channel[c].cc1.mode = 0;
	channel[c].cc2.mode = 0;
	if(play_system_mode == XG_SYSTEM_MODE){
		channel[c].cc3.mode = 1;
		channel[c].cc4.mode = 1;
	}else{
		channel[c].cc3.mode = 0;
		channel[c].cc4.mode = 0;	  
	}
	channel[c].mod.val = 0;
	channel[c].bend.val = 0;
	channel[c].caf.val = 0;
	channel[c].paf.val = 0;  
	channel[c].cc1.val = 0;
	channel[c].cc2.val = 0;
	channel[c].cc3.val = 0;
	channel[c].cc4.val = 0;
	channel[c].cc1.num = 16;
	channel[c].cc2.num = 17;
	channel[c].cc3.num = 18;
	channel[c].cc4.num = 19;	
	channel[c].bend.pitch = 2;
	if (play_system_mode == GM2_SYSTEM_MODE)
		channel[c].mod.lfo1_pitch_depth = 50.0; // 50cent
	else
		channel[c].mod.lfo1_pitch_depth = DIV_127 * 0x0A * 600.0; // max=600cent def=0x0A 47cent

	init_rx(c);
	channel[c].note_limit_high = 127;
	channel[c].note_limit_low = 0;
	channel[c].vel_limit_high = 127;
	channel[c].vel_limit_low = 0;

	free_drum_effect(c);
	
	/* SD */
	channel[c].gm2_inst = 1; // contemp set
	/* SD MFX */
	// assign 1ch:part 2ch:part 3:part other:none
	// select1ch:MFX1 2ch:MFX2 3ch:MFX3 other:MFX1
	channel[c].sd_output_assign = c < SD_MFX_EFFECT_NUM ? 2 : 1; // 0:mfx 1:none 2:part
	channel[c].sd_output_mfx_select = c < SD_MFX_EFFECT_NUM ? c : 0; // 0:MFX1 1:MFX2 2:MFX3
	channel[c].sd_dry_send_level = 127;
	channel[c].mfx_part_type = 0;
	memset(channel[c].mfx_part_param, 0, sizeof(channel[c].mfx_part_param));
	channel[c].mfx_part_dry_send = 127;
	channel[c].mfx_part_send_reverb = 0;
	channel[c].mfx_part_send_chorus = 0;
	channel[c].mfx_part_ctrl_source[0] = 0;
	channel[c].mfx_part_ctrl_source[1] = 0;
	channel[c].mfx_part_ctrl_source[2] = 0;
	channel[c].mfx_part_ctrl_source[3] = 0;
	channel[c].mfx_part_ctrl_sens[0] = 0x40;
	channel[c].mfx_part_ctrl_sens[1] = 0x40;
	channel[c].mfx_part_ctrl_sens[2] = 0x40;
	channel[c].mfx_part_ctrl_sens[3] = 0x40;
	channel[c].mfx_part_ctrl_assign[0] = 0;
	channel[c].mfx_part_ctrl_assign[1] = 0;
	channel[c].mfx_part_ctrl_assign[2] = 0;
	channel[c].mfx_part_ctrl_assign[3] = 0;
	/* SD Chorus */
	channel[c].chorus_part_type = 0;
	memset(channel[c].chorus_part_param, 0, sizeof(channel[c].chorus_part_param));
	channel[c].chorus_part_efx_level = 127;
	channel[c].chorus_part_send_reverb = 0;
	channel[c].chorus_part_output_select = 0;
	/* SD Reverb */
	channel[c].reverb_part_type = 0;
	memset(channel[c].reverb_part_param, 0, sizeof(channel[c].reverb_part_param));
	channel[c].reverb_part_efx_level = 100;
	
	for (i = 0; i < MAX_ELEMENT; i++) {
		safe_free(channel[c].seq_counters[i]);
		channel[c].seq_counters[i] = NULL;
		channel[c].seq_num_counters[i] = 0;
	}
}

/* Process the Reset All Controllers event CC#121 */
static void reset_controllers(int c)
{
	channel[c].expression = 127; /* SCC-1 does this. */
	channel[c].sustain = 0;
	channel[c].sostenuto = 0;
	channel[c].pitchbend = 0x2000;
	channel[c].portamento = 0;
	channel[c].portamento_control = -1;
	update_portamento_controls(c);
	channel[c].legato = -1;
	update_legato_status(c);
	channel[c].lastlrpn = channel[c].lastmrpn = 0;
	channel[c].nrpn = -1;
	channel[c].rpn_7f7f_flag = 1;
}

static void redraw_controllers(int c)
{
    ctl_mode_event(CTLE_VOLUME, 1, c, channel[c].volume);
    ctl_mode_event(CTLE_EXPRESSION, 1, c, channel[c].expression);
    ctl_mode_event(CTLE_SUSTAIN, 1, c, channel[c].sustain);
    ctl_mode_event(CTLE_MOD_WHEEL, 1, c, channel[c].mod.val);
    ctl_mode_event(CTLE_PITCH_BEND, 1, c, channel[c].pitchbend);
    ctl_prog_event(c);
    ctl_mode_event(CTLE_TEMPER_TYPE, 1, c, channel[c].temper_type);
    ctl_mode_event(CTLE_MUTE, 1,
    		c, (IS_SET_CHANNELMASK(channel_mute, c)) ? 1 : 0);
    ctl_mode_event(CTLE_CHORUS_EFFECT, 1, c, get_chorus_level(c));
    ctl_mode_event(CTLE_REVERB_EFFECT, 1, c, get_reverb_level(c));
    if (play_system_mode == XG_SYSTEM_MODE)
        ctl_mode_event(CTLE_INSERTION_EFFECT, 1, c, is_insertion_effect_xg(c));
    else if (play_system_mode == SD_SYSTEM_MODE)
        ctl_mode_event(CTLE_INSERTION_EFFECT, 1, c, is_mfx_effect_sd(c));
    else
        ctl_mode_event(CTLE_INSERTION_EFFECT, 1, c, channel[c].insertion_effect);
}

#ifdef MULTI_THREAD_COMPUTE
static void reset_midi_thread(void);
#endif

static void recompute_channel_lfo(int ch);
static void recompute_channel_amp(int ch);
static void recompute_channel_filter(int ch);
static void recompute_channel_pitch(int ch);

///r
void reset_midi(int playing)
{
	int i, cnt, module = get_module();
	
#ifdef MULTI_THREAD_COMPUTE
	reset_midi_thread();
#endif
	compute_skip_count = -1;
		
	switch(play_system_mode){
	case GS_SYSTEM_MODE:
	case CM_SYSTEM_MODE:	
		nrpn_vib_delay_mode = NRPN_PARAM_GS_DELAY;
		nrpn_vib_rate_mode = NRPN_PARAM_GS_RATE;
		nrpn_filter_freq_mode = NRPN_PARAM_GS_CUTOFF;
	//	nrpn_filter_reso_mode = NRPN_PARAM_GS_RESO;
		nrpn_filter_hpf_freq_mode = NRPN_PARAM_GS_CUTOFF_HPF;
		nrpn_env_attack_mode = NRPN_PARAM_GS_ATTACK;
		nrpn_env_decay_mode = NRPN_PARAM_GS_DECAY;
		nrpn_env_release_mode = NRPN_PARAM_GS_RELEASE;
		break;
	case XG_SYSTEM_MODE:
		nrpn_vib_delay_mode = NRPN_PARAM_XG_DELAY;
		nrpn_vib_rate_mode = NRPN_PARAM_XG_RATE;
		nrpn_filter_freq_mode = NRPN_PARAM_XG_CUTOFF;
	//	nrpn_filter_reso_mode = NRPN_PARAM_XG_RESO;
		nrpn_filter_hpf_freq_mode = NRPN_PARAM_XG_CUTOFF_HPF;
		nrpn_env_attack_mode = NRPN_PARAM_XG_ATTACK;
		nrpn_env_decay_mode = NRPN_PARAM_XG_DECAY;
		nrpn_env_release_mode = NRPN_PARAM_XG_RELEASE;
		break;
	case SD_SYSTEM_MODE:
	case GM_SYSTEM_MODE:
	case GM2_SYSTEM_MODE:
	default:
		nrpn_vib_delay_mode = NRPN_PARAM_GM_DELAY;
		nrpn_vib_rate_mode = NRPN_PARAM_GM_RATE;
		nrpn_filter_freq_mode = NRPN_PARAM_GM_CUTOFF;
	//	nrpn_filter_reso_mode = NRPN_PARAM_GM_RESO;
		nrpn_filter_hpf_freq_mode = NRPN_PARAM_GM_CUTOFF_HPF;
		nrpn_env_attack_mode = NRPN_PARAM_GM_ATTACK;
		nrpn_env_decay_mode = NRPN_PARAM_GM_DECAY;
		nrpn_env_release_mode = NRPN_PARAM_GM_RELEASE;
		break;
	}
	if(otd.vibrato_rate != 0 )
		nrpn_vib_rate = otd.vibrato_rate * DIV_100;
	else
		nrpn_vib_rate = NRPN_VIBRATO_RATE;
	if (otd.vibrato_cent != 0 )
		nrpn_vib_depth_cent = otd.vibrato_cent;
	else switch(play_system_mode){
	case GS_SYSTEM_MODE:
		switch(module){
		case MODULE_SC55:
		case MODULE_SC88:
		case MODULE_SC88PRO:
		case MODULE_MT32:
		case MODULE_CM32L:
		case MODULE_CM32P:
		case MODULE_CM64:
		case MODULE_CM500A:
		case MODULE_CM500D:
		case MODULE_CM64_SN01:
		case MODULE_CM64_SN02:
		case MODULE_CM64_SN03:
		case MODULE_CM64_SN04:
		case MODULE_CM64_SN05:
		case MODULE_CM64_SN06:
		case MODULE_CM64_SN07:
		case MODULE_CM64_SN08:
		case MODULE_CM64_SN09:
		case MODULE_CM64_SN10:
		case MODULE_CM64_SN11:
		case MODULE_CM64_SN12:
		case MODULE_CM64_SN13:
		case MODULE_CM64_SN14:
		case MODULE_CM64_SN15:
		// CM関連モジュールではGSリセット無効だが・・一応
			nrpn_vib_depth_cent = 75; // 9.6cent ? /* GS NRPN from -9.6 cents to +9.45 cents. */
			break;
		case MODULE_SC8850:
		default:
			nrpn_vib_depth_cent = 100;
			break;
		}
		break;
	case CM_SYSTEM_MODE:		
		nrpn_vib_depth_cent = 75;
		break;
	case SD_SYSTEM_MODE:
	case GM2_SYSTEM_MODE:
	default:
		nrpn_vib_depth_cent = 100;
		break;
	}
	if (otd.vibrato_delay != 0 )
		nrpn_vib_delay = otd.vibrato_delay * playmode_rate_ms;
	else
		nrpn_vib_delay = NRPN_VIBRATO_DELAY * playmode_rate_ms;
	if(otd.filter_freq != 0 )
		nrpn_filter_freq = otd.filter_freq;
	else
		nrpn_filter_freq = NRPN_FILTER_FREQ_MAX;
	if(otd.filter_reso != 0 )
		nrpn_filter_reso = otd.filter_reso * DIV_10;
	else
		nrpn_filter_reso = NRPN_FILTER_RESO_MAX;	
	// env calc
	switch(play_system_mode){
	case GS_SYSTEM_MODE:
		env_attack_calc = gs_env_attack_calc;
		env_decay_calc = gs_env_decay_calc;
		env_release_calc = gs_env_release_calc;
		break;
	case XG_SYSTEM_MODE:
		env_attack_calc = xg_env_attack_calc;
		env_decay_calc = xg_env_decay_calc;
		env_release_calc = xg_env_release_calc;
		break;
	case SD_SYSTEM_MODE:
	case GM2_SYSTEM_MODE:
		env_attack_calc = gm2_env_attack_calc;
		env_decay_calc = gm2_env_decay_calc;
		env_release_calc = gm2_env_release_calc;
		break;
	case GM_SYSTEM_MODE:
	default:
		env_attack_calc = gm_env_attack_calc;
		env_decay_calc = gm_env_decay_calc;
		env_release_calc = gm_env_release_calc;
		break;
	}

	for (i = 0; i < MAX_CHANNELS; i++) {
		initialize_controllers(i);
		reset_controllers(i);
		/* The rest of these are unaffected
		 * by the Reset All Controllers event
		 */
		channel[i].program = default_program[i];
		channel[i].panning = NO_PANNING;
		channel[i].pan_random = 0;	
///r
		if(adjust_panning_immediately && play_system_mode == XG_SYSTEM_MODE){
			channel[i].ch_amp[0] = channel[i].ch_amp[1] = 0;
			init_envelope2(&channel[i].vol_env , channel[i].ch_amp[0], channel[i].ch_amp[1], ch_vol_env_count);
			if(opt_mix_envelope)
				init_envelope2(&channel[i].mix_env , channel[i].vol_env.vol[0], channel[i].vol_env.vol[1], mix_env_count);
		}
		memset(ch_buffer[i], 0, synth_buffer_size); // clear

// VST channel convert DATA_T to int32
#ifdef VST_LOADER_ENABLE
#if (defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)) && !defined(VSTWRAP_EXT)	
		memset(vst_ch_vpblist[i], 0, synth_buffer_size); // clear
#endif
#endif

		/* tone bank or drum set */
		if (ISDRUMCHANNEL(i)) {
			channel[i].bank = 0;
			channel[i].altassign = drumset[0]->alt;
		} else {
			if (special_tonebank >= 0)
				channel[i].bank = special_tonebank;
			else
				channel[i].bank = default_tonebank;
		}
		channel[i].bank_lsb = channel[i].bank_msb = 0;
///r
		if (play_system_mode == XG_SYSTEM_MODE && i % 16 == 9)
			channel[i].bank_msb = 127;	// Use MSB=127 for XG
		else if (play_system_mode == GM2_SYSTEM_MODE && i % 16 == 9)
			channel[i].bank_msb = 120;	// Use MSB=120 for GM2
		else if (play_system_mode == SD_SYSTEM_MODE && i % 16 == 9)
			channel[i].bank_msb = 104;	// Use MSB=105 for SD
		else if (play_system_mode == KG_SYSTEM_MODE && i % 16 == 9)
			channel[i].bank_msb = 61;	// Use MSB=61 for AG
		update_rpn_map(i, RPN_ADDR_FFFF, 0);
		channel[i].special_sample = 0;
		channel[i].key_shift = 0;
		channel[i].mapID = get_default_mapID(i);
		channel[i].lasttime = INT_MIN / 2;
		
		recompute_channel_lfo(i);
		recompute_channel_amp(i);
		recompute_channel_filter(i);
		recompute_channel_pitch(i);

	}
	if (playing) {
		kill_all_voices();
		if (temper_type_mute) {
			if (temper_type_mute & 1)
				FILL_CHANNELMASK(channel_mute);
			else
				CLEAR_CHANNELMASK(channel_mute);
		}
		for (i = 0; i < MAX_CHANNELS; i++)
			redraw_controllers(i);
		if (midi_streaming && free_instruments_afterwards) {
			free_instruments(0);
			/* free unused memory */
			cnt = free_global_mblock();
			if (cnt > 0)
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
						"%d memory blocks are free", cnt);
		}
	} else
		reset_voices();

	master_volume_ratio = 0xffff;
	adjust_amplification();
	init_freq_table_tuning();
	master_tuning = 0;
	if (current_file_info) {
		COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
		COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);
	} else {
		COPY_CHANNELMASK(drumchannels, default_drumchannels);
		COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);
	}
///r
	change_output_volume(output_amplification);
	ctl_mode_event(CTLE_MASTER_VOLUME, 0, output_amplification, 0);
	ctl_mode_event(CTLE_KEY_OFFSET, 0, note_key_offset, 0);
	ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);


	//elion add
	{
		int c;
		CtlEvent ct;
		for (c = 0; c < MAX_CHANNELS; c++) {
			ct.type = CTLE_VOLUME;
			ct.v1 = c;
			ct.v2 = channel[c].volume;
			ctl->event(&ct);

			ct.type = CTLE_EXPRESSION;
			ct.v2 = channel[c].expression;
			ctl->event(&ct);
		}
	}
}

static void recompute_channel_lfo(int ch)
{	
	Channel *cp = channel + ch;
	/* MIDI controllers LFO */
	FLOAT_T rate1 = 0, rate2 = 0;
	FLOAT_T adepth1 = 0, adepth2 = 0;
	FLOAT_T fdepth1 = 0, fdepth2 = 0;
	FLOAT_T pdepth1 = 0, pdepth2 = 0;

	if(opt_channel_pressure) {
		rate1 += (cp->mod.lfo1_rate)
			+ (cp->bend.lfo1_rate)
			+ (cp->caf.lfo1_rate)
			+ (cp->cc1.lfo1_rate)
			+ (cp->cc2.lfo1_rate)
			+ (cp->cc3.lfo1_rate)
			+ (cp->cc4.lfo1_rate);
		adepth1 += get_midi_controller_lfo1_amp_depth(&(cp->mod))
			+ get_midi_controller_lfo1_amp_depth(&(cp->bend))
			+ get_midi_controller_lfo1_amp_depth(&(cp->caf))
			+ get_midi_controller_lfo1_amp_depth(&(cp->cc1))
			+ get_midi_controller_lfo1_amp_depth(&(cp->cc2))
			+ get_midi_controller_lfo1_amp_depth(&(cp->cc3))
			+ get_midi_controller_lfo1_amp_depth(&(cp->cc4));
		fdepth1 += get_midi_controller_lfo1_filter_depth(&(cp->mod))
			+ get_midi_controller_lfo1_filter_depth(&(cp->bend))
			+ get_midi_controller_lfo1_filter_depth(&(cp->caf))
			+ get_midi_controller_lfo1_filter_depth(&(cp->cc1))
			+ get_midi_controller_lfo1_filter_depth(&(cp->cc2))
			+ get_midi_controller_lfo1_filter_depth(&(cp->cc3))
			+ get_midi_controller_lfo1_filter_depth(&(cp->cc4));
		pdepth1 += get_midi_controller_lfo1_pitch_depth(&(cp->mod))
			+ get_midi_controller_lfo1_pitch_depth(&(cp->bend))
			+ get_midi_controller_lfo1_pitch_depth(&(cp->caf))
			+ get_midi_controller_lfo1_pitch_depth(&(cp->cc1))
			+ get_midi_controller_lfo1_pitch_depth(&(cp->cc2))
			+ get_midi_controller_lfo1_pitch_depth(&(cp->cc3))
			+ get_midi_controller_lfo1_pitch_depth(&(cp->cc4));
		rate2 += (cp->mod.lfo2_rate)
			+ (cp->bend.lfo2_rate)
			+ (cp->caf.lfo2_rate)
			+ (cp->cc1.lfo2_rate)
			+ (cp->cc2.lfo2_rate)
			+ (cp->cc3.lfo2_rate)
			+ (cp->cc4.lfo2_rate);
		adepth2 += get_midi_controller_lfo2_amp_depth(&(cp->mod))
			+ get_midi_controller_lfo2_amp_depth(&(cp->bend))
			+ get_midi_controller_lfo2_amp_depth(&(cp->caf))
			+ get_midi_controller_lfo2_amp_depth(&(cp->cc1))
			+ get_midi_controller_lfo2_amp_depth(&(cp->cc2))
			+ get_midi_controller_lfo2_amp_depth(&(cp->cc3))
			+ get_midi_controller_lfo2_amp_depth(&(cp->cc4));
		fdepth2 += get_midi_controller_lfo2_filter_depth(&(cp->mod))
			+ get_midi_controller_lfo2_filter_depth(&(cp->bend))
			+ get_midi_controller_lfo2_filter_depth(&(cp->caf))
			+ get_midi_controller_lfo2_filter_depth(&(cp->cc1))
			+ get_midi_controller_lfo2_filter_depth(&(cp->cc2))
			+ get_midi_controller_lfo2_filter_depth(&(cp->cc3))
			+ get_midi_controller_lfo2_filter_depth(&(cp->cc4));
		pdepth2 += get_midi_controller_lfo2_pitch_depth(&(cp->mod))
			+ get_midi_controller_lfo2_pitch_depth(&(cp->bend))
			+ get_midi_controller_lfo2_pitch_depth(&(cp->caf))
			+ get_midi_controller_lfo2_pitch_depth(&(cp->cc1))
			+ get_midi_controller_lfo2_pitch_depth(&(cp->cc2))
			+ get_midi_controller_lfo2_pitch_depth(&(cp->cc3))
			+ get_midi_controller_lfo2_pitch_depth(&(cp->cc4));
	}	
	if(rate1 > 10.0)			{rate1 = 10.0;}
	else if(rate1 < -10.0)		{rate1 = -10.0;}
	cp->lfo_rate[0] = rate1;
	if(adepth1 > 1.0)			 {adepth1 = 1.0;}
	cp->lfo_amp_depth[0] = adepth1;	
	if(fdepth1 > 2400.0)		{fdepth1 = 2400.0;}
	else if(fdepth1 < -2400.0)	{fdepth1 = -2400.0;}
	cp->lfo_cutoff_depth[0] = fdepth1;	
	pdepth1 += (double)cp->param_vibrato_depth * DIV_64 * nrpn_vib_depth_cent; // NRPN in -64~64
	if (pdepth1 > 600.0)		{pdepth1 = 600.0;}
	else if (pdepth1 < -600.0)	{pdepth1 = -600.0;}
	cp->lfo_pitch_depth[0] = pdepth1;
	if(rate2 > 10.0)			{rate2 = 10.0;}
	else if(rate2 < -10.0)		{rate2 = -10.0;}
	cp->lfo_rate[1] = rate2;
	if(adepth2 > 1.0)			{adepth2 = 1.0;}
	cp->lfo_amp_depth[1] = adepth2;
	if(fdepth2 > 2400.0)		{fdepth2 = 2400.0;}
	else if(fdepth2 < -2400.0)	{fdepth2 = -2400.0;}
	cp->lfo_cutoff_depth[1] = fdepth2;
	if (pdepth2 > 600.0)		{pdepth2 = 600.0;}
	else if (pdepth2 < -600.0)	{pdepth2 = -600.0;}
	cp->lfo_pitch_depth[1] = pdepth2;
}

void recompute_voice_lfo(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel;	
	Channel *cp = channel + ch;
	FLOAT_T rate1 = 0, rate2 = 0;

	// rate
	rate1 = vp->lfo_rate[0];
	if(otd.vibrato_rate != 0 ){
		rate1 += (double)cp->param_vibrato_rate * DIV_64 * nrpn_vib_rate; // NRPN in -64~63
	}else if(cp->param_vibrato_rate){ 
		int mul = (nrpn_vib_rate_mode == NRPN_PARAM_XG_RATE) ? 1 : 2;
		rate1 = calc_nrpn_param(rate1, cp->param_vibrato_rate * mul, nrpn_vib_rate_mode);// NRPN in -64~63
	}
	if(vp->paf_ctrl){ // PAf
		rate1 += cp->paf.lfo1_rate;
		rate2 += cp->paf.lfo2_rate;
	}
	rate1 += cp->lfo_rate[0];
	if(rate1 > VIBRATO_RATE_MAX)
		rate1 = VIBRATO_RATE_MAX;
#if 1 // 1:stop lfo
	else if(rate1 < 0)
		rate1 = 0;	
#else
	else if(rate1 < VIBRATO_RATE_MIN)
		rate1 = VIBRATO_RATE_MIN;	
#endif
	if(vp->sample->vibrato_freq < 0 || vp->sample->vibrato_sweep < 0)
		init_oscillator(&vp->lfo1, 0, 0, 0, OSC_TYPE_SINE, 0);
	else
		reset_oscillator(&vp->lfo1, rate1);	
	rate2 += vp->lfo_rate[1] + cp->lfo_rate[1];
	if(rate2 > TREMOLO_RATE_MAX)
		rate2 = TREMOLO_RATE_MAX;
#if 1 // 1:stop lfo
	else if(rate2 < 0)
		rate2 = 0;	
#else
	else if(rate2 < TREMOLO_RATE_MIN)
		rate2 = TREMOLO_RATE_MIN;
#endif
	if(vp->sample->tremolo_freq < 0 || vp->sample->tremolo_sweep < 0)
		init_oscillator(&vp->lfo2, 0, 0, 0, OSC_TYPE_SINE, 0);
	else
		reset_oscillator(&vp->lfo2, rate2);	
}

static void init_voice_lfo(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel;	
	Channel *cp = channel + ch;
	FLOAT_T rate1 = 0, rate2 = 0;
	int32 sweep1, sweep2, delay1, delay2;

	/* lfo1 */
	if(vp->sample->vibrato_freq < 0 || vp->sample->vibrato_sweep < 0){
		vp->lfo_amp_depth[0] = 0;
		vp->lfo_rate[0] = 0;
		init_oscillator(&vp->lfo1, 0, 0, 0, OSC_TYPE_SINE, 0);
	}else{
		vp->lfo_amp_depth[0] = 0;
		rate1 = vp->lfo_rate[0] = (FLOAT_T)vp->sample->vibrato_freq * DIV_1000;
		rate1 += cp->lfo_rate[0];
		if(rate1 > VIBRATO_RATE_MAX)
			rate1 = VIBRATO_RATE_MAX;
#if 1 // 1:stop lfo
		else if(rate1 < 0)
			rate1 = 0;	
#else
		else if(rate1 < VIBRATO_RATE_MIN)
			rate1 = VIBRATO_RATE_MIN;	
#endif
		sweep1 = (FLOAT_T)vp->sample->vibrato_sweep * playmode_rate_ms;
		delay1 = (FLOAT_T)vp->sample->vibrato_delay * DIV_1000;	
		if(otd.vibrato_delay != 0 ){
			delay1 += (double)cp->param_vibrato_delay * DIV_64 * nrpn_vib_delay; // NRPN in -64~63	
		}else if(cp->param_vibrato_delay){ 
			delay1 = calc_nrpn_param((double)delay1 * div_playmode_rate, cp->param_vibrato_delay * 2, nrpn_vib_delay_mode) * play_mode->rate; // NRPN in -64~63
		}
		delay1 += cp->vibrato_delay;
		if(delay1 < 0) delay1 = 0;
		init_oscillator(&vp->lfo1, rate1, delay1, sweep1, OSC_TYPE_SINE, 0);
	}
	/* lfo2 */	
	if(vp->sample->tremolo_freq < 0 || vp->sample->tremolo_sweep < 0){
		vp->lfo_amp_depth[1] = 0;
		vp->lfo_rate[1] = 0;
		init_oscillator(&vp->lfo2, 0, 0, 0, OSC_TYPE_SINE, 0);
	}else{
		vp->lfo_amp_depth[1] = 0;
		rate2 = vp->lfo_rate[1] = (FLOAT_T)vp->sample->tremolo_freq * DIV_1000;	
		rate2 += cp->lfo_rate[1];
		if(rate2 > TREMOLO_RATE_MAX)
			rate2 = TREMOLO_RATE_MAX;
#if 1 // 1:stop lfo
		else if(rate2 < 0)
			rate2 = 0;	
#else
		else if(rate2 < TREMOLO_RATE_MIN)
			rate2 = TREMOLO_RATE_MIN;
#endif
		sweep2 = (FLOAT_T)vp->sample->tremolo_sweep * playmode_rate_ms;
		delay2 = (FLOAT_T)vp->sample->tremolo_delay * DIV_1000;	
		if(delay2 < 0) delay2 = 0;
		init_oscillator(&vp->lfo2, rate2, delay2, sweep2, OSC_TYPE_SINE, 0);
	}
}

///r
const FLOAT_T cvt_pitch_cent_tune = (FLOAT_T)M_13BIT * DIV_100; 
// cent to tuning (100cent = 1semitone = 13bit , 
// bend_coarse[tuning >> 13] , bend_fine[(tuning >> 5) & 0xff]

static void recompute_channel_pitch(int ch)
{
	Channel *cp = channel + ch;
	const int32 pitch_max = (24 << 13); // semitone to tune
	const int32 pitch_min = (-24 << 13); // semitone to tune
	int32 tuning = 0;	

	/* MIDI controllers pitch control */
	if (opt_channel_pressure) {
		tuning += get_midi_controller_pitch(&(cp->mod))
		//	+ get_midi_controller_pitch(&(cp->bend)) 
			+ get_midi_controller_pitch(&(cp->caf))
			+ get_midi_controller_pitch(&(cp->cc1))
			+ get_midi_controller_pitch(&(cp->cc2))
			+ get_midi_controller_pitch(&(cp->cc3))
			+ get_midi_controller_pitch(&(cp->cc4));
	}	
	if(tuning > pitch_max) {tuning = pitch_max;}
	else if(tuning < pitch_min) {tuning = pitch_min;}	
	/* At least for GM2, it's recommended not to apply master_tuning for drum channels */
	tuning += ISDRUMCHANNEL(ch) ? 0 : master_tuning;
	/* Pitch Bend Control */
	if (cp->pitchbend != 0x2000){
		int val = cp->pitchbend - 0x2000;
		int pitch = (val > 0) ? cp->rpnmap[RPN_ADDR_0000] : cp->rpnmap[RPN_ADDR_0040]; // - : low control
		pitch = pitch > 0x7F ? (pitch - 128) : pitch; // uint8 to int
		tuning += val * pitch; // 13bit
	}
	/* Master Fine Tuning, Master Coarse Tuning */
	/* fine: [0..128] => [-256..256]
	 * 1 coarse = 256 fine (= 1 note)
	 * 1 fine = 2^5 tuning
	 */
	tuning += (cp->rpnmap[RPN_ADDR_0001] - 0x40
			+ (cp->rpnmap[RPN_ADDR_0002] - 0x40) * 64) << 7;	
	cp->pitch = tuning;
	/* XG Detune */
	/* -128~+127 to -12.8Hz~+12.7Hz (at A3 440Hz */
	if(cp->detune_param != 0x80)
		cp->detune = (440.0 + (FLOAT_T)((int)cp->detune_param - 0x80) * DIV_10) * DIV_440;
	else
		cp->detune = 1.0;
	/* Temper Control */
	if (! opt_pure_intonation && opt_temper_control) {
		int8 tt = cp->temper_type;
		uint8 tp = cp->rpnmap[RPN_ADDR_0003];
		int32 *table = NULL;
		switch (tt) {
		case 0:
			table = freq_table_tuning[tp];
			break;
		case 1:
			if (current_temper_keysig < 8)
				table = freq_table_pytha[current_temper_freq_table];
			else
				table = freq_table_pytha[current_temper_freq_table + 12];
			break;
		case 2:
			if (current_temper_keysig < 8)
				table = freq_table_meantone[current_temper_freq_table + ((temper_adj) ? 36 : 0)];
			else
				table = freq_table_meantone[current_temper_freq_table + ((temper_adj) ? 24 : 12)];
			break;
		case 3:
			if (current_temper_keysig < 8)
				table = freq_table_pureint[current_temper_freq_table + ((temper_adj) ? 36 : 0)];
			else
				table = freq_table_pureint[current_temper_freq_table + ((temper_adj) ? 24 : 12)];
			break;
		default:	/* user-defined temperament */
			if ((tt -= 0x40) >= 0 && tt < 4) {
				if (current_temper_keysig < 8)
					table = freq_table_user[tt][current_temper_freq_table + ((temper_adj) ? 36 : 0)];
				else
					table = freq_table_user[tt][current_temper_freq_table + ((temper_adj) ? 24 : 12)];
			} else
				table = freq_table;
			break;
		}
		cp->freq_table = table;
	}
}

///r
void recompute_voice_pitch(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel, note = vp->note;
	Channel *cp = channel + ch;
	int32 tuning;
	FLOAT_T depth1 = 0, depth2 = 0;
	const int32 porta_shift = 13 - PORTAMENTO_CONTROL_BIT; // tuning(13bit) - PORTAMENTO_CONTROL_RATIO(bit)

	if (! vp->sample->sample_rate)
		return;
	
	/* master tune , MIDI controllers pitch control */
	tuning = vp->init_tuning;
	tuning += cp->pitch;
	/* MIDI controllers */
	if(vp->paf_ctrl){ // PAf
		tuning += get_midi_controller_pitch(&(cp->paf));
		depth1 += get_midi_controller_lfo1_pitch_depth(&(cp->paf));
		depth2 += get_midi_controller_lfo2_pitch_depth(&(cp->paf));
	}
	/* LFO pitch depth */
	if(vp->lfo1.mode){
		depth1 += vp->sample->vibrato_to_pitch;
		depth1 += cp->lfo_pitch_depth[0];
		if (depth1 > 600.0) {depth1 = 600.0;}
		else if (depth1 < 0) {depth1 = 0;}
	//	tuning += vp->lfo1.out * (FLOAT_T)(depth1 << 13) * DIV_100 + 0.5;
		tuning += vp->lfo1.out * depth1 * cvt_pitch_cent_tune + 0.5; // * (1 << 13) cent to bend_fine/course
	}
	if(vp->lfo2.mode){
		depth2 += vp->sample->tremolo_to_pitch;		
		depth2 += cp->lfo_pitch_depth[1];	
		if (depth2 > 600.0) {depth2 = 600.0;}
		else if (depth2 < 0) {depth2 = 0;}
	//	tuning += vp->lfo2.out * (FLOAT_T)(depth2 << 13) * DIV_100 + 0.5;
		tuning += vp->lfo2.out * depth2 * cvt_pitch_cent_tune + 0.5; // * (1 << 13) cent to bend_fine/course
	}
	/* Modulation Envelope Pitch Depth , Pitch Envelope */
	if (opt_modulation_envelope) {
		// modulation envelope
		if (vp->sample->modenv_to_pitch) {
		//	tuning += vp->last_modenv_volume * (vp->sample->modenv_to_pitch << 13) * DIV_100 + 0.5;
			tuning += vp->mod_env.volume * (FLOAT_T)vp->sample->modenv_to_pitch * cvt_pitch_cent_tune + 0.5;
		}
		// pitch envelope
		if(vp->pit_env.vol){
			tuning += vp->pit_env.vol * cvt_pitch_cent_tune + 0.5; // cent to bend_fine/course
		}
	}
	/* Portamento / Legato */
	if (opt_portamento){
#if (PORTAMENTO_CONTROL_BIT == 13)
		tuning += vp->porta_out;
#else
		tuning += (vp->porta_out << (13 - PORTAMENTO_CONTROL_BIT)); // tuning(13bit)
#endif
	}
	/* calc freq */ /* XG Detune */ /* GS Pitch Offset Fine */
	if (tuning == 0){
		if (cp->detune == 1.0 && vp->sample->tune == 1.0 && cp->pitch_offset_fine == 0.0){
			vp->pitchfactor = 1.0;
			vp->orig_pitchfactor = 1.0;
			vp->frequency = vp->orig_frequency;
		}else{
			vp->pitchfactor = cp->detune * vp->sample->tune;
			vp->orig_pitchfactor = 1.0;
			vp->frequency = vp->orig_frequency * vp->pitchfactor + cp->pitch_offset_fine * 1000.0;
			vp->cache = NULL;
		}
		vp->prev_tuning = 0; 
	} else {
		if (vp->prev_tuning != tuning) {
			if (tuning >= 0)
				vp->pitchfactor = bend_fine[(tuning >> 5) & 0xff] * bend_coarse[(tuning >> 13) & 0x7f];
			else
				vp->pitchfactor = 1.0 / (bend_fine[((-tuning) >> 5) & 0xff] * bend_coarse[((-tuning) >> 13) & 0x7f]);
			vp->orig_pitchfactor = vp->pitchfactor;
			vp->prev_tuning = tuning;
		}
		vp->pitchfactor = vp->orig_pitchfactor * cp->detune * vp->sample->tune;
		vp->frequency = vp->orig_frequency * vp->pitchfactor + cp->pitch_offset_fine * 1000.0;
		vp->cache = NULL;
	}
	
}

static void init_voice_pitch(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel, note = vp->note;
	Channel *cp = channel + ch;
	int32 tuning = 0;
	
	if (! vp->sample->sample_rate)
		return;
	/* for Drum */
	if (ISDRUMCHANNEL(ch) && cp->drums[note] != NULL){
		/* for NRPN Coarse Pitch of Drum (GS) & Fine Pitch of Drum (XG) */
		if(cp->drums[note]->fine || cp->drums[note]->coarse)
			tuning += (cp->drums[note]->fine + cp->drums[note]->coarse * 64) << 7;		
		/* Velocity Pitch Sense */
		if(cp->drums[note]->drum_velo_pitch_sens)
			tuning += (FLOAT_T)cp->drums[note]->drum_velo_pitch_sens * DIV_16
				* (FLOAT_T)vp->velocity * DIV_127 * 150.0 * cvt_pitch_cent_tune + 0.5; // -16~+16 max150cent
	}
	/* GS/XG - Scale Tuning */
	if (! ISDRUMCHANNEL(ch)) {
		int8 st = cp->scale_tuning[note % 12];
		tuning += ((st << 13) + 50) / 100;
		if (cp->prev_scale_tuning != st) {
			cp->prev_scale_tuning = st;
		}
	}
	/* Temper Control */
	if (! opt_pure_intonation && opt_temper_control && vp->temper_instant) {
		vp->orig_frequency = cp->freq_table[note];
	}
	/* init_tuning */
	vp->init_tuning = tuning;
}

///r
static void recompute_channel_amp(int ch)
{	
	Channel *cp = channel + ch;
	FLOAT_T tempamp;

	/* MIDI volume expression are linear in perceived volume, 0-127
	 * use a lookup table for the non-linear scalings		 */	
	if (opt_user_volume_curve != 0)
		tempamp = user_vol_table[cp->volume] * user_vol_table[cp->expression]; /* 14 bits */
	else switch(play_system_mode){
	case SD_SYSTEM_MODE:
	case GM2_SYSTEM_MODE:
		tempamp = gm2_vol_table[cp->volume] * gm2_vol_table[cp->expression]; /* 14 bits */
		break;
	case GS_SYSTEM_MODE:	/* use measured curve */ 
	case CM_SYSTEM_MODE:	/* use measured curve */ 
		tempamp = sc_vol_table[cp->volume] * sc_vol_table[cp->expression]; /* 14 bits */
		break;
	default:
		if (IS_CURRENT_MOD_FILE)	/* use linear curve */
			tempamp = cp->volume * cp->expression; /* 14 bits */
		else	/* use generic exponential curve */
			tempamp = perceived_vol_table[cp->volume] *	perceived_vol_table[cp->expression]; /* 14 bits */
		break;
	}
	/* MIDI controllers amplitude control */
	if(opt_channel_pressure) {
		FLOAT_T ctrl = 1.0;
		ctrl += get_midi_controller_amp(&(cp->mod))
			+ get_midi_controller_amp(&(cp->bend))
			+ get_midi_controller_amp(&(cp->caf))
			+ get_midi_controller_amp(&(cp->cc1))
			+ get_midi_controller_amp(&(cp->cc2))
			+ get_midi_controller_amp(&(cp->cc3))
			+ get_midi_controller_amp(&(cp->cc4));
		if(ctrl > 2.0) ctrl = 2.0; // +100% 
		else if(ctrl < 0.0) ctrl = 0.0; // -100%
		tempamp *= ctrl;
	}
	tempamp *= DIV_14BIT;
	if(tempamp > 1.0)	tempamp = 1.0;
	/* for XG channel mixer */
	if(adjust_panning_immediately && play_system_mode == XG_SYSTEM_MODE){
		cp->amp = 1.0;	// for voice amp
		if(!(play_mode->encoding & PE_MONO)){		
			int pan = (cp->panning == NO_PANNING) ? 64 : cp->panning;
			tempamp *= DIV_7BIT; // pan_table 7bit
			cp->ch_amp[0] = tempamp * pan_table[128 - pan];
			cp->ch_amp[1] = tempamp * pan_table[pan];
		}else{
			cp->ch_amp[0] = cp->ch_amp[1] = tempamp;
		}
	}else{
		cp->amp = tempamp;	// for voice amp
		cp->ch_amp[0] = cp->ch_amp[1] = 1.0;
	}
}	

///r
void recompute_voice_amp(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel;
	Channel *cp = channel + ch;
	FLOAT_T tempamp;	
	FLOAT_T depth1 = 0, depth2 = 0;
	
	/* velocity, NRPN MIDI controllers amplitude control */
	tempamp = vp->init_amp;
	/* volume, expression, MIDI controllers amplitude control */
	tempamp *= cp->amp;	
	if(vp->paf_ctrl){ // PAf
		tempamp *= 1.0 + get_midi_controller_amp(&(cp->paf));
		// LFO amp
		depth1 += get_midi_controller_lfo1_amp_depth(&(cp->paf));
		depth2 += get_midi_controller_lfo2_amp_depth(&(cp->paf));
	}	
	/* applying panning to amplitude */
	if(vp->panned == PANNED_MYSTERY)
   	{
		if(vp->pan_ctrl){ // update panning
			// panning : 0 ~ 127 // 入力されるパン,テーブルの値域
			// pan : 0=center // パンの加減はセンター0基準で処理
			// sample_pan : -0.5~+0.5 0.0=center
			int pan = 0, note = vp->note;
			
			if(ISDRUMCHANNEL(ch) && cp->drums[note] != NULL && cp->drums[note]->drum_panning != NO_PANNING)
				pan += (int)cp->drums[note]->drum_panning - 64;
			else
				pan += (int)vp->sample->def_pan - 64;
			if(!(adjust_panning_immediately && play_system_mode == XG_SYSTEM_MODE) && cp->panning != NO_PANNING)
				pan += (int)cp->panning - 64;
			pan += 64;
			if (pan > 127)
				pan = 127;
			else if (pan < 0)
				pan = 0;
			vp->pan_amp[0] = pan_table[128 - pan] * (0.5 - vp->sample->sample_pan);
			vp->pan_amp[1] = pan_table[pan] * (0.5 + vp->sample->sample_pan);
			vp->pan_ctrl = 0; // done
		}
		tempamp *= DIV_6BIT; // 7bit-1
		vp->left_amp = tempamp * vp->pan_amp[0];
		vp->right_amp = tempamp * vp->pan_amp[1];
   	} else { // mono
		vp->right_amp = vp->left_amp = tempamp * DIV_7BIT; // 7bit
    }
	
	// LFO to amp
	// vol_envを使用しない 後の工程  mix.c update_tremolo() -> apply_envelope_to_amp() -> mix_mystery_signal()
	depth1 += (FLOAT_T)vp->sample->vibrato_to_amp * DIV_10000;
	depth1 += cp->lfo_amp_depth[0];
	if(depth1 > 1.0) {depth1 = 1.0;}
	else if(depth1 < 0.0) {depth1 = 0.0;}
	vp->lfo_amp_depth[0] = depth1;	
	depth2 += (FLOAT_T)vp->sample->tremolo_to_amp * DIV_10000;
	depth2 += cp->lfo_amp_depth[1];
	if(depth2 > 1.0) {depth2 = 1.0;}
	else if(depth2 < 0.0) {depth2 = 0.0;}
	vp->lfo_amp_depth[1] = depth2;
}

static int32 calc_velocity(int32 ch,int32 vel)
{
	int32 velocity;
	velocity = channel[ch].velocity_sense_depth * vel / 64 + (channel[ch].velocity_sense_offset - 64) * 2;
	if(velocity > 127) {velocity = 127;}
	return velocity;
}

///r
static void init_voice_amp(int v)
{
	Voice *vp = &voice[v];
	int ch = vp->channel;
	int32 velo = calc_velocity(ch, vp->velocity) & 0x7F;
	FLOAT_T tempamp;

	/* input_volume and other are percentages, used to scale
	 *  amplitude directly, NOT perceived volume	*/	
	tempamp = input_volume * vp->sample->volume * vp->sample->cfg_amp;
	/* MIDI velocity are linear in perceived volume, 0-127
	 * use a lookup table for the non-linear scalings		 */	
	if (opt_user_volume_curve != 0)
		tempamp *= user_vol_table[velo]; /* 7 bits */
	else switch(play_system_mode){
	case SD_SYSTEM_MODE:
	case GM2_SYSTEM_MODE:
		tempamp *= gm2_vol_table[velo];	/* velocity: not in GM2 standard *//* 7 bits */
		break;
	case GS_SYSTEM_MODE:	/* use measured curve */ 
	case CM_SYSTEM_MODE:	/* use measured curve */ 
		tempamp *= sc_vel_table[velo]; /* 7 bits */
		break;
	default:
		if (IS_CURRENT_MOD_FILE)	/* use linear curve */
			tempamp *= velo; /* 7 bits */
		else	/* use generic exponential curve */
			tempamp *= perceived_vol_table[velo]; /* 7 bits */
		break;
	}	
	tempamp *= DIV_7BIT; // 7bit
	/* NRPN - drum instrument tva level */
	if(ISDRUMCHANNEL(ch)) {
		int note = vp->note;
		if(channel[ch].drums[note] != NULL)
			tempamp *= channel[ch].drums[note]->drum_level;
		tempamp *= (double)opt_drum_power * DIV_100;	/* global drum power */
	}
	/* filter gain */
	if (vp->fc.type != 0) {
		tempamp *= vp->lpf_gain;
	}
	/* init_amp */
	vp->init_amp = tempamp;
}

///r
double voice_filter_reso = 1.0;
double voice_filter_gain = 1.0;

#define VOICE_FILTER_FREQ_MIN (100) // Hz
#define VOICE_FILTER2_FREQ_MAX (2000) // Hz

/* lowpass/highpass filter */
static void recompute_channel_filter(int ch)
{
	Channel *cp = channel + ch;
	FLOAT_T reso = 0;
	FLOAT_T cutoff_cent = 0;

	if(cp->special_sample > 0) {return;}
	//if(!ISDRUMCHANNEL(ch)) {
	//		cp->cutoff_cent = 10000.0*sin(((double)cp->param_cutoff_freq + 128.0) * DIV_256 * M_PI + 4.71238898); // elion
		if(otd.filter_freq != 0)
			cutoff_cent += (FLOAT_T)cp->param_cutoff_freq * DIV_64 * nrpn_filter_freq;
	//	cp->hpf_cutoff_cent = (FLOAT_T)cp->hpf_param_cutoff_freq * DIV_64 * nrpn_filter_freq;
	//	}
	//	cp->resonance_dB = (double)cp->param_resonance * RESONANCE_COEFF; // elion
		reso += (FLOAT_T)cp->param_resonance * DIV_64 * nrpn_filter_reso;
		cp->hpf_resonance_dB = (FLOAT_T)cp->hpf_param_resonance * DIV_64 * nrpn_filter_reso;
	//}	
	cp->resonance_dB = reso;
	if(opt_channel_pressure) {
		cutoff_cent += get_midi_controller_filter_cutoff(&(cp->mod))
			+ get_midi_controller_filter_cutoff(&(cp->bend))
			+ get_midi_controller_filter_cutoff(&(cp->caf))
			+ get_midi_controller_filter_cutoff(&(cp->cc1))
			+ get_midi_controller_filter_cutoff(&(cp->cc2))
			+ get_midi_controller_filter_cutoff(&(cp->cc3))
			+ get_midi_controller_filter_cutoff(&(cp->cc4));
	}	
	if(cutoff_cent > 9600.0)
		cutoff_cent = 9600.0;
	else if(cutoff_cent < -9600.0)
		cutoff_cent = -9600.0;
	cp->cutoff_cent = cutoff_cent;
}

static void recompute_voice_filter2(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel, note = vp->note;
	Channel *cp = channel + ch;
	int val;
	FLOAT_T freq, reso, cent, coef = 1.0;
	FilterCoefficients *fc = &vp->fc2;

	if(fc->type == 0) {return;}
	
	/* MIDI controllers filter control */
	val = vp->init_hpf_val;
	freq = vp->hpf_orig_freq;
//	cent = vp->init_hpf_cent;
	reso = vp->init_hpf_reso;
	val += cp->hpf_param_cutoff_freq;
//	cent += cp->hpf_cutoff_cent;
	reso += cp->hpf_resonance_dB;	
	/* NRPN */
	if(val)
		freq = calc_nrpn_param(freq, val * 2, nrpn_filter_hpf_freq_mode); // NRPN in -64~63
	// convert cent to mul
//	if(cent != 0) {coef *= POW2(cent * DIV_1200);}
	// original param 	
//	freq = vp->hpf_orig_freq * (coef);
	reso = vp->hpf_orig_reso + reso;
	// Resonace effect control 
	reso *= voice_filter_reso;
	// Spec limit (SoundFont Spec 2.01 , other Sampler
	if(freq > VOICE_FILTER2_FREQ_MAX)	{freq = VOICE_FILTER2_FREQ_MAX;}
	else if(freq < 20)	{freq = 20;}
	if(reso > 96.0f)	{reso = 96.0f;}
	else if(reso < 0.0f){reso = 0.0f;}
	// set filter param
	set_voice_filter2_freq(fc, freq);
	set_voice_filter2_reso(fc, reso);
}

void recompute_voice_filter(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel, note = vp->note;
	Channel *cp = channel + ch;
	int val;
	FLOAT_T freq, reso, cent, coef = 1.0, depth1 = 0, depth2 = 0;
	FilterCoefficients *fc = &vp->fc;
	Sample *sp = vp->sample;

	recompute_voice_filter2(v); // hpf
	if(fc->type == 0) {return;}	
	
	// original param 
	freq = vp->lpf_orig_freq;	
	reso = vp->lpf_orig_reso;
	/* MIDI controllers filter control */	
	val = vp->init_lpf_val;
	cent = vp->init_lpf_cent;
	reso += vp->init_lpf_reso;
	val += cp->param_cutoff_freq;
	cent += cp->cutoff_cent;
	reso += cp->resonance_dB;
	/* NRPN */
	if(otd.filter_freq == 0)
		if(val)
			freq = calc_nrpn_param(freq, val * 2, nrpn_filter_freq_mode); // NRPN in -64~63
	/* Soft Pedal */
	if(cp->soft_pedal != 0) {
		if(note > 49) {	/* tre corde */
			coef *= 1.0 - 0.20 * ((FLOAT_T)cp->soft_pedal) * DIV_127;
		} else {	/* una corda (due corde) */
			coef *= 1.0 - 0.25 * ((FLOAT_T)cp->soft_pedal) * DIV_127;
		}
	}	
	if(vp->paf_ctrl){ // PAf
		cent += get_midi_controller_filter_cutoff(&(cp->paf));
		depth1 += get_midi_controller_lfo1_filter_depth(&(cp->paf));
		depth2 += get_midi_controller_lfo2_filter_depth(&(cp->paf));		
	}
	/* lfo1,lfo2 to filter cutoff frequency */
	if(vp->lfo1.mode){
		depth1 += (FLOAT_T)sp->vibrato_to_fc;
		depth1 += cp->lfo_cutoff_depth[0];
		cent += vp->lfo1.out * depth1;
	}	
	if(vp->lfo2.mode){
		depth2 += (FLOAT_T)sp->tremolo_to_fc;
		depth2 += cp->lfo_cutoff_depth[1];
		cent += vp->lfo2.out * depth2;
	}
	/* modulation_envelope to filter cutoff frequency */
	if(opt_modulation_envelope) {
		if(sp->modenv_to_fc || cp->modenv_depth) {
			cent += ((FLOAT_T)sp->modenv_to_fc + cp->modenv_depth) * vp->mod_env.volume;
		}
	}
	// convert cent to mul
	if(cent != 0) {coef *= POW2(cent * DIV_1200);}	
	freq *= coef;	
	// Resonace effect control 
	reso *= voice_filter_reso;
	// filter test
//	freq = 20; // test freq
//	reso = 48.0f; // test reso
	// cutoff low limit
	if(sp->cutoff_low_limit < 0 && !ISDRUMCHANNEL(ch))
		vp->lpf_freq_low_limit = (FLOAT_T)vp->frequency * DIV_1000; // update
	if(freq < vp->lpf_freq_low_limit)
		freq = vp->lpf_freq_low_limit;
	// Spec limit (SoundFont Spec 2.01 , other Sampler
	if(freq > 20000)	{freq = 20000;}
	else if(freq < VOICE_FILTER_FREQ_MIN)	{freq = VOICE_FILTER_FREQ_MIN;}
	if(reso > 96.0f)	{reso = 96.0f;}
	else if(reso < 0.0f){reso = 0.0f;}		
	// set filter param
	set_voice_filter1_freq(fc, freq);
	set_voice_filter1_reso(fc, reso);
}

void init_voice_filter2(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel, note = vp->note;
	FilterCoefficients *fc = &vp->fc2;
	Sample *sp = vp->sample;
	FLOAT_T freq, reso;
	
#ifdef __W32__
	if(IsBadReadPtr(sp, sizeof(Sample))){
#else
	if(sp == NULL || sp == -1l){
#endif		
		ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "ERROR! ch:%d note:%d  sample null pointer.", ch, note);
		fc->type = 0;
		return;
	}

	if(sp->hpf[0] >= 0)
		set_voice_filter2_type(fc, sp->hpf[0]);
	else if(opt_hpf_def)
		set_voice_filter2_type(fc, opt_hpf_def);
	else
		set_voice_filter2_type(fc, 0);
	if(fc->type == 0)
		return;
	set_voice_filter2_ext_rate(fc, 0); // set sample rate

	if(sp->hpf[1] > 0)
		freq = sp->hpf[1];
	else
		freq = 10.0;	
	if(sp->hpf[2] > 0)
		reso = (FLOAT_T)sp->hpf[2];
	else
		reso = 0.0;
	// Resonace effect control 
	reso *= voice_filter_reso;
	// Spec limit (SoundFont Spec 2.01 , other Sampler
	if(freq > VOICE_FILTER2_FREQ_MAX)	{freq = VOICE_FILTER2_FREQ_MAX;}
	else if(freq < 10)	{freq = 10;}
	if(reso > 96.0f)	{reso = 96.0f;}
	else if(reso < 0.0f){reso = 0.0f;}
	// set filter param
	vp->hpf_orig_freq = freq;
	vp->hpf_orig_reso = reso;
	// optimize init
	{
		Channel *cp = channel + ch;
		int val = 0; 
	//	FLOAT_T cent = 0.0; 
		FLOAT_T reso = 0.0;		
		/* for Drum */
		if(ISDRUMCHANNEL(ch) && cp->drums[note] != NULL) {
			/* NRPN Drum Instrument HPF Cutoff */
			val += cp->drums[note]->drum_hpf_cutoff_freq;
		//	cent += (FLOAT_T)(cp->drums[note]->drum_hpf_cutoff_freq) * DIV_64 * nrpn_filter_freq;
			/* NRPN Drum Instrument HPF Resonance */
			reso += (FLOAT_T)cp->drums[note]->drum_hpf_resonance * DIV_64 * nrpn_filter_reso;
		}
		vp->init_hpf_val = val;
	//	vp->init_hpf_cent = cent;
		vp->init_hpf_reso = reso;
	}
}

void init_voice_filter(int v)
{
	Voice *vp = voice + v;
	int ch = vp->channel, note = vp->note;
	FilterCoefficients *fc = &vp->fc;
	Sample *sp = vp->sample;
	FLOAT_T freq, reso;
	
#ifdef __W32__
	if(IsBadReadPtr(sp, sizeof(Sample))){
#else
	if(sp == NULL || sp == -1l){
#endif		
		ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "ERROR! ch:%d note:%d  sample null pointer.", ch, note);
		fc->type = 0;
		return;
	}

	if(sp->lpf_type >= 0)
		set_voice_filter1_type(fc, sp->lpf_type);
	else if(opt_lpf_def)
		set_voice_filter1_type(fc, opt_lpf_def);
	else
		set_voice_filter1_type(fc, 0);
	if(fc->type == 0)
		return;
	set_voice_filter1_ext_rate(fc, 0); // set sample rate
	if(sp->cutoff_freq > 0)
		freq = sp->cutoff_freq;
	else
		freq = 20000;
	// SF2reso cB
	reso = (FLOAT_T)sp->resonance * DIV_10;
	// Resonace effect control 
	reso *= voice_filter_reso;
	// cutoff low limit	
	if(sp->cutoff_low_limit < 0){
		vp->lpf_freq_low_limit = VOICE_FILTER_FREQ_MIN; // update recompute_voice_filter()
	}else{
		double keyf = 1.0;
		if(sp->cutoff_low_keyf)
			keyf = POW2((double)(vp->note - 60) * (double)sp->cutoff_low_keyf * DIV_1200);	
		vp->lpf_freq_low_limit = sp->cutoff_low_limit * keyf;
	}
	// Spec limit (SoundFont Spec 2.01 , other Sampler
	if(freq > 20000)	{freq = 20000;}
	else if(freq < VOICE_FILTER_FREQ_MIN)	{freq = VOICE_FILTER_FREQ_MIN;}
	if(reso > 96.0f)	{reso = 96.0f;}
	else if(reso < 0.0f){reso = 0.0f;}
	// set filter param
	vp->lpf_orig_freq = freq;
	vp->lpf_orig_reso = reso;
	// Filter gain control 
	vp->lpf_gain = voice_filter_gain;
	
	// optimize init
	{
		int ch = vp->channel, note = vp->note;
		Channel *cp = channel + ch;
		int val = 0; 
		FLOAT_T cent = 0.0; reso = 0.0;	
		
		/* for Drum */
		if(ISDRUMCHANNEL(ch) && cp->drums[note] != NULL) {
			/* NRPN Drum Instrument Filter Cutoff */
			if(otd.filter_freq == 0)
				val += cp->drums[note]->drum_cutoff_freq;
			else
				cent += (FLOAT_T)(cp->drums[note]->drum_cutoff_freq) * DIV_64 * nrpn_filter_freq;		
			/* NRPN Drum Instrument Filter Resonance */
			reso += (FLOAT_T)cp->drums[note]->drum_resonance * DIV_64 * nrpn_filter_reso;
			/* Velocity LPF Cutoff Sense */
			if(cp->drums[note]->drum_velo_cutoff_sens)
			cent += (FLOAT_T)cp->drums[note]->drum_velo_cutoff_sens * DIV_16
				* (FLOAT_T)vp->velocity * DIV_127 * 150.0; // -16~+16 max600cent
		}
		/* velocity to filter cutoff frequency */
		if(sp->vel_to_fc) {	
			if(vp->velocity > sp->vel_to_fc_threshold)
				cent += (FLOAT_T)sp->vel_to_fc * ( (127.0 - (FLOAT_T)vp->velocity) * DIV_127);
			else
				cent += (FLOAT_T)sp->vel_to_fc * ( (127.0 - (FLOAT_T)sp->vel_to_fc_threshold) * DIV_127);
		}
		/* velocity to filter resonance */
		if(sp->vel_to_resonance) {
			reso += (FLOAT_T)vp->velocity * DIV_127 * DIV_10 * sp->vel_to_resonance;
		}
		/* filter cutoff key-follow */
		if(sp->key_to_fc) {
			cent += (FLOAT_T)sp->key_to_fc * (FLOAT_T)(vp->note - sp->key_to_fc_bpo);
		}
		vp->init_lpf_val = val;
		vp->init_lpf_cent = cent;
		vp->init_lpf_reso = reso;
	}
}

void recompute_resample_filter(int v)
{
	Voice *vp = voice + v;
	FilterCoefficients *fc = &vp->rf_fc;
	FLOAT_T freq;
	
	if(!fc->type)
		return;	
	freq = vp->rs_sample_rate_root * (FLOAT_T)vp->frequency;  // rate/2 rate>0
	if(freq > playmode_rate_div2) 
		freq = playmode_rate_div2; // rate/2
	set_resample_filter_freq(fc, freq);
}

void init_resample_filter(int v)
{	
	Voice *vp = voice + v;
	Sample *sp = vp->sample;
	FilterCoefficients *fc = &vp->rf_fc;
	FLOAT_T freq;

#ifdef INT_SYNTH
	if(sp->inst_type == INST_MMS || sp->inst_type == INST_SCC){
		set_resample_filter_type(fc, 0); // off
		return;
	}
#endif
	if(sp->sample_rate_org)
		vp->rs_sample_rate_root = (FLOAT_T)sp->sample_rate_org * DIV_2 / (FLOAT_T)sp->root_freq_org;
	else
		vp->rs_sample_rate_root = (FLOAT_T)sp->sample_rate * DIV_2 / (FLOAT_T)sp->root_freq;
	if(opt_resample_over_sampling)
		set_resample_filter_ext_rate(fc, play_mode->rate * opt_resample_over_sampling); // set sample rate * n
	else
		set_resample_filter_ext_rate(fc, 0); // set sample rate
	set_resample_filter_type(fc, opt_resample_filter);
}

float calc_drum_tva_level(int ch, int note, int level)
{
	int def_level, nbank, nprog;
	ToneBank *bank;
	int elm = 0;

	if(channel[ch].special_sample > 0) {return 1.0;}

	nbank = channel[ch].bank;
	nprog = note;
	instrument_map(channel[ch].mapID, &nbank, &nprog);

	if(ISDRUMCHANNEL(ch)) {
		bank = drumset[nbank];
		if(bank == NULL) {bank = drumset[0];}
	} else {
		return 1.0;
	}
///r
	if(bank->tone[nprog][elm] == NULL)
		def_level = -1;
	else
		def_level = bank->tone[nprog][elm]->tva_level;

	if(def_level == -1 || def_level == 0) {def_level = 127;}
	else if(def_level > 127) {def_level = 127;}

	return (sc_drum_level_table[level] / sc_drum_level_table[def_level]);
}

///r
static int32 calc_emu_delay(MidiEvent *e)
{
	static int32 cnt = 0, ptime = -1;
	static uint8 ch[MAX_CHANNELS * 128], note[MAX_CHANNELS * 128]; // note 0~127
	int i, match;

	if(ISDRUMCHANNEL(e->channel)) return 0;

	if(e->time != ptime)
		cnt = 0;
	else
		cnt++;
	ptime = e->time;
	ch[cnt] = e->channel;
	note[cnt] = MIDI_EVENT_NOTE(e);

	if(cnt == 0) return 0;

	match = 0;
	for(i = 0; i < cnt; i++)
		if(ch[i] != ch[cnt] && note[i] == note[cnt])
			match++;

//	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "emu_delay_time: %d : %d", match, test); // test

	if(match == 0)  return 0;
	return (int32)((FLOAT_T)match * playmode_rate_dms * emu_delay_time); // 0.1msec

}

static int32 calc_random_delay(int ch, int note)
{
	int nbank, nprog;
	ToneBank *bank;
	int elm = 0;

	if(channel[ch].special_sample > 0) {return 0;}

	nbank = channel[ch].bank;

	if(ISDRUMCHANNEL(ch)) {
		nprog = note;
		instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = drumset[nbank];
		if (bank == NULL) {bank = drumset[0];}
	} else {
		nprog = channel[ch].program;
		if(nprog == SPECIAL_PROGRAM) {return 0;}
		instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = tonebank[nbank];
		if(bank == NULL) {bank = tonebank[0];}
	}

	if (bank->tone[nprog][elm] == NULL) {return 0;}
	if (bank->tone[nprog][elm]->rnddelay == 0) {return 0;}
	else {return (int32)((double)bank->tone[nprog][elm]->rnddelay * playmode_rate_ms
		* (get_pink_noise_light(&global_pink_noise_light) + 1.0f) * 0.5);}
}

static void recompute_bank_parameter_tone(int ch)
{
	int nbank, nprog;
	ToneBank *bank;
	int elm = 0;

	if(channel[ch].special_sample > 0) {return;}
	if(ISDRUMCHANNEL(ch)) {return;}
	nbank = channel[ch].bank;
	nprog = channel[ch].program;
	if (nprog == SPECIAL_PROGRAM) {return;}
	instrument_map(channel[ch].mapID, &nbank, &nprog);
	bank = tonebank[nbank];
	if (bank == NULL) {bank = tonebank[0];}

	if(bank->tone[nprog][elm]){
		if(channel[ch].legato == -1)
			channel[ch].legato = bank->tone[nprog][elm]->legato;
		channel[ch].damper_mode = bank->tone[nprog][elm]->damper_mode;
		channel[ch].loop_timeout = bank->tone[nprog][elm]->loop_timeout;
	}else{
		channel[ch].legato = 0;
		channel[ch].damper_mode = 0;
		channel[ch].loop_timeout = 0;
	}
}

static void recompute_bank_parameter_drum(int ch, int note)
{
	int nbank, nprog;
	ToneBank *bank;
	struct DrumParts *drum;
	int elm = 0;

	if(channel[ch].special_sample > 0) {return;}

	nbank = channel[ch].bank;

	if(ISDRUMCHANNEL(ch)) {
		nprog = note;
		instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = drumset[nbank];
		if (bank == NULL) {bank = drumset[0];}
		if (channel[ch].drums[note] == NULL){
			play_midi_setup_drums(ch, note);
		}
		drum = channel[ch].drums[note];
		if(bank->tone[nprog][elm]){
			if(drum->reverb_level == -1 && bank->tone[nprog][elm]->reverb_send != -1)
				drum->reverb_level = bank->tone[nprog][elm]->reverb_send;
			if(drum->chorus_level == -1 && bank->tone[nprog][elm]->chorus_send != -1)
				drum->chorus_level = bank->tone[nprog][elm]->chorus_send;
			if(drum->delay_level == -1 && bank->tone[nprog][elm]->delay_send != -1)
				drum->delay_level = bank->tone[nprog][elm]->delay_send;
			if (drum->rx_note_off == -1)
				set_rx_drum(drum, RX_NOTE_OFF, bank->tone[nprog][elm]->rx_note_off);
		}
	}
}

///r
Instrument *play_midi_load_instrument(int dr, int bk, int prog, int elm, int *elm_max)
{
	ToneBank **bank = (dr) ? drumset : tonebank;
	ToneBankElement *tone, *tone0;
	Instrument *ip = NULL;
	int load_success = 0;
	int tmp_num = 0;

	if (bank[bk] == NULL)
		alloc_instrument_bank(dr, bk);
	
	tone = bank[bk]->tone[prog][elm];
	/* tone->name is NULL if "soundfont" directive is used, and ip is NULL when not preloaded */
	/* dr: not sure but only drumsets are concerned at the moment */
	if(tone){
		if (dr && !tone->name && ((ip = tone->instrument) == NULL)
			  && (ip = load_instrument(dr, bk, prog, elm)) != NULL) {
			tone->instrument = ip;
			tone->name = safe_strdup(DYNAMIC_INSTRUMENT_NAME);
			load_success = 1;
			tmp_num = bank[bk]->tone[prog][0]->element_num;
			goto end;
		} else if (tone->name) {
			/* Instrument is found. */
			ip = tone->instrument;
#ifndef SUPPRESS_CHANNEL_LAYER
			if (ip == NULL){
				ip = tone->instrument = load_instrument(dr, bk, prog, elm);
			}
#endif
			if (ip == NULL || IS_MAGIC_INSTRUMENT(ip)) {
				tone->instrument = MAGIC_ERROR_INSTRUMENT;
			} else {
				load_success = 1;
				tmp_num = bank[bk]->tone[prog][0]->element_num;
			}
			goto end;
		}
	}
	/* Instrument is not found.
		Try to load the instrument from bank 0 */
	tone0 = bank[0]->tone[prog][elm];
	if(tone0){
		if ((ip = tone0->instrument) == NULL)
			ip = tone0->instrument = load_instrument(dr, 0, prog, elm);
		if (ip == NULL || IS_MAGIC_INSTRUMENT(ip)) {
			tone0->instrument = MAGIC_ERROR_INSTRUMENT;
		} else {	
			if(tone == NULL){	
				if(alloc_tone_bank_element(&bank[bk]->tone[prog][elm])){
					ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "play_midi_load_instrument ToneBankElement malloc error ");
					return NULL;
				}
				tone = bank[bk]->tone[prog][elm];
			}
			copy_tone_bank_element(tone, tone0);
			tone->instrument = ip;
			load_success = 1;
			tmp_num = bank[0]->tone[prog][0]->element_num;
			goto end;
		}
	}
end:
	if(elm_max)
		*elm_max = tmp_num;
	if (load_success)
		aq_add(NULL, 0);	/* Update software buffer */

	if (ip == MAGIC_ERROR_INSTRUMENT)
		return NULL;

	return ip;
}


#if 0
/* reduce_voice_CPU() may not have any speed advantage over reduce_voice().
 * So this function is not used, now.
 */

/* The goal of this routine is to free as much CPU as possible without
   loosing too much sound quality.  We would like to know how long a note
   has been playing, but since we usually can't calculate this, we guess at
   the value instead.  A bad guess is better than nothing.  Notes which
   have been playing a short amount of time are killed first.  This causes
   decays and notes to be cut earlier, saving more CPU time.  It also causes
   notes which are closer to ending not to be cut as often, so it cuts
   a different note instead and saves more CPU in the long run.  ON voices
   are treated a little differently, since sound quality is more important
   than saving CPU at this point.  Duration guesses for loop regions are very
   crude, but are still better than nothing, they DO help.  Non-looping ON
   notes are cut before looping ON notes.  Since a looping ON note is more
   likely to have been playing for a long time, we want to keep it because it
   sounds better to keep long notes.
*/
static int reduce_voice_CPU(void)
{
    int32 lv, v, vr;
    int i, j, lowest=-0x7FFFFFFF;
    int32 duration;

    i = upper_voices;
    lv = 0x7FFFFFFF;
    
    /* Look for the decaying note with the longest remaining decay time */
    /* Protect drum decays.  They do not take as much CPU (?) and truncating
       them early sounds bad, especially on snares and cymbals */
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
	/* skip notes that don't need resampling (most drums) */
	if (voice[j].sample->note_to_use)
	    continue;
	if(voice[j].status & ~(VOICE_ON | VOICE_DIE | VOICE_SUSTAINED))
	{
	    /* Choose note with longest decay time remaining */
	    /* This frees more CPU than choosing lowest volume */
	    if (!voice[j].envelope_increment) duration = 0;
	    else duration =
	    	(voice[j].envelope_target - voice[j].envelope_volume) /
	    	voice[j].envelope_increment;
	    v = -duration;
	    if(v < lv)
	    {
		lv = v;
		lowest = j;
	    }
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	/* This can still cause a click, but if we had a free voice to
	   spare for ramping down this note, we wouldn't need to kill it
	   in the first place... Still, this needs to be fixed. Perhaps
	   we could use a reserve of voices to play dying notes only. */

	cut_notes++;
	return lowest;
    }

    /* try to remove VOICE_DIE before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(voice[j].status & ~(VOICE_ON | VOICE_SUSTAINED))
      {
	/* continue protecting non-resample decays */
	if (voice[j].status & ~(VOICE_DIE) && voice[j].sample->note_to_use)
		continue;

	/* choose note which has been on the shortest amount of time */
	/* this is a VERY crude estimate... */
	if (voice[j].sample->modes & MODES_LOOPING)
	    duration = voice[j].sample_offset - voice[j].sample->loop_start;
	else
	    duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = duration;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -1)
    {
	cut_notes++;
	return lowest;
    }

    /* try to remove VOICE_SUSTAINED before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(voice[j].status & VOICE_SUSTAINED)
      {
	/* choose note which has been on the shortest amount of time */
	/* this is a VERY crude estimate... */
	if (voice[j].sample->modes & MODES_LOOPING)
	    duration = voice[j].sample_offset - voice[j].sample->loop_start;
	else
	    duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = duration;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;
	return lowest;
    }
	
    lost_notes++;

    /* try to remove non-looping voices first */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(!(voice[j].sample->modes & MODES_LOOPING))
      {
	/* score notes based on both volume AND duration */
	/* this scoring function needs some more tweaking... */
	duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = voice[j].left_mix * duration;
	vr = voice[j].right_mix * duration;
	if(voice[j].panned == PANNED_MYSTERY && vr > v)
	    v = vr;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	return lowest;
    }

    lv = 0x7FFFFFFF;
    lowest = 0;
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
	if (!(voice[j].sample->modes & MODES_LOOPING)) continue;

	/* score notes based on both volume AND duration */
	/* this scoring function needs some more tweaking... */
	duration = voice[j].sample_offset - voice[j].sample->loop_start;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = voice[j].left_mix * duration;
	vr = voice[j].right_mix * duration;
	if(voice[j].panned == PANNED_MYSTERY && vr > v)
	    v = vr;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }

    return lowest;
}
#endif

/* this reduces voices while maintaining sound quality */
static int reduce_voice(void)
{
    int32 lv, v;
    int i, j, lowest=-0x7FFFFFFF;

    i = upper_voices;
    lv = 0x7FFFFFFF;
    
    /* Look for the decaying note with the smallest volume */
    /* Protect drum decays.  Truncating them early sounds bad, especially on
       snares and cymbals */
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE ||
	   (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
	    continue;
	
	if(voice[j].status & ~(VOICE_ON | VOICE_DIE | VOICE_SUSTAINED))
	{
	    /* find lowest volume */
	    v = voice[j].left_mix;
	    if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    	v = voice[j].right_mix;
	    if(v < lv)
	    {
		lv = v;
		lowest = j;
	    }
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	/* This can still cause a click, but if we had a free voice to
	   spare for ramping down this note, we wouldn't need to kill it
	   in the first place... Still, this needs to be fixed. Perhaps
	   we could use a reserve of voices to play dying notes only. */

	cut_notes++;
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* try to remove VOICE_DIE before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].status & ~(VOICE_ON | VOICE_SUSTAINED))
      {
	/* continue protecting drum decays */
	if (voice[j].status & ~(VOICE_DIE) &&
	    (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
		continue;
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -1)
    {
	cut_notes++;
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* try to remove VOICE_SUSTAINED before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].status & VOICE_SUSTAINED)
      {
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }
	
    lost_notes++;

    /* remove non-drum VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
        if(voice[j].status & VOICE_FREE ||
	   (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
	   	continue;

	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* remove all other types of notes */
    lv = 0x7FFFFFFF;
    lowest = 0;
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE)
	    continue;
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }

    free_voice(lowest);
    if(!prescanning_flag)
	ctl_note_event(lowest);
    return lowest;
}

void free_voice(int v1)
{
    voice[v1].status = VOICE_FREE;
    voice[v1].temper_instant = 0;
}

static int find_free_voice(void)
{
    int i, nv = voices, lowest;
    int32 lv, v;

    for(i = 0; i < nv; i++)
	if(voice[i].status == VOICE_FREE)
	{
	    if(upper_voices <= i)
		upper_voices = i + 1;
	    return i;
	}

    upper_voices = voices;

    /* Look for the decaying note with the lowest volume */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(i = 0; i < nv; i++)
    {
	if(voice[i].status & ~(VOICE_ON | VOICE_DIE) &&
	   !(voice[i].sample && voice[i].sample->note_to_use && ISDRUMCHANNEL(voice[i].channel)))
	{
	    v = voice[i].left_mix;
	    if((voice[i].panned==PANNED_MYSTERY) && (voice[i].right_mix>v))
		v = voice[i].right_mix;
	    if(v<lv)
	    {
		lv = v;
		lowest = i;
	    }
	}
    }
    if(lowest != -1 && !prescanning_flag)
    {
	free_voice(lowest);
	ctl_note_event(lowest);
    }
    return lowest;
}


///r
static int find_samples(MidiEvent *e, int *vlist)
{
	int i, j, ch, bank, prog, note = -1, noteo = -1, nv = 0, nvo = 0, nvt = 0; // nv
	SpecialPatch *s;
	Instrument *ip;
	int elm, elm_max = 0;
	
	ch = e->channel;
	if (channel[ch].special_sample > 0) {
		if ((s = special_patch[channel[ch].special_sample]) == NULL) {
			ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
					"Strange: Special patch %d is not installed",
					channel[ch].special_sample);
			return 0;
		}
		note = e->a + channel[ch].key_shift + note_key_offset;
		note = (note < 0) ? 0 : ((note > 127) ? 127 : note);
		return select_play_sample(s->sample, s->samples, &note, vlist, e, nv, 0);
	}
	bank = channel[ch].bank;
	if (ISDRUMCHANNEL(ch)) {
		ip = NULL;
		note = e->a & 0x7f;
		instrument_map(channel[ch].mapID, &bank, &note);
		noteo = note;
//		for(elm = 0; elm < drumset[bank]->tone[note][0].element_num + 1; elm++){
//		for (elm = 0; drumset[bank] && elm < drumset[bank]->tone[note][0].element_num + 1; elm++) {
//		for(elm = 0; elm < MAX_ELEMENT; elm++){
		for(elm = 0; elm <= elm_max; elm++){			
			ip = NULL;
			if (! (ip = play_midi_load_instrument(1, bank, note, elm, &elm_max))) // change elm_max
				if(elm == 0)
					return 0;	/* No instrument? Then we can't play. */	
				else
					break;
		/* if (ip->type == INST_GUS && ip->samples != 1)
			ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
					"Strange: percussion instrument with %d samples!",
					ip->samples); */
		/* "keynum" of SF2, and patch option "note=" */
			if (ip->sample->note_to_use)
				note = ip->sample->note_to_use;
			nvo = nv;
			nv = select_play_sample(ip->sample, ip->samples, &note, vlist, e, nv, elm);
			nvt += nv;
			/* Replace the sample if the sample is cached. */
			if (! prescanning_flag) {
				if (ip->sample->note_to_use)
					note = MIDI_EVENT_NOTE(e);
				for (i = nvo; i < nv; i++) { // i = 0
					j = vlist[i];
					if (! opt_realtime_playing && allocate_cache_size > 0
							&& ! channel[ch].portamento) {
						voice[j].cache = resamp_cache_fetch(voice[j].sample, note);
						if (voice[j].cache)	/* cache hit */
							voice[j].sample = voice[j].cache->resampled;
					} else
						voice[j].cache = NULL;
				}
			}
			note = noteo;
		}
//		if (!ip)
//			return 0;	/* No instrument? Then we can't play. */
	} else if ((prog = channel[ch].program) == SPECIAL_PROGRAM){
		ip = default_instrument;
		note = ((ip->sample->note_to_use) ? ip->sample->note_to_use : e->a) + channel[ch].key_shift + note_key_offset;
		note = (note < 0) ? 0 : ((note > 127) ? 127 : note);
		nv = select_play_sample(ip->sample, ip->samples, &note, vlist, e, nv, 0);
		nvt += nv;
	} else {
		instrument_map(channel[ch].mapID, &bank, &prog);
//		for(elm = 0; elm < tonebank[bank]->tone[prog][0].element_num + 1; elm++){
//		for(elm = 0; tonebank[bank] && elm < tonebank[bank]->tone[prog][0].element_num + 1; elm++){
//		for(elm = 0; elm < MAX_ELEMENT; elm++){
		for(elm = 0; elm <= elm_max; elm++){
			ip = NULL;
			if (! (ip = play_midi_load_instrument(0, bank, prog, elm, &elm_max))) // change elm_max
				if(elm == 0)
					return 0;	/* No instrument? Then we can't play. */	
				else
					break;
			note = ((ip->sample->note_to_use) ? ip->sample->note_to_use : e->a) + channel[ch].key_shift + note_key_offset;
			note = (note < 0) ? 0 : ((note > 127) ? 127 : note);
			nvo = nv;
			nv = select_play_sample(ip->sample, ip->samples, &note, vlist, e, nv, elm);
			nvt += nv;
			/* Replace the sample if the sample is cached. */
			if (! prescanning_flag) {
				if (ip->sample->note_to_use)
					note = MIDI_EVENT_NOTE(e);
				for (i = nvo; i < nv; i++) { // i = 0
					j = vlist[i];
					if (! opt_realtime_playing && allocate_cache_size > 0
							&& ! channel[ch].portamento) {
						voice[j].cache = resamp_cache_fetch(voice[j].sample, note);
						if (voice[j].cache)	/* cache hit */
							voice[j].sample = voice[j].cache->resampled;
					} else
						voice[j].cache = NULL;
				}
			}
		}
//		if(!nvt)
		if(nvt == 0)
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, "Strange: ch %d note %d can't select play sample.", ch, e->a);
//		if (!ip)
//			return 0;	/* No instrument? Then we can't play. */
	}
/*
for(elm が追加されたことで ip の有効範囲はループ内になったのでキャッシュ置換も中に入れる必要ができた
noteo : note_to_use で変更された場合 for(elm 条件に影響する？
nvo : ip と対応する vlist の nv の範囲を指定する必要がある
elm_max : 未定義バンクの場合,代替先バンクのelement_numが必要なのでplay_midi_load_instrument()内で更新
*/
#if 0
	/* Replace the sample if the sample is cached. */
	if (! prescanning_flag) {
		if (ip->sample->note_to_use)
			note = MIDI_EVENT_NOTE(e);
		for (i = 0; i < nv; i++) {
			j = vlist[i];
			if (! opt_realtime_playing && allocate_cache_size > 0
					&& ! channel[ch].portamento) {
				voice[j].cache = resamp_cache_fetch(voice[j].sample, note);
				if (voice[j].cache)	/* cache hit */
					voice[j].sample = voice[j].cache->resampled;
			} else
				voice[j].cache = NULL;
		}
	}
#endif
	return nv;
}
///r
static int select_play_sample(Sample *splist,
		int nsp, int *note, int *vlist, MidiEvent *e, int nv, int elm)
{
	int ch = e->channel, kn = e->a & 0x7f, vel = e->b;
	int32 f, fs, ft, fst, fc, fr, cdiff, diff, sample_link;
	int8 tt = channel[ch].temper_type;
	uint8 tp = channel[ch].rpnmap[RPN_ADDR_0003];
	Sample *sp, *spc, *spr;
	int16 sf, sn;
	double ratio;
	int i, j, k, nvc, nvo = nv;
	
	if (ISDRUMCHANNEL(ch)){
		f = freq_table[*note];
	//	fs = f;
	}else {
		if (opt_pure_intonation) {
			if (current_keysig < 8)
				f = freq_table_pureint[current_freq_table][*note];
			else
				f = freq_table_pureint[current_freq_table + 12][*note];
		} else if (opt_temper_control)
			switch (tt) {
			case 0:
				f = freq_table_tuning[tp][*note];
				break;
			case 1:
				if (current_temper_keysig < 8)
					f = freq_table_pytha[
							current_temper_freq_table][*note];
				else
					f = freq_table_pytha[
							current_temper_freq_table + 12][*note];
				break;
			case 2:
				if (current_temper_keysig < 8)
					f = freq_table_meantone[current_temper_freq_table
							+ ((temper_adj) ? 36 : 0)][*note];
				else
					f = freq_table_meantone[current_temper_freq_table
							+ ((temper_adj) ? 24 : 12)][*note];
				break;
			case 3:
				if (current_temper_keysig < 8)
					f = freq_table_pureint[current_temper_freq_table
							+ ((temper_adj) ? 36 : 0)][*note];
				else
					f = freq_table_pureint[current_temper_freq_table
							+ ((temper_adj) ? 24 : 12)][*note];
				break;
			default:	/* user-defined temperament */
				if ((tt -= 0x40) >= 0 && tt < 4) {
					if (current_temper_keysig < 8)
						f = freq_table_user[tt][current_temper_freq_table
								+ ((temper_adj) ? 36 : 0)][*note];
					else
						f = freq_table_user[tt][current_temper_freq_table
								+ ((temper_adj) ? 24 : 12)][*note];
				} else
					f = freq_table[*note];
				break;
			}
		else
			f = freq_table[*note];
		if (! opt_pure_intonation && opt_temper_control
				&& tt == 0 && f != freq_table[*note]) {
			*note = log(f / 440000.0) * DIV_LN2 * 12 + 69.5;
			*note = (*note < 0) ? 0 : ((*note > 127) ? 127 : *note);
		//	fs = freq_table[*note];
		}
		//else
		//	fs = freq_table[*note];
	}
#if 0 // random vel
	{
		int32 rvel, rnd = rand(), flg = rnd & 0x1000, ofs = rnd & 0x7;
		rvel = flg ? (vel - ofs) : (vel + ofs);
		if(rvel > 127)
			rvel = vel - ofs;
		else if(rvel < 0)
			rvel = vel + ofs;
		vel = rvel;
	}
#endif

	// allocate round robin counters
	if (channel[ch].seq_num_counters[elm] == 0) {
		int i;

		channel[ch].seq_num_counters[elm] = nsp;
		channel[ch].seq_counters[elm] = (int32 *)safe_malloc(sizeof(int32) * nsp);
		for (i = 0; i < nsp; i++)
			channel[ch].seq_counters[elm][i] = 1;
	}

#if 1
/*
サンプル指定は ノーナンバー,ベロシティ が各レンジ内にあるもの全て
次に指定のサンプル設定(scale_factor)を元に再生周波数を指定
*/
	{
	int rand_calculated = 0;
	FLOAT_T rand_val;

	for (i = 0, sp = splist; i < nsp; i++, sp++) {
		if (((sp->low_key <= *note && sp->high_key >= *note))
			&& sp->low_vel <= vel && sp->high_vel >= vel) {	

			if (sp->modes & MODES_TRIGGER_RANDOM) {
				if (!rand_calculated) {
					rand_val = genrand_real2();
					rand_calculated = 1;
				}

				if (rand_val < sp->lorand || sp->hirand <= rand_val)
					continue;
			}

			if (sp->seq_length > 0) {
				int32 seq_count = channel[ch].seq_counters[elm][i];
				channel[ch].seq_counters[elm][i]++;

				if (channel[ch].seq_counters[elm][i] > sp->seq_length)
					channel[ch].seq_counters[elm][i] = 1;

				if (seq_count != sp->seq_position)
					continue;
			}

			/* GUS/SF2 - Scale Tuning */
			if ((sf = sp->scale_factor) != 1024) {
				sn = sp->scale_freq;
				ratio = pow(2.0, (double)((*note - sn) * (sf - 1024)) * DIV_12288);
				ft = f * ratio + 0.5;
			} else
				ft = f;
			if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
				if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
					ft = ft * ratio + 0.5;
			j = vlist[nv] = find_voice(e);
			voice[j].orig_frequency = ft;
			MYCHECK(voice[j].orig_frequency);
			voice[j].sample = sp;
			voice[j].status = VOICE_ON;
			nv++;
		}
	}
	// move to find_samples() (for add_elm
	//if(nv == 0)
	//	ctl->cmsg(CMSG_WARNING, VERB_NOISY, 
	//		"Strange: ch %d note %d can't select play sample.", ch, *note);
	return nv;
	}
#else
//	nv = 0;	
	for (i = 0, sp = splist; i < nsp; i++, sp++) {
		/* GUS/SF2 - Scale Tuning */
		if ((sf = sp->scale_factor) != 1024) {
			sn = sp->scale_freq;
			ratio = pow(2.0, (double)((*note - sn) * (sf - 1024)) * DIV_12288);
			ft = f * ratio + 0.5, fst = fs * ratio + 0.5;
		} else
			ft = f, fst = fs;
		if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
			if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
				ft = ft * ratio + 0.5, fst = fst * ratio + 0.5;
		if (sp->low_freq <= fst && sp->high_freq >= fst
				&& sp->low_vel <= vel && sp->high_vel >= vel
				&& ! (sp->inst_type == INST_SF2
				&& sp->sample_type == SF_SAMPLETYPE_RIGHT)) {
			j = vlist[nv] = find_voice(e);
			voice[j].orig_frequency = ft;
			MYCHECK(voice[j].orig_frequency);
			voice[j].sample = sp;
			voice[j].status = VOICE_ON;
			nv++;
		}
	}
	if (nv == 0) {	/* we must select at least one sample. */
		fr = fc = 0;
		spc = spr = NULL;
		cdiff = 0x7fffffff;
		for (i = 0, sp = splist; i < nsp; i++, sp++) {
			/* GUS/SF2 - Scale Tuning */
			if ((sf = sp->scale_factor) != 1024) {
				sn = sp->scale_freq;
				ratio = pow(2.0, (double)((*note - sn) * (sf - 1024)) * DIV_12288);
				ft = f * ratio + 0.5, fst = fs * ratio + 0.5;
			} else
				ft = f, fst = fs;
			if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
				if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
					ft = ft * ratio + 0.5, fst = fst * ratio + 0.5;
			diff = abs(sp->root_freq - fst);
			if (diff < cdiff) {
				if (sp->inst_type == INST_SF2
						&& sp->sample_type == SF_SAMPLETYPE_RIGHT) {
					fr = ft;	/* reserve */
					spr = sp;	/* reserve */
				} else {
					fc = ft;
					spc = sp;
					cdiff = diff;
				}
			}
		}
		/* If spc is not NULL, a makeshift sample is found. */
		/* Otherwise, it's a lonely right sample, but better than nothing. */
		j = vlist[nv] = find_voice(e);
		voice[j].orig_frequency = (spc) ? fc : fr;
		MYCHECK(voice[j].orig_frequency);
		voice[j].sample = (spc) ? spc : spr;
		voice[j].status = VOICE_ON;
		nv++;
	}
	nvc = nv;
	for (i = nvo; i < nvc; i++) { // i = 0
		spc = voice[vlist[i]].sample;
		/* If it's left sample, there must be right sample. */
		if (spc->inst_type == INST_SF2
				&& spc->sample_type == SF_SAMPLETYPE_LEFT) {
			sample_link = spc->sf_sample_link;
			for (j = 0, sp = splist; j < nsp; j++, sp++)
				if (sp->inst_type == INST_SF2
						&& sp->sample_type == SF_SAMPLETYPE_RIGHT
						&& sp->sf_sample_index == sample_link) {
					/* right sample is found. */
					/* GUS/SF2 - Scale Tuning */
					if ((sf = sp->scale_factor) != 1024) {
						sn = sp->scale_freq;
						ratio = pow(2.0, (double)((*note - sn) * (sf - 1024)) * DIV_12288);
						ft = f * ratio + 0.5;
					} else
						ft = f;
					if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
						if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
							ft = ft * ratio + 0.5;
					k = vlist[nv] = find_voice(e);
					voice[k].orig_frequency = ft;
					MYCHECK(voice[k].orig_frequency);
					voice[k].sample = sp;
					voice[k].status = VOICE_ON;
					nv++;
					break;
				}
		}
	}
	return nv;
#endif
}

static double get_play_note_ratio(int ch, int note)
{
	int play_note = channel[ch].drums[note]->play_note;
	int bank = channel[ch].bank;
	ToneBank *dbank;
	int def_play_note;
	int elm = 0;
	
	if (play_note == -1)
		return 1.0;
	instrument_map(channel[ch].mapID, &bank, &note);
	dbank = (drumset[bank]) ? drumset[bank] : drumset[0];
	if(dbank->tone[note][elm] == NULL)
		return 1.0;
	if ((def_play_note = dbank->tone[note][elm]->play_note) == -1)
		return 1.0;
	if (play_note >= def_play_note)
		return bend_coarse[(play_note - def_play_note) & 0x7f];
	else
		return 1 / bend_coarse[(def_play_note - play_note) & 0x7f];
}

///r
/*
kill_note(i); // cut sustain
finish_note(i); // drop sustain ? NOTE_OFF ?
cut_note(i) // cut short release // altassign,mono
cut_note2(i) // cut short release // overlaped voice
*/
static void voice_control(MidiEvent *e)
{
	int i;
	int ch = e->channel;
	int note = MIDI_EVENT_NOTE(e);
	int mono_check = channel[ch].mono;
	AlternateAssign *altassign = find_altassign(channel[ch].altassign, note);
	
	for (i = 0; i < upper_voices; i++){
		if (voice[i].status == VOICE_FREE || voice[i].channel != ch)
			continue;
#if 1 // opt_max_channel_voices
		if (voice[i].channel_voice_count > 0)
			voice[i].channel_voice_count--;
		else
			cut_note2(i);
#endif
		if (!opt_overlap_voice_allow) {
			// overlap_voice OFF
			if(voice[i].note == note)
				kill_note(i);
			else if (altassign && find_altassign(altassign, voice[i].note))
				kill_note(i);
			else if (mono_check)
				kill_note(i);
		}
		else if (altassign && find_altassign(altassign, voice[i].note))
			cut_note(i);
		else if (mono_check)
			cut_note(i);
		else if (voice[i].note != note || channel[ch].assign_mode == 2)
			continue;
		else if (channel[ch].assign_mode == 0)
			cut_note(i);
		else if (channel[ch].assign_mode == 1){
			if (voice[i].overlap_count > 0)
				voice[i].overlap_count--;
			else
				cut_note2(i);
		}		
	}
}

/* Only one instance of a note can be playing on a single channel. */
static int find_voice(MidiEvent *e)
{
	int i, lowest = -1;

	for (i = 0; i < upper_voices; i++)
		if (voice[i].status == VOICE_FREE) {
			lowest = i;	/* lower volume */
			break;
		}
	if (lowest != -1)	/* Found a free voice. */
		return lowest;
	if (upper_voices < voices)
		return upper_voices++;
	return reduce_voice();
}

///r
// DIV_12288 = 1.0 / (double)(12 * (1 << 10)); 
int32 get_note_freq(Sample *sp, int note)
{	
	/* GUS/SF2 - Scale Tuning */
	if (sp->scale_factor != 1024) {
		double ratio = pow(2.0, (double)((note - sp->scale_freq) * (sp->scale_factor - 1024)) * DIV_12288);
		return (int32)((double)freq_table[note] * ratio + 0.5);
	}else
		return freq_table[note];
}

#if 0
int32 get_note_freq(Sample *sp, int note)
{
	int32 f;
	int16 sf, sn;
	double ratio;
	
	f = freq_table[note];
	/* GUS/SF2 - Scale Tuning */
	if ((sf = sp->scale_factor) != 1024) {
		sn = sp->scale_freq;
		ratio = pow(2.0, (note - sn) * (sf - 1024) / 12288.0);
		f = f * ratio + 0.5;
	}
	return f;
}
#endif


static void update_modulation_wheel(int ch)
{
	recompute_channel_lfo(ch);
	recompute_channel_amp(ch);
	recompute_channel_filter(ch);
	recompute_channel_pitch(ch);
//   int i, uv = upper_voices;
//    for(i = 0; i < uv; i++)
//	if(voice[i].status != VOICE_FREE && voice[i].channel == ch)
//	{
//	    /* Set/Reset mod-wheel */
////		voice[i].vibrato_control_counter = voice[i].vibrato_phase = 0; //elion comment out
//	    recompute_amp(i);
//		apply_envelope_to_amp(i);
//	    recompute_freq(i);
//		recompute_voice_filter(i);
//	}
}


static void drop_portamento(int ch)
{
    int i, uv = upper_voices;

    channel[ch].porta_status = 0;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_status)
	{
	    voice[i].porta_status = 0;
	//    recompute_freq(i);
		recompute_voice_pitch(i);
	}
	//channel[ch].porta_last_note_fine = -1;
}

static void update_portamento_controls(int ch)
{
    if(!channel[ch].portamento && channel[ch].portamento_control == -1){
	//	|| (channel[ch].portamento_time_msb | channel[ch].portamento_time_lsb) == 0){
		if(channel[ch].porta_status)
			drop_portamento(ch);
	} else {
#if 1 // use portament_time_table		
		double st = portament_time_table_xg[channel[ch].portamento_time_msb & 0x7F];
		channel[ch].porta_dpb = st * PORTAMENTO_CONTROL_RATIO * div_playmode_rate;
#else
		const double tuning = PORTAMENTO_TIME_TUNING * 256.0 * 256.0;
		double mt, st;
		mt = midi_time_table[channel[ch].portamento_time_msb & 0x7F] *
			midi_time_table2[channel[ch].portamento_time_lsb & 0x7F] * tuning;
		st = (double)1000.0 / mt; // semitone/sec
		channel[ch].porta_dpb = st * PORTAMENTO_CONTROL_RATIO * div_playmode_rate;
#endif
	//	channel[ch].porta_status = 1;
    }
}

static void update_portamento_time(int ch)
{
    int i, uv = upper_voices;
    int dpb;
    int32 ratio;

    update_portamento_controls(ch);
    dpb = channel[ch].porta_dpb;
    //ratio = channel[ch].porta_status;

    for(i = 0; i < uv; i++)
    {
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_status)
	{
	 //   voice[i].porta_status = ratio;
	    voice[i].porta_dpb = dpb;
	//    recompute_freq(i);
	//	recompute_voice_pitch(i);
	}
    }
}

static void drop_legato(int ch)
{
    int i, uv = upper_voices;

    channel[ch].legato_status = 0;
	if(channel[ch].legato_note > 127)
		return;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].note == channel[ch].legato_note)
	{
	    voice[i].porta_status = 0;
	    cut_note(i);
	//    recompute_freq(i);
	//	recompute_voice_pitch(i);
	}
	channel[ch].legato_note = 255;
	channel[ch].legato_flag = 0;
}

static void update_legato_status(int ch)
{
	if(channel[ch].legato <= 0){
		if(channel[ch].legato_status)
			drop_legato(ch);
	}else{
#if 1
	//	const double ss = 4069.0104166666670; // semitone / sec
		const double ss = 12000.0; // semitone / sec
		channel[ch].porta_dpb = ss * PORTAMENTO_CONTROL_RATIO * div_playmode_rate;
#else
		const double st = 1.0 / 0.06250 * PORTAMENTO_TIME_TUNING * 0.3 * 256.0 * 256;
		double st = (double)1.0 / mt; // semitone / ms
		channel[ch].porta_dpb = st * PORTAMENTO_CONTROL_RATIO * 1000.0 * div_playmode_rate;
#endif
		//channel[ch].porta_status = 1;
	}
}

/*! update legato for a voice. */
static void update_voice_legato(int ch)
{
	int i;

	for(i = 0; i < upper_voices; i++){
		if(voice[i].status != VOICE_FREE &&
			voice[i].channel == ch &&
			voice[i].note == channel[ch].legato_note){
			voice[i].porta_next_pb = channel[ch].porta_next_pb;
			voice[i].porta_dpb = channel[ch].porta_dpb;	
			if(voice[i].porta_pb != voice[i].porta_next_pb)
				voice[i].porta_status = 1;
		}
	}	
}

/*! initialize portamento or legato for a voice. */
static void init_voice_portamento(int v)
{
	Voice *vp = &(voice[v]);
	int ch = vp->channel;

	vp->porta_status = 0;
	vp->porta_out = 0;
	if(!opt_portamento)
		return;
	if(channel[ch].porta_status || channel[ch].legato_flag) {
		vp->porta_note_fine = channel[ch].porta_note_fine;
		vp->porta_next_pb = channel[ch].porta_next_pb;
		vp->porta_dpb = channel[ch].porta_dpb;
		vp->porta_pb = channel[ch].porta_pb;		
		if(vp->porta_pb != vp->porta_next_pb)
			vp->porta_status = 1;
	}
}

static int note_off_legato(MidiEvent *e)
{
	int i, ch = e->channel, note = MIDI_EVENT_NOTE(e);
	int count, max = 0, fnote = -1;
	
// -2:return finish_note()しない
// -1:thru finish_note()する
// 0<=:change note finish_note()する
	if(!channel[ch].legato_status){
		channel[ch].legato_flag = 0;
		return -1; // thru
	}
	count = channel[ch].legato_hist[note];		
	if(!count) 
		return -1; // thru
	channel[ch].legato_hist[note] = 0; // clear flag	
	if(note == channel[ch].legato_note)
		ctl_note_event2(ch, note, VOICE_SUSTAINED, channel[ch].legato_velo); // legatoの基準ノートなので表示残しておく
	else
		ctl_note_event2(ch, note, VOICE_FREE, channel[ch].legato_velo);
#if 0 // 残っている最近のノートへピッチ変更
	for (i = 0; i < 128; i++){
		if(channel[ch].legato_hist[i] > count) // 今note_offしたnoteがnoteonしたときより後にnoteonしたnoteを探す
			return -2; // finish_note()しない
		if(channel[ch].legato_hist[i] > max){ // 今note_offしたnoteがnoteonしたときより前にnoteonした最近のnoteを探す
			max = channel[ch].legato_hist[i];
			fnote = i;
		}
	}
	if(fnote != -1){ // noteがある場合		
		uint8 pnote = channel[ch].legato_last_note;
		channel[ch].legato_last_note = fnote;
		channel[ch].porta_next_pb = fnote * 256;
		update_voice_legato(ch);
		ctl->cmsg(CMSG_INFO,VERB_DEBUG, "Legato: change note:%d (CH:%d NOTE:%d noteoff)", 
			fnote, ch, note);
		ctl_note_event2(ch, pnote, VOICE_SUSTAINED, channel[ch].legato_velo);
		ctl_note_event2(ch, note, VOICE_ON, channel[ch].legato_velo); // e->b = velocity
		return -2; // finish_note()しない
	}
#else // noteoffではピッチ変更しない noteがある間はlegato_noteを継続する
	for (i = 0; i < 128; i++){
		if(channel[ch].legato_hist[i]) // noteon中のnoteを探す
			return -2; // finish_note()しない
	}
#endif
	// 最近のnoteがない場合 (全てnoteoffした場合
	ctl->cmsg(CMSG_INFO,VERB_DEBUG, "Legato: finish note:%d (CH:%d NOTE:%d noteoff)", 
		channel[ch].legato_note, ch, note);
	channel[ch].legato_flag = 0;
	channel[ch].legato_status = 0; // legato終了
	return channel[ch].legato_note; // last_vidq()を通すために legato開始時のnoteに書き換え 後はfinish_note()
}

/*! initialize portamento or legato for a channel. */
static int init_channel_portamento(MidiEvent *e)
{
#if 1 // portamento and leagato
    int i, ch = e->channel, note = MIDI_EVENT_NOTE(e);
	int32 times = 0, fine = note * PORTAMENTO_CONTROL_RATIO;

	if(!(channel[ch].portamento_control != -1
		|| channel[ch].portamento
		|| channel[ch].legato > 0)){
		channel[ch].porta_last_note_fine = fine; // for portamento
		return 0;
	}
	if(channel[ch].legato > 0 && channel[ch].legato_flag){ // 複数ノートの2ノート目以降
		if(channel[ch].legato_status || channel[ch].porta_status
			&& (times = e->time - channel[ch].legato_note_time) > (play_mode->rate >> 2) ) { // legato start
			if(!channel[ch].legato_status){
				channel[ch].legato_status = 1; // legato start
				channel[ch].legato_note = channel[ch].legato_last_note; // set基準ノート
				memset(channel[ch].legato_hist, 0, sizeof(channel[ch].legato_hist)); // init						
				channel[ch].legato_hist[channel[ch].legato_note] = channel[ch].legato_status;
				ctl->cmsg(CMSG_INFO,VERB_DEBUG,
					"Legato: start legato. base note:%d (CH:%d NOTE:%d) time:%d", 
					channel[ch].legato_note, ch, note, times);
			}
			if(!(channel[ch].portamento_control != -1 || channel[ch].portamento))
				update_legato_status(ch);
			channel[ch].legato_hist[note] = (++channel[ch].legato_status);
			channel[ch].porta_next_pb = fine;
			update_voice_legato(ch);
			ctl->cmsg(CMSG_INFO,VERB_DEBUG,
				"Legato: update legato. change note:%d legato (CH:%d NOTE:%d)", note, ch, note);
			ctl_note_event2(ch, channel[ch].legato_last_note, VOICE_SUSTAINED, channel[ch].legato_velo);
			ctl_note_event2(ch, note, VOICE_ON, channel[ch].legato_velo);
			channel[ch].legato_last_note = note;
			channel[ch].porta_last_note_fine = fine; // for portamento
			return 1; // legato start
		}
	}			
	// start note (legatoの基準ノートになる可能性がある
	if(channel[ch].portamento_control != -1){ // portament_control
		update_portamento_controls(ch);	
		channel[ch].porta_status = 1;
		channel[ch].porta_pb = channel[ch].portamento_control * PORTAMENTO_CONTROL_RATIO;
		ctl->cmsg(CMSG_INFO,VERB_DEBUG,
			"Portamento Control: start note:%d to base note:%d (CH:%d NOTE:%d)", 
			channel[ch].portamento_control, note, ch, note);	
		channel[ch].portamento_control = -1;
		channel[ch].portamento = 127;
		channel[ch].porta_dpb = (double)(12000 * PORTAMENTO_CONTROL_RATIO) * div_playmode_rate; // 12000st/s instant
	}else if(channel[ch].portamento){ // portament
		update_portamento_controls(ch);	
		channel[ch].porta_status = 1;
		if(channel[ch].porta_last_note_fine == -1) // first noteon
			channel[ch].porta_pb = fine;
		else
			channel[ch].porta_pb = channel[ch].porta_last_note_fine;		
		ctl->cmsg(CMSG_INFO,VERB_DEBUG,
			"Portamento: start note:%d to base note:%d (CH:%d NOTE:%d)", 
			channel[ch].porta_last_note_fine >> PORTAMENTO_CONTROL_BIT, note, ch, note);	
	}else{ // if(channel[ch].legato > 0)
		update_legato_status(ch);
		channel[ch].porta_status = 1;
		channel[ch].porta_pb = fine;
	}
	channel[ch].porta_next_pb = fine;
	channel[ch].porta_note_fine = fine;
	channel[ch].legato_note = 255; // 無効なノートナンバー
	channel[ch].legato_velo = e->b; // velocity
	channel[ch].legato_flag = 1;	
	channel[ch].legato_note_time = e->time;
	channel[ch].legato_last_note = note;
	channel[ch].porta_last_note_fine = fine;
	return 0;
#else // portamento or leagato
    int i, ch = e->channel, note = MIDI_EVENT_NOTE(e);
	int32 times, fine = note * PORTAMENTO_CONTROL_RATIO;

	if(channel[ch].portamento_control != -1){ // portament_control
		update_portamento_controls(ch);	
		channel[ch].porta_status = 1;
		channel[ch].porta_last_note_fine = channel[ch].portamento_control * PORTAMENTO_CONTROL_RATIO;
		channel[ch].porta_note_fine = fine;
		channel[ch].porta_pb = channel[ch].porta_last_note_fine;
		channel[ch].porta_next_pb = channel[ch].porta_note_fine;		
		ctl->cmsg(CMSG_INFO,VERB_DEBUG,
			"Portamento Control: start note:%d to base note:%d (CH:%d NOTE:%d)", 
			channel[ch].portamento_control, note, ch, note);	
		channel[ch].portamento_control = -1;
	}else if(channel[ch].portamento){ // portament
		update_portamento_controls(ch);	
		channel[ch].porta_status = 1;
		if(channel[ch].porta_last_note_fine == -1) // first noteon
			channel[ch].porta_last_note_fine = fine;
		channel[ch].porta_note_fine = fine;
		channel[ch].porta_pb = channel[ch].porta_last_note_fine;
		channel[ch].porta_next_pb = channel[ch].porta_note_fine;		
		ctl->cmsg(CMSG_INFO,VERB_DEBUG,
			"Portamento: start note:%d to base note:%d (CH:%d NOTE:%d)", 
			channel[ch].porta_last_note_fine >> PORTAMENTO_CONTROL_BIT, note, ch, note);	
	}else if(channel[ch].legato > 0){ // legato	
		if(channel[ch].legato_flag){ // 複数ノートの2ノート目以降
#if 0
#ifdef _DEBUG
ctl->cmsg(CMSG_INFO,VERB_NORMAL, "Legato: times:%d = now - prev:%d", 
e->time - channel[ch].legato_note_time,  channel[ch].legato_note_time, ch, note);
#endif
#endif
			if(!channel[ch].legato_status
				&& (times = e->time - channel[ch].legato_note_time) < (play_mode->rate >> 2) ){ // legato start
#if 0
#ifdef _DEBUG
ctl->cmsg(CMSG_INFO,VERB_NORMAL,
"Legato: disable legato times:%d (times:%d CH:%d NOTE:%d)", times, ch, note);
#endif
#endif
			}else{	
				if(!channel[ch].legato_status){
					channel[ch].legato_status = 1; // legato start
					channel[ch].legato_note = channel[ch].legato_last_note; // set基準ノート
					memset(channel[ch].legato_hist, 0, sizeof(channel[ch].legato_hist)); // init						
					channel[ch].legato_hist[channel[ch].legato_note] = channel[ch].legato_status;
					ctl->cmsg(CMSG_INFO,VERB_DEBUG,
						"Legato: start legato. base note:%d (CH:%d NOTE:%d) time:%d", 
						channel[ch].legato_note, ch, note, times);
				}	
				update_legato_status(ch);
				channel[ch].legato_hist[note] = (++channel[ch].legato_status);
				channel[ch].porta_next_pb = fine;
				update_voice_legato(ch);
				ctl->cmsg(CMSG_INFO,VERB_DEBUG,
					"Legato: update legato. change note:%d legato (CH:%d NOTE:%d)", note, ch, note);
				ctl_note_event2(ch, channel[ch].legato_last_note, VOICE_SUSTAINED, channel[ch].legato_velo);
				ctl_note_event2(ch, note, VOICE_ON, channel[ch].legato_velo);
				channel[ch].legato_last_note = note;
				channel[ch].porta_last_note_fine = fine; // for portamento
				return 1; // legato start
			}
		}
		// normal (legatoの基準ノートになる可能性がある ある程度初期化が必要
		channel[ch].legato_note = 255; // 無効なノートナンバー
		channel[ch].porta_note_fine = fine;
		channel[ch].porta_pb = fine;
		channel[ch].porta_next_pb = fine;
		channel[ch].legato_velo = e->b; // velocity
		channel[ch].legato_flag = 1;	
		channel[ch].legato_note_time = e->time;
		channel[ch].legato_last_note = note;
	}
	channel[ch].porta_last_note_fine = fine; // for portamento
	return 0;
#endif
}



///r
static void recompute_amp_envelope_follow(int v, int ch)
{
	Voice *vp = voice + v; 
	Channel *cp = channel + ch;
	int i, note = vp->note, dr = ISDRUMCHANNEL(ch);
	double notesub = (double)(vp->note - vp->sample->envelope_keyf_bpo); 
	double amp_velsub = (double)(vp->velocity - vp->sample->envelope_velf_bpo);
	struct DrumParts *dp = NULL;
	int add_param[6];

	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++)
		vp->amp_env.follow[i] = 1.0;	
	for(i = 0; i < 6; i++)
		add_param[i] = 0;
	
	if (dr){
		if(cp->drums[note] != NULL){
			dp = cp->drums[note];
			// amp
			if(dp->drum_envelope_rate[EG_GUS_ATTACK] != -1)
				add_param[EG_GUS_ATTACK] = (dp->drum_envelope_rate[EG_GUS_ATTACK] - 0x40);
			if(dp->drum_envelope_rate[EG_GUS_DECAY] != -1)
				add_param[EG_GUS_DECAY] = (dp->drum_envelope_rate[EG_GUS_DECAY] - 0x40);
			if(dp->drum_envelope_rate[EG_GUS_SUSTAIN] != -1)
				add_param[EG_GUS_SUSTAIN] = (dp->drum_envelope_rate[EG_GUS_SUSTAIN] - 0x40);
			if(dp->drum_envelope_rate[EG_GUS_RELEASE1] != -1)
				add_param[EG_GUS_RELEASE1] = (dp->drum_envelope_rate[EG_GUS_RELEASE1] - 0x40);
		}
	}else{
		if(cp->envelope_rate[EG_GUS_ATTACK] != -1)
			add_param[EG_GUS_ATTACK] = (cp->envelope_rate[EG_GUS_ATTACK] - 0x40);
		if(cp->envelope_rate[EG_GUS_DECAY] != -1)
			add_param[EG_GUS_DECAY] = (cp->envelope_rate[EG_GUS_DECAY] - 0x40);
		if(cp->envelope_rate[EG_GUS_SUSTAIN] != -1)
			add_param[EG_GUS_SUSTAIN] = (cp->envelope_rate[EG_GUS_SUSTAIN] - 0x40);
		if(cp->envelope_rate[EG_GUS_RELEASE1] != -1)
			add_param[EG_GUS_RELEASE1] = (cp->envelope_rate[EG_GUS_RELEASE1] - 0x40);
		/* envelope key-follow */	
		if (vp->sample->envelope_keyf[EG_GUS_ATTACK])
			vp->amp_env.follow[ENV0_ATTACK_STAGE] *= POW2(notesub * (double)vp->sample->envelope_keyf[EG_GUS_ATTACK] * DIV_1200);
		if (vp->sample->envelope_keyf[EG_GUS_DECAY])
			vp->amp_env.follow[ENV0_HOLD_STAGE] *= POW2(notesub * (double)vp->sample->envelope_keyf[EG_GUS_DECAY] * DIV_1200);
		if (vp->sample->envelope_keyf[EG_GUS_SUSTAIN])
			vp->amp_env.follow[ENV0_DECAY_STAGE] *= POW2(notesub * (double)vp->sample->envelope_keyf[EG_GUS_SUSTAIN] * DIV_1200);
		if (vp->sample->envelope_keyf[EG_GUS_RELEASE1])
			vp->amp_env.follow[ENV0_RELEASE1_STAGE] *= POW2(notesub * (double)vp->sample->envelope_keyf[EG_GUS_RELEASE1] * DIV_1200);
		if (vp->sample->envelope_keyf[EG_GUS_RELEASE2])
			vp->amp_env.follow[ENV0_RELEASE2_STAGE] *= POW2(notesub * (double)vp->sample->envelope_keyf[EG_GUS_RELEASE2] * DIV_1200);
		if (vp->sample->envelope_keyf[EG_GUS_RELEASE3])
			vp->amp_env.follow[ENV0_RELEASE3_STAGE] *= POW2(notesub * (double)vp->sample->envelope_keyf[EG_GUS_RELEASE3] * DIV_1200);
	}	
	/* envelope velocity-follow */
	if (vp->sample->envelope_velf[EG_GUS_ATTACK])
		vp->amp_env.follow[ENV0_ATTACK_STAGE] *= POW2(amp_velsub * (double)vp->sample->envelope_velf[EG_GUS_ATTACK] * DIV_1200);
	if (vp->sample->envelope_velf[EG_GUS_DECAY])
		vp->amp_env.follow[ENV0_HOLD_STAGE] *= POW2(amp_velsub * (double)vp->sample->envelope_velf[EG_GUS_DECAY] * DIV_1200);
	if (vp->sample->envelope_velf[EG_GUS_SUSTAIN])
		vp->amp_env.follow[ENV0_DECAY_STAGE] *= POW2(amp_velsub * (double)vp->sample->envelope_velf[EG_GUS_SUSTAIN] * DIV_1200);
	if (vp->sample->envelope_velf[EG_GUS_RELEASE1])
		vp->amp_env.follow[ENV0_RELEASE1_STAGE] *= POW2(amp_velsub * (double)vp->sample->envelope_velf[EG_GUS_RELEASE1] * DIV_1200);
	if (vp->sample->envelope_velf[EG_GUS_RELEASE2])
		vp->amp_env.follow[ENV0_RELEASE2_STAGE] *= POW2(amp_velsub * (double)vp->sample->envelope_velf[EG_GUS_RELEASE2] * DIV_1200);
	if (vp->sample->envelope_velf[EG_GUS_RELEASE3])
		vp->amp_env.follow[ENV0_RELEASE3_STAGE] *= POW2(amp_velsub * (double)vp->sample->envelope_velf[EG_GUS_RELEASE3] * DIV_1200);
	/* NRPN */
	if(add_param[EG_GUS_ATTACK]){
		FLOAT_T sub_ofs_div_cr = fabs(0.0 - vp->amp_env.offset[ENV0_ATTACK_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->amp_env.rate[ENV0_ATTACK_STAGE];
		time_ms = calc_nrpn_param(time_ms, (double)add_param[EG_GUS_ATTACK] * env_attack_calc, nrpn_env_attack_mode);
		if(time_ms <= 0.0) 
			vp->amp_env.rate[ENV0_ATTACK_STAGE] = ENV0_OFFSET_MAX;
		else
			vp->amp_env.rate[ENV0_ATTACK_STAGE] = sub_ofs_div_cr / time_ms;
	}
	if(add_param[EG_GUS_DECAY]){
		FLOAT_T sub_ofs_div_cr = fabs(vp->amp_env.offset[ENV0_ATTACK_STAGE] - vp->amp_env.offset[ENV0_HOLD_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->amp_env.rate[ENV0_HOLD_STAGE];
		time_ms *= lookup_nrpn_param(64.0 + (double)add_param[EG_GUS_DECAY] * env_decay_calc, nrpn_env_decay_mode);
		if(time_ms < 5.0) time_ms = 5.0;
		vp->amp_env.rate[ENV0_HOLD_STAGE] = sub_ofs_div_cr / time_ms;
	}
	if(add_param[EG_GUS_SUSTAIN]){
		FLOAT_T sub_ofs_div_cr = fabs(vp->amp_env.offset[ENV0_HOLD_STAGE] - vp->amp_env.offset[ENV0_DECAY_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->amp_env.rate[ENV0_DECAY_STAGE];
		time_ms *= lookup_nrpn_param(64.0 + (double)add_param[EG_GUS_SUSTAIN] * env_decay_calc, nrpn_env_decay_mode);
		if(time_ms < 5.0) time_ms = 5.0;
		vp->amp_env.rate[ENV0_DECAY_STAGE] = sub_ofs_div_cr / time_ms;
	}
	if(add_param[EG_GUS_RELEASE1]){
		FLOAT_T sub_ofs_div_cr = fabs(ENV0_OFFSET_MAX - vp->amp_env.offset[ENV0_RELEASE1_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->amp_env.rate[ENV0_RELEASE1_STAGE];
		time_ms = calc_nrpn_param(time_ms, (double)add_param[EG_GUS_RELEASE1] * env_release_calc, nrpn_env_release_mode);
		if(time_ms < 5.0) time_ms = 5.0;
		vp->amp_env.rate[ENV0_RELEASE1_STAGE] = sub_ofs_div_cr / time_ms;
	}
}

static void recompute_mod_envelope_follow(int v, int ch)
{
	Voice *vp = voice + v; 
	Channel *cp = channel + ch;
	int i, note = vp->note, dr = ISDRUMCHANNEL(ch);
	double notesub = (double)(vp->note - vp->sample->modenv_keyf_bpo);
	double mod_velsub = (double)(vp->velocity - vp->sample->modenv_velf_bpo);
	struct DrumParts *dp = NULL;
	int add_param[6];

		
	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++)
		vp->mod_env.follow[i] = 1.0;	
	for(i = 0; i < 6; i++)
		add_param[i] = 0;

	if (dr){
		if(cp->drums[note] != NULL){
			dp = cp->drums[note];
			if(dp->drum_envelope_rate[EG_GUS_ATTACK] != -1)
				add_param[EG_GUS_ATTACK] = (dp->drum_envelope_rate[EG_GUS_ATTACK] - 0x40);
			if(dp->drum_envelope_rate[EG_GUS_DECAY] != -1)
				add_param[EG_GUS_DECAY] = (dp->drum_envelope_rate[EG_GUS_DECAY] - 0x40);
			if(dp->drum_envelope_rate[EG_GUS_SUSTAIN] != -1)
				add_param[EG_GUS_SUSTAIN] = (dp->drum_envelope_rate[EG_GUS_SUSTAIN] - 0x40);
			if(dp->drum_envelope_rate[EG_GUS_RELEASE1] != -1)
				add_param[EG_GUS_RELEASE1] = (dp->drum_envelope_rate[EG_GUS_RELEASE1] - 0x40);
		}
	}else{
		if(cp->envelope_rate[EG_GUS_ATTACK] != -1)
			add_param[EG_GUS_ATTACK] = (cp->envelope_rate[EG_GUS_ATTACK] - 0x40);
		if(cp->envelope_rate[EG_GUS_DECAY] != -1)
			add_param[EG_GUS_DECAY] = (cp->envelope_rate[EG_GUS_DECAY] - 0x40);
		if(cp->envelope_rate[EG_GUS_SUSTAIN] != -1)
			add_param[EG_GUS_SUSTAIN] = (cp->envelope_rate[EG_GUS_SUSTAIN] - 0x40);
		if(cp->envelope_rate[EG_GUS_RELEASE1] != -1)
			add_param[EG_GUS_RELEASE1] = (cp->envelope_rate[EG_GUS_RELEASE1] - 0x40);
		/* envelope key-follow */	
		if (vp->sample->modenv_keyf[EG_GUS_ATTACK])
			vp->mod_env.follow[ENV0_ATTACK_STAGE] *= POW2(notesub * (double)vp->sample->modenv_keyf[EG_GUS_ATTACK] * DIV_1200);
		if (vp->sample->modenv_keyf[EG_GUS_DECAY])
			vp->mod_env.follow[ENV0_HOLD_STAGE] *= POW2(notesub * (double)vp->sample->modenv_keyf[EG_GUS_DECAY] * DIV_1200);
		if (vp->sample->modenv_keyf[EG_GUS_SUSTAIN])
			vp->mod_env.follow[ENV0_DECAY_STAGE] *= POW2(notesub * (double)vp->sample->modenv_keyf[EG_GUS_SUSTAIN] * DIV_1200);
		if (vp->sample->modenv_keyf[EG_GUS_RELEASE1])
			vp->mod_env.follow[ENV0_RELEASE1_STAGE] *= POW2(notesub * (double)vp->sample->modenv_keyf[EG_GUS_RELEASE1] * DIV_1200);
		if (vp->sample->modenv_keyf[EG_GUS_RELEASE2])
			vp->mod_env.follow[ENV0_RELEASE2_STAGE] *= POW2(notesub * (double)vp->sample->modenv_keyf[EG_GUS_RELEASE2] * DIV_1200);
		if (vp->sample->modenv_keyf[EG_GUS_RELEASE3])
			vp->mod_env.follow[ENV0_RELEASE3_STAGE] *= POW2(notesub * (double)vp->sample->modenv_keyf[EG_GUS_RELEASE3] * DIV_1200);
	}	
	/* envelope velocity-follow */
	if (vp->sample->modenv_velf[EG_GUS_ATTACK])
		vp->mod_env.follow[ENV0_ATTACK_STAGE] *= POW2(mod_velsub * (double)vp->sample->modenv_velf[EG_GUS_ATTACK] * DIV_1200);
	if (vp->sample->modenv_velf[EG_GUS_DECAY])
		vp->mod_env.follow[ENV0_HOLD_STAGE] *= POW2(mod_velsub * (double)vp->sample->modenv_velf[EG_GUS_DECAY] * DIV_1200);
	if (vp->sample->modenv_velf[EG_GUS_SUSTAIN])
		vp->mod_env.follow[ENV0_DECAY_STAGE] *= POW2(mod_velsub * (double)vp->sample->modenv_velf[EG_GUS_SUSTAIN] * DIV_1200);
	if (vp->sample->modenv_velf[EG_GUS_RELEASE1])
		vp->mod_env.follow[ENV0_RELEASE1_STAGE] *= POW2(mod_velsub * (double)vp->sample->modenv_velf[EG_GUS_RELEASE1] * DIV_1200);
	if (vp->sample->modenv_velf[EG_GUS_RELEASE2])
		vp->mod_env.follow[ENV0_RELEASE2_STAGE] *= POW2(mod_velsub * (double)vp->sample->modenv_velf[EG_GUS_RELEASE2] * DIV_1200);
	if (vp->sample->modenv_velf[EG_GUS_RELEASE3])
		vp->mod_env.follow[ENV0_RELEASE3_STAGE] *= POW2(mod_velsub * (double)vp->sample->modenv_velf[EG_GUS_RELEASE3] * DIV_1200);
	/* NRPN */
	if(add_param[EG_GUS_ATTACK]){
		FLOAT_T sub_ofs_div_cr = fabs(0.0 - vp->mod_env.offset[ENV0_ATTACK_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->mod_env.rate[ENV0_ATTACK_STAGE];
		time_ms = calc_nrpn_param(time_ms, (double)add_param[EG_GUS_ATTACK] * env_attack_calc, nrpn_env_attack_mode);
		if(time_ms <= 0.0) 
			vp->mod_env.rate[ENV0_ATTACK_STAGE] = ENV0_OFFSET_MAX;
		else
			vp->mod_env.rate[ENV0_ATTACK_STAGE] = sub_ofs_div_cr / time_ms;
	}
	if(add_param[EG_GUS_DECAY]){
		FLOAT_T sub_ofs_div_cr = fabs(vp->mod_env.offset[ENV0_ATTACK_STAGE] - vp->mod_env.offset[ENV0_HOLD_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->mod_env.rate[ENV0_HOLD_STAGE];
		time_ms *= lookup_nrpn_param(64.0 + (double)add_param[EG_GUS_DECAY] * env_decay_calc, nrpn_env_decay_mode);
		if(time_ms < 5.0) time_ms = 5.0;
		vp->mod_env.rate[ENV0_HOLD_STAGE] = sub_ofs_div_cr / time_ms;
	}
	if(add_param[EG_GUS_SUSTAIN]){
		FLOAT_T sub_ofs_div_cr = fabs(vp->mod_env.offset[ENV0_HOLD_STAGE] - vp->mod_env.offset[ENV0_DECAY_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->mod_env.rate[ENV0_DECAY_STAGE];
		time_ms *= lookup_nrpn_param(64.0 + (double)add_param[EG_GUS_SUSTAIN] * env_decay_calc, nrpn_env_decay_mode);
		if(time_ms < 5.0) time_ms = 5.0;
		vp->mod_env.rate[ENV0_DECAY_STAGE] = sub_ofs_div_cr / time_ms;
	}
	if(add_param[EG_GUS_RELEASE1]){
		FLOAT_T sub_ofs_div_cr = fabs(ENV0_OFFSET_MAX - vp->mod_env.offset[ENV0_RELEASE1_STAGE]) * div_playmode_rate * 1000.0;
		FLOAT_T time_ms = sub_ofs_div_cr / vp->mod_env.rate[ENV0_RELEASE1_STAGE];
		time_ms = calc_nrpn_param(time_ms, (double)add_param[EG_GUS_RELEASE1] * env_release_calc, nrpn_env_release_mode);
		if(time_ms < 5.0) time_ms = 5.0;
		vp->mod_env.rate[ENV0_RELEASE1_STAGE] = sub_ofs_div_cr / time_ms;
	}
}

static void set_amp_envelope_param(int v)
{
	int i;
	Voice *vp = voice + v;

	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++){
		if(i == ENV0_END_STAGE)
			continue;		
		if(i == ENV0_SUSTAIN_STAGE){
			vp->amp_env.rate[i] = div_control_ratio;
			vp->amp_env.offset[i] = 0;
		}else if(i == ENV0_RELEASE4_STAGE){
			vp->amp_env.rate[i] = (double)OFFSET_MAX * DIV_RELEASE4_TIME * div_playmode_rate;
			vp->amp_env.offset[i] = 0;
		}else if(i < ENV0_RELEASE1_STAGE){
			vp->amp_env.rate[i] = (double)vp->sample->envelope_rate[i - 1] * div_control_ratio;
			vp->amp_env.offset[i] = (double)vp->sample->envelope_offset[i - 1];
		}else{
			vp->amp_env.rate[i] = (double)vp->sample->envelope_rate[i - 2] * div_control_ratio;
			vp->amp_env.offset[i] = (double)vp->sample->envelope_offset[i - 2];
		}
	}
	vp->amp_env.curve[ENV0_ATTACK_STAGE] = LINEAR_CURVE;
	if(vp->sample->inst_type == INST_SF2){
		for(i = ENV0_ATTACK_STAGE + 1; i < ENV0_STAGE_LIST_MAX; i++)
			vp->amp_env.curve[i] = SF2_VOL_CURVE;
	}else switch(play_system_mode){
	case GS_SYSTEM_MODE:
		for(i = ENV0_ATTACK_STAGE + 1; i < ENV0_STAGE_LIST_MAX; i++)
			vp->amp_env.curve[i] = GS_VOL_CURVE;
		break;
	case XG_SYSTEM_MODE:
		for(i = ENV0_ATTACK_STAGE + 1; i < ENV0_STAGE_LIST_MAX; i++)
			vp->amp_env.curve[i] = XG_VOL_CURVE;
		break;
	case GM2_SYSTEM_MODE:
	case GM_SYSTEM_MODE:
	default:
		for(i = ENV0_ATTACK_STAGE + 1; i < ENV0_STAGE_LIST_MAX; i++)
			vp->amp_env.curve[i] = DEF_VOL_CURVE;
		break;
	}
}

static void set_mod_envelope_param(int v)
{
	int i;
	Voice *vp = voice + v;

	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++){
		if(i == ENV0_END_STAGE)
			continue;
		if(i == ENV0_SUSTAIN_STAGE){
			vp->mod_env.rate[i] = div_control_ratio;
			vp->mod_env.offset[i] = 0;
		}else if(i == ENV0_RELEASE4_STAGE){
			vp->mod_env.rate[i] = (double)OFFSET_MAX * DIV_RELEASE4_TIME * div_playmode_rate;
			vp->mod_env.offset[i] = 0;
		}else if(i < ENV0_RELEASE1_STAGE){
			vp->mod_env.rate[i] = (double)vp->sample->modenv_rate[i - 1] * div_control_ratio;
			vp->mod_env.offset[i] = (double)vp->sample->modenv_offset[i - 1];
		}else{
			vp->mod_env.rate[i] = (double)vp->sample->modenv_rate[i - 2] * div_control_ratio;
			vp->mod_env.offset[i] = (double)vp->sample->modenv_offset[i - 2];
		}
	}
	for(i = 0; i < ENV0_STAGE_LIST_MAX; i++){
		if(i == ENV0_ATTACK_STAGE)
			vp->mod_env.curve[i] = SF2_CONVEX;
		else
			vp->mod_env.curve[i] = SF2_CONCAVE;
	}

}

/* initialize amp/mod/pitch envelope */
static void init_voice_envelope(int i)
{
	Voice *vp = voice + i;
	int ch = vp->channel;
	int note = vp->note;
	int add_delay_cnt = vp->add_delay_cnt;
	
	/* initialize amp envelope */
	if(vp->sample->modes & MODES_ENVELOPE){		
		init_envelope0(&vp->amp_env);
		set_amp_envelope_param(i);
		recompute_amp_envelope_follow(i, ch);
		vp->amp_env.offdelay = vp->amp_env.delay = vp->sample->envelope_delay + add_delay_cnt;
		vp->amp_env.offdelay += env_add_offdelay_count;
		if(min_sustain_time == 1)
			vp->amp_env.count[ENV0_SUSTAIN_STAGE] = 1; /* The sustain stage is ignored. */
		else if(channel[ch].loop_timeout > 0 && channel[ch].loop_timeout * 1000 < min_sustain_time)
			vp->amp_env.count[ENV0_SUSTAIN_STAGE] = channel[ch].loop_timeout * play_mode->rate; /* timeout (See also "#extension timeout" line in *.cfg file */
		else if(min_sustain_time <= 0)
			vp->amp_env.count[ENV0_SUSTAIN_STAGE] = 0;
		else
			vp->amp_env.count[ENV0_SUSTAIN_STAGE] = playmode_rate_ms * min_sustain_time;
		apply_envelope0_param(&vp->amp_env);
	}else{	
		init_envelope0(&vp->amp_env);
		vp->amp_env.offdelay = vp->amp_env.delay = vp->sample->envelope_delay + add_delay_cnt;
		vp->amp_env.volume = 1.0;
	}
	/* initialize modulation envelope */
	if (opt_modulation_envelope && vp->sample->modes & MODES_ENVELOPE){
		init_envelope0(&vp->mod_env);	
		set_mod_envelope_param(i);	
		recompute_mod_envelope_follow(i, ch);
		vp->mod_env.delay = vp->sample->modenv_delay;
		vp->mod_env.offdelay = vp->sample->modenv_delay + env_add_offdelay_count;
		apply_envelope0_param(&vp->mod_env);
	}else{
		init_envelope0(&vp->mod_env);
		vp->mod_env.delay = vp->sample->modenv_delay;
		vp->mod_env.offdelay = vp->sample->modenv_delay + env_add_offdelay_count;
		vp->mod_env.volume = 1.0;
	}
	/* initialize pitch envelope */
	if (opt_modulation_envelope && vp->sample->modes & MODES_ENVELOPE){
		FLOAT_T ini_lv = vp->sample->pitch_envelope[0]; // cent init lv
		FLOAT_T atk_lv = vp->sample->pitch_envelope[1]; // cent attack lv
		FLOAT_T atk_tm = vp->sample->pitch_envelope[2]; // ms   attacl time
		FLOAT_T dcy_lv = vp->sample->pitch_envelope[3]; // cent decay1 lv
		FLOAT_T dcy_tm = vp->sample->pitch_envelope[4]; // ms   decay1 time
		FLOAT_T sus_lv = vp->sample->pitch_envelope[5]; // cent decay2 lv
		FLOAT_T sus_tm = vp->sample->pitch_envelope[6]; // ms   decay2 time
		FLOAT_T rls_lv = vp->sample->pitch_envelope[7]; // cent release lv
		FLOAT_T rls_tm = vp->sample->pitch_envelope[8]; // ms   release time
		if(channel[ch].pit_env_level[0] != 0x40)
			ini_lv += (double)(channel[ch].pit_env_level[0] - 64) * DIV_64 * 1280.0; // cent
		if(channel[ch].pit_env_level[1] != 0x40)
			atk_lv += (double)(channel[ch].pit_env_level[1] - 64) * DIV_64 * 1280.0; // cent
		if(channel[ch].pit_env_level[2] != 0x40)
			dcy_lv += (double)(channel[ch].pit_env_level[2] - 64) * DIV_64 * 1280.0; // cent
		if(channel[ch].pit_env_level[3] != 0x40)
			sus_lv += (double)(channel[ch].pit_env_level[3] - 64) * DIV_64 * 1280.0; // cent
		if(channel[ch].pit_env_level[4] != 0x40)
			rls_lv += (double)(channel[ch].pit_env_level[4] - 64) * DIV_64 * 1280.0; // cent
		if(channel[ch].pit_env_time[0] != 0x40)
			atk_tm *= POW2((double)(channel[ch].pit_env_time[0] - 64) * DIV_32); // mul 0.5~2.0
		if(channel[ch].pit_env_time[1] != 0x40)
			dcy_tm *= POW2((double)(channel[ch].pit_env_time[1] - 64) * DIV_32); // mul 0.5~2.0
		if(channel[ch].pit_env_time[2] != 0x40)
			sus_tm *= POW2((double)(channel[ch].pit_env_time[2] - 64) * DIV_32); // mul 0.5~2.0
		if(channel[ch].pit_env_time[3] != 0x40)
			rls_tm *= POW2((double)(channel[ch].pit_env_time[3] - 64) * DIV_32); // mul 0.5~2.0
		init_envelope4(&vp->pit_env, 0, env_add_offdelay_count, 
			ini_lv, 
			atk_lv, atk_tm * playmode_rate_ms, 
			dcy_lv, dcy_tm * playmode_rate_ms, 
			sus_lv, sus_tm * playmode_rate_ms,  
			rls_lv, rls_tm * playmode_rate_ms );
	}else{
		init_envelope4(&vp->pit_env, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
}

// called mix.c mix_voice() , thread_mix.c mix_voice_thread()
void update_voice(int i)
{
	Voice *vp = voice + i;

	// 同時処理が必要になったらビットフラグ分岐に変更する可能性があるので 定数は2^nにしておく
	// playmidi.h /* update_voice options: */
	switch(vp->update_voice){
	case UPDATE_VOICE_FINISH_NOTE: // finish note
		reset_envelope0_release(&vp->amp_env, ENV0_KEEP);
		reset_envelope0_release(&vp->mod_env, ENV0_KEEP);
		reset_envelope4_noteoff(&vp->pit_env);
#ifdef INT_SYNTH
		if(vp->sample->inst_type == INST_MMS)
			noteoff_int_synth_mms(i);
		else if(vp->sample->inst_type == INST_SCC)
			noteoff_int_synth_scc(i);
#endif
#ifdef VOICE_EFFECT
		noteoff_voice_effect(i);	    /* voice effect */
#endif
		break;
	case UPDATE_VOICE_CUT_NOTE: // cut_note
		reset_envelope0_release(&vp->amp_env, cut_short_rate);
		break;
	case UPDATE_VOICE_CUT_NOTE2: // cut_note2
		reset_envelope0_release(&vp->amp_env, cut_short_rate2);
		break;		
	case UPDATE_VOICE_KILL_NOTE: // kill_note
		reset_envelope0_release(&vp->amp_env, ramp_out_rate);
		break;		
	}
	vp->update_voice = 0; // flag clear
}

// called mix.c mix_voice() , thread_mix.c mix_voice_thread()
void init_voice(int i)
{
	Voice *vp = voice + i;
	int ch = vp->channel;
	
	vp->init_voice = 1; // init flag
	/* pan */
	vp->panned = (play_mode->encoding & PE_MONO) ? PANNED_CENTER : PANNED_MYSTERY;
	vp->pan_ctrl = 1; // pan update recompute_voice_amp()	
	init_voice_resample(i);
#ifdef INT_SYNTH
	if(vp->sample->inst_type == INST_MMS)
		init_int_synth_mms(i);
	else if(vp->sample->inst_type == INST_SCC)
		init_int_synth_scc(i);
#endif
	init_voice_portamento(i); /* portamento or legato */
	init_voice_envelope(i);   /* amp/mod/pitch envelope */
	init_voice_lfo(i);        /* lfo1,lfo2 */
	init_voice_filter(i);     /* resonant lowpass filter */
	init_voice_filter2(i);    /* resonant highpass filter */	
	init_voice_pitch(i);      /* pitch */
	init_voice_amp(i);        /* after init_resample_filter() */	
	recompute_voice_lfo(i);
	recompute_voice_filter(i); 
	recompute_voice_pitch(i); 
	recompute_voice_amp(i);	
	init_resample_filter(i);  /* resample filter , after voice recomp */
	recompute_resample_filter(i); /* after voice recomp */
	/* initialize volume envelope */
	init_envelope2(&vp->vol_env , vp->left_amp, vp->right_amp, vol_env_count);
	/* initialize mix envelope */
	apply_envelope_to_amp(i);
	if(opt_mix_envelope)
		init_envelope2(&vp->mix_env , vp->left_mix, vp->right_mix, mix_env_count);  
#ifdef VOICE_EFFECT
	init_voice_effect(i);	    /* voice effect */
#endif	
}

///r
static void start_note(MidiEvent *e, int i, int vid, int cnt, int add_delay_cnt)
{
	Voice *vp = voice + i;
	int ch = e->channel;
	int note = MIDI_EVENT_NOTE(e);
	int j;

	/* status , control */
	vp->status = VOICE_ON;
	vp->channel = ch;
	vp->note = note;
	vp->velocity = e->b;
	vp->vid = vid;  
	vp->sostenuto = 0;
	vp->paf_ctrl = 0;
	vp->overlap_count = opt_overlap_voice_count;
	vp->channel_voice_count = opt_max_channel_voices;
	vp->mod_update_count = -1;
	vp->init_voice = 0; // flag clear
	vp->update_voice = 0; // flag clear
	vp->finish_voice = 0; // flag clear
	vp->add_delay_cnt = add_delay_cnt;
	/* for pitch ctrl */
	vp->prev_tuning = 0;
	vp->pitchfactor = 0;
	vp->orig_pitchfactor = 1.0;
	/* for resample */
	j = channel[ch].special_sample;  
	if(!j){
		vp->reserve_offset = 0;
	}else{ // special_patch
		if(!special_patch[j]){
			vp->reserve_offset = 0;
		}else{
			vp->reserve_offset = (splen_t)special_patch[j]->sample_offset << FRACTION_BITS;
			if(vp->sample->modes & MODES_LOOPING)  {
				if(vp->reserve_offset > vp->sample->loop_end)
					vp->reserve_offset = vp->sample->loop_start;
			} else if(vp->reserve_offset > vp->sample->data_length){
				free_voice(i);
				return;
			}
		}  
	}
	/* update ctl */
	if(!prescanning_flag)
		ctl_note_event(i);
}



static void finish_note(int i)
{
    if (voice[i].sample->modes & MODES_ENVELOPE)
    {
		/* We need to get the envelope out of Sustain stage. */
		/* Note that voice[i].envelope_stage < EG_GUS_RELEASE1 */
//		voice[i].status = VOICE_OFF;		
///r
		
		if(voice[i].status & (VOICE_FREE | VOICE_DIE | VOICE_OFF))
			return;
		
		if(!(voice[i].sample->modes & MODES_NO_NOTEOFF))
		{
			voice[i].status = VOICE_OFF;
			voice[i].update_voice = UPDATE_VOICE_FINISH_NOTE; // finish note
		}
		ctl_note_event(i);
	}
    else
    {
		if(current_file_info->pcm_mode != PCM_MODE_NON)
		{
			free_voice(i);
			ctl_note_event(i);
		}
		else
		{
			/* Set status to OFF so resample_voice() will let this voice out
			of its loop, if any. In any case, this voice dies when it
				hits the end of its data (ofs>=data_length). */
			if(voice[i].status != VOICE_OFF)
			{
				if(!(voice[i].sample->modes & MODES_NO_NOTEOFF))
					voice[i].status = VOICE_OFF;
				ctl_note_event(i);
			}
		}
    }
}


static void set_envelope_time(int ch, int val, int stage)
{
	val = val & 0x7F;
	switch(stage) {
	case EG_ATTACK:	/* Attack */
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Attack Time (CH:%d VALUE:%d)", ch, val);
		break;
	case EG_DECAY: /* Decay */
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Decay Time (CH:%d VALUE:%d)", ch, val);
		break;
	case EG_RELEASE:	/* Release */
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Release Time (CH:%d VALUE:%d)", ch, val);
		break;
	default:
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"? Time (CH:%d VALUE:%d)", ch, val);
	}
	channel[ch].envelope_rate[stage] = val;
}



///r

/*! note_on() (prescanning) */
static void note_on_prescan(MidiEvent *ev)
{
	int i, ch = ev->channel, note = MIDI_EVENT_NOTE(ev);
	int32 random_delay = 0;

	if(ISDRUMCHANNEL(ch) &&
	   channel[ch].drums[note] != NULL &&
	   !get_rx_drum(channel[ch].drums[note], RX_NOTE_ON)) {	/* Rx. Note On */
		return;
	}
	if(channel[ch].note_limit_low > note ||
		channel[ch].note_limit_high < note ||
		channel[ch].vel_limit_low > ev->b ||
		channel[ch].vel_limit_high < ev->b) {
		return;
	}

  //  if(!channel[ch].portamento && channel[ch].portamento_control != -1)
	{
		int nv;
		int vlist[32];
		Voice *vp;

		nv = find_samples(ev, vlist);

		if((allocate_cache_size > 0) &&
			ISDRUMCHANNEL(ch) ||
			(!channel[ch].portamento && channel[ch].portamento_control != -1) ){
			for(i = 0; i < nv; i++)
			{
				vp = voice + vlist[i];
				start_note(ev, vlist[i], 0, nv - i - 1, random_delay);
				resamp_cache_refer_on(vp, ev->time);
				vp->status = VOICE_FREE;
				vp->temper_instant = 0;
			}
		}
	}
}

static void note_on(MidiEvent *e)
{
    int i, nv, v, ch = e->channel, note = MIDI_EVENT_NOTE(e);
    int vlist[32];
    int vid;
	int32 random_delay = 0;
	int porta_flg = 0;
	int dr = ISDRUMCHANNEL(ch);
		
	ch = e->channel;
	note = MIDI_EVENT_NOTE(e);
	
	if(dr &&  channel[ch].drums[note] != NULL &&
	   !get_rx_drum(channel[ch].drums[note], RX_NOTE_ON)) {	/* Rx. Note On */
		return;
	}
	if(channel[ch].note_limit_low > note ||
		channel[ch].note_limit_high < note ||
		channel[ch].vel_limit_low > e->b ||
		channel[ch].vel_limit_high < e->b) {
		return;
	}
	voice_control(e);
    if((nv = find_samples(e, vlist)) == 0)
	return;
	
	if(dr){
		recompute_bank_parameter_drum(ch, note);	
	}else{
		if(!channel[ch].program_flag) // PCがなかった場合(recompute_bank_paramしてない場合
			recompute_bank_parameter_tone(ch);
		/* portamento , legato */
		if(opt_portamento)
			if(init_channel_portamento(e)) /* portamento , legato */
				return;
	}

	if(! opt_realtime_playing && emu_delay_time > 0)
		random_delay = calc_random_delay(ch, note) + calc_emu_delay(e);
	else
		random_delay = calc_random_delay(ch, note);
	
    vid = new_vidq(e->channel, note);
    for(i = 0; i < nv; i++) {
		v = vlist[i];
		if(ISDRUMCHANNEL(ch) &&
		   channel[ch].drums[note] != NULL &&
		   channel[ch].drums[note]->pan_random){
			channel[ch].drums[note]->drum_panning = int_rand(128);
		}else if(channel[ch].pan_random)	{
			channel[ch].panning = int_rand(128);
			ctl_mode_event(CTLE_PANNING, 1, ch, channel[ch].panning);
		}
		start_note(e, v, vid, nv - i - 1, random_delay);
    }	
}
///r
/*! sostenuto is now implemented as an instant sustain */
static void update_sostenuto_controls(int ch)
{
  int uv = upper_voices, i;

  if(ISDRUMCHANNEL(ch)) {return;}

	for(i = 0; i < uv; i++)
		if ((voice[i].status & VOICE_ON) && voice[i].channel == ch)
		{
			voice[i].sostenuto = channel[ch].sostenuto;
			ctl_note_event(i);
		}

}
///r
/*! redamper / half damper effect for piano instruments */
static void update_redamper_controls(int ch)
{
  int uv = upper_voices, i;
  
  if(ISDRUMCHANNEL(ch) || channel[ch].damper_mode == 0 || channel[ch].sustain == 0) {return;}

	for(i = 0; i < uv; i++)
		if ((voice[i].status & (VOICE_OFF | VOICE_SUSTAINED)) && voice[i].channel == ch)
		{
			voice[i].status = VOICE_SUSTAINED;
			reset_envelope0_damper(&voice[i].amp_env, channel[ch].sustain);
			reset_envelope0_damper(&voice[i].mod_env, channel[ch].sustain);
			ctl_note_event(i);
#ifdef INT_SYNTH
		if(voice[i].sample->inst_type == INST_MMS)
			damper_int_synth_mms(i, channel[ch].sustain);
		else if(voice[i].sample->inst_type == INST_SCC)
			damper_int_synth_scc(i, channel[ch].sustain);
#endif

#ifdef VOICE_EFFECT
			damper_voice_effect(i, channel[ch].sustain);
#endif
		}
}
///r
static void note_off(MidiEvent *e)
{
  int uv = upper_voices, i;
  int ch, note, vid;
  int elm = 0;

  ch = e->channel;
  note = MIDI_EVENT_NOTE(e);

  if(ISDRUMCHANNEL(ch))
  {
      int nbank, nprog;

      nbank = channel[ch].bank;
      nprog = note;
      instrument_map(channel[ch].mapID, &nbank, &nprog);
      
      if (channel[ch].drums[nprog] != NULL){
		  if(!get_rx_drum(channel[ch].drums[nprog], RX_NOTE_OFF)){ // disable NOTE_OFF
			  return;
		  }else{
			  ToneBank *bank;
			  bank = drumset[nbank];
			  if(bank == NULL) bank = drumset[0];          
			  if (bank->tone[nprog][elm]){
				  /* uh oh, this drum doesn't have an instrument loaded yet */
				  if (bank->tone[nprog][elm]->instrument == NULL)
					  return;
				  /* this drum is not loaded for some reason (error occured?) */
				  if (IS_MAGIC_INSTRUMENT(bank->tone[nprog][elm]->instrument))
					  return;
				  /* only disallow Note Off if the drum sample is not looped */
				  //if (!(bank->tone[nprog][elm].instrument->sample->modes & MODES_LOOPING))
					 // return;	/* Note Off is not allowed. */
			  }
		  }
	  }
  }
  /* legato */
  if(opt_portamento){
	  int tmp = note_off_legato(e); // -2:return -1:thru 0<=:change note
	  if(tmp < -1)
		  return;
	  else if(tmp >= 0)
		  note = tmp;
  }

  if ((vid = last_vidq(ch, note)) == -1)
      return;
  for (i = 0; i < uv; i++)
  {	  
      if(voice[i].status == VOICE_ON && voice[i].channel == ch && voice[i].note == note && voice[i].vid == vid)
      {
		  if(voice[i].sostenuto){
			  voice[i].status = VOICE_SUSTAINED;
			  ctl_note_event(i);
		  }else if(channel[ch].sustain){
			  voice[i].status = VOICE_SUSTAINED;
			  if(channel[ch].damper_mode){
				reset_envelope0_damper(&voice[i].amp_env, channel[ch].sustain);
				reset_envelope0_damper(&voice[i].mod_env, channel[ch].sustain);
			  }
			  ctl_note_event(i);
		  }else{
			  finish_note(i);
		  }
      }
  }
}


/* Process the All Notes Off event */
static void all_notes_off(int c)
{
  int i, uv = upper_voices;
  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "All notes off on channel %d", c);
  for(i = 0; i < uv; i++)
    if (voice[i].status==VOICE_ON &&
	voice[i].channel==c)
      {
	if (channel[c].sustain)
	  {
	    voice[i].status=VOICE_SUSTAINED;
	    ctl_note_event(i);
	  }
	else
	  finish_note(i);
      }
  for(i = 0; i < 128; i++)
      vidq_head[c * 128 + i] = vidq_tail[c * 128 + i] = 0;
}

/* Process the All Sounds Off event */
static void all_sounds_off(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].channel==c &&
	(voice[i].status & ~(VOICE_FREE | VOICE_DIE)))
      {
	kill_note(i);
      }
  for(i = 0; i < 128; i++)
      vidq_head[c * 128 + i] = vidq_tail[c * 128 + i] = 0;
}

/*! adjust polyphonic key pressure (PAf, PAT) */
static void adjust_pressure(MidiEvent *e)
{
    int i, uv = upper_voices;
    int note, ch;

    if(opt_channel_pressure)
    {
	ch = e->channel;
    note = MIDI_EVENT_NOTE(e);
	channel[ch].paf.val = e->b;
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf (CH:%d NOTE:%d VAL:%d)", ch, note, channel[ch].paf.val);
    for(i = 0; i < uv; i++)
		if(voice[i].status == VOICE_ON &&
		   voice[i].channel == ch &&
		   voice[i].note == note)
		{
			voice[i].paf_ctrl = 1;
			//recompute_amp(i);
			//apply_envelope_to_amp(i);
			//recompute_freq(i);
			//recompute_voice_filter(i);
		}
	}
}

/*! adjust channel pressure (channel aftertouch, CAf, CAT) */
static void adjust_channel_pressure(MidiEvent *e)
{
    if(opt_channel_pressure)
    {
	int i, uv = upper_voices;
	int ch;

	ch = e->channel;
	channel[ch].caf.val = e->a;
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf (CH:%d VAL:%d)", ch, channel[ch].caf.val);
	recompute_channel_lfo(ch);
	recompute_channel_amp(ch);
	recompute_channel_filter(ch);
	recompute_channel_pitch(ch);
	  
	//for(i = 0; i < uv; i++)
	//{
	//    if(voice[i].status == VOICE_ON && voice[i].channel == ch)
	//    {
	//		recompute_amp(i);
	//		apply_envelope_to_amp(i);
	//		recompute_freq(i);
	//		recompute_voice_filter(i);
	//	}
	//}
    }
}

static void adjust_panning(int c)
{
    int i, uv = upper_voices;
	
	recompute_channel_amp(c);
	if(!adjust_panning_immediately)
		return;
    for(i = 0; i < uv; i++) {
		if ((voice[i].channel==c) &&
			(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))){
			voice[i].pan_ctrl = 1;
		}
    }
}

void play_midi_setup_drums(int ch, int note)
{	
    channel[ch].drums[note] = (struct DrumParts *)new_segment(&playmidi_pool, sizeof(struct DrumParts));
    reset_drum_controllers(channel[ch].drums, note);
	channel[ch].drum_effect_flag |= 0x2; // add note
}

static void adjust_drum_panning(int ch, int note)
{
 //   int i, uv = upper_voices;

 //   for(i = 0; i < uv; i++) {
	//	if(voice[i].channel == ch &&
	//	   voice[i].note == note &&
	//	   (voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
	//	{
	//		recompute_amp(i);
	//		apply_envelope_to_amp(i);
	//	}
	//}
}

static void drop_sustain(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status == VOICE_SUSTAINED && voice[i].channel == c)
      finish_note(i);
}

static void adjust_all_pitch(void)
{
	int ch, i, uv = upper_voices;
	
	for (ch = 0; ch < MAX_CHANNELS; ch++){
		recompute_channel_pitch(ch);
	}
	//for (i = 0; i < uv; i++)
	//	if (voice[i].status != VOICE_FREE)
	//		recompute_freq(i);
}

static void adjust_pitch(int c)
{
	recompute_channel_lfo(c);
	recompute_channel_amp(c);
	recompute_channel_filter(c);
	recompute_channel_pitch(c);
 // int i, uv = upper_voices;
 // for(i = 0; i < uv; i++)
 //   if (voice[i].status != VOICE_FREE && voice[i].channel == c)
	//recompute_freq(i);
}

static void adjust_volume(int c)
{
	recompute_channel_amp(c);
 // int i, uv = upper_voices;
 // for(i = 0; i < uv; i++)
 //   if (voice[i].channel == c &&
	//(voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
 //     {
	//recompute_amp(i);
	//apply_envelope_to_amp(i);
 //     }
}

int get_reverb_level(int ch)
{
	if (channel[ch].reverb_level == -1)
		return (opt_reverb_control < 0) ? -opt_reverb_control & 0x7f : DEFAULT_REVERB_SEND_LEVEL;
	return channel[ch].reverb_level;
}

static void set_reverb_level(int ch, int level)
{
	if (level == -1) {
		channel[ch].reverb_level = (opt_reverb_control < 0)	? -opt_reverb_control & 0x7f : DEFAULT_REVERB_SEND_LEVEL;
	}else{
		channel[ch].reverb_level = level;
	}
}

int get_chorus_level(int ch)
{
#ifdef DISALLOW_DRUM_BENDS
    if(ISDRUMCHANNEL(ch))
	return 0; /* Not supported drum channel chorus */
#endif
	if(channel[ch].chorus_level == -1)
		return (opt_chorus_control<0) ? -opt_chorus_control : 0;
  if(opt_chorus_control == 1)
		return channel[ch].chorus_level;
  return -opt_chorus_control;
}


void free_drum_effect(int ch)
{
	int i, j;
	struct DrumPartEffect *de;
	if (channel[ch].drum_effect != NULL) {
		for (i = 0; i < channel[ch].drum_effect_num; i++) {	
			de = &(channel[ch].drum_effect[i]);	

#ifdef MULTI_THREAD_COMPUTE	

			for (j = 0; j < 4; j++) {
#ifdef ALIGN_SIZE
				aligned_free(de->buf_ptr[j]);
#else	
				safe_free(de->buf_ptr[j]);
#endif
				de->buf_ptr[j] = NULL;
			}
			de->buf = NULL;

#else  // ! MULTI_THREAD_COMPUTE
			
#ifdef ALIGN_SIZE
			aligned_free(de->buf);
#else
			safe_free(de->buf);
#endif
			de->buf = NULL;	

#endif // MULTI_THREAD_COMPUTE	

		}
		safe_free(channel[ch].drum_effect);
		channel[ch].drum_effect = NULL;
	}
	channel[ch].drum_effect_num = 0;
	channel[ch].drum_effect_flag = 0;
}

///r
static void make_drum_effect(int ch)
{
	int i, j, note, num = 0, size;
	int8 note_table[128];
	struct DrumParts *drum;
	struct DrumPartEffect *de;

	if (!opt_drum_effect) {return;}
	if (channel[ch].drums == NULL) {return;}

	// init
	if(!(channel[ch].drum_effect_flag & 0x1)){ 
		free_drum_effect(ch);
		memset(note_table, 0, sizeof(int8) * 128);
		for(i = 0; i < 128; i++) {
			if ((drum = channel[ch].drums[i]) == NULL)
				continue;	
			if (drum->reverb_level != -1 || drum->chorus_level != -1 || drum->delay_level != -1 
				|| (opt_eq_control && drum->eq_xg.valid)) {
				note_table[num++] = i;
			}
		}
		channel[ch].drum_effect = (struct DrumPartEffect *)safe_malloc(sizeof(struct DrumPartEffect) * 128); // 128=all ,  def: num
		size = AUDIO_BUFFER_SIZE * 2 * sizeof(DATA_T);

		for(i = 0; i < num; i++) {
			de = &(channel[ch].drum_effect[i]);
			de->note = note_table[i];
			drum = channel[ch].drums[de->note];
			de->reverb_send = (int32)drum->reverb_level * (int32)get_reverb_level(ch) / 127;
	//	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "dfx init reverb send: ch:%d note:%d : level:%d", ch, de->note, drum->reverb_level); // test
			de->chorus_send = (int32)drum->chorus_level * (int32)channel[ch].chorus_level / 127;
			de->delay_send = (int32)drum->delay_level * (int32)channel[ch].delay_level / 127;
			de->eq_xg = &drum->eq_xg;		
#ifdef MULTI_THREAD_COMPUTE	
			for (j = 0; j < 4; j++) {
#ifdef ALIGN_SIZE
				de->buf_ptr[j] = (DATA_T *)aligned_malloc(size, ALIGN_SIZE);
#else	
				de->buf_ptr[j] = (DATA_T *)safe_malloc(size);
#endif
				memset(de->buf_ptr[j], 0, size);
			}
			de->buf = de->buf_ptr[0];
#else  // ! MULTI_THREAD_COMPUTE
#ifdef ALIGN_SIZE
			de->buf = (DATA_T *)aligned_malloc(size, ALIGN_SIZE);
#else
			de->buf = (DATA_T *)safe_malloc(size);
#endif
			memset(de->buf, 0, size);	
#endif // MULTI_THREAD_COMPUTE	
		//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Drum Effect : init note (CH:%d note:%d)", ch, de->note);
		}
		channel[ch].drum_effect_num = num;
		channel[ch].drum_effect_flag = 0x1;
		return;
	}	

	// add note
	if (channel[ch].drum_effect_flag & 0x2) { 
		memset(note_table, 0, sizeof(int8) * 128);		
		for(i = 0; i < 128; i++) {
			if ((drum = channel[ch].drums[i]) == NULL)
				continue;	
			if (drum->reverb_level != -1 || drum->chorus_level != -1 || drum->delay_level != -1 || (opt_eq_control && drum->eq_xg.valid)) {
				int dup = 0;
				for (j = 0; j < channel[ch].drum_effect_num; j++) {
					de = &(channel[ch].drum_effect[j]);
					if(i == de->note){
						dup = 1;
						break;
					}
				}
				if(dup) continue;
				note_table[num++] = i;
			}
		}	
		size = AUDIO_BUFFER_SIZE * 2 * sizeof(DATA_T);
		for(i = 0; i < num; i++) {
			int newnum = channel[ch].drum_effect_num + i;
			if(newnum >= 128){
				ctl->cmsg(CMSG_INFO, VERB_NORMAL, "ERROR! Drum Effect : over 128 note (CH:%d)", ch);
				channel[ch].drum_effect_flag = 0; // reset
				make_drum_effect(ch); // reinit
				return;
			}
			de = &(channel[ch].drum_effect[newnum]);
			de->note = note_table[i];
			drum = channel[ch].drums[de->note];
			de->reverb_send = (int32)drum->reverb_level * (int32)get_reverb_level(ch) / 127;
	//	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "dfx add reverb send: ch:%d note:%d : level:%d", ch, de->note, drum->reverb_level); // test
			de->chorus_send = (int32)drum->chorus_level * (int32)channel[ch].chorus_level / 127;
			de->delay_send = (int32)drum->delay_level * (int32)channel[ch].delay_level / 127;
			de->eq_xg = &drum->eq_xg;		
#ifdef MULTI_THREAD_COMPUTE	
			for (j = 0; j < 4; j++) {
#ifdef ALIGN_SIZE
				de->buf_ptr[j] = (DATA_T *)aligned_malloc(size, ALIGN_SIZE);
#else	
				de->buf_ptr[j] = (DATA_T *)safe_malloc(size);
#endif
				memset(de->buf_ptr[j], 0, size);
			}
			de->buf = de->buf_ptr[0];
#else  // ! MULTI_THREAD_COMPUTE
#ifdef ALIGN_SIZE
			de->buf = (DATA_T *)aligned_malloc(size, ALIGN_SIZE);
#else
			de->buf = (DATA_T *)safe_malloc(size);
#endif
			memset(de->buf, 0, size);	
#endif // MULTI_THREAD_COMPUTE				
		//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Drum Effect : add note (CH:%d note:%d)", ch, de->note);
		}
		channel[ch].drum_effect_num += num;
		channel[ch].drum_effect_flag = 0x1;
		return;
	}

	// update effect
	if (channel[ch].drum_effect_flag & 0x4) {
		if (! channel[ch].drum_effect_num) return;			
		for (i = 0; i < channel[ch].drum_effect_num; i++) {
			de = &(channel[ch].drum_effect[i]);
			drum = channel[ch].drums[de->note];
			de->reverb_send = (int32)drum->reverb_level * (int32)get_reverb_level(ch) / 127;
	//	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "dfx update reverb send: ch:%d note:%d : level:%d", ch, de->note, drum->reverb_level); // test
			de->chorus_send = (int32)drum->chorus_level * (int32)channel[ch].chorus_level / 127;
			de->delay_send = (int32)drum->delay_level * (int32)channel[ch].delay_level / 127;
		}
		channel[ch].drum_effect_flag = 0x1;
		return;
	}
}


static void adjust_master_volume(void)
{
  //int i, uv = upper_voices;
  adjust_amplification();
  //for(i = 0; i < uv; i++)
  //    if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
  //    {
	 // recompute_amp(i);
	 // apply_envelope_to_amp(i);
  //    }
}

int midi_drumpart_change(int ch, int isdrum)
{
	if (IS_SET_CHANNELMASK(drumchannel_mask, ch))
		return 0;
	if (isdrum) {
		SET_CHANNELMASK(drumchannels, ch);
		//SET_CHANNELMASK(current_file_info->drumchannels, ch);
	} else {
		UNSET_CHANNELMASK(drumchannels, ch);
		//UNSET_CHANNELMASK(current_file_info->drumchannels, ch);
	}
	return 1;
}


int16 sd_mfx_patch_param[13][9][128][33];

static void sd_mfx_patch_change(int ch, int map, int bank, int prog)
{
	int i, num, mfx;
 	if(channel[ch].sd_output_assign != 2)
		return;
	switch (map) {
	case 80: num = 0; break; /* Special 1 */
	case 81: num = 1; break; /* Special 2 */
	case 87: num = 2; break; /* Usr Inst*/
	case 96: num = 3; break; /* Clsc Inst */
	case 97: num = 4; break; /* Cntn Inst */
	case 98: num = 5; break; /* Solo Inst */
	case 99: num = 6; break; /* Ehnc Inst */
	case 86: num = 7; break; /* Usr Drum */
	case 104: num = 8; break; /* Clsc Drum */
	case 105: num = 9; break; /* Cntn Drum */
	case 106: num = 10; break; /* Solo Drum */
	case 107: num = 11; break; /* Ehnc Drum */
	//case 121: num = 12; break; /* GM2 tone */ // test
	default: num = -1; break;
	}
	if(num < 0) // error
		return;
	channel[ch].mfx_part_type = sd_mfx_patch_param[num][bank][prog][0];
	for(i = 0; i < 32; i++)
		channel[ch].mfx_part_param[i] = sd_mfx_patch_param[num][bank][prog][i + 1];
	if(channel[ch].mfx_part_type <= 0){
		ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 0);
		return;
	}
	mfx = channel[ch].sd_output_mfx_select;
	realloc_mfx_effect_sd(&mfx_effect_sd[mfx], 3);
	ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 1);
}

void midi_program_change(int ch, int prog)
{
	int dr = ISDRUMCHANNEL(ch);
	int newbank, b, p, map;
	int elm = 0;
	
///r
	channel[ch].program_flag = 1;

	switch (play_system_mode) {
	case GS_SYSTEM_MODE:	/* GS */
///r 
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Prog GS");

		if ((map = channel[ch].bank_lsb) == 0) {
			map = channel[ch].tone_map0_number;
		}
		switch (map) {
		case 0:		/* No change */
			break;
		case 1:
			channel[ch].mapID = (dr) ? SC_55_DRUM_MAP : SC_55_TONE_MAP;
			break;
		case 2:
			channel[ch].mapID = (dr) ? SC_88_DRUM_MAP : SC_88_TONE_MAP;
			break;
		case 3:
			channel[ch].mapID = (dr) ? SC_88PRO_DRUM_MAP : SC_88PRO_TONE_MAP;
			break;
		case 4:
			channel[ch].mapID = (dr) ? SC_8850_DRUM_MAP : SC_8850_TONE_MAP;
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_msb;
		break;
///r
/*
//elion add
		}
		if (map == MODULE_SDMODE){
			if (channel[ch].bank_msb >= 104 && channel[ch].bank_msb <= 107){
				midi_drumpart_change(ch, 1);
				channel[ch].mapID = SDXX_DRUM1_MAP + (channel[ch].bank_msb - 104);
				channel[ch].bank_msb = 0;
				newbank = channel[ch].bank_msb;
			}else{
				if (channel[ch].bank_msb == 120) { // gm2 drum
					midi_drumpart_change(ch, 1);
					channel[ch].mapID = GM2_DRUM_MAP;
					channel[ch].bank_msb = 0x78;
					newbank = channel[ch].bank_lsb;
				}else if (channel[ch].bank_msb == 121) { // gm2 inst
					midi_drumpart_change(ch, 0);
					channel[ch].mapID = GM2_TONE_MAP;
					channel[ch].bank_msb = 0;
					newbank = channel[ch].bank_lsb;
				}else if (channel[ch].bank_msb >= 96 && channel[ch].bank_msb <= 99) {// sd map
					channel[ch].mapID = SDXX_TONE_MAP;
					midi_drumpart_change(ch, 0); 
					channel[ch].bank_msb = 0;
					newbank = channel[ch].bank_msb;
				}else if (channel[ch].bank_msb == 80 || channel[ch].bank_msb == 81 || channel[ch].bank_msb == 87){
					channel[ch].mapID = INST_NO_MAP; // spacial
				}else if (channel[ch].bank_msb == 127)
					channel[ch].mapID = INST_NO_MAP; // mt/cm
				else
					channel[ch].mapID = (dr) ? SC_8850_DRUM_MAP : SC_8850_TONE_MAP;
			}
			dr = ISDRUMCHANNEL(ch);
			break;
		}
		break;
*/
	case XG_SYSTEM_MODE:	/* XG */
///r 
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Prog XG");
		switch (channel[ch].bank_msb) {
		case 0:		/* Normal */
#if 0
			if (ch == 9 && channel[ch].bank_lsb == 127
					&& channel[ch].mapID == XG_DRUM_KIT_MAP)
				/* FIXME: Why this part is drum?  Is this correct? */
				break;
#endif
/* Eric's explanation for the FIXME (March 2004):
 *
 * I don't have the original email from my archived inbox, but I found a
 * reply I made in my archived sent-mail from 1999.  A September 5th message
 * to Masanao Izumo is discussing a problem with a "reapxg.mid", a file which
 * I still have, and how it issues an MSB=0 with a program change on ch 9, 
 * thus turning it into a melodic channel.  The strange thing is, this doesn't
 * happen on XG hardware, nor on the XG softsynth.  It continues to play as a
 * normal drum.  The author of the midi file obviously intended it to be
 * drumset 16 too.  The original fix was to detect LSB == -1, then break so
 * as to not set it to a melodic channel.  I'm guessing that this somehow got
 * mutated into checking for 127 instead, and the current FIXME is related to
 * the original hack from Sept 1999.  The Sept 5th email discusses patches
 * being applied to version 2.5.1 to get XG drums to work properly, and a
 * Sept 7th email to someone else discusses the fixes being part of the
 * latest 2.6.0-beta3.  A September 23rd email to Masanao Izumo specifically
 * mentions the LSB == -1 hack (and reapxg.mid not playing "correctly"
 * anymore), as well as new changes in 2.6.0 that broke a lot of other XG
 * files (XG drum support was extremely buggy in 1999 and we were still trying
 * to figure out how to initialize things to reproduce hardware behavior).  An
 * October 5th email says that 2.5.1 was correct, 2.6.0 had very broken XG
 * drum changes, and 2.6.1 still has problems.  Further discussions ensued
 * over what was "correct": to follow the XG spec, or to reproduce
 * "features" / bugs in the hardware.  I can't find the rest of the
 * discussions, but I think it ended with us agreeing to just follow the spec
 * and not try to reproduce the hardware strangeness.  I don't know how the
 * current FIXME wound up the way it is now.  I'm still going to guess it is
 * related to the old reapxg.mid hack.
 *
 * Now that reset_midi() initializes channel[ch].bank_lsb to 0 instead of -1,
 * checking for LSB == -1 won't do anything anymore, so changing the above
 * FIXME to the original == -1 won't do any good.  It is best to just #if 0
 * it out and leave it here as a reminder that there is at least one XG
 * hardware / softsynth "bug" that is not reproduced by timidity at the
 * moment.
 *
 * If the current FIXME actually reproduces some other XG hadware bug that
 * I don't know about, then it may have a valid purpose.  I just don't know
 * what that purpose is at the moment.  Perhaps someone else does?  I still
 * have src going back to 2.10.4, and the FIXME comment was already there by
 * then.  I don't see any entries in the Changelog that could explain it
 * either.  If someone has src from 2.5.1 through 2.10.3 and wants to
 * investigate this further, go for it :)
 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_NORMAL_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
///r
		case 126:	/* SFX kit & MU2000 sampling kit*/
			if(113 <= prog && prog <= 116){ // MU2000 sampling kit
				midi_drumpart_change(ch, 1);
				channel[ch].mapID = XG_SAMPLING126_MAP;
				dr = ISDRUMCHANNEL(ch);
			}else{ // SFX kit
				midi_drumpart_change(ch, 1);
				channel[ch].mapID = XG_SFX_KIT_MAP;
				dr = ISDRUMCHANNEL(ch);
			}
		case 127:	/* Drum kit */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = XG_DRUM_KIT_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 63:	/* FREE voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_FREE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 48:	/* MU100 exclusive voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_MU100EXC_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 16:	/* MU2000 sampling voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_SAMPLING16_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 32:	/* PCM-USER voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_PCM_USER_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 64:	/* PCM-SFX voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_PCM_SFX_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 80:	/* PCM-A voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_PCM_A_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 96:	/* PCM-B voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_PCM_B_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 33:	/* VA-USER voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_VA_USER_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 65:	/* VA-SFX voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_VA_SFX_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 81:	/* VA-A voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_VA_A_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 97:	/* VA-B voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_VA_B_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 34:	/* SG-USER voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_SG_USER_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 66:	/* SG-SFX voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_SG_SFX_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 82:	/* SG-A voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_SG_A_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 98:	/* SG-B voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_SG_B_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 35:	/* FM-USER voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_FM_USER_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 67:	/* FM-SFX voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_FM_SFX_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 83:	/* FM-A voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_FM_A_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 99:	/* FM-B voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_FM_B_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_lsb;
		break;
///r
	case SD_SYSTEM_MODE:	/* SD */
	case GM2_SYSTEM_MODE:	/* GM2 */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Prog GM2/SD");
		switch (channel[ch].bank_msb) {
		case 80:	/* Special 1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE80_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 81:	/* Special 2 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE81_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 87:	/* Usr Inst */ /* SD-50 Preset Inst */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE87_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 89:	/* SD-50 Solo Inst */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE89_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 96:	/* Clsc Inst */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE96_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 97:	/* Cntn Inst */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE97_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 98:	/* Solo Inst */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE98_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 99:	/* Ehnc Inst */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SDXX_TONE99_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 86:	/* Usr Drum */ /* SD-50 Preset Rhythm */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = SDXX_DRUM86_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 104:	/* Clsc Drum */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = SDXX_DRUM104_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 105:	/* Cntn Drum */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = SDXX_DRUM105_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 106:	/* Solo Drum */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = SDXX_DRUM106_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 107:	/* Ehnc Drum */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = SDXX_DRUM107_MAP;
			dr = ISDRUMCHANNEL(ch);
			sd_mfx_patch_change(ch, channel[ch].bank_msb, channel[ch].bank_lsb, prog);
			break;
		case 120:	/* GM2 Drum */
			midi_drumpart_change(ch, 1);			
			if(play_system_mode == SD_SYSTEM_MODE || is_sd_module()){
				switch(channel[ch].gm2_inst){
				case 0: // classic set
					channel[ch].mapID = SDXX_DRUM104_MAP;
					break;
				default:
				case 1: // contemp set
					channel[ch].mapID = SDXX_DRUM105_MAP;
					break;
				case 2: // solo set
					channel[ch].mapID = SDXX_DRUM106_MAP;
					break;
				case 3: // enhance set
					channel[ch].mapID = SDXX_DRUM107_MAP;
					break;
				}
			}else
				channel[ch].mapID = GM2_DRUM_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 121:	/* GM2 Inst */
			midi_drumpart_change(ch, 0);
			if(play_system_mode == SD_SYSTEM_MODE || is_sd_module()){
				switch(channel[ch].gm2_inst){
				case 0: // classic set
					channel[ch].mapID = SDXX_TONE96_MAP;
					break;
				default:
				case 1: // contemp set
					channel[ch].mapID = SDXX_TONE97_MAP;
					break;
				case 2: // solo set
					channel[ch].mapID = SDXX_TONE98_MAP;
					break;
				case 3: // enhance set
					channel[ch].mapID = SDXX_TONE99_MAP;
					break;
				}
			}else
				channel[ch].mapID = GM2_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_lsb;
		break;
	case KG_SYSTEM_MODE:	/* AG */ /* NX */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Prog KG");

		switch (channel[ch].bank_msb) {
		case 0:		/* GMa/y */
			midi_drumpart_change(ch, 0);
			if(opt_default_module == MODULE_AG10)
				channel[ch].mapID = NX5R_TONE0_MAP;
			else if(opt_default_module == MODULE_05RW)
				channel[ch].mapID = K05RW_TONE0_MAP;
			else if(opt_default_module == MODULE_NX5R)
				channel[ch].mapID = NX5R_TONE0_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 1:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE1_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 2:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE2_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 3:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE3_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 4:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE4_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 5:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE5_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 6:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE6_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 7:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE7_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 8:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE8_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 9:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE9_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 10:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE10_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 11:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE11_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 16:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE16_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 17:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE17_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 18:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE18_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 19:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE19_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 24:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE24_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 25:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE25_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 26:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE26_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 32:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE32_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 33:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE33_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 40:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE40_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 56:		/* GMb */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = K05RW_TONE56_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 57:		/* GMb */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = K05RW_TONE57_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 61:	/* rDrm */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = NX5R_DRUM61_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 62:	/* kDrum */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = K05RW_DRUM62_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 64:	/* ySFX voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE64_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 80:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE80_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 81:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE81_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 82:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE82_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 83:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE83_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 88:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE88_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 89:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE89_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 90:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE90_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 91:		/* r1 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE91_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 125:		/* CM */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = NX5R_TONE125_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 126:	/* yDrm1 */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = NX5R_DRUM126_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 127:	/* yDrm2 */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = NX5R_DRUM127_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_lsb;
		break;
	case CM_SYSTEM_MODE:	/* MT32 CMxx */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Prog CM");
		if(opt_default_module == MODULE_CM500D){ // CM500D 1-9GS 10-15LA 
			if(ch < 10)
				channel[ch].mapID = (dr) ? SC_55_DRUM_MAP : SC_55_TONE_MAP;
			else
				channel[ch].mapID = CM32L_TONE_MAP;
		}else if(ch > 15){ // other CM
			channel[ch].mapID = (dr) ? SC_88PRO_DRUM_MAP : SC_88PRO_TONE_MAP;
		}else if((ch & (16 - 1)) == 9){
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = CM32_DRUM_MAP;
	//		channel[ch].mapID = (opt_default_module == MODULE_MT32) ? MT32_DRUM_MAP : CM32_DRUM_MAP;
			dr = ISDRUMCHANNEL(ch);
		}else if((ch & (16 - 1)) < 9){
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = CM32L_TONE_MAP;
	//		channel[ch].mapID = (opt_default_module == MODULE_MT32) ? MT32_TONE_MAP : CM32L_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
		}else if(prog < 64){
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = CM32P_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
		}else switch (opt_default_module) {
		case MODULE_CM64_SN01:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN01_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
		case MODULE_CM64_SN02:
			if(prog < 71){
				midi_drumpart_change(ch, 1);
				channel[ch].mapID = SN02_DRUM_MAP;
				dr = ISDRUMCHANNEL(ch);
			}else{
				midi_drumpart_change(ch, 0);
				channel[ch].mapID = SN02_TONE_MAP;
				dr = ISDRUMCHANNEL(ch);
			}
			break;
		case MODULE_CM64_SN03:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN03_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN04:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN04_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN05:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN05_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN06:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN06_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN07:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN07_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN08:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN08_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN09:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN09_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN10:
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = SN10_DRUM_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN11:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN11_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN12:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN12_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN13:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN13_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN14:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN14_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case MODULE_CM64_SN15:
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = SN15_TONE_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_msb;
		break;
	default:
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Prog Def");
		newbank = channel[ch].bank_msb;
		break;
	}
	
	{
		int i;
		for (i = 0; i < MAX_ELEMENT; i++) {
			safe_free(channel[ch].seq_counters[i]);
			channel[ch].seq_counters[i] = NULL;
			channel[ch].seq_num_counters[i] = 0;
		}
	}

	if (dr) {
		channel[ch].bank = prog;	// newbank is ignored
		channel[ch].program = prog;
		if (drumset[prog] == NULL || drumset[prog]->alt == NULL)
			channel[ch].altassign = drumset[0]->alt;
		else
			channel[ch].altassign = drumset[prog]->alt;
		ctl_mode_event(CTLE_DRUMPART, 1, ch, 1);
	} else {
		channel[ch].bank = (special_tonebank >= 0)
				? special_tonebank : newbank;
		channel[ch].program = (default_program[ch] == SPECIAL_PROGRAM)
				? SPECIAL_PROGRAM : prog;
		channel[ch].altassign = NULL;
		ctl_mode_event(CTLE_DRUMPART, 1, ch, 0);
///r
		recompute_bank_parameter_tone(ch);
		if (opt_realtime_playing && (play_mode->flag & PF_PCM_STREAM)) {
			int elm_max = 0;
			b = channel[ch].bank, p = prog;
			instrument_map(channel[ch].mapID, &b, &p);
		//	for(elm = 0; elm < tonebank[b]->tone[p][elm].element_num + 1; elm++)
		//	for(elm = 0; tonebank[b] && elm < tonebank[b]->tone[p][elm].element_num + 1; elm++)
			for(elm = 0; elm <= elm_max; elm++)
				play_midi_load_instrument(0, b, p, elm, &elm_max); // change elm_max
		}
	}
}


static int32 conv_pitch_control(int val)
{
	if(val > 0x58) {val = 0x58;}
	else if(val < 0x28) {val = 0x28;}
	return (val - 0x40); // max +-24semitone
}

static float conv_cutoff_control(int val)
{
	return (double)(val - 0x40) * DIV_64 * 9600.0; // max +-9600cent
}

static float conv_amp_control(int val)
{
	return (double)(val - 0x40) * DIV_64; // max +-100%
}

static float conv_lfo_rate(int val)
{
//	return (int16)(0.0318f * val * val + 0.6858f * val + 0.5f);
	return (double)(val - 0x40) * DIV_64 * 10.0; // max +-10Hz
}

static float conv_lfo_amp_depth(int val)
{
	return (double)val * DIV_128; // max 100%
}

static float conv_lfo_pitch_depth(int val)
{
//	return (int16)(0.0318f * val * val + 0.6858f * val + 0.5f);
	return (double)val * DIV_127 * 600.0; // max 600cent
}

static float conv_lfo_filter_depth(int val)
{
//	return (int16)((0.0318f * val * val + 0.6858f * val) * 4.0f + 0.5f);
	return (double)val * DIV_127 * 2400.0; // max 2400cent
}

static float conv_modenv_depth(int val)
{
	return (double)(val - 0x40) * DIV_64 * 9600.0; // max +-9600cent
}

/*! process system exclusive sent from parse_sysex_event_multi(). */
static void process_sysex_event(int ev, int ch, int val, int b)
{
	int temp, msb, note;

	if (ch >= MAX_CHANNELS)
		return;
	if (ev == ME_SYSEX_MSB) {
		channel[ch].sysex_msb_addr = b;
		channel[ch].sysex_msb_val = val;
	} else if(ev == ME_SYSEX_GS_MSB) {
		channel[ch].sysex_gs_msb_addr = b;
		channel[ch].sysex_gs_msb_val = val;
	} else if(ev == ME_SYSEX_XG_MSB) {
		channel[ch].sysex_xg_msb_addr = b;
		channel[ch].sysex_xg_msb_val = val;
	} else if(ev == ME_SYSEX_SD_MSB) {
		channel[ch].sysex_sd_msb_addr = b;
		channel[ch].sysex_sd_msb_val = val;
	} else if(ev == ME_SYSEX_SD_HSB) {
		channel[ch].sysex_sd_hsb_addr = b;
		channel[ch].sysex_sd_hsb_val = val;
	} else if(ev == ME_SYSEX_LSB) {	/* Universal system exclusive message */
		msb = channel[ch].sysex_msb_addr;
		note = channel[ch].sysex_msb_val;
		channel[ch].sysex_msb_addr = channel[ch].sysex_msb_val = 0;
		switch(b)
		{
		case 0x00:	/* CAf Pitch Control */
			channel[ch].caf.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf Pitch Control (CH:%d %d semitones)", ch, channel[ch].caf.pitch);
			break;
		case 0x01:	/* CAf Filter Cutoff Control */
			channel[ch].caf.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].caf.cutoff);
			break;
		case 0x02:	/* CAf Amplitude Control */
			channel[ch].caf.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf Amplitude Control (CH:%d %.2f)", ch, channel[ch].caf.amp);
			break;
		case 0x03:	/* CAf LFO1 Rate Control */
			channel[ch].caf.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].caf.lfo1_rate);
			break;
		case 0x04:	/* CAf LFO1 Pitch Depth */
			channel[ch].caf.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].caf.lfo1_pitch_depth); 
			break;
		case 0x05:	/* CAf LFO1 Filter Depth */
			channel[ch].caf.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].caf.lfo1_tvf_depth); 
			break;
		case 0x06:	/* CAf LFO1 Amplitude Depth */
			channel[ch].caf.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].caf.lfo1_tva_depth); 
			break;
		case 0x07:	/* CAf LFO2 Rate Control */
			channel[ch].caf.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO2 Rate Control (CH:%d %.2f Hz)", ch, channel[ch].caf.lfo2_rate);
			break;
		case 0x08:	/* CAf LFO2 Pitch Depth */
			channel[ch].caf.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].caf.lfo2_pitch_depth); 
			break;
		case 0x09:	/* CAf LFO2 Filter Depth */
			channel[ch].caf.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].caf.lfo2_tvf_depth); 
			break;
		case 0x0A:	/* CAf LFO2 Amplitude Depth */
			channel[ch].caf.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAf LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].caf.lfo2_tva_depth); 
			break;
		case 0x0B:	/* PAf Pitch Control */
			channel[ch].paf.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf Pitch Control (CH:%d %d semitones)", ch, channel[ch].paf.pitch);
			break;
		case 0x0C:	/* PAf Filter Cutoff Control */
			channel[ch].paf.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].paf.cutoff);
			break;
		case 0x0D:	/* PAf Amplitude Control */
			channel[ch].paf.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf Amplitude Control (CH:%d %.2f)", ch, channel[ch].paf.amp);
			break;
		case 0x0E:	/* PAf LFO1 Rate Control */
			channel[ch].paf.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].paf.lfo1_rate);
			break;
		case 0x0F:	/* PAf LFO1 Pitch Depth */
			channel[ch].paf.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].paf.lfo1_pitch_depth); 
			break;
		case 0x10:	/* PAf LFO1 Filter Depth */
			channel[ch].paf.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].paf.lfo1_tvf_depth); 
			break;
		case 0x11:	/* PAf LFO1 Amplitude Depth */
			channel[ch].paf.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].paf.lfo1_tva_depth); 
			break;
		case 0x12:	/* PAf LFO2 Rate Control */
			channel[ch].paf.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].paf.lfo2_rate);
			break;
		case 0x13:	/* PAf LFO2 Pitch Depth */
			channel[ch].paf.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].paf.lfo2_pitch_depth); 
			break;
		case 0x14:	/* PAf LFO2 Filter Depth */
			channel[ch].paf.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].paf.lfo2_tvf_depth); 
			break;
		case 0x15:	/* PAf LFO2 Amplitude Depth */
			channel[ch].paf.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "PAf LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].paf.lfo2_tva_depth); 
			break;
		case 0x16:	/* MOD Pitch Control */
			channel[ch].mod.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD Pitch Control (CH:%d %d semitones)", ch, channel[ch].mod.pitch);
			break;
		case 0x17:	/* MOD Filter Cutoff Control */
			channel[ch].mod.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].mod.cutoff);
			break;
		case 0x18:	/* MOD Amplitude Control */
			channel[ch].mod.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD Amplitude Control (CH:%d %.2f)", ch, channel[ch].mod.amp);
			break;
		case 0x19:	/* MOD LFO1 Rate Control */
			channel[ch].mod.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].mod.lfo1_rate);
			break;
		case 0x1A:	/* MOD LFO1 Pitch Depth */
			channel[ch].mod.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].mod.lfo1_pitch_depth); 
			break;
		case 0x1B:	/* MOD LFO1 Filter Depth */
			channel[ch].mod.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].mod.lfo1_tvf_depth); 
			break;
		case 0x1C:	/* MOD LFO1 Amplitude Depth */
			channel[ch].mod.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].mod.lfo1_tva_depth); 
			break;
		case 0x1D:	/* MOD LFO2 Rate Control */
			channel[ch].mod.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].mod.lfo2_rate);
			break;
		case 0x1E:	/* MOD LFO2 Pitch Depth */
			channel[ch].mod.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].mod.lfo2_pitch_depth); 
			break;
		case 0x1F:	/* MOD LFO2 Filter Depth */
			channel[ch].mod.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].mod.lfo2_tvf_depth); 
			break;
		case 0x20:	/* MOD LFO2 Amplitude Depth */
			channel[ch].mod.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "MOD LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].mod.lfo2_tva_depth); 
			break;
		case 0x21:	/* BEND Pitch Control */
			channel[ch].bend.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND Pitch Control (CH:%d %d semitones)", ch, channel[ch].bend.pitch);
			break;
		case 0x22:	/* BEND Filter Cutoff Control */
			channel[ch].bend.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].bend.cutoff);
			break;
		case 0x23:	/* BEND Amplitude Control */
			channel[ch].bend.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND Amplitude Control (CH:%d %.2f)", ch, channel[ch].bend.amp);
			break;
		case 0x24:	/* BEND LFO1 Rate Control */
			channel[ch].bend.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].bend.lfo1_rate);
			break;
		case 0x25:	/* BEND LFO1 Pitch Depth */
			channel[ch].bend.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].bend.lfo1_pitch_depth); 
			break;
		case 0x26:	/* BEND LFO1 Filter Depth */
			channel[ch].bend.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].bend.lfo1_tvf_depth); 
			break;
		case 0x27:	/* BEND LFO1 Amplitude Depth */
			channel[ch].bend.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].bend.lfo1_tva_depth); 
			break;
		case 0x28:	/* BEND LFO2 Rate Control */
			channel[ch].bend.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].bend.lfo2_rate);
			break;
		case 0x29:	/* BEND LFO2 Pitch Depth */
			channel[ch].bend.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].bend.lfo2_pitch_depth); 
			break;
		case 0x2A:	/* BEND LFO2 Filter Depth */
			channel[ch].bend.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].bend.lfo2_tvf_depth); 
			break;
		case 0x2B:	/* BEND LFO2 Amplitude Depth */
			channel[ch].bend.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].bend.lfo2_tva_depth); 
			break;
		case 0x2C:	/* CC1 Pitch Control */
			channel[ch].cc1.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 Pitch Control (CH:%d %d semitones)", ch, channel[ch].cc1.pitch);
			break;
		case 0x2D:	/* CC1 Filter Cutoff Control */
			channel[ch].cc1.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].cc1.cutoff);
			break;
		case 0x2E:	/* CC1 Amplitude Control */
			channel[ch].cc1.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 Amplitude Control (CH:%d %.2f)", ch, channel[ch].cc1.amp);
			break;
		case 0x2F:	/* CC1 LFO1 Rate Control */
			channel[ch].cc1.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc1.lfo1_rate);
			break;
		case 0x30:	/* CC1 LFO1 Pitch Depth */
			channel[ch].cc1.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc1.lfo1_pitch_depth); 
			break;
		case 0x31:	/* CC1 LFO1 Filter Depth */
			channel[ch].cc1.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc1.lfo1_tvf_depth); 
			break;
		case 0x32:	/* CC1 LFO1 Amplitude Depth */
			channel[ch].cc1.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc1.lfo1_tva_depth); 
			break;
		case 0x33:	/* CC1 LFO2 Rate Control */
			channel[ch].cc1.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc1.lfo2_rate);
			break;
		case 0x34:	/* CC1 LFO2 Pitch Depth */
			channel[ch].cc1.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc1.lfo2_pitch_depth); 
			break;
		case 0x35:	/* CC1 LFO2 Filter Depth */
			channel[ch].cc1.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc1.lfo2_tvf_depth); 
			break;
		case 0x36:	/* CC1 LFO2 Amplitude Depth */
			channel[ch].cc1.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc1.lfo2_tva_depth); 
			break;
		case 0x37:	/* CC2 Pitch Control */
			channel[ch].cc2.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 Pitch Control (CH:%d %d semitones)", ch, channel[ch].cc2.pitch);
			break;
		case 0x38:	/* CC2 Filter Cutoff Control */
			channel[ch].cc2.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].cc2.cutoff);
			break;
		case 0x39:	/* CC2 Amplitude Control */
			channel[ch].cc2.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 Amplitude Control (CH:%d %.2f)", ch, channel[ch].cc2.amp);
			break;
		case 0x3A:	/* CC2 LFO1 Rate Control */
			channel[ch].cc2.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc2.lfo1_rate);
			break;
		case 0x3B:	/* CC2 LFO1 Pitch Depth */
			channel[ch].cc2.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc2.lfo1_pitch_depth); 
			break;
		case 0x3C:	/* CC2 LFO1 Filter Depth */
			channel[ch].cc2.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc2.lfo1_tvf_depth); 
			break;
		case 0x3D:	/* CC2 LFO1 Amplitude Depth */
			channel[ch].cc2.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc2.lfo1_tva_depth); 
			break;
		case 0x3E:	/* CC2 LFO2 Rate Control */
			channel[ch].cc2.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc2.lfo2_rate);
			break;
		case 0x3F:	/* CC2 LFO2 Pitch Depth */
			channel[ch].cc2.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc2.lfo2_pitch_depth); 
			break;
		case 0x40:	/* CC2 LFO2 Filter Depth */
			channel[ch].cc2.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc2.lfo2_tvf_depth); 
			break;
		case 0x41:	/* CC2 LFO2 Amplitude Depth */
			channel[ch].cc2.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc2.lfo2_tva_depth); 
			break;
		case 0x42:	/* Note Limit Low */
			channel[ch].note_limit_low = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Note Limit Low (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x43:	/* Note Limit High */
			channel[ch].note_limit_high = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Note Limit High (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x44:	/* Velocity Limit Low */
			channel[ch].vel_limit_low = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Velocity Limit Low (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x45:	/* Velocity Limit High */
			channel[ch].vel_limit_high = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Velocity Limit High (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x46:	/* Rx. Note Off */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			set_rx_drum(channel[ch].drums[note], RX_NOTE_OFF, val);
			channel[ch].drums[note]->rx_note_off = 1;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Rx. Note Off (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
			break;
		case 0x47:	/* Rx. Note On */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			set_rx_drum(channel[ch].drums[note], RX_NOTE_ON, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Rx. Note On (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
			break;
		case 0x48:	/* Rx. Pitch Bend */
			set_rx(ch, RX_PITCH_BEND, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Pitch Bend (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x49:	/* Rx. Channel Pressure */
			set_rx(ch, RX_CH_PRESSURE, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Channel Pressure (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4A:	/* Rx. Program Change */
			set_rx(ch, RX_PROGRAM_CHANGE, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Program Change (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4B:	/* Rx. Control Change */
			set_rx(ch, RX_CONTROL_CHANGE, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Control Change (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4C:	/* Rx. Poly Pressure */
			set_rx(ch, RX_POLY_PRESSURE, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Poly Pressure (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4D:	/* Rx. Note Message */
			set_rx(ch, RX_NOTE_MESSAGE, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Note Message (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4E:	/* Rx. RPN */
			set_rx(ch, RX_RPN, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. RPN (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4F:	/* Rx. NRPN */
			set_rx(ch, RX_NRPN, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. NRPN (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x50:	/* Rx. Modulation */
			set_rx(ch, RX_MODULATION, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Modulation (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x51:	/* Rx. Volume */
			set_rx(ch, RX_VOLUME, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Volume (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x52:	/* Rx. Panpot */
			set_rx(ch, RX_PANPOT, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Panpot (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x53:	/* Rx. Expression */
			set_rx(ch, RX_EXPRESSION, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Expression (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x54:	/* Rx. Hold1 */
			set_rx(ch, RX_HOLD1, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Hold1 (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x55:	/* Rx. Portamento */
			set_rx(ch, RX_PORTAMENTO, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Portamento (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x56:	/* Rx. Sostenuto */
			set_rx(ch, RX_SOSTENUTO, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Sostenuto (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x57:	/* Rx. Soft */
			set_rx(ch, RX_SOFT, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Soft (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x58:	/* Rx. Bank Select */
			set_rx(ch, RX_BANK_SELECT, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Bank Select (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x59:	/* Rx. Bank Select LSB */
			set_rx(ch, RX_BANK_SELECT_LSB, val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Rx. Bank Select LSB (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x60:	/* Reverb Type (GM2) */
			if (val > 8) {val = 8;}
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Type (%d)", val);
			set_reverb_macro_gm2(val);
			recompute_reverb_status_gs();
			init_ch_reverb();
			break;
		case 0x61:	/* Chorus Type (GM2) */
			if (val > 5) {val = 5;}
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Type (%d)", val);
			set_chorus_macro_gm2(val);
			recompute_chorus_status_gs();
			init_ch_chorus();
			break;
///r
		case 0x62: /* Pitch EG Initial Level*/
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Initial Level (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_level[0] = val;			
			break;
		case 0x63:	/* Pitch EG Attack Level */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Attack Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_level[1] = val;	
			break;
		case 0x64:	/* Pitch EG Attack Time */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Attack Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_time[0] = val;			
		    break;
		case 0x65:	/* Pitch EG Decay Level */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Decay Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_level[2] = val;	
			break;
		case 0x66:	/* Pitch EG Decay Time */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Decay Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_time[1] = val;			
		    break;
		case 0x67:	/* Pitch EG Sustain Level */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Sustain Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_level[3] = val;	
			break;
		case 0x68:	/* Pitch EG Sustain Time */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Sustain Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_time[2] = val;			
		    break;
		case 0x69:	/* Pitch EG Release Level */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Release Level (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_level[4] = val;			
		    break;
		case 0x6A:	/* Pitch EG Release Time */
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Pitch EG Release Time (CH:%d VAL:%d)", ch, val);
			channel[ch].pit_env_time[3] = val;			
		    break;	
		case 0x6B:	/* Filter EG Depth */
			channel[ch].modenv_depth = conv_modenv_depth(val);			
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "Filter EG Depth (CH:%d VAL:%d)", ch, val - 0x40);
		    break;				
		case 0x6C:	/* GM2 Instrument Select */
			if(ch == 0xFF){
				int i;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "GM2 Instrument Select (CH:all VAL:%d)", ch, val);
				for(i = 0; i < MAX_CHANNELS; i++)
					channel[i].gm2_inst = val;
			}else{
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "GM2 Instrument Select (CH:%d VAL:%d)", ch, val);
				channel[ch].gm2_inst = val;
			}
			break;
		case 0x6D:	/* CC1 number */		
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC1 number (CH:%d NUM:%d)", ch, val);
			channel[ch].cc1.num = val;	
		    break;		
		case 0x6E:	/* CC2 number */		
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC2 number (CH:%d NUM:%d)", ch, val);
			channel[ch].cc2.num = val;	
		    break;		
		case 0x6F:	/* CC3 number */		
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 number (CH:%d NUM:%d)", ch, val);
			channel[ch].cc3.num = val;	
		    break;		
		case 0x70:	/* CC4 number */		
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 number (CH:%d NUM:%d)", ch, val);
			channel[ch].cc4.num = val;	
		    break;
			
		case 0x80:	/* CC3 Pitch Control */
			channel[ch].cc3.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 Pitch Control (CH:%d %d semitones)", ch, channel[ch].cc3.pitch);
			break;
		case 0x81:	/* CC3 Filter Cutoff Control */
			channel[ch].cc3.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].cc3.cutoff);
			break;
		case 0x82:	/* CC3 Amplitude Control */
			channel[ch].cc3.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 Amplitude Control (CH:%d %.2f)", ch, channel[ch].cc3.amp);
			break;
		case 0x83:	/* CC3 LFO1 Rate Control */
			channel[ch].cc3.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc3.lfo1_rate);
			break;
		case 0x84:	/* CC3 LFO1 Pitch Depth */
			channel[ch].cc3.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc3.lfo1_pitch_depth); 
			break;
		case 0x85:	/* CC3 LFO1 Filter Depth */
			channel[ch].cc3.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc3.lfo1_tvf_depth); 
			break;
		case 0x86:	/* CC3 LFO1 Amplitude Depth */
			channel[ch].cc3.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc3.lfo1_tva_depth); 
			break;
		case 0x87:	/* CC3 LFO2 Rate Control */
			channel[ch].cc3.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc3.lfo2_rate);
			break;
		case 0x88:	/* CC3 LFO2 Pitch Depth */
			channel[ch].cc3.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc3.lfo2_pitch_depth); 
			break;
		case 0x89:	/* CC3 LFO2 Filter Depth */
			channel[ch].cc3.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc3.lfo2_tvf_depth); 
			break;
		case 0x8A:	/* CC3 LFO2 Amplitude Depth */
			channel[ch].cc3.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC3 LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc3.lfo2_tva_depth); 
			break;

		case 0x90:	/* CC4 Pitch Control */
			channel[ch].cc4.pitch = conv_pitch_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 Pitch Control (CH:%d %d semitones)", ch, channel[ch].cc4.pitch);
			break;
		case 0x91:	/* CC4 Filter Cutoff Control */
			channel[ch].cc4.cutoff = conv_cutoff_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].cc4.cutoff);
			break;
		case 0x92:	/* CC4 Amplitude Control */
			channel[ch].cc4.amp = conv_amp_control(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 Amplitude Control (CH:%d %.2f)", ch, channel[ch].cc4.amp);
			break;
		case 0x93:	/* CC4 LFO1 Rate Control */
			channel[ch].cc4.lfo1_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc4.lfo1_rate);
			break;
		case 0x94:	/* CC4 LFO1 Pitch Depth */
			channel[ch].cc4.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO1 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc4.lfo1_pitch_depth); 
			break;
		case 0x95:	/* CC4 LFO1 Filter Depth */
			channel[ch].cc4.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO1 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc4.lfo1_tvf_depth); 
			break;
		case 0x96:	/* CC4 LFO1 Amplitude Depth */
			channel[ch].cc4.lfo1_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc4.lfo1_tva_depth); 
			break;
		case 0x97:	/* CC4 LFO2 Rate Control */
			channel[ch].cc4.lfo2_rate = conv_lfo_rate(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc4.lfo2_rate);
			break;
		case 0x98:	/* CC4 LFO2 Pitch Depth */
			channel[ch].cc4.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO2 Pitch Depth (CH:%d %.2f cents)", ch, channel[ch].cc4.lfo2_pitch_depth); 
			break;
		case 0x99:	/* CC4 LFO2 Filter Depth */
			channel[ch].cc4.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO2 Filter Depth (CH:%d %.2f cents)", ch, channel[ch].cc4.lfo2_tvf_depth); 
			break;
		case 0x9A:	/* CC4 LFO2 Amplitude Depth */
			channel[ch].cc4.lfo2_tva_depth = conv_lfo_amp_depth(val);
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "CC4 LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc4.lfo2_tva_depth); 
			break;
		default:
			break;
		}
		return;
	} else if(ev == ME_SYSEX_GS_LSB) {	/* GS system exclusive message */
		msb = channel[ch].sysex_gs_msb_addr;
		note = channel[ch].sysex_gs_msb_val;
		channel[ch].sysex_gs_msb_addr = channel[ch].sysex_gs_msb_val = 0;
		switch(b)
		{
		case 0x00:	/* EQ ON/OFF */
			if(!opt_eq_control) {break;}
			if(channel[ch].eq_gs != val) {
				if(val) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS EQ ON (CH:%d)",ch);
				} else {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS EQ OFF (CH:%d)",ch);
				}
			}
			channel[ch].eq_gs = val;
			break;
		case 0x01:	/* EQ LOW FREQ */
			if(!opt_eq_control) {break;}
			eq_status_gs.low_freq = val;
			recompute_eq_status_gs();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS EQ LOW FREQ (%d)",val);
			break;
		case 0x02:	/* EQ LOW GAIN */
			if(!opt_eq_control) {break;}
			eq_status_gs.low_gain = val;
			recompute_eq_status_gs();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS EQ LOW GAIN (%d dB)",val - 0x40);
			break;
		case 0x03:	/* EQ HIGH FREQ */
			if(!opt_eq_control) {break;}
			eq_status_gs.high_freq = val;
			recompute_eq_status_gs();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS EQ HIGH FREQ (%d)",val);
			break;
		case 0x04:	/* EQ HIGH GAIN */
			if(!opt_eq_control) {break;}
			eq_status_gs.high_gain = val;
			recompute_eq_status_gs();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS EQ HIGH GAIN (%d dB)",val - 0x40);
			break;
		case 0x05:	/* Reverb Macro */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Macro (%d)",val);
			set_reverb_macro_gs(val);
			recompute_reverb_status_gs();
			init_ch_reverb();
			break;
		case 0x06:	/* Reverb Character */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Character (%d)",val);
			if (reverb_status_gs.character != val) {
				reverb_status_gs.character = val;
				recompute_reverb_status_gs();
				init_ch_reverb();
			}
			break;
		case 0x07:	/* Reverb Pre-LPF */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Pre-LPF (%d)",val);
			if(reverb_status_gs.pre_lpf != val) {
				reverb_status_gs.pre_lpf = val;
				recompute_reverb_status_gs();
			}
			break;
		case 0x08:	/* Reverb Level */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Level (%d)",val);
			if(reverb_status_gs.level != val) {
				reverb_status_gs.level = val;
				recompute_reverb_status_gs();
				init_ch_reverb();
			}
			break;
		case 0x09:	/* Reverb Time */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Time (%d)",val);
			if(reverb_status_gs.time != val) {
				reverb_status_gs.time = val;
				recompute_reverb_status_gs();
				init_ch_reverb();
			}
			break;
		case 0x0A:	/* Reverb Delay Feedback */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Delay Feedback (%d)",val);
			if(reverb_status_gs.delay_feedback != val) {
				reverb_status_gs.delay_feedback = val;
				recompute_reverb_status_gs();
				init_ch_reverb();
			}
			break;
		case 0x0C:	/* Reverb Predelay Time */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Predelay Time (%d)",val);
			if(reverb_status_gs.pre_delay_time != val) {
				reverb_status_gs.pre_delay_time = val;
				recompute_reverb_status_gs();
				init_ch_reverb();
			}
			break;
		case 0x0D:	/* Chorus Macro */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Macro (%d)",val);
			set_chorus_macro_gs(val);
			recompute_chorus_status_gs();
			init_ch_chorus();
			break;
		case 0x0E:	/* Chorus Pre-LPF */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Pre-LPF (%d)",val);
			if (chorus_status_gs.pre_lpf != val) {
				chorus_status_gs.pre_lpf = val;
				recompute_chorus_status_gs();
			}
			break;
		case 0x0F:	/* Chorus Level */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Level (%d)",val);
			if (chorus_status_gs.level != val) {
				chorus_status_gs.level = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x10:	/* Chorus Feedback */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Feedback (%d)",val);
			if (chorus_status_gs.feedback != val) {
				chorus_status_gs.feedback = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x11:	/* Chorus Delay */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Delay (%d)",val);
			if (chorus_status_gs.delay != val) {
				chorus_status_gs.delay = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x12:	/* Chorus Rate */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Rate (%d)",val);
			if (chorus_status_gs.rate != val) {
				chorus_status_gs.rate = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x13:	/* Chorus Depth */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Depth (%d)",val);
			if (chorus_status_gs.depth != val) {
				chorus_status_gs.depth = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x14:	/* Chorus Send Level to Reverb */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send Level to Reverb (%d)",val);
			if (chorus_status_gs.send_reverb != val) {
				chorus_status_gs.send_reverb = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x15:	/* Chorus Send Level to Delay */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send Level to Delay (%d)",val);
			if (chorus_status_gs.send_delay != val) {
				chorus_status_gs.send_delay = val;
				recompute_chorus_status_gs();
				init_ch_chorus();
			}
			break;
		case 0x16:	/* Delay Macro */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Macro (%d)",val);
			set_delay_macro_gs(val);
			recompute_delay_status_gs();
			init_ch_delay();
			break;
		case 0x17:	/* Delay Pre-LPF */
			if (val > 7) {val = 7;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Pre-LPF (%d)",val);
			val &= 0x7;
			if (delay_status_gs.pre_lpf != val) {
				delay_status_gs.pre_lpf = val;
				recompute_delay_status_gs();
			}
			break;
		case 0x18:	/* Delay Time Center */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Center (%d)",val);
			if (delay_status_gs.time_c != val) {
				delay_status_gs.time_c = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x19:	/* Delay Time Ratio Left */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Ratio Left (%d)",val);
			if (val == 0) {val = 1;}
			if (delay_status_gs.time_l != val) {
				delay_status_gs.time_l = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x1A:	/* Delay Time Ratio Right */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Ratio Right (%d)",val);
			if (val == 0) {val = 1;}
			if (delay_status_gs.time_r != val) {
				delay_status_gs.time_r = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x1B:	/* Delay Level Center */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Center (%d)",val);
			if (delay_status_gs.level_center != val) {
				delay_status_gs.level_center = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x1C:	/* Delay Level Left */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Left (%d)",val);
			if (delay_status_gs.level_left != val) {
				delay_status_gs.level_left = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x1D:	/* Delay Level Right */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Right (%d)",val);
			if (delay_status_gs.level_right != val) {
				delay_status_gs.level_right = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x1E:	/* Delay Level */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level (%d)",val);
			if (delay_status_gs.level != val) {
				delay_status_gs.level = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x1F:	/* Delay Feedback */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Feedback (%d)",val);
			if (delay_status_gs.feedback != val) {
				delay_status_gs.feedback = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x20:	/* Delay Send Level to Reverb */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Send Level to Reverb (%d)",val);
			if (delay_status_gs.send_reverb != val) {
				delay_status_gs.send_reverb = val;
				recompute_delay_status_gs();
				init_ch_delay();
			}
			break;
		case 0x21:	/* Velocity Sense Depth */
			channel[ch].velocity_sense_depth = val;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Depth (CH:%d VAL:%d)",ch,val);
			break;
		case 0x22:	/* Velocity Sense Offset */
			channel[ch].velocity_sense_offset = val;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Offset (CH:%d VAL:%d)",ch,val);
			break;
		case 0x23:	/* Insertion Effect ON/OFF */
			if(!opt_insertion_effect) {break;}
			if(channel[ch].insertion_effect != val) {
				if(val) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS Insertion Effect ON (CH:%d)",ch);}
				else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS Insertion Effect OFF (CH:%d)",ch);}
			}
			channel[ch].insertion_effect = val;
			ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, channel[ch].insertion_effect);
			break;
		case 0x24:	/* Assign Mode */
			channel[ch].assign_mode = val;
			if(val == 0) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Single (CH:%d)",ch);
			} else if(val == 1) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Limited-Multi (CH:%d)",ch);
			} else if(val == 2) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Full-Multi (CH:%d)",ch);
			}
			break;
		case 0x25:	/* TONE MAP-0 NUMBER */
			channel[ch].tone_map0_number = val;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Tone Map-0 Number (CH:%d VAL:%d)",ch,val);
			break;
		case 0x26:	/* Pitch Offset Fine */
			{
				if(val != 0x80)
					channel[ch].pitch_offset_fine = (FLOAT_T)((int)val - 0x80) * DIV_10;
				else
					channel[ch].pitch_offset_fine = 0.0;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Pitch Offset Fine (CH:%d %3fHz)",ch,channel[ch].pitch_offset_fine);
			}
			break;
		case 0x27:	/* Insertion Effect Type MSB */
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.type_msb = val;
			//temp = insertion_effect_gs.type;
			//if(temp == insertion_effect_gs.type) {
			//	recompute_insertion_effect_gs(1);
			//} else {
			//	realloc_insertion_effect_gs();
			//}
			break;
		case 0x28:	/* Insertion Effect Type LSB */
			if(!opt_insertion_effect) {break;}
			temp = insertion_effect_gs.type;
			insertion_effect_gs.type_lsb = val;
			insertion_effect_gs.type = ((int32)insertion_effect_gs.type_msb << 8) | (int32)insertion_effect_gs.type_lsb;
			if(temp == insertion_effect_gs.type) {
				recompute_insertion_effect_gs(1);
			} else {
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GS Insertion Effect TYPE (%02X %02X)", 
					insertion_effect_gs.type_msb, insertion_effect_gs.type_lsb);
				realloc_insertion_effect_gs();
			}
			break;
		case 0x29: /* Insertion Effect Parameter */
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
		case 0x2E:
		case 0x2F:
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
		case 0x3C:
			if(!opt_insertion_effect) {break;}
			temp = b - 0x29;
			if(val >= 0 && val <= 127){
				insertion_effect_gs.set_param[temp] = val;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Parameter %d (VAL:%d)", temp + 1, val);
				recompute_insertion_effect_gs(1);
			}else{
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "ERROR! GS Insertion Parameter %d (VAL:%d)", temp + 1, val);
			}
			break;
		case 0x3D:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.send_reverb = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Send Reverb Level (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x3E:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.send_chorus = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Send Chorus Level (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x3F:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.send_delay = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Send Delay Level (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x40:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.control_source1 = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Control Source 1 (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x41:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.control_depth1 = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Control Depth 1 (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x42:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.control_source2 = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Control Source 2 (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x43:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.control_depth2 = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion Control Depth 2 (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x44:
			if(!opt_insertion_effect) {break;}
			insertion_effect_gs.send_eq_switch = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY, "GS Insertion EQ Switch (VAL:%d)",val);
			recompute_insertion_effect_gs(1);
			break;
		case 0x45:	/* Rx. Channel */
			reset_controllers(ch);
			redraw_controllers(ch);
			all_notes_off(ch);
			if (val == 0x80)
				remove_channel_layer(ch);
			else
				add_channel_layer(ch, val);
			break;
		case 0x46:	/* Channel Msg Rx Port */
			reset_controllers(ch);
			redraw_controllers(ch);
			all_notes_off(ch);
			channel[ch].port_select = val;
			break;
		case 0x47:	/* Play Note Number */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			channel[ch].drums[note]->play_note = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Play Note (CH:%d NOTE:%d VAL:%d)",
				ch, note, channel[ch].drums[note]->play_note);
			break;
		default:
			break;
		}
		return;
	} else if(ev == ME_SYSEX_XG_LSB) {	/* XG system exclusive message */
		msb = channel[ch].sysex_xg_msb_addr;
		note = channel[ch].sysex_xg_msb_val;
///r
		if (note == 2 && msb >= 0 && msb < 0x40) {	/* Effect 1 */
			note = 0;	/* not use */
			if(b >= 0x40){ /* variation effect */
				msb -= 1; 	/* variation effect num start 1 , dim 0*/
				/* msb=0 variation 1 */ 
				/* msb=n variation n+1 */ 
				if(msb < 0 || msb >= XG_VARIATION_EFFECT_NUM){
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "ERROR! XG Variation Effect Number (%d)", msb);
					return;
				}
			}
			switch(b)
			{
			case 0x00:	/* Reverb Type MSB */
				reverb_status_xg.type_msb = val;
				//if (reverb_status_xg.type_msb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Type MSB (%02X)", val);
				//	reverb_status_xg.type_msb = val;
				//	realloc_effect_xg(&reverb_status_xg);
				//}
				break;
			case 0x01:	/* Reverb Type LSB */
				temp = reverb_status_xg.type;
				reverb_status_xg.type_lsb = val;	
				reverb_status_xg.type = ((int32)reverb_status_xg.type_msb << 8) | (int32)reverb_status_xg.type_lsb;				
				if (temp == reverb_status_xg.type){
					recompute_effect_xg(&reverb_status_xg, 1);
				}else{
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Type (%02X %02X)", 
						reverb_status_xg.type_msb, reverb_status_xg.type_lsb);
					realloc_effect_xg(&reverb_status_xg);
				}
				//if (reverb_status_xg.type_msb = val || reverb_status_xg.type_lsb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Type LSB (%02X)", val);
				//	reverb_status_xg.type_lsb = val;
				//	realloc_effect_xg(&reverb_status_xg);
				//}
				break;
			case 0x02:	/* Reverb Parameter 1 - 10 */
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Parameter %d (%d)", b - 0x02 + 1, val);
				if (reverb_status_xg.param_lsb[b - 0x02] != val) {
					reverb_status_xg.param_lsb[b - 0x02] = val;
					recompute_effect_xg(&reverb_status_xg, 1);
				}
				break;
			case 0x0C:	/* Reverb Return */
	#if 1	/* XG specific reverb is not currently implemented */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Return (%d)", val);
				if (reverb_status_xg.ret != val) {
					reverb_status_xg.ret = val;
					recompute_effect_xg(&reverb_status_xg, 1);
				}
	#else	/* use GS reverb instead */
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Return (%d)", val);
				if (reverb_status_gs.level != val) {
					reverb_status_gs.level = val;
					recompute_reverb_status_gs();
					init_ch_reverb();
				}
	#endif
				break;
			case 0x0D:	/* Reverb Pan */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Pan (%d)", val);
				if (reverb_status_xg.pan != val) {
					reverb_status_xg.pan = val;
					recompute_effect_xg(&reverb_status_xg, 1);
				}
				break;
			case 0x10:	/* Reverb Parameter 11 - 16 */
			case 0x11:
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
				temp = b - 0x10 + 10;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Reverb Parameter %d (%d)", temp + 1, val);
				if (reverb_status_xg.param_lsb[temp] != val) {
					reverb_status_xg.param_lsb[temp] = val;
					recompute_effect_xg(&reverb_status_xg, 1);
				}
				break;
			case 0x20:	/* Chorus Type MSB */
				chorus_status_xg.type_msb = val;
				//if (chorus_status_xg.type_msb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Type MSB (%02X)", val);
				//	chorus_status_xg.type_msb = val;
				//	realloc_effect_xg(&chorus_status_xg);
				//}
				break;
			case 0x21:	/* Chorus Type LSB */
				temp = chorus_status_xg.type;
				chorus_status_xg.type_lsb = val;
				chorus_status_xg.type = ((int32)chorus_status_xg.type_msb << 8) | (int32)chorus_status_xg.type_lsb;				
				if (temp == chorus_status_xg.type){
					recompute_effect_xg(&chorus_status_xg, 1);
				}else{
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Type (%02X %02X)", 
						chorus_status_xg.type_msb, chorus_status_xg.type_lsb);
					realloc_effect_xg(&chorus_status_xg);
				}
				//if (chorus_status_xg.type_lsb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Type LSB (%02X)", val);
				//	chorus_status_xg.type_lsb = val;
				//	realloc_effect_xg(&chorus_status_xg);
				//}
				break;
			case 0x22:	/* Chorus Parameter 1 - 10 */
			case 0x23:
			case 0x24:
			case 0x25:
			case 0x26:
			case 0x27:
			case 0x28:
			case 0x29:
			case 0x2A:
			case 0x2B:
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Parameter %d (%d)", b - 0x22 + 1, val);
				if (chorus_status_xg.param_lsb[b - 0x22] != val) {
					chorus_status_xg.param_lsb[b - 0x22] = val;
					recompute_effect_xg(&chorus_status_xg, 1);
				}
				break;
			case 0x2C:	/* Chorus Return */
	#if 1	/* XG specific chorus is not currently implemented */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Return (%d)", val);
				if (chorus_status_xg.ret != val) {
					chorus_status_xg.ret = val;
					recompute_effect_xg(&chorus_status_xg, 1);
				}
	#else	/* use GS chorus instead */
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Return (%d)", val);
				if (chorus_status_gs.level != val) {
					chorus_status_gs.level = val;
					recompute_chorus_status_gs();
					init_ch_chorus();
				}
	#endif
				break;
			case 0x2D:	/* Chorus Pan */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Pan (%d)", val);
				if (chorus_status_xg.pan != val) {
					chorus_status_xg.pan = val;
					recompute_effect_xg(&chorus_status_xg, 1);
				}
				break;
			case 0x2E:	/* Send Chorus To Reverb */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Send Chorus To Reverb (%d)", val);
				if (chorus_status_xg.send_reverb != val) {
					chorus_status_xg.send_reverb = val;
					recompute_effect_xg(&chorus_status_xg, 1);
				}
				break;
			case 0x30:	/* Chorus Parameter 11 - 16 */
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x35:
				temp = b - 0x30 + 10;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Chorus Parameter %d (%d)", temp + 1, val);
				if (chorus_status_xg.param_lsb[temp] != val) {
					chorus_status_xg.param_lsb[temp] = val;
					recompute_effect_xg(&chorus_status_xg, 1);
				}
				break;
			case 0x40:	/* Variation Type MSB */
				variation_effect_xg[msb].type_msb = val;
				//if (variation_effect_xg[msb].type_msb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Type MSB (%02X)", msb + 1, val);
				//	variation_effect_xg[msb].type_msb = val;
				//	realloc_effect_xg(&variation_effect_xg[msb]);
				//}
				break;
			case 0x41:	/* Variation Type LSB */
				temp = variation_effect_xg[msb].type;
				variation_effect_xg[msb].type_lsb = val;
				variation_effect_xg[msb].type = ((int32)variation_effect_xg[msb].type_msb << 8) | (int32)variation_effect_xg[msb].type_lsb;
				if(temp == variation_effect_xg[msb].type){
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}else{
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Type (%02X %02X)", msb + 1, 
						variation_effect_xg[msb].type_msb, variation_effect_xg[msb].type_lsb);
					realloc_effect_xg(&variation_effect_xg[msb]);
				}
				//if (variation_effect_xg[msb].type_lsb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Type LSB (%02X)", msb + 1, val);
				//	variation_effect_xg[msb].type_lsb = val;
				//	realloc_effect_xg(&variation_effect_xg[msb]);
				//}
				break;
			case 0x42:	/* Variation Parameter 1 - 10 MSB */
			case 0x44:
			case 0x46:
			case 0x48:
			case 0x4A:
			case 0x4C:
			case 0x4E:
			case 0x50:
			case 0x52:
			case 0x54:
				temp = (b - 0x42) / 2;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Parameter MSB %d (%d)", msb + 1, temp, val);
				if (variation_effect_xg[msb].set_param_msb[temp] != val) {
					variation_effect_xg[msb].set_param_msb[temp] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x43:	/* Variation Parameter 1 - 10 LSB */
			case 0x45:
			case 0x47:
			case 0x49:
			case 0x4B:
			case 0x4D:
			case 0x4F:
			case 0x51:
			case 0x53:
			case 0x55:
				temp = (b - 0x43) / 2;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Parameter LSB %d (%d)", msb + 1, temp, val);
				if (variation_effect_xg[msb].set_param_lsb[temp] != val) {
					variation_effect_xg[msb].set_param_lsb[temp] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x56:	/* Variation Return */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Return (%d)", msb + 1, val);
				if (variation_effect_xg[msb].ret != val) {
					variation_effect_xg[msb].ret = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x57:	/* Variation Pan */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Pan (%d)", msb + 1, val);
				if (variation_effect_xg[msb].pan != val) {
					variation_effect_xg[msb].pan = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x58:	/* Send Variation To Reverb */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Send Reverb (%d)", msb + 1, val);
				if (variation_effect_xg[msb].send_reverb != val) {
					variation_effect_xg[msb].send_reverb = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x59:	/* Send Variation To Chorus */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Send Chorus (%d)", msb + 1, val);
				if (variation_effect_xg[msb].send_chorus != val) {
					variation_effect_xg[msb].send_chorus = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x5A:	/* Variation Connection */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Connection (%d)", msb + 1, val);
				if (variation_effect_xg[msb].connection != val) {
					variation_effect_xg[msb].connection = val; // 0: insertion 1:system
					recompute_effect_xg(&variation_effect_xg[msb], 1);	
					{
						int i;
						for (i = 0; i < MAX_CHANNELS; i++)
							ctl_mode_event(CTLE_INSERTION_EFFECT, 1, i, is_insertion_effect_xg(i));
					}
				}
				break;
			case 0x5B:	/* Variation Part */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Part (%d)", msb + 1, val);
				if (variation_effect_xg[msb].part != val) {
					variation_effect_xg[msb].part = val; // 0~63:ON 64~126:disable(AD part) 127:OFF
					recompute_effect_xg(&variation_effect_xg[msb], 1);
					{
						int i;
						for (i = 0; i < MAX_CHANNELS; i++)
							ctl_mode_event(CTLE_INSERTION_EFFECT, 1, i, is_insertion_effect_xg(i));
					}
				}
				break;
			case 0x5C:	/* MW Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "MW XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[0] != val) {
					variation_effect_xg[msb].control_depth[0] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x5D:	/* BEND Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[1] != val) {
					variation_effect_xg[msb].control_depth[1] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x5E:	/* CAT Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAT XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[2] != val) {
					variation_effect_xg[msb].control_depth[2] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x5F:	/* AC1 Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "AC1 XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[3] != val) {
					variation_effect_xg[msb].control_depth[3] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x60:	/* AC2 Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "AC2 XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[4] != val) {
					variation_effect_xg[msb].control_depth[4] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x61:	/* CBC1 Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "CBC1 XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[5] != val) {
					variation_effect_xg[msb].control_depth[5] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x62:	/* CBC2 Variation Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "CBC2 XG Variation %d Control Depth (%d)", msb + 1, val);
				if (variation_effect_xg[msb].control_depth[6] != val) {
					variation_effect_xg[msb].control_depth[6] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			case 0x70:	/* Variation Parameter 11 - 16 */
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
				temp = b - 0x70 + 10;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Variation %d Parameter %d (%d)", msb + 1, temp + 1, val);
				if (variation_effect_xg[msb].set_param_lsb[temp] != val) {
					variation_effect_xg[msb].set_param_lsb[temp] = val;
					recompute_effect_xg(&variation_effect_xg[msb], 1);
				}
				break;
			default:
				break;
			}

		} else if (note == 2 && msb == 0x40) {	/* Multi EQ */
			switch(b)
			{
			case 0x00:	/* EQ type */
				if(opt_eq_control) {
					if(val == 0) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ type (0: Flat)");}
					else if(val == 1) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ type (1: Jazz)");}
					else if(val == 2) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ type (2: Pops)");}
					else if(val == 3) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ type (3: Rock)");}
					else if(val == 4) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ type (4: Concert)");}
					multi_eq_xg.type = val;
					set_multi_eq_type_xg(val);
					recompute_multi_eq_xg();
				}
				break;
			case 0x01:	/* EQ gain1 */
				if(opt_eq_control) {
					if(val > 0x4C) {val = 0x4C;}
					else if(val < 0x34) {val = 0x34;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ gain1 (%d dB)", val - 0x40);
					multi_eq_xg.gain1 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x02:	/* EQ frequency1 */
				if(opt_eq_control) {
					if(val > 60) {val = 60;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ frequency1 (%d Hz)", (int32)eq_freq_table_xg[val]);
					multi_eq_xg.freq1 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x03:	/* EQ Q1 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ Q1 (%f)", (double)val / 10.0);
					multi_eq_xg.q1 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x04:	/* EQ shape1 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ shape1 (%d)", val);
					multi_eq_xg.shape1 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x05:	/* EQ gain2 */
				if(opt_eq_control) {
					if(val > 0x4C) {val = 0x4C;}
					else if(val < 0x34) {val = 0x34;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ gain2 (%d dB)", val - 0x40);
					multi_eq_xg.gain2 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x06:	/* EQ frequency2 */
				if(opt_eq_control) {
					if(val > 60) {val = 60;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ frequency2 (%d Hz)", (int32)eq_freq_table_xg[val]);
					multi_eq_xg.freq2 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x07:	/* EQ Q2 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ Q2 (%f)", (double)val / 10.0);
					multi_eq_xg.q2 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x09:	/* EQ gain3 */
				if(opt_eq_control) {
					if(val > 0x4C) {val = 0x4C;}
					else if(val < 0x34) {val = 0x34;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ gain3 (%d dB)", val - 0x40);
					multi_eq_xg.gain3 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x0A:	/* EQ frequency3 */
				if(opt_eq_control) {
					if(val > 60) {val = 60;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ frequency3 (%d Hz)", (int32)eq_freq_table_xg[val]);
					multi_eq_xg.freq3 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x0B:	/* EQ Q3 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ Q3 (%f)", (double)val / 10.0);
					multi_eq_xg.q3 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x0D:	/* EQ gain4 */
				if(opt_eq_control) {
					if(val > 0x4C) {val = 0x4C;}
					else if(val < 0x34) {val = 0x34;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ gain4 (%d dB)", val - 0x40);
					multi_eq_xg.gain4 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x0E:	/* EQ frequency4 */
				if(opt_eq_control) {
					if(val > 60) {val = 60;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ frequency4 (%d Hz)", (int32)eq_freq_table_xg[val]);
					multi_eq_xg.freq4 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x0F:	/* EQ Q4 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ Q4 (%f)", (double)val / 10.0);
					multi_eq_xg.q4 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x11:	/* EQ gain5 */
				if(opt_eq_control) {
					if(val > 0x4C) {val = 0x4C;}
					else if(val < 0x34) {val = 0x34;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ gain5 (%d dB)", val - 0x40);
					multi_eq_xg.gain5 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x12:	/* EQ frequency5 */
				if(opt_eq_control) {
					if(val > 60) {val = 60;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ frequency5 (%d Hz)", (int32)eq_freq_table_xg[val]);
					multi_eq_xg.freq5 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x13:	/* EQ Q5 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ Q5 (%f)", (double)val / 10.0);
					multi_eq_xg.q5 = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x14:	/* EQ shape5 */
				if(opt_eq_control) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG EQ shape5 (%d)", val);
					multi_eq_xg.shape5 = val;
					recompute_multi_eq_xg();
				}
				break;
			}

		} else if (note == 3 && msb >= 0) {	/* Effect 2 */
			/* msb=0 : insertion 1 */
			/* msb=n : insertion n+1 */
			if ( msb >= XG_INSERTION_EFFECT_NUM) {
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "ERROR! XG Insertion Effect Number (%d)", msb);
				return;
			}
			switch(b)
			{
			case 0x00:	/* Insertion Effect Type MSB */
				if(!opt_insertion_effect) {break;}
				insertion_effect_xg[msb].type_msb = val;				
				//if (insertion_effect_xg[msb].type_msb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Type MSB (%d %02X)", msb + 1, note, val);
				//	insertion_effect_xg[msb].type_msb = val;
				//	realloc_effect_xg(&insertion_effect_xg[msb]);
				//}
				break;
			case 0x01:	/* Insertion Effect Type LSB */
				if(!opt_insertion_effect) {break;}
				temp = insertion_effect_xg[msb].type;
				insertion_effect_xg[msb].type_lsb = val;				
				insertion_effect_xg[msb].type = ((int32)insertion_effect_xg[msb].type_msb << 8) | (int32)insertion_effect_xg[msb].type_lsb;				
				if(temp == insertion_effect_xg[msb].type){
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}else{
					ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Type (%d %02X %02X)", msb + 1, note, 
						insertion_effect_xg[msb].type_msb, insertion_effect_xg[msb].type_lsb);
					realloc_effect_xg(&insertion_effect_xg[msb]);
				}
				//if (insertion_effect_xg[msb].type_lsb != val) {
				//	ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Type LSB (%d %02X)", msb + 1, note, val);
				//	insertion_effect_xg[msb].type_lsb = val;
				//	realloc_effect_xg(&insertion_effect_xg[msb]);
				//}
				break;
			case 0x02:	/* Insertion Effect Parameter 1 - 10 */
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
			case 0x08:
			case 0x09:
			case 0x0A:
			case 0x0B:
				if (insertion_effect_xg[msb].use_msb) {break;}
				temp = b - 0x02;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Parameter %d (%d %d)", msb + 1, temp + 1, note, val);
				if (insertion_effect_xg[msb].set_param_lsb[temp] != val) {
					insertion_effect_xg[msb].set_param_lsb[temp] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x0C:	/* Insertion Effect Part */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Part (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].part != val) {
					insertion_effect_xg[msb].part = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
					{
						int i;
						for (i = 0; i < MAX_CHANNELS; i++)
							ctl_mode_event(CTLE_INSERTION_EFFECT, 1, i, is_insertion_effect_xg(i));
					}
				}
				break;
			case 0x0D:	/* MW Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "MW XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[0] != val) {
					insertion_effect_xg[msb].control_depth[0] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x0E:	/* BEND Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "BEND XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[1] != val) {
					insertion_effect_xg[msb].control_depth[1] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x0F:	/* CAT Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "CAT XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[2] != val) {
					insertion_effect_xg[msb].control_depth[2] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x10:	/* AC1 Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "AC1 XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[3] != val) {
					insertion_effect_xg[msb].control_depth[3] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x11:	/* AC2 Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "AC2 XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[4] != val) {
					insertion_effect_xg[msb].control_depth[4] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x12:	/* CBC1 Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "CBC1 XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[5] != val) {
					insertion_effect_xg[msb].control_depth[5] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x13:	/* CBC2 Insertion Control Depth */
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "CBC2 XG Insertion %d Control Depth (%d %d)", msb + 1, note, val);
				if (insertion_effect_xg[msb].control_depth[6] != val) {
					insertion_effect_xg[msb].control_depth[6] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x20:	/* Insertion Effect Parameter 11 - 16 */
			case 0x21:
			case 0x22:
			case 0x23:
			case 0x24:
			case 0x25:
				temp = b - 0x20 + 10;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Parameter %d (%d %d)", msb + 1, temp + 1, note, val);
				if (insertion_effect_xg[msb].set_param_lsb[temp] != val) {
					insertion_effect_xg[msb].set_param_lsb[temp] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x30:	/* Insertion Effect Parameter 1 - 10 MSB */
			case 0x32:
			case 0x34:
			case 0x36:
			case 0x38:
			case 0x3A:
			case 0x3C:
			case 0x3E:
			case 0x40:
			case 0x42:
				if (!insertion_effect_xg[msb].use_msb) {break;}
				temp = (b - 0x30) / 2;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Parameter %d MSB (%d %d)", msb + 1, temp + 1, note, val);
				if (insertion_effect_xg[msb].set_param_msb[temp] != val) {
					insertion_effect_xg[msb].set_param_msb[temp] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			case 0x31:	/* Insertion Effect Parameter 1 - 10 LSB */
			case 0x33:
			case 0x35:
			case 0x37:
			case 0x39:
			case 0x3B:
			case 0x3D:
			case 0x3F:
			case 0x41:
			case 0x43:
				if (!insertion_effect_xg[msb].use_msb) {break;}
				temp = (b - 0x31) / 2;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "XG Insertion %d Parameter %d LSB (%d %d)", msb + 1, temp + 1, note, val);
				if (insertion_effect_xg[msb].set_param_lsb[temp] != val) {
					insertion_effect_xg[msb].set_param_lsb[temp] = val;
					recompute_effect_xg(&insertion_effect_xg[msb], 1);
				}
				break;
			default:
				break;
			}

		} else if (note == 8 && msb == 0) {	/* Multi Part */
	///r
			switch(b)
			{
	/*
			case 0x01:	// Bank Select MSB
				channel[ch].bank_msb = val;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG SysEx Bank Select MSB (CH:%d)",ch);
				break;
			case 0x02:	// Bank Select LSB
				channel[ch].bank_lsb = val;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG SysEx Bank Select LSB (CH:%d)",ch);
				break;
			case 0x03:	// Program Change
				channel[ch].program = val;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG SysEx Program Change (CH:%d)",ch);
				break;
			case 0x04:	// Rcv CHANNEL, remapped from 0x04 ?
				reset_controllers(ch);
				redraw_controllers(ch);
				all_notes_off(ch);
				if (val == 0x7f)
					remove_channel_layer(ch);
				else {
					if((ch < REDUCE_CHANNELS) != (val < REDUCE_CHANNELS)) {
						channel[ch].port_select = ch < REDUCE_CHANNELS ? 1 : 0;
					}
					if((ch % REDUCE_CHANNELS) != (val % REDUCE_CHANNELS)) {
						add_channel_layer(ch, val);
					}
				}
				break;
			case 0x05:	// Mono/Poly Mode
				channel[ch].mono = val;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG SysEx Mono/Poly Mode (CH:%d)",ch);
				break;
	*/
	///r

			case 0x06:	/* Same Note Number Key On Assign */
				if(val == 0) {
					channel[ch].assign_mode = 0;
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Single (CH:%d)",ch);
				} else if(val == 1) {
					channel[ch].assign_mode = 1; // XG Multi : Not Full Multi
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Multi (CH:%d)",ch);
				} else if(val == 2) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Inst is not supported. (CH:%d)",ch);
				} else if(val == 3) {
					channel[ch].assign_mode = 2; // Timidity Full Multi
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Inst is not supported. (CH:%d)",ch);
				}
				break;
	/*
			case 0x07:	// Part Mode
				midi_drumpart_change(ch, TRUE);
				channel[ch].bank_msb = 127;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"XG SysEx Part Mode (CH:%d)",ch);
				break;
			case 0x08:	// note shift
				break;
	*/
			case 0x09:	/* Detune 1st bit */
				channel[ch].detune_param &= 0x0F; // clear bit7-4
				channel[ch].detune_param |= (val << 4) & 0xF0; // add val bit3-0 to bit7-4
				{
					float tmpf = (FLOAT_T)((int)channel[ch].detune_param - 0x80) * DIV_10;
					if(channel[ch].detune_param != 0x80)
						ctl->cmsg(CMSG_INFO,VERB_NOISY,"Detune 1st bit (CH:%d detune:%.1fHz VAL:%02X)", ch, tmpf, val);
					else
						ctl->cmsg(CMSG_INFO,VERB_NOISY,"Detune 1st bit (CH:%d detune:0.0Hz VAL:%02X)", ch, val);
				}
				break;
			case 0x0A:	/* Detune 2nd bit */
				channel[ch].detune_param &= 0xF0; // clear bit3-0
				channel[ch].detune_param |= val & 0x0F; // add val bit3-0 to bit3-0
				{
					float tmpf = (FLOAT_T)((int)channel[ch].detune_param - 0x80) * DIV_10;
					if(channel[ch].detune_param != 0x80)
						ctl->cmsg(CMSG_INFO,VERB_NOISY,"Detune 2nd bit (CH:%d detune:%.1fHz VAL:%02X)", ch, tmpf, val);
					else
						ctl->cmsg(CMSG_INFO,VERB_NOISY,"Detune 2nd bit (CH:%d detune:0.0Hz VAL:%02X)", ch, val);
				}
				break;
			case 0x11:	/* Dry Level */
				channel[ch].dry_level = val;
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Dry Level (CH:%d VAL:%d)", ch, val);
				break;
			case 0x99:	/* Rcv CHANNEL, remapped from 0x04 */
				reset_controllers(ch);
				redraw_controllers(ch);
				all_notes_off(ch);
				if (val == 0x7f)
					remove_channel_layer(ch);
				else {
					if((ch < REDUCE_CHANNELS) != (val < REDUCE_CHANNELS)) {
						channel[ch].port_select = ch < REDUCE_CHANNELS ? 1 : 0;
					}
					if((ch % REDUCE_CHANNELS) != (val % REDUCE_CHANNELS)) {
						add_channel_layer(ch, val);
					}
				}
				break;
			}
		} else if ((note & 0xF0) == 0x30) {	/* Drum Setup */
			note = msb;
			switch(b)
			{
			case 0x0E:	/* EG Decay1 */
				if (channel[ch].drums[note] == NULL)
					play_midi_setup_drums(ch, note);
				ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Drum Instrument EG Decay1 (CH:%d NOTE:%d VAL:%d)",
					ch, note, val);
				channel[ch].drums[note]->drum_envelope_rate[EG_DECAY1] = val;
				break;
			case 0x0F:	/* EG Decay2 */
				if (channel[ch].drums[note] == NULL)
					play_midi_setup_drums(ch, note);
				ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Drum Instrument EG Decay2 (CH:%d NOTE:%d VAL:%d)",
					ch, note, val);
				channel[ch].drums[note]->drum_envelope_rate[EG_DECAY2] = val;
				break;
			case 0x60:	/* Velocity Pitch Sense */				
				if (channel[ch].drums[note] == NULL)
					play_midi_setup_drums(ch, note);
				ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Drum Velocity Pitch Sense (CH:%d NOTE:%d VAL:%d)",
					ch, note, val - 0x40);
				channel[ch].drums[note]->drum_velo_pitch_sens = clip_int(val - 0x40, -16, 16);
				break;
			case 0x61:	/* Velocity LPF Cutoff Sense */
				if (channel[ch].drums[note] == NULL)
					play_midi_setup_drums(ch, note);
				ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Drum Velocity LPF Cutoff Sense (CH:%d NOTE:%d VAL:%d)",
					ch, note, val - 0x40);
				channel[ch].drums[note]->drum_velo_cutoff_sens = clip_int(val - 0x40, -16, 16);
				break;
			default:
				break;
			}
		}
		return;

	} else if(ev == ME_SYSEX_SD_LSB) {	/* SD system exclusive message */
		int hsb = channel[ch].sysex_sd_hsb_addr;
		int hval = channel[ch].sysex_sd_hsb_val;
		int part = channel[ch].sysex_sd_msb_addr;
		int mfx = channel[ch].sysex_sd_msb_val;
		
		switch(hsb){
		case 0x00: // System // System Common
			break;
		case 0x01: // System // System Common
			break;
		case 0x02: // System // System EQ
			switch(b){
			case 0x00: // EQ Switch
				if(opt_eq_control) {
					if(val == 0) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ (OFF)");}
					else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ (ON)");}
					multi_eq_sd.sw = val ? 1 : 0;
					recompute_multi_eq_sd();
				}
				break;
			case 0x01: // EQ Left Low Frequency
				if(opt_eq_control) {
					if(val > 1) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left Low Freq (400Hz)");}
					else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left Low Freq (200Hz");}
					multi_eq_sd.freq_ll = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x02: // EQ Left Low Gain
				if(opt_eq_control) {					
					if(val > 0x1E) {val = 0x1E;}
					else if(val < 0x00) {val = 0x00;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left Low Gain (%ddB)", (int)val - 0x0F);
					multi_eq_sd.gain_ll = val;
					recompute_multi_eq_xg();
				}	
				break;
			case 0x03: // EQ Left High Frequency
				if(opt_eq_control) {
					if(val > 2) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left High Freq (8000Hz)");}
					else if(val > 1) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left High Freq (4000Hz)");}
					else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left High Freq (2000Hz)");}
					multi_eq_sd.freq_hl = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x04: // EQ Left High Gain
				if(opt_eq_control) {					
					if(val > 0x1E) {val = 0x1E;}
					else if(val < 0x00) {val = 0x00;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left High Gain (%ddB)", (int)val - 0x0F);
					multi_eq_sd.gain_hl = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x05: // EQ Right Low Frequency
				if(opt_eq_control) {
					if(val > 1) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Right Low Freq (400Hz)");}
					else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Left Right Freq (200Hz)");}
					multi_eq_sd.freq_lr = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x06: // EQ Right Low Gain
				if(opt_eq_control) {					
					if(val > 0x1E) {val = 0x1E;}
					else if(val < 0x00) {val = 0x00;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Right Low Gain (%ddB)", (int)val - 0x0F);
					multi_eq_sd.gain_lr = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x07: // EQ Right High Frequency
				if(opt_eq_control) {
					if(val > 2) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Right High (Freq 8000Hz)");}
					else if(val > 1) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Right High Freq (4000Hz)");}
					else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Right High Freq (2000Hz)");}
					multi_eq_sd.freq_hr = val;
					recompute_multi_eq_xg();
				}
				break;
			case 0x08: // EQ Right High Gain
				if(opt_eq_control) {					
					if(val > 0x1E) {val = 0x1E;}
					else if(val < 0x00) {val = 0x00;}
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"SD EQ Right High Gain (%ddB)", (int)val - 0x0F);
					multi_eq_sd.gain_hr = val;
					recompute_multi_eq_xg();
				}
				break;
			}
			break;
		case 0x03: // Audio // Audio Common
			break;
		case 0x04: // Audio // Audio Common Mixer Preset Mode
			break;
		case 0x05: // Audio // Audio Common Mixer Free Edit Mode
			break;
		case 0x06: // Audio // Audio Common AFX Parameter
			break;
		case 0x07: // Temporary Multitimbre // Multitimbre Common
			switch(b){
			case 0x00:
				break;
			case 0x30: // MFX 1/2/3 source
				if(val == 0){
					if(mfx_effect_sd[mfx].efx_source != -1){
						channel[mfx_effect_sd[mfx].efx_source].sd_output_assign = 1; // mfx off
						ctl_mode_event(CTLE_INSERTION_EFFECT, 1, mfx_effect_sd[mfx].efx_source, 0);
					}
					mfx_effect_sd[mfx].efx_source = -1; // common
					mfx_effect_sd[mfx].type = &mfx_effect_sd[mfx].common_type;
					mfx_effect_sd[mfx].set_param = mfx_effect_sd[mfx].common_param;	
				}else{
					int part = val - 1; // part ?
					if(part >= 32 || part < 0) {return;}
					mfx_effect_sd[mfx].efx_source = part; // part
					mfx_effect_sd[mfx].type = &channel[part].mfx_part_type;
					mfx_effect_sd[mfx].set_param = channel[part].mfx_part_param;
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX source (MFX:%d VAL:%02X)", mfx, val);
				realloc_mfx_effect_sd(&mfx_effect_sd[mfx], 0);
				break;
			case 0x33: // Chorus source
				if(val == 0){
					chorus_status_sd.efx_source = -1; // common
					chorus_status_sd.type = &chorus_status_sd.common_type;
					chorus_status_sd.set_param = chorus_status_sd.common_param;	
				}else{
					int part = val - 1; // part ?
					if(part >= 32 || part < 0) {return;}
					chorus_status_sd.efx_source = part; // part
					chorus_status_sd.type = &channel[part].chorus_part_type;
					chorus_status_sd.set_param = channel[part].chorus_part_param;
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Chorus source (VAL:%02X)", val);
				realloc_chorus_status_sd(&chorus_status_sd, 0);
				break;
			case 0x34: // Reverb source
				if(val == 0){
					reverb_status_sd.efx_source = -1; // common
					reverb_status_sd.type = &chorus_status_sd.common_type;
					reverb_status_sd.set_param = chorus_status_sd.common_param;	
				}else{
					int part = val - 1; // part ?
					if(part >= 32 || part < 0) {return;}
					reverb_status_sd.efx_source = part; // part
					reverb_status_sd.type = &channel[part].reverb_part_type;
					reverb_status_sd.set_param = channel[part].reverb_part_param;
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Reverb source (VAL:%02X)", val);
				realloc_reverb_status_sd(&reverb_status_sd, 0);
				break;
			case 0x35: // MFX 1 control channel
				if(val >= 0 && val <= 15)
					mfx_effect_sd[0].ctrl_channel = val;
				else
					mfx_effect_sd[0].ctrl_channel = -1; // off
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MFX 1 control channel (%02X)", mfx_effect_sd[0].ctrl_channel);
				break;
			case 0x36: // MFX 2 control channel
				if(val >= 0 && val <= 15)
					mfx_effect_sd[1].ctrl_channel = val;
				else
					mfx_effect_sd[1].ctrl_channel = -1; // off
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MFX 2 control channel (%02X)", mfx_effect_sd[1].ctrl_channel);
				break;
			case 0x37: // MFX 3 control channel
				if(val >= 0 && val <= 15)
					mfx_effect_sd[2].ctrl_channel = val;
				else
					mfx_effect_sd[2].ctrl_channel = -1; // off
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MFX 3 control channel (%02X)", mfx_effect_sd[2].ctrl_channel);
				break;
			case 0x38: // MFX 1 control port
				mfx_effect_sd[0].ctrl_port = val ? 1 : 0;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MFX 1 control port (%02X)", mfx_effect_sd[0].ctrl_port);
				break;
			case 0x39: // MFX 2 control port
				mfx_effect_sd[1].ctrl_port = val ? 1 : 0;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MFX 2 control port (%02X)", mfx_effect_sd[1].ctrl_port);
				break;
			case 0x3A: // MFX 3 control port
				mfx_effect_sd[2].ctrl_port = val ? 1 : 0;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MFX 3 control port (%02X)", mfx_effect_sd[2].ctrl_port);
				break;
			}
			break;
		case 0x08: // Temporary Multitimbre // Multitimbre Common Chorus
			if(!opt_chorus_control) {break;}			
			switch(b){
			case 0x00: // Chorus Type
				temp = chorus_status_sd.common_type;
				chorus_status_sd.common_type = val;
				if(temp == val) {
					recompute_chorus_status_sd(&chorus_status_sd, 1);
				} else {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Chorus Type (VAL:%02X)", val);
					if(val == 0x03) // param[0]:type
						chorus_status_sd.common_param[0] = 0x7F; // type default 
					realloc_chorus_status_sd(&chorus_status_sd, 0);
				}
				break;
			case 0x01: // Chorus Level
				chorus_status_sd.common_efx_level = val;
				recompute_chorus_status_sd(&chorus_status_sd, 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Chorus Level (VAl:%02X)", val);	
				break;
			case 0x02: // Chorus Output Select
				chorus_status_sd.common_output_select = val;
				recompute_chorus_status_sd(&chorus_status_sd, 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Chorus Output Select (VAl:%02X)", val);	
				break;
			case 0x03: // Chorus Parameter 1
			case 0x04: // Chorus Parameter 2
			case 0x05: // Chorus Parameter 3
			case 0x06: // Chorus Parameter 4
			case 0x07: // Chorus Parameter 5
			case 0x08: // Chorus Parameter 6
			case 0x09: // Chorus Parameter 7
			case 0x0A: // Chorus Parameter 8
			case 0x0B: // Chorus Parameter 9
			case 0x0C: // Chorus Parameter 10
			case 0x0D: // Chorus Parameter 11
			case 0x0E: // Chorus Parameter 12
				temp = b - 0x03;
				if(chorus_status_sd.common_type == 0x03 && temp == 0){ // type
					int prev = chorus_status_sd.common_param[temp];
					chorus_status_sd.common_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					if(prev != chorus_status_sd.common_param[temp])
						realloc_chorus_status_sd(&chorus_status_sd, 0);
					else
						recompute_chorus_status_sd(&chorus_status_sd, 1);
				}else{
					chorus_status_sd.common_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					recompute_chorus_status_sd(&chorus_status_sd, 1);
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Chorus parameter (param:%02X VAL:%02X)", temp, chorus_status_sd.common_param[temp]);
				break;
			}
			break;
		case 0x09: // Temporary Multitimbre // Multitimbre Common Reverb
			if(!opt_reverb_control) {break;}			
			switch(b){
			case 0x00: // Reverb Type
				temp = reverb_status_sd.common_type;
				reverb_status_sd.common_type = val;
				if(temp == val) {
					recompute_reverb_status_sd(&reverb_status_sd, 1);
				} else {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Reverb Type (VAL:%02X)", val);
					if(val == 0x01 || val == 0x05) // param[0]:type
						reverb_status_sd.common_param[0] = 0x7F; // type default 
					realloc_reverb_status_sd(&reverb_status_sd, 0);
				}
				break;
			case 0x01: // Reverb Level
				reverb_status_sd.common_efx_level = val;
				recompute_reverb_status_sd(&reverb_status_sd, 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Reverb Level (VAl:%02X)", val);	
				break;
			case 0x02: // Reverb Parameter 1
			case 0x03: // Reverb Parameter 2
			case 0x04: // Reverb Parameter 3
			case 0x05: // Reverb Parameter 4
			case 0x06: // Reverb Parameter 5
			case 0x07: // Reverb Parameter 6
			case 0x08: // Reverb Parameter 7
			case 0x09: // Reverb Parameter 8
			case 0x0A: // Reverb Parameter 9
			case 0x0B: // Reverb Parameter 10
			case 0x0C: // Reverb Parameter 11
			case 0x0D: // Reverb Parameter 12
			case 0x0E: // Reverb Parameter 13
			case 0x0F: // Reverb Parameter 14
			case 0x10: // Reverb Parameter 15
			case 0x11: // Reverb Parameter 16
			case 0x12: // Reverb Parameter 17
			case 0x13: // Reverb Parameter 18
			case 0x14: // Reverb Parameter 19
			case 0x15: // Reverb Parameter 20
				temp = b - 0x02;
				if((reverb_status_sd.common_type == 0x01 || reverb_status_sd.common_type == 0x05) && temp == 0){ // type
					int prev = reverb_status_sd.common_param[temp];
					reverb_status_sd.common_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					if(prev != reverb_status_sd.common_param[temp])
						realloc_reverb_status_sd(&reverb_status_sd, 0);
					else
						recompute_reverb_status_sd(&reverb_status_sd, 1);
				}else{
					reverb_status_sd.common_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					recompute_reverb_status_sd(&reverb_status_sd, 1);
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Reverb parameter (param:%02X VAL:%02X)", temp, reverb_status_sd.common_param[temp]);
				break;
			}
			break;
		case 0x0A: // Temporary Multitimbre // Multitimbre Common MFX1/2/3
			if(!opt_insertion_effect) {break;}			
			if(mfx >= SD_MFX_EFFECT_NUM || mfx < 0) {
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Error : SD Common MFX num (%02X)", mfx);
				break;
			}
			switch(b)	{
			case 0x00: // MFX Type
				temp = mfx_effect_sd[mfx].common_type;
				mfx_effect_sd[mfx].common_type = val;
				if(temp == val) {
					recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				} else {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX type (MFX:%02X VAL:%02X)", mfx, val);
					realloc_mfx_effect_sd(&mfx_effect_sd[mfx], 0);
				}
				if(val > 0){
					int i;
					for(i = 0; i < 32; i++)
						if(channel[i].sd_output_assign == 0 && channel[i].sd_output_mfx_select == mfx)
							ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 1);
				}else{
					int i;
					for(i = 0; i < 32; i++)
						if(channel[i].sd_output_assign == 0 && channel[i].sd_output_mfx_select == mfx)
							ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 0);
				}
				break;
			case 0x01: // MFX Dry Send Level
				mfx_effect_sd[mfx].common_dry_send = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common Dry Send Level (MFX:%02X VAL:%02X)", mfx, val);		
				break;
			case 0x02: // MFX Chorus Send Level
				mfx_effect_sd[mfx].common_send_chorus = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Chorus Send Level (MFX:%02X VAL:%02X)", mfx, val);	
				break;	
			case 0x03: // MFX Reverb Send Level
				mfx_effect_sd[mfx].common_send_reverb = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Reverb Send Level (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			//case 0x04: // none
			case 0x05: // MFX Control 1 Source
				mfx_effect_sd[mfx].common_ctrl_source[0] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 1 Source (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x06: // MFX Control 1 Sens 
				mfx_effect_sd[mfx].common_ctrl_sens[0] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 1 Sens (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x07: // MFX Control 2 Source
				mfx_effect_sd[mfx].common_ctrl_source[1] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 2 Source (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x08: // MFX Control 2 Sens 
				mfx_effect_sd[mfx].common_ctrl_sens[1] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 2 Sens (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x09: // MFX Control 3 Source
				mfx_effect_sd[mfx].common_ctrl_source[2] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 3 Source (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x0A: // MFX Control 3 Sens 
				mfx_effect_sd[mfx].common_ctrl_sens[2] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 3 Sens (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x0B: // MFX Control 4 Source
				mfx_effect_sd[mfx].common_ctrl_source[3] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 4 Source (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x0C: // MFX Control 4 Sens 
				mfx_effect_sd[mfx].common_ctrl_sens[3] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control 4 Sens (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x0D: // MFX Control Assign 1
				mfx_effect_sd[mfx].common_ctrl_assign[0] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control Assign 1 (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x0E: // MFX Control Assign 2
				mfx_effect_sd[mfx].common_ctrl_assign[1] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control Assign 2 (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x0F: // MFX Control Assign 3
				mfx_effect_sd[mfx].common_ctrl_assign[2] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control Assign 3 (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x10: // MFX Control Assign 4
				mfx_effect_sd[mfx].common_ctrl_assign[3] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX Control Assign 4 (MFX:%02X VAL:%02X)", mfx, val);	
				break;
			case 0x11: // MFX param
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
			case 0x18:
			case 0x19:
			case 0x1A:
			case 0x1B:
			case 0x1C:
			case 0x1D:
			case 0x1E:
			case 0x1F:
			case 0x20:
			case 0x21:	
			case 0x22:	
			case 0x23:	
			case 0x24:	
			case 0x25:	
			case 0x26:	
			case 0x27:	
			case 0x28:	
			case 0x29:	
			case 0x2A:	
			case 0x2B:	
			case 0x2C:	
			case 0x2D:	
			case 0x2E:	
			case 0x2F:	
			case 0x30:	
			case 0x31:	
				temp = b - 0x11;
				mfx_effect_sd[mfx].common_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Common MFX parameter (MFX:%02X param:%02X VAL:%02X)", mfx, temp, mfx_effect_sd[mfx].set_param[temp]);
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				break;
			}
			break;
		case 0x0B: // Temporary Multitimbre // Multitimbre Part
			switch(b){
			case 0x1C: // Part Dry Send Level
				channel[part].sd_dry_send_level = val;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Dry Send Level (%02X %02X)", part, val);
				break;
			case 0x1F: // Part Output Assign
				if(val >= 33 || val < 0) {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Error : SD Part Output Assign (%02X %02X)", part, val);
					return;
				}
				channel[part].sd_output_assign = val > 2 ? 2 : val; // 0:MFX 1:NONE 2:Patch
				if(channel[part].sd_output_assign == 0)
					ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 1);
				else if(channel[part].sd_output_assign == 1)
					ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 0);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Output Assign (%02X %02X)", part, val);	
				break;
			case 0x20: // Part Output MFX Select
				if(val >= SD_MFX_EFFECT_NUM || val < 0) {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Error : SD Part Output MFX Select (%02X %02X)", part, val);
					return;
				}	
				channel[part].sd_output_mfx_select = val;
				if(channel[part].sd_output_assign != 1 && *mfx_effect_sd[val].type > 0)
					ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 1);
				else
					ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 0);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Output MFX Select (%02X %02X)", part, val);
				break;
			}
			break;
		case 0x0C: // Temporary Multitimbre // Multitimbre MIDI
			break;
		case 0x0D: // Temporary Patch // Patch Common
			break;
		case 0x0E: // Temporary Patch // Patch Common Chorus // Temporary Rhythm // Rhythm Common Chorus
			switch(b)	{
			case 0x00:	// Chorus Type part				
				temp = channel[part].chorus_part_type;
				channel[part].chorus_part_type = val;
				if(temp == val) {
					recompute_chorus_status_sd(&chorus_status_sd, 1);
				} else {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Chorus Type (VAL:%02X)", val);
					if(val == 0x03) // param[0]:type
						channel[part].chorus_part_param[0] = 0x7F; // type default 
					realloc_chorus_status_sd(&chorus_status_sd, 0);
				}
				break;
			case 0x01:	// Chorus Level part	
				channel[part].chorus_part_efx_level = val;
				recompute_chorus_status_sd(&chorus_status_sd, 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Chorus Level (VAl:%02X)", val);	
				break;
			case 0x02:	// Chorus Output Select part	
				channel[part].chorus_part_output_select = val;
				recompute_chorus_status_sd(&chorus_status_sd, 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Chorus Output Select (VAl:%02X)", val);	
				break;
			case 0x03:	// Chorus Parameter 1 part	
			case 0x04:	// Chorus Parameter 2 part	
			case 0x05:	// Chorus Parameter 3 part	
			case 0x06:	// Chorus Parameter 4 part	
			case 0x07:	// Chorus Parameter 5 part	
			case 0x08:	// Chorus Parameter 6 part	
			case 0x09:	// Chorus Parameter 7 part	
			case 0x0A:	// Chorus Parameter 8 part	
			case 0x0B:	// Chorus Parameter 9 part	
			case 0x0C:	// Chorus Parameter 10 part	
			case 0x0D:	// Chorus Parameter 11 part		
			case 0x0E:	// Chorus Parameter 12 part	
				temp = b - 0x03;
				if(channel[part].chorus_part_type == 0x03 && temp == 0){ // type
					int prev = channel[part].chorus_part_param[temp];
					channel[part].chorus_part_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					if(prev != channel[part].chorus_part_param[temp])
						realloc_chorus_status_sd(&chorus_status_sd, 0);
					else
						recompute_chorus_status_sd(&chorus_status_sd, 1);
				}else{
					channel[part].chorus_part_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					recompute_chorus_status_sd(&chorus_status_sd, 1);
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Chorus parameter (param:%02X VAL:%02X)", temp, channel[part].chorus_part_param[temp]);
				break;
			}
			break;
		case 0x0F: // Temporary Patch // Patch Common Reverb // Temporary Rhythm // Rhythm Common Reverb
			switch(b)	{
			case 0x00:	// Reverb Type part				
				temp = channel[part].reverb_part_type;
				channel[part].reverb_part_type = val;
				if(temp == val) {
					recompute_reverb_status_sd(&reverb_status_sd, 1);
				} else {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Reverb Type (VAL:%02X)", val);
					if(val == 0x01 || val == 0x05) // param[0]:type
						channel[part].reverb_part_param[0] = 0x7F; // type default 
					realloc_reverb_status_sd(&reverb_status_sd, 0);
				}
				break;
			case 0x01:	// Reverb Level part	
				channel[part].reverb_part_efx_level = val;
				recompute_reverb_status_sd(&reverb_status_sd, 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Reverb Level (VAl:%02X)", val);	
				break;
			case 0x02:	// Reverb Parameter 1 part
			case 0x03:	// Reverb Parameter 2 part
			case 0x04:	// Reverb Parameter 3 part
			case 0x05:	// Reverb Parameter 4 part
			case 0x06:	// Reverb Parameter 5 part
			case 0x07:	// Reverb Parameter 6 part
			case 0x08:	// Reverb Parameter 7 part
			case 0x09:	// Reverb Parameter 8 part
			case 0x0A:	// Reverb Parameter 9 part
			case 0x0B:	// Reverb Parameter 10 part
			case 0x0C:	// Reverb Parameter 11 part
			case 0x0D:	// Reverb Parameter 12 part
			case 0x0E:	// Reverb Parameter 13 part
			case 0x0F:	// Reverb Parameter 14 part
			case 0x10:	// Reverb Parameter 15 part
			case 0x11:	// Reverb Parameter 16 part
			case 0x12:	// Reverb Parameter 17 part
			case 0x13:	// Reverb Parameter 18 part
			case 0x14:	// Reverb Parameter 19 part
			case 0x15:	// Reverb Parameter 20 part
				temp = b - 0x02;
				if((channel[part].reverb_part_type == 0x01 || channel[part].reverb_part_type == 0x05) && temp == 0){ // type
					int prev = channel[part].reverb_part_param[temp];
					channel[part].reverb_part_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					if(prev != channel[part].reverb_part_param[temp])
						realloc_reverb_status_sd(&reverb_status_sd, 0);
					else
						recompute_reverb_status_sd(&reverb_status_sd, 1);
				}else{
					channel[part].reverb_part_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
					recompute_reverb_status_sd(&reverb_status_sd, 1);
				}
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Reverb Parameter (param:%02X VAL:%02X)", temp, channel[part].reverb_part_param[temp]);
				break;
			}
			break;
		case 0x10: // Temporary Patch // Patch Common MFX // Temporary Rhythm // Rhythm Common MFX
			mfx = channel[part].sd_output_mfx_select;
			if(!opt_insertion_effect) {break;}
			switch(b)	{
			case 0x00:	/* MFX Type part */
				temp = channel[part].mfx_part_type;
				channel[part].mfx_part_type = val;				
				if(temp == val) {
					recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				} else {
					ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX type (%02X %02X)", part, val);
					realloc_mfx_effect_sd(&mfx_effect_sd[mfx], 0);
				}
				if(val > 0)
					ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 1);
				else
					ctl_mode_event(CTLE_INSERTION_EFFECT, 1, ch, 0);
				break;
			case 0x01: // MFX Dry Send Level
				channel[part].mfx_part_dry_send = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part Dry Send Level (%02X %02X)", part, val);		
				break;
			case 0x02: // MFX Chorus Send Level
				channel[part].mfx_part_send_chorus = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Chorus Send Level (%02X %02X)", part, val);	
				break;	
			case 0x03: // MFX Reverb Send Level
				channel[part].mfx_part_send_reverb = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Reverb Send Level (%02X %02X)", part, val);	
				break;
			//case 0x04: // none
			case 0x05: // MFX Control 1 Source
				channel[part].mfx_part_ctrl_source[0] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 1 Source (%02X %02X)", part, val);	
				break;
			case 0x06: // MFX Control 1 Sens 
				channel[part].mfx_part_ctrl_sens[0] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 1 Sens (%02X %02X)", part, val);	
				break;
			case 0x07: // MFX Control 2 Source
				channel[part].mfx_part_ctrl_source[1] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 2 Source (%02X %02X)", part, val);	
				break;
			case 0x08: // MFX Control 2 Sens 
				channel[part].mfx_part_ctrl_sens[1] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 2 Sens (%02X %02X)", part, val);	
				break;
			case 0x09: // MFX Control 3 Source
				channel[part].mfx_part_ctrl_source[2] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 3 Source (%02X %02X)", part, val);	
				break;
			case 0x0A: // MFX Control 3 Sens 
				channel[part].mfx_part_ctrl_sens[2] = val;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 3 Sens (%02X %02X)", part, val);	
				break;
			case 0x0B: // MFX Control 4 Source
				channel[part].mfx_part_ctrl_source[3] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 4 Source (%02X %02X)", part, val);	
				break;
			case 0x0C: // MFX Control 4 Sens 
				channel[part].mfx_part_ctrl_sens[3] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control 4 Sens (%02X %02X)", part, val);	
				break;
			case 0x0D: // MFX Control Assign 1
				channel[part].mfx_part_ctrl_assign[0] = val;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control Assign 1 (%02X %02X)", part, val);	
				break;
			case 0x0E: // MFX Control Assign 2
				channel[part].mfx_part_ctrl_assign[1] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control Assign 2 (%02X %02X)", part, val);	
				break;
			case 0x0F: // MFX Control Assign 3
				channel[part].mfx_part_ctrl_assign[2] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control Assign 3 (%02X %02X)", part, val);	
				break;
			case 0x10: // MFX Control Assign 4
				channel[part].mfx_part_ctrl_assign[3] = val;
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Control Assign 4 (%02X %02X)", part, val);	
				break;
			case 0x11: // MFX parameter
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
			case 0x18:
			case 0x19:
			case 0x1A:
			case 0x1B:
			case 0x1C:
			case 0x1D:
			case 0x1E:
			case 0x1F:
			case 0x20:
			case 0x21:	
			case 0x22:	
			case 0x23:	
			case 0x24:	
			case 0x25:	
			case 0x26:	
			case 0x27:	
			case 0x28:	
			case 0x29:	
			case 0x2A:	
			case 0x2B:	
			case 0x2C:	
			case 0x2D:	
			case 0x2E:	
			case 0x2F:	
			case 0x30:	
			case 0x31:	
				temp = b - 0x11;
				channel[part].mfx_part_param[temp] = (int32)(((uint16)(hval & 0xFF) << 8) | val) - 0x8000;
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "SD Part MFX Parameter (%02X %02X %02X)", part, temp, channel[part].mfx_part_param[temp]);
				recompute_mfx_effect_sd(&mfx_effect_sd[mfx], 1);
				break;
			}
			break;
		case 0x11: // Temporary Patch // Patch Common TMT
			break;
		case 0x12: // Temporary Patch // Patch Common Tone
			break;
		case 0x13: // Temporary Rhythm // Rhythm Common
			break;
		case 0x17: // Temporary Rhythm // Rhythm Common Tone
			break;
		default:
			break;
		}
	}
}

/* 
play_midi_prescan()で使用するprocess_sysex_event()の簡易版
必要なのは ノート関連 Rx.関連 MasterVolume関連
エフェクトなどは不要 
コンソール出力 コントロール表示 も削除 ctl->cmsg() redraw_controllers()
*/
static void process_sysex_event_prescan(int ev, int ch, int val, int b)
{
	int temp, msb, note;

	if (ch >= MAX_CHANNELS)
		return;
	if (ev == ME_SYSEX_MSB) {
		channel[ch].sysex_msb_addr = b;
		channel[ch].sysex_msb_val = val;
	} else if(ev == ME_SYSEX_GS_MSB) {
		channel[ch].sysex_gs_msb_addr = b;
		channel[ch].sysex_gs_msb_val = val;
	} else if(ev == ME_SYSEX_XG_MSB) {
		channel[ch].sysex_xg_msb_addr = b;
		channel[ch].sysex_xg_msb_val = val;
	} else if(ev == ME_SYSEX_SD_MSB) {
		channel[ch].sysex_sd_msb_addr = b;
		channel[ch].sysex_sd_msb_val = val;
	} else if(ev == ME_SYSEX_SD_HSB) {
		channel[ch].sysex_sd_hsb_addr = b;
		channel[ch].sysex_sd_hsb_val = val;
	} else if(ev == ME_SYSEX_LSB) {	/* Universal system exclusive message */
		msb = channel[ch].sysex_msb_addr;
		note = channel[ch].sysex_msb_val;
		channel[ch].sysex_msb_addr = channel[ch].sysex_msb_val = 0;
		switch(b)
		{
		case 0x42:	/* Note Limit Low */
			channel[ch].note_limit_low = val;
			break;
		case 0x43:	/* Note Limit High */
			channel[ch].note_limit_high = val;
			break;
		case 0x44:	/* Velocity Limit Low */
			channel[ch].vel_limit_low = val;
			break;
		case 0x45:	/* Velocity Limit High */
			channel[ch].vel_limit_high = val;
			break;
		case 0x46:	/* Rx. Note Off */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			set_rx_drum(channel[ch].drums[note], RX_NOTE_OFF, val);
			channel[ch].drums[note]->rx_note_off = 1;
			break;
		case 0x47:	/* Rx. Note On */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			set_rx_drum(channel[ch].drums[note], RX_NOTE_ON, val);
			break;
		case 0x48:	/* Rx. Pitch Bend */
			set_rx(ch, RX_PITCH_BEND, val);
			break;
		case 0x49:	/* Rx. Channel Pressure */
			set_rx(ch, RX_CH_PRESSURE, val);
			break;
		case 0x4A:	/* Rx. Program Change */
			set_rx(ch, RX_PROGRAM_CHANGE, val);
			break;
		case 0x4B:	/* Rx. Control Change */
			set_rx(ch, RX_CONTROL_CHANGE, val);
			break;
		case 0x4C:	/* Rx. Poly Pressure */
			set_rx(ch, RX_POLY_PRESSURE, val);
			break;
		case 0x4D:	/* Rx. Note Message */
			set_rx(ch, RX_NOTE_MESSAGE, val); 
			break;
		case 0x4E:	/* Rx. RPN */
			set_rx(ch, RX_RPN, val);
			break;
		case 0x4F:	/* Rx. NRPN */
			set_rx(ch, RX_NRPN, val);
			break;
		case 0x50:	/* Rx. Modulation */
			set_rx(ch, RX_MODULATION, val);
			break;
		case 0x51:	/* Rx. Volume */
			set_rx(ch, RX_VOLUME, val);
			break;
		case 0x52:	/* Rx. Panpot */
			set_rx(ch, RX_PANPOT, val);
			break;
		case 0x53:	/* Rx. Expression */
			set_rx(ch, RX_EXPRESSION, val); 
			break;
		case 0x54:	/* Rx. Hold1 */
			set_rx(ch, RX_HOLD1, val);
			break;
		case 0x55:	/* Rx. Portamento */
			set_rx(ch, RX_PORTAMENTO, val);
			break;
		case 0x56:	/* Rx. Sostenuto */
			set_rx(ch, RX_SOSTENUTO, val);
			break;
		case 0x57:	/* Rx. Soft */
			set_rx(ch, RX_SOFT, val);
			break;
		case 0x58:	/* Rx. Bank Select */
			set_rx(ch, RX_BANK_SELECT, val); 
			break;
		case 0x59:	/* Rx. Bank Select LSB */
			set_rx(ch, RX_BANK_SELECT_LSB, val);
			break;	
		case 0x6C:	/* GM2 Instrument Select */
			if(ch == 0xFF){
				int i;
				for(i = 0; i < MAX_CHANNELS; i++)
					channel[i].gm2_inst = val;
			}else{
				channel[ch].gm2_inst = val;
			}
			break;
		default:
			break;
		}
		return;
	} else if(ev == ME_SYSEX_GS_LSB) {	/* GS system exclusive message */
		msb = channel[ch].sysex_gs_msb_addr;
		note = channel[ch].sysex_gs_msb_val;
		channel[ch].sysex_gs_msb_addr = channel[ch].sysex_gs_msb_val = 0;
		switch(b)
		{
		case 0x21:	/* Velocity Sense Depth */
			channel[ch].velocity_sense_depth = val;
			break;
		case 0x22:	/* Velocity Sense Offset */
			channel[ch].velocity_sense_offset = val;
			break;
		case 0x24:	/* Assign Mode */
			channel[ch].assign_mode = val;
			break;
		case 0x25:	/* TONE MAP-0 NUMBER */
			channel[ch].tone_map0_number = val;
			break;
		case 0x45:	/* Rx. Channel */
			reset_controllers(ch);
			all_notes_off(ch);
			if (val == 0x80)
				remove_channel_layer(ch);
			else
				add_channel_layer(ch, val);
			break;
		case 0x46:	/* Channel Msg Rx Port */
			reset_controllers(ch);
			all_notes_off(ch);
			channel[ch].port_select = val;
			break;
		case 0x47:	/* Play Note Number */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			channel[ch].drums[note]->play_note = val;
			break;
		default:
			break;
		}
		return;
	} else if(ev == ME_SYSEX_XG_LSB) {	/* XG system exclusive message */
		msb = channel[ch].sysex_xg_msb_addr;
		note = channel[ch].sysex_xg_msb_val;
///r
		if (note == 2 && msb >= 0 && msb < 0x40) {	/* Effect 1 */
			note = 0;	/* not use */
			if(b >= 0x40){ /* variation effect */
				msb -= 1; 	/* variation effect num start 1 , dim 0*/
				/* msb=0 variation 1 */ 
				/* msb=n variation n+1 */ 
				if(msb < 0 || msb >= XG_VARIATION_EFFECT_NUM){
					return;
				}
			}

		} else if (note == 2 && msb == 0x40) {	/* Multi EQ */

		} else if (note == 3 && msb >= 0) {	/* Effect 2 */
			/* msb=0 : insertion 1 */
			/* msb=n : insertion n+1 */
			if ( msb >= XG_INSERTION_EFFECT_NUM) {
				return;
			}

		} else if (note == 8 && msb == 0) {	/* Multi Part */
			switch(b)
			{
	/*
			case 0x01:	// Bank Select MSB
				channel[ch].bank_msb = val;
				break;
			case 0x02:	// Bank Select LSB
				channel[ch].bank_lsb = val;
				break;
			case 0x03:	// Program Change
				channel[ch].program = val;
				break;
			case 0x04:	// Rcv CHANNEL, remapped from 0x04 ?
				reset_controllers(ch);
				all_notes_off(ch);
				if (val == 0x7f)
					remove_channel_layer(ch);
				else {
					if((ch < REDUCE_CHANNELS) != (val < REDUCE_CHANNELS)) {
						channel[ch].port_select = ch < REDUCE_CHANNELS ? 1 : 0;
					}
					if((ch % REDUCE_CHANNELS) != (val % REDUCE_CHANNELS)) {
						add_channel_layer(ch, val);
					}
				}
				break;
			case 0x05:	// Mono/Poly Mode
				channel[ch].mono = val;
				break;
	*/
			case 0x06:	/* Same Note Number Key On Assign */
				if(val == 0) {
					channel[ch].assign_mode = 0;
				} else if(val == 1) {
					channel[ch].assign_mode = 1; // XG Multi : Not Full Multi
				} else if(val == 2) {
				} else if(val == 3) {
					channel[ch].assign_mode = 2; // Timidity Full Multi
				}
				break;
	/*
			case 0x07:	// Part Mode
				midi_drumpart_change(ch, TRUE);
				channel[ch].bank_msb = 127;
				break;
			case 0x08:	// note shift
				break;
	*/
			case 0x99:	/* Rcv CHANNEL, remapped from 0x04 */
				reset_controllers(ch);
				all_notes_off(ch);
				if (val == 0x7f)
					remove_channel_layer(ch);
				else {
					if((ch < REDUCE_CHANNELS) != (val < REDUCE_CHANNELS)) {
						channel[ch].port_select = ch < REDUCE_CHANNELS ? 1 : 0;
					}
					if((ch % REDUCE_CHANNELS) != (val % REDUCE_CHANNELS)) {
						add_channel_layer(ch, val);
					}
				}
				break;
			}
		} else if ((note & 0xF0) == 0x30) {	/* Drum Setup */
			note = msb;
		}
		return;

	}
}

static void play_midi_prescan(MidiEvent *ev)
{
    int i, j, k, ch, orig_ch, port_ch, offset, layered;
    
    if(opt_amp_compensation) {mainvolume_max = 0;}
    else {mainvolume_max = 0x7f;}
 //   compensation_ratio = 1.0;

    prescanning_flag = 1;
    change_system_mode(DEFAULT_SYSTEM_MODE);
    reset_midi(0);
	if(allocate_cache_size > 0)
		resamp_cache_reset();

	while (ev->type != ME_EOT) {
#ifndef SUPPRESS_CHANNEL_LAYER
		orig_ch = ev->channel;
		layered = ! IS_SYSEX_EVENT_TYPE(ev);
		for (j = 0; j < MAX_CHANNELS; j += 16) {
			port_ch = (orig_ch + j) % MAX_CHANNELS;
			offset = port_ch & ~0xf;
			for (k = offset; k < offset + 16; k++) {
				if (! layered && (j || k != offset))
					continue;
				if (layered) {
					if (! IS_SET_CHANNELMASK(
							channel[k].channel_layer, port_ch)
							|| channel[k].port_select != (orig_ch >> 4))
						continue;
					ev->channel = k;
				}
#endif
	ch = ev->channel;

	switch(ev->type)
	{
	  case ME_NOTEON:
		note_on_prescan(ev);
	    break;

	  case ME_NOTEOFF:
		if(allocate_cache_size > 0)
			resamp_cache_refer_off(ch, MIDI_EVENT_NOTE(ev), ev->time);
	    break;

	  case ME_PORTAMENTO_TIME_MSB:
		if(opt_portamento){
			channel[ch].portamento_time_msb = ev->a;
		}
	    break;

	  case ME_PORTAMENTO_TIME_LSB:
		if(opt_portamento){
			channel[ch].portamento_time_lsb = ev->a;
		}
	    break;

	  case ME_PORTAMENTO:		  
		if(!get_rx(ch, RX_PORTAMENTO)) break;
		if(opt_portamento){
			channel[ch].portamento = (ev->a >= 64);
		}
	    break;

      case ME_PORTAMENTO_CONTROL:
		if(opt_portamento){
			channel[ch].portamento_control = ev->a;
		}
		break;

	  case ME_RESET_CONTROLLERS:
	    reset_controllers(ch);
		if(allocate_cache_size > 0)
			resamp_cache_refer_alloff(ch, ev->time);
	    break;

	  case ME_PROGRAM:
	    midi_program_change(ch, ev->a);
	    break;

	  case ME_TONE_BANK_MSB:
	    channel[ch].bank_msb = ev->a;
	    break;

	  case ME_TONE_BANK_LSB:
	    channel[ch].bank_lsb = ev->a;
	    break;

	  case ME_RESET:
	    change_system_mode(ev->a);
	    reset_midi(0);
	    break;

	  case ME_MASTER_TUNING:
	  case ME_PITCHWHEEL:
		adjust_pitch(ch);
	  case ME_ALL_NOTES_OFF:
	  case ME_ALL_SOUNDS_OFF:
	  case ME_MONO:
	  case ME_POLY:
		if(allocate_cache_size > 0)
			resamp_cache_refer_alloff(ch, ev->time);
	    break;
///r
	  case ME_DRUMPART:
		if(play_system_mode == XG_SYSTEM_MODE && ev->a)
			channel[ch].bank_msb = 127; // Drum kit
	    if(midi_drumpart_change(ch, ev->a))
		midi_program_change(ch, channel[ch].program);
	    break;

	  case ME_KEYSHIFT:
		if(allocate_cache_size > 0)
			resamp_cache_refer_alloff(ch, ev->time);
	    channel[ch].key_shift = (int)ev->a - 0x40;
	    break;
		
	  case ME_SCALE_TUNING:
		if(allocate_cache_size > 0)
			resamp_cache_refer_alloff(ch, ev->time);
		channel[ch].scale_tuning[ev->a] = ev->b;
		break;

	  case ME_MAINVOLUME:
		if(opt_amp_compensation)
			if (ev->a > mainvolume_max) {
			  mainvolume_max = ev->a;
			  ctl->cmsg(CMSG_INFO,VERB_DEBUG,"ME_MAINVOLUME/max (CH:%d VAL:%#x)",ev->channel,ev->a);
			}
	    break;	

	  case ME_SYSEX_LSB:
	    process_sysex_event_prescan(ME_SYSEX_LSB,ch,ev->a,ev->b);
	    break;

	  case ME_SYSEX_MSB:
	    process_sysex_event_prescan(ME_SYSEX_MSB,ch,ev->a,ev->b);
	    break;

	  case ME_SYSEX_GS_LSB:
	    process_sysex_event_prescan(ME_SYSEX_GS_LSB,ch,ev->a,ev->b);
	    break;

	  case ME_SYSEX_GS_MSB:
	    process_sysex_event_prescan(ME_SYSEX_GS_MSB,ch,ev->a,ev->b);
	    break;

	  case ME_SYSEX_XG_LSB:
	    process_sysex_event_prescan(ME_SYSEX_XG_LSB,ch,ev->a,ev->b);
	    break;

	  case ME_SYSEX_XG_MSB:
	    process_sysex_event_prescan(ME_SYSEX_XG_MSB,ch,ev->a,ev->b);
	    break;

	case ME_SYSEX_SD_LSB:
	//	process_sysex_event_prescan(ME_SYSEX_SD_LSB,ch,ev->a,ev->b);
	    break;

	case ME_SYSEX_SD_MSB:
	//	process_sysex_event_prescan(ME_SYSEX_SD_MSB,ch,ev->a,ev->b);
	    break;

	case ME_SYSEX_SD_HSB:
	//	process_sysex_event_prescan(ME_SYSEX_SD_HSB,ch,ev->a,ev->b);
	    break;
	}
#ifndef SUPPRESS_CHANNEL_LAYER
			}
		}
		ev->channel = orig_ch;
#endif
	ev++;
    }
		
    /* calculate compensation ratio */
	if(opt_amp_compensation){
		if(mainvolume_max < 1)
			mainvolume_max = 1; // div0
		if (opt_user_volume_curve != 0) {
			compensation_ratio = (double)127.0 / user_vol_table[mainvolume_max & 0x7F];
		} else if (play_system_mode == SD_SYSTEM_MODE) {
			compensation_ratio = (double)127.0 / gm2_vol_table[mainvolume_max & 0x7F];
		} else if (play_system_mode == GM2_SYSTEM_MODE) {
			compensation_ratio = (double)127.0 / gm2_vol_table[mainvolume_max & 0x7F];
		} else if(play_system_mode == GS_SYSTEM_MODE) {	/* use measured curve */ 
			compensation_ratio = (double)127.0 / sc_vol_table[mainvolume_max & 0x7F];
		} else if(play_system_mode == CM_SYSTEM_MODE) {	/* use measured curve */
			compensation_ratio = (double)127.0/ sc_vol_table[mainvolume_max & 0x7F];
		} else if (IS_CURRENT_MOD_FILE) {	/* use linear curve */
			compensation_ratio = (double)127.0 / (double)(mainvolume_max & 0x7F);
		} else {	/* use generic exponential curve */
			compensation_ratio = (double)127.0 / perceived_vol_table[mainvolume_max & 0x7F];
		}
		ctl->cmsg(CMSG_INFO,VERB_DEBUG,"Compensation ratio:%lf",compensation_ratio);
	}
    //if (0 < mainvolume_max && mainvolume_max < 0x7f) {
    //  compensation_ratio = pow((double)0x7f/(double)mainvolume_max, 4);
    //  ctl->cmsg(CMSG_INFO,VERB_DEBUG,"Compensation ratio:%lf",compensation_ratio);
    //}	
	
	if(allocate_cache_size > 0){
		for(i = 0; i < MAX_CHANNELS; i++)
			resamp_cache_refer_alloff(i, ev->time);
		resamp_cache_create();
	}
    prescanning_flag = 0;
}

static int last_rpn_addr(int ch)
{
	int lsb, msb, addr, i;
	struct rpn_tag_map_t *addrmap;
	struct rpn_tag_map_t {
		int addr, mask, tag;
	};
	static struct rpn_tag_map_t nrpn_addr_map[] = {
		{0x0108, 0xffff, NRPN_ADDR_0108},
		{0x0109, 0xffff, NRPN_ADDR_0109},
		{0x010a, 0xffff, NRPN_ADDR_010A},
		{0x0120, 0xffff, NRPN_ADDR_0120},
		{0x0121, 0xffff, NRPN_ADDR_0121},
		{0x0124, 0xffff, NRPN_ADDR_0124}, // hpf
		{0x0125, 0xffff, NRPN_ADDR_0125}, // hpf
		{0x0130, 0xffff, NRPN_ADDR_0130},
		{0x0131, 0xffff, NRPN_ADDR_0131},
		{0x0132, 0xffff, NRPN_ADDR_0132},
		{0x0133, 0xffff, NRPN_ADDR_0133},
		{0x0134, 0xffff, NRPN_ADDR_0134},
		{0x0135, 0xffff, NRPN_ADDR_0135},
		{0x0136, 0xffff, NRPN_ADDR_0136},
		{0x0137, 0xffff, NRPN_ADDR_0137},
		{0x0138, 0xffff, NRPN_ADDR_0138},
		{0x0139, 0xffff, NRPN_ADDR_0139},
		{0x013A, 0xffff, NRPN_ADDR_013A},
		{0x013B, 0xffff, NRPN_ADDR_013B},
		{0x013C, 0xffff, NRPN_ADDR_013C},
		{0x013D, 0xffff, NRPN_ADDR_013D},
		{0x0163, 0xffff, NRPN_ADDR_0163},
		{0x0164, 0xffff, NRPN_ADDR_0164},
		{0x0166, 0xffff, NRPN_ADDR_0166},
		{0x1400, 0xff00, NRPN_ADDR_1400},
		{0x1500, 0xff00, NRPN_ADDR_1500},
		{0x1600, 0xff00, NRPN_ADDR_1600},
		{0x1700, 0xff00, NRPN_ADDR_1700},
		{0x1800, 0xff00, NRPN_ADDR_1800},
		{0x1900, 0xff00, NRPN_ADDR_1900},
		{0x1a00, 0xff00, NRPN_ADDR_1A00},
		{0x1c00, 0xff00, NRPN_ADDR_1C00},
		{0x1d00, 0xff00, NRPN_ADDR_1D00},
		{0x1e00, 0xff00, NRPN_ADDR_1E00},
		{0x1f00, 0xff00, NRPN_ADDR_1F00},
		{0x2400, 0xff00, NRPN_ADDR_2400}, // hpf
		{0x2500, 0xff00, NRPN_ADDR_2500}, // hpf
		{0x3000, 0xff00, NRPN_ADDR_3000},
		{0x3100, 0xff00, NRPN_ADDR_3100},
		{0x3200, 0xff00, NRPN_ADDR_3200},
		{0x3300, 0xff00, NRPN_ADDR_3300},
		{0x3400, 0xff00, NRPN_ADDR_3400},
		{0x3500, 0xff00, NRPN_ADDR_3500},
		{0x3600, 0xff00, NRPN_ADDR_3600},
		{0x3700, 0xff00, NRPN_ADDR_3700},
		{0x3800, 0xff00, NRPN_ADDR_3800},
		{0x3900, 0xff00, NRPN_ADDR_3900},
		{0x3A00, 0xff00, NRPN_ADDR_3A00},
		{0x3B00, 0xff00, NRPN_ADDR_3B00},
		{0x3C00, 0xff00, NRPN_ADDR_3C00},
		{0x3D00, 0xff00, NRPN_ADDR_3D00},
		{-1, -1, 0}
	};
	static struct rpn_tag_map_t rpn_addr_map[] = {
		{0x0000, 0xffff, RPN_ADDR_0000},
		{0x0001, 0xffff, RPN_ADDR_0001},
		{0x0002, 0xffff, RPN_ADDR_0002},
		{0x0003, 0xffff, RPN_ADDR_0003},
		{0x0004, 0xffff, RPN_ADDR_0004},
		{0x0005, 0xffff, RPN_ADDR_0005},
		{0x0040, 0xffff, RPN_ADDR_0040},
		{0x7f7f, 0xffff, RPN_ADDR_7F7F},
		{0xffff, 0xffff, RPN_ADDR_FFFF},
		{-1, -1}
	};
	
	if (channel[ch].nrpn == -1)
		return -1;
	lsb = channel[ch].lastlrpn;
	msb = channel[ch].lastmrpn;
	if (lsb == 0xff || msb == 0xff)
		return -1;
	addr = (msb << 8 | lsb);
	if (channel[ch].nrpn)
		addrmap = nrpn_addr_map;
	else
		addrmap = rpn_addr_map;
	for (i = 0; addrmap[i].addr != -1; i++)
		if (addrmap[i].addr == (addr & addrmap[i].mask))
			return addrmap[i].tag;
	return -1;
}

static void update_rpn_map(int ch, int addr, int update_now)
{
	int val, drumflag, i, note;
	
	val = channel[ch].rpnmap[addr];
	drumflag = 0;
	switch (addr) {
	case NRPN_ADDR_0108:	/* Vibrato Rate */
		if (opt_nrpn_vibrato) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Vibrato Rate (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_vibrato_rate = val - 64;
		}
		if (update_now){
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;
	case NRPN_ADDR_0109:	/* Vibrato Depth */
		if (opt_nrpn_vibrato) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Vibrato Depth (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_vibrato_depth = val - 64;
		}
		if (update_now){
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;
	case NRPN_ADDR_010A:	/* Vibrato Delay */
		if (opt_nrpn_vibrato) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Vibrato Delay (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_vibrato_delay = val - 64;
		}
		if (update_now){
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;
	case NRPN_ADDR_0120:	/* Filter Cutoff Frequency */
		//if (opt_lpf_def) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Filter Cutoff (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_cutoff_freq = val - 64;
			if (update_now)
			//	update_channel_filter(ch);
				recompute_channel_filter(ch);
		//}
		break;
	case NRPN_ADDR_0121:	/* Filter Resonance */
		//if (opt_lpf_def) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Filter Resonance (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_resonance = val - 64;
			if (update_now)
			//	update_channel_filter(ch);
				recompute_channel_filter(ch);
		//}
		break;
///r
	case NRPN_ADDR_0124:	/* HPF Filter Cutoff Frequency */
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN HPF Filter Cutoff (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].hpf_param_cutoff_freq = val - 64;
			if (update_now)
			//	update_channel_filter2(ch);
				recompute_channel_filter(ch);
		break;
	case NRPN_ADDR_0125:	/* HPF Filter Resonance */
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN HPF Filter Resonance (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].hpf_param_resonance = val - 64;
			if (update_now)
			//	update_channel_filter2(ch);
				recompute_channel_filter(ch);
		break;
	case NRPN_ADDR_0130:	/* EQ BASS */
		if (opt_eq_control) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ BASS (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].eq_xg.bass = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0131:	/* EQ TREBLE */
		if (opt_eq_control) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ TREBLE (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].eq_xg.treble = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0132:	/* EQ MID-BASS */
		if (opt_eq_control) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ MID-BASS (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].eq_xg.mid_bass = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0133:	/* EQ MID-TREBLE */
		if (opt_eq_control) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ MID-TREBLE (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].eq_xg.mid_treble = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0134:	/* EQ BASS frequency */
		if (opt_eq_control) {
			if(val < 4) {val = 4;}
			else if(val > 40) {val = 40;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ BASS frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].eq_xg.bass_freq = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0135:	/* EQ TREBLE frequency */
		if (opt_eq_control) {
			if(val < 28) {val = 28;}
			else if(val > 58) {val = 58;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ TREBLE frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].eq_xg.treble_freq = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0136:	/* EQ MID-BASS frequency */
		if (opt_eq_control) {
			if(val < 0x0E) {val = 0x0E;}
			else if(val > 0x36) {val = 0x36;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ MID-BASS frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].eq_xg.mid_bass_freq = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0137:	/* EQ MID-TREBLE frequency */
		if (opt_eq_control) {
			if(val < 0x0E) {val = 0x0E;}
			else if(val > 0x36) {val = 0x36;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ MID-TREBLE frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].eq_xg.mid_treble_freq = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0138:	/* EQ BASS Q */
		if (opt_eq_control) {
			if(val < 0x01) {val = 0x01;}
			else if(val > 0x78) {val = 0x78;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ BASS Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].eq_xg.bass_q = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0139:	/* EQ TREBLE Q */
		if (opt_eq_control) {
			if(val < 0x01) {val = 0x01;}
			else if(val > 0x78) {val = 0x78;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ TREBLE Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].eq_xg.treble_q = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_013A:	/* EQ MID-BASS Q */
		if (opt_eq_control) {
			if(val < 0x01) {val = 0x01;}
			else if(val > 0x78) {val = 0x78;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ MID-BASS Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].eq_xg.mid_bass_q = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_013B:	/* EQ MID-TREBLE Q */
		if (opt_eq_control) {
			if(val < 0x01) {val = 0x01;}
			else if(val > 0x78) {val = 0x78;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ MID-TREBLE Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].eq_xg.mid_treble_q = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_013C:	/* EQ BASS shape */
		if (opt_eq_control) {
			if(!val) {
				channel[ch].eq_xg.bass_shape = 0x00;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ BASS shape (CH:%d shalving)", ch);			
			}else{
				channel[ch].eq_xg.bass_shape = 0x01;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ BASS shape (CH:%d peaking)", ch);
			}
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_013D:	/* EQ TREBLE shape */
		if (opt_eq_control) {
			if(!val) {
				channel[ch].eq_xg.treble_shape =0x00;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ TREBLE shape (CH:%d shalving)", ch);			
			}else{
				channel[ch].eq_xg.treble_shape =0x01;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN EQ TREBLE shape (CH:%d peaking)", ch);
			}
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0163:	/* Attack Time */
		if (opt_tva_attack) {set_envelope_time(ch, val, EG_ATTACK);}
		break;
	case NRPN_ADDR_0164:	/* EG Decay Time */
		if (opt_tva_decay) {set_envelope_time(ch, val, EG_DECAY);}
		break;
	case NRPN_ADDR_0166:	/* EG Release Time */
		if (opt_tva_release) {set_envelope_time(ch, val, EG_RELEASE);}
		break;
	case NRPN_ADDR_1400:	/* Drum Filter Cutoff (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Drum Instrument Filter Cutoff (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_cutoff_freq = val - 64;
		break;
	case NRPN_ADDR_1500:	/* Drum Filter Resonance (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Drum Instrument Filter Resonance (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_resonance = val - 64;
		break;
	case NRPN_ADDR_1600:	/* Drum EG Attack Time (XG) */
		drumflag = 1;
		if (opt_tva_attack) {
			val = val & 0x7f;
			note = channel[ch].lastlrpn;
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			val	-= 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Drum Instrument Attack Time (CH:%d NOTE:%d VAL:%d)",
					ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[EG_ATTACK] = val;
		}
		break;
	case NRPN_ADDR_1700:	/* Drum EG Decay Time (XG) */
		drumflag = 1;
		if (opt_tva_decay) {
			val = val & 0x7f;
			note = channel[ch].lastlrpn;
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			val	-= 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"NRPN Drum Instrument Decay Time (CH:%d NOTE:%d VAL:%d)",
					ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[EG_DECAY1] =
				channel[ch].drums[note]->drum_envelope_rate[EG_DECAY2] = val;
		}
		break;
	case NRPN_ADDR_1800:	/* Coarse Pitch of Drum (GS) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->coarse = val - 64;
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
			"NRPN Drum Instrument Pitch Coarse (CH:%d NOTE:%d VAL:%d)",
			ch, note, channel[ch].drums[note]->coarse);
		break;
	case NRPN_ADDR_1900:	/* Fine Pitch of Drum (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->fine = val - 64;
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Drum Instrument Pitch Fine (CH:%d NOTE:%d VAL:%d)",
				ch, note, channel[ch].drums[note]->fine);
		break;
	case NRPN_ADDR_1A00:	/* Level of Drum */	 
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Drum Instrument TVA Level (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_level =
				calc_drum_tva_level(ch, note, val);
		break;
	case NRPN_ADDR_1C00:	/* Panpot of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if(val == 0) {
			val = int_rand(128);
			channel[ch].drums[note]->pan_random = 1;
		} else
			channel[ch].drums[note]->pan_random = 0;
		channel[ch].drums[note]->drum_panning = val;
		//if (update_now && adjust_panning_immediately
		//		&& ! channel[ch].pan_random)
		//	adjust_drum_panning(ch, note);
		break;
	case NRPN_ADDR_1D00:	/* Reverb Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Reverb Send Level of Drum (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		if (channel[ch].drums[note]->reverb_level != val) {
			if(channel[ch].drum_effect_flag)
				channel[ch].drum_effect_flag |= 0x4; // update effect
		}
		channel[ch].drums[note]->reverb_level = val;
		break;
	case NRPN_ADDR_1E00:	/* Chorus Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Chorus Send Level of Drum (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		if (channel[ch].drums[note]->chorus_level != val) {
			if(channel[ch].drum_effect_flag)
				channel[ch].drum_effect_flag |= 0x4; // update effect
		}
		channel[ch].drums[note]->chorus_level = val;
		
		break;
	case NRPN_ADDR_1F00:	/* Variation Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Delay Send Level of Drum (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		if (channel[ch].drums[note]->delay_level != val) {
			if(channel[ch].drum_effect_flag)
				channel[ch].drum_effect_flag |= 0x4; // update effect
		}
		channel[ch].drums[note]->delay_level = val;
		break;
	case NRPN_ADDR_2400:	/* Drum HPF Cutoff (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Drum Instrument HPF Cutoff (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_hpf_cutoff_freq = val - 64;
		break;
	case NRPN_ADDR_2500:	/* Drum HPF Resonance (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"NRPN Drum Instrument HPF Resonance (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_hpf_resonance = val - 64;
		break;
	case NRPN_ADDR_3000:	/* Drum EQ BASS */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ BASS (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].drums[note]->eq_xg.bass = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3100:	/* Drum EQ TREBLE */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);		
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ TREBLE (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].drums[note]->eq_xg.treble = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3200:	/* Drum EQ MID-BASS */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ MID-BASS (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].drums[note]->eq_xg.mid_bass = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3300:	/* Drum EQ MID-TREBLE */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);		
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ MID-TREBLE (CH:%d %.2f dB)", ch, DIV_64 * 12.0 * (double)(val - 0x40));
			channel[ch].drums[note]->eq_xg.mid_treble = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3400:	/* Drum EQ BASS frequency */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			if(val < 4) {val = 4;}
			else if(val > 40) {val = 40;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ BASS frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].drums[note]->eq_xg.bass_freq = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3500:	/* Drum EQ TREBLE frequency */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->eq_xg.treble_freq = val;
		if (opt_eq_control && opt_drum_effect) {
			if(val < 28) {val = 28;}
			else if(val > 58) {val = 58;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ TREBLE frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].drums[note]->eq_xg.treble_freq = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3600:	/* Drum EQ MID-BASS frequency */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			if(val < 0x0E) {val = 0x0E;}
			else if(val > 0x36) {val = 0x36;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ MID-BASS frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].drums[note]->eq_xg.mid_bass_freq = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3700:	/* Drum EQ MID-TREBLE frequency */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			if(val < 0x0E) {val = 0x0E;}
			else if(val > 0x36) {val = 0x36;}
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ MID-TREBLE frequency (CH:%d %d Hz)", ch, (int32)eq_freq_table_xg[val]);
			channel[ch].drums[note]->eq_xg.mid_treble_freq = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3800:	/* Drum EQ BASS Q */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ BASS Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].drums[note]->eq_xg.bass_q = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3900:	/* Drum EQ TREBLE Q */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);		
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ TREBLE Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].drums[note]->eq_xg.treble_q = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3A00:	/* Drum EQ MID-BASS Q */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->eq_xg.mid_bass = val;	
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ MID-BASS Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].drums[note]->eq_xg.mid_bass_q = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3B00:	/* Drum EQ MID-TREBLE Q */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);		
		if (opt_eq_control && opt_drum_effect) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ MID-TREBLE Q (CH:%d %.2f)", ch, (double)val * DIV_10);
			channel[ch].drums[note]->eq_xg.mid_treble_q = val;
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3C00:	/* Drum EQ BASS shape */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (opt_eq_control && opt_drum_effect) {
			if(!val){
				channel[ch].drums[note]->eq_xg.bass_shape = 0x0;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ BASS shape (CH:%d shelving)", ch);
			}else{
				channel[ch].drums[note]->eq_xg.bass_shape = 0x1;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ BASS shape (CH:%d peaking)", ch);
			}
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case NRPN_ADDR_3D00:	/* Drum EQ TREBLE shape */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);		
		if (opt_eq_control && opt_drum_effect) {
			if(!val){
				channel[ch].drums[note]->eq_xg.treble_shape = 0x0;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ TREBLE shape (CH:%d shelving)", ch);
			}else{
				channel[ch].drums[note]->eq_xg.treble_shape = 0x1;
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"NRPN Drum EQ TREBLE shape (CH:%d peaking)", ch);
			}
			recompute_part_eq_xg(&(channel[ch].drums[note]->eq_xg));
		}
		break;
	case RPN_ADDR_0000:		/* Pitch bend sensitivity */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "RPN Pitch Bend Sensitivity (CH:%d VALUE:%d)", ch, val);
		/* for mod2mid.c, arpeggio */
		if (IS_CURRENT_MOD_FILE){
			if(channel[ch].rpnmap[RPN_ADDR_0000] > 0x7F)
				channel[ch].rpnmap[RPN_ADDR_0000] = 0x7F;
		}else if (play_system_mode == XG_SYSTEM_MODE && channel[ch].rpnmap[RPN_ADDR_0000] > 0x7F){
			if(channel[ch].rpnmap[RPN_ADDR_0000] < 0xE8) // 0x100 -24
				channel[ch].rpnmap[RPN_ADDR_0000] = 0xE8; // -24
		}else{
			if(channel[ch].rpnmap[RPN_ADDR_0000] > 24)
				channel[ch].rpnmap[RPN_ADDR_0000] = 24;
		}
		channel[ch].rpnmap[RPN_ADDR_0040] = channel[ch].rpnmap[RPN_ADDR_0000]; // update low control
		break;
	case RPN_ADDR_0001:		/* Master Fine Tuning */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"RPN Master Fine Tuning (CH:%d VALUE:%d)", ch, val);
		break;
	case RPN_ADDR_0002:		/* Master Coarse Tuning */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"RPN Master Coarse Tuning (CH:%d VALUE:%d)", ch, val);
		break;
	case RPN_ADDR_0003:		/* Tuning Program Select */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"RPN Tuning Program Select (CH:%d VALUE:%d)", ch, val);
		for (i = 0; i < upper_voices; i++)
			if (voice[i].status != VOICE_FREE) {
				voice[i].temper_instant = 1;
			//	recompute_freq(i);
				recompute_voice_pitch(i);
			}
		break;
	case RPN_ADDR_0004:		/* Tuning Bank Select */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"RPN Tuning Bank Select (CH:%d VALUE:%d)", ch, val);
		for (i = 0; i < upper_voices; i++)
			if (voice[i].status != VOICE_FREE) {
				voice[i].temper_instant = 1;
			//	recompute_freq(i);
				recompute_voice_pitch(i);
			}
		break;
	case RPN_ADDR_0005:		/* Modulation Depth Range */
	//	channel[ch].mod.lfo1_pitch_depth = (((int32)channel[ch].rpnmap[RPN_ADDR_0005] << 7) | channel[ch].rpnmap_lsb[RPN_ADDR_0005]) * 100 / 128;
		if (play_system_mode == SD_SYSTEM_MODE){
			channel[ch].mod.lfo1_pitch_depth = 
				(double)(((int32)channel[ch].rpnmap[RPN_ADDR_0005] << 8) | channel[ch].rpnmap_lsb[RPN_ADDR_0005])
				* 0.03662109375; // cent 600/16384 max0x4000
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"RPN Modulation Depth Range (CH:%d MSB:%d LSB:%d cent:%.2f)", ch, 
					channel[ch].rpnmap[RPN_ADDR_0005], channel[ch].rpnmap_lsb[RPN_ADDR_0005], channel[ch].mod.lfo1_pitch_depth);
		}else{ // GM2_SYSTEM_MODE
			channel[ch].mod.lfo1_pitch_depth = (double)(channel[ch].rpnmap[RPN_ADDR_0005] * 100)
				+ (double)channel[ch].rpnmap_lsb[RPN_ADDR_0005] * 100.0 * DIV_128; // cent
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"RPN Modulation Depth Range (CH:%d MSB:%d LSB:%d cent:%.2f)", ch, 
					channel[ch].rpnmap[RPN_ADDR_0005], channel[ch].rpnmap_lsb[RPN_ADDR_0005], channel[ch].mod.lfo1_pitch_depth);
		}
		break;
	case RPN_ADDR_0040:		/* Pitch bend sensitivity low control */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "RPN Pitch Bend Sensitivity Low Control (CH:%d VALUE:%d)", ch, val);
		if (play_system_mode == XG_SYSTEM_MODE && channel[ch].rpnmap[RPN_ADDR_0040] > 0x7F){
			if(channel[ch].rpnmap[RPN_ADDR_0040] < 0xE8) // 0x100 -24
				channel[ch].rpnmap[RPN_ADDR_0040] = 0xE8; // -24
		}else{
			if(channel[ch].rpnmap[RPN_ADDR_0040] > 24)
				channel[ch].rpnmap[RPN_ADDR_0040] = 24;
		}
		break;
	case RPN_ADDR_7F7F:		/* RPN reset */
		channel[ch].rpn_7f7f_flag = 1;
		break;
	case RPN_ADDR_FFFF:		/* RPN initialize */
		/* All reset to defaults */
		channel[ch].rpn_7f7f_flag = 0;
		memset(channel[ch].rpnmap, 0, sizeof(channel[ch].rpnmap));
		channel[ch].lastlrpn = channel[ch].lastmrpn = 0;
		channel[ch].nrpn = -1;
		channel[ch].rpnmap[RPN_ADDR_0000] = 2;
		channel[ch].rpnmap[RPN_ADDR_0001] = 0x40;
		channel[ch].rpnmap[RPN_ADDR_0002] = 0x40;
		channel[ch].rpnmap_lsb[RPN_ADDR_0005] = 0x40;
		channel[ch].rpnmap[RPN_ADDR_0005] = 0;	/* +- 50 cents */
		channel[ch].rpnmap[RPN_ADDR_0040] = 2; // = RPN_ADDR_0000 = -2*-1
		break;
	}
	drumflag = 0;
	if (drumflag && midi_drumpart_change(ch, 1)) {
		midi_program_change(ch, channel[ch].program);
		if (update_now)
			ctl_prog_event(ch);
	}
}

/*
cbc値を通常ctrl値に変換 0~127 -> -64~63 -> -63~63 -> -127~127
ここで変換するのはget_midi_controller_amp()~等をそのまま使用するため 
*/
static int calc_cbc_val(int val)
{
	const int32 coef = (double)127 / (double)63 * (double)(1L << 24); // 127/63
	if(val == 0x40) return 0;
	val -= 0x40;
	if(val < -63) val = 63;
	return imuldiv24(val, coef);
}

/*
bend値を通常ctrl値に変換 0~16383 -> -8192~+8191 -> -8191~+8191 -> -127~127
ここで変換するのはget_midi_controller_amp()~等をそのまま使用するため 
*/
int calc_bend_val(int val)
{
	const int32 coef = (double)127 / (double)8191 * (double)(1L << 16); // 127/8191
	if(val == 0x2000) return 0;
	val -= 0x2000;
	if(val < -8191) val = 8191;
	return imuldiv16(val, coef);
}

static void process_channel_control_event(MidiEvent *ev)
{
	int ch, cc;

	if(!opt_channel_pressure)
		return;
	if(ev->type < ME_TONE_BANK_MSB || ev->type > ME_UNDEF_CTRL_CHNG)
		return; // not cc
	ch = ev->channel;
	cc = ev->b;	// original ctrl change number (if ev->type==ctrl_change
	/*! adjust channel control 1 (CC1) */
	if(channel[ch].cc1.num != -1 && channel[ch].cc1.num == cc){
		if(channel[ch].cc1.mode){ // CBC mode
			channel[ch].cc1.val = calc_cbc_val(ev->a);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC1(CBC) (CH:%d VAL:%d CVT:%d)",ch,ev->a,channel[ch].cc1.val);
		}else{
			channel[ch].cc1.val = ev->a;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC1 (CH:%d VAL:%d)",ch,ev->a);
		}
		recompute_channel_lfo(ch);
		recompute_channel_amp(ch);
		recompute_channel_filter(ch);
		recompute_channel_pitch(ch);
		control_effect_xg(ch);
	}
	/*! adjust channel control 2 (CC2) */
	if(channel[ch].cc2.num != -1 && channel[ch].cc2.num == cc){
		if(channel[ch].cc2.mode){ // CBC mode
			channel[ch].cc2.val = calc_cbc_val(ev->a);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC2(CBC) (CH:%d VAL:%d CVT:%d)",ch,ev->a,channel[ch].cc2.val);
		}else{
			channel[ch].cc2.val = ev->a;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC2 (CH:%d VAL:%d)",ch,ev->a);
		}
		recompute_channel_lfo(ch);
		recompute_channel_amp(ch);
		recompute_channel_filter(ch);
		recompute_channel_pitch(ch);
		control_effect_xg(ch);
	}
	/*! adjust channel control 3 (CC3) */
	if(channel[ch].cc3.num != -1 && channel[ch].cc3.num == cc){
		if(channel[ch].cc3.mode){ // CBC mode
			channel[ch].cc3.val = calc_cbc_val(ev->a);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC3(CBC) (CH:%d VAL:%d CVT:%d)",ch,ev->a, channel[ch].cc3.val);
		}else{
			channel[ch].cc3.val = ev->a;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC3 (CH:%d VAL:%d)",ch,ev->a);
		}
		recompute_channel_lfo(ch);
		recompute_channel_amp(ch);
		recompute_channel_filter(ch);
		recompute_channel_pitch(ch);
		control_effect_xg(ch);
	}
	/*! adjust channel control 4 (CC4) */
	if(channel[ch].cc4.num != -1 && channel[ch].cc4.num == cc){
		if(channel[ch].cc4.mode){ // CBC mode
			channel[ch].cc4.val = calc_cbc_val(ev->a);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC4(CBC) (CH:%d VAL:%d CVT:%d)",ch,ev->a, channel[ch].cc4.val);
		}else{
			channel[ch].cc4.val = ev->a;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"CC4 (CH:%d VAL:%d)",ch,ev->a);
		}
		recompute_channel_lfo(ch);
		recompute_channel_amp(ch);
		recompute_channel_filter(ch);
		recompute_channel_pitch(ch);
		control_effect_xg(ch);
	}
}




static void seek_forward(int32 until_time)
{
    int32 i;
    int j, k, ch, orig_ch, port_ch, offset, layered;

    playmidi_seek_flag = 1;
    wrd_midi_event(WRD_START_SKIP, WRD_NOARG);
	while (MIDI_EVENT_TIME(current_event) < until_time) {
#ifndef SUPPRESS_CHANNEL_LAYER
		orig_ch = current_event->channel;
		layered = ! IS_SYSEX_EVENT_TYPE(current_event);
		for (j = 0; j < MAX_CHANNELS; j += 16) {
			port_ch = (orig_ch + j) % MAX_CHANNELS;
			offset = port_ch & ~0xf;
			for (k = offset; k < offset + 16; k++) {
				if (! layered && (j || k != offset))
					continue;
				if (layered) {
					if (! IS_SET_CHANNELMASK(
							channel[k].channel_layer, port_ch)
							|| channel[k].port_select != (orig_ch >> 4))
						continue;
					current_event->channel = k;
				}
#endif
	ch = current_event->channel;
		
	if(current_event->type >= ME_TONE_BANK_MSB && current_event->type <= ME_UNDEF_CTRL_CHNG)
		if(!get_rx(ch, RX_CONTROL_CHANGE)) 
			current_event->type = ME_NONE;

	process_channel_control_event(current_event);
	
	if(opt_insertion_effect){		
		switch(play_system_mode) {
		case GS_SYSTEM_MODE:
			control_effect_gs(current_event);
			break;
		case SD_SYSTEM_MODE:
			control_effect_sd(current_event);
			break;
		default:
			break;
		}
	}
	
	switch(current_event->type)
	{
	  case ME_PITCHWHEEL:
		if(!get_rx(ch, RX_PITCH_BEND)) break;
		{
			int tmp = (int)current_event->a + (int)current_event->b * 128;
			channel[ch].pitchbend = tmp;
			channel[ch].bend.val = calc_bend_val(tmp);
		}
		adjust_pitch(ch);
		control_effect_xg(ch);
	    break;

	  case ME_MAINVOLUME:
		if(!get_rx(ch, RX_VOLUME)) break;
	    channel[ch].volume = current_event->a;
		adjust_volume(ch);
	    break;

	  case ME_MASTER_VOLUME:
	    master_volume_ratio =
		(int32)current_event->a + 256 * (int32)current_event->b;
		adjust_master_volume();
	    break;

	  case ME_PAN:
		if(!get_rx(ch, RX_VOLUME)) break;
	    channel[ch].panning = current_event->a;
	    channel[ch].pan_random = 0;
		if(!channel[ch].pan_random)
			adjust_panning(ch);
	    break;

	  case ME_EXPRESSION:		  
		if(!get_rx(ch, RX_PANPOT)) break;
	    channel[ch].expression=current_event->a;
		adjust_volume(ch);
	    break;

	  case ME_KEYPRESSURE:
		if(!get_rx(ch, RX_POLY_PRESSURE)) break;
	    adjust_pressure(current_event);
	    break;

	  case ME_CHANNEL_PRESSURE:
		if(!get_rx(ch, RX_PROGRAM_CHANGE)) break;
	    adjust_channel_pressure(current_event);
		control_effect_xg(ch);
	    break;
		
	  case ME_PROGRAM:
		if(!get_rx(ch, RX_PROGRAM_CHANGE)) break;
	    midi_program_change(ch, current_event->a);
	    break;

///r
	  case ME_SUSTAIN:		
		if(!get_rx(ch, RX_HOLD1)) break;		  	  
		if (channel[ch].damper_mode == 0) {	/* half-damper is not allowed. */
			if (current_event->a >= 64) {
				if(channel[ch].sustain == 0){
					channel[ch].sustain = 127;
				}
			}else {
				channel[ch].sustain = 0;
			}
		}else{
			if(channel[ch].sustain != current_event->a){
				channel[ch].sustain = current_event->a;
			}
		}
#if 0
		  channel[ch].sustain = current_event->a;
		  if (channel[ch].damper_mode == 0) {	/* half-damper is not allowed. */
			  if (channel[ch].sustain >= 64) {channel[ch].sustain = 127;}
			  else {channel[ch].sustain = 0;}
		  }
#endif
	    break;

	  case ME_SOSTENUTO:
		if(!get_rx(ch, RX_SOSTENUTO)) break;	
		  channel[ch].sostenuto = (current_event->a >= 64);
	    break;

	  case ME_LEGATO_FOOTSWITCH:
		if(opt_portamento){
        channel[ch].legato = (current_event->a >= 64);
		}
	    break;

      case ME_HOLD2:
        break;

	  case ME_FOOT:
	    break;

	  case ME_BREATH:
	    break;

	  case ME_BALANCE:
	    break;

	  case ME_RESET_CONTROLLERS:
	    reset_controllers(ch);
	    break;

	  case ME_TONE_BANK_MSB:
		if(!get_rx(ch, RX_BANK_SELECT)) break;
	    channel[ch].bank_msb = current_event->a;
	    break;

	  case ME_TONE_BANK_LSB:
		if(!get_rx(ch, RX_BANK_SELECT_LSB)) break;
	    channel[ch].bank_lsb = current_event->a;
	    break;

	  case ME_MODULATION_WHEEL:
		if(!get_rx(ch, RX_MODULATION)) break;
		if(opt_modulation_wheel){
			channel[ch].mod.val = current_event->a;
			update_modulation_wheel(ch);
			control_effect_xg(ch);
		}
	    break;

	  case ME_PORTAMENTO_TIME_MSB:
		if(opt_portamento){
			channel[ch].portamento_time_msb = current_event->a;
		}
	    break;

	  case ME_PORTAMENTO_TIME_LSB:
		if(opt_portamento){
			channel[ch].portamento_time_lsb = current_event->a;
		}
	    break;

	  case ME_PORTAMENTO:
		if(!get_rx(ch, RX_PORTAMENTO)) break;
		if(opt_portamento){
			channel[ch].portamento = (current_event->a >= 64);
		}
	    break;

      case ME_PORTAMENTO_CONTROL:
		if(opt_portamento){
			channel[ch].portamento_control = current_event->a;
		}
		break;

	  case ME_MONO:
	    channel[ch].mono = 1;
	    break;

	  case ME_POLY:
	    channel[ch].mono = 0;
	    break;

	  case ME_SOFT_PEDAL:
		if(!get_rx(ch, RX_SOFT)) break;
		  //if(opt_lpf_def) {
			  channel[ch].soft_pedal = current_event->a;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Soft Pedal (CH:%d VAL:%d)",ch,channel[ch].soft_pedal);
		  //}
		  break;

	  case ME_HARMONIC_CONTENT:
		  //if(opt_lpf_def) {
			  channel[ch].param_resonance = current_event->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Harmonic Content (CH:%d VAL:%d)",ch,channel[ch].param_resonance);
		//	  update_channel_filter(ch);//elion add.
//			  recompute_voice_filter(ch);// elion add.
			  recompute_channel_filter(ch);
		  //}
		  break;

	  case ME_BRIGHTNESS:
		  //if(opt_lpf_def) {
			  channel[ch].param_cutoff_freq = current_event->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Brightness (CH:%d VAL:%d)",ch,channel[ch].param_cutoff_freq);
		//	  update_channel_filter(ch);//elion add.
//			  recompute_voice_filter(ch);// elion add.
			  recompute_channel_filter(ch);
		  //}
		  break;

	  case ME_VIBRATO_RATE:
		if (opt_nrpn_vibrato) {
			channel[ch].param_vibrato_rate = current_event->a - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,"Vibrato Rate (CH:%d VAL:%d)",ch,current_event->a - 64);
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;

	  case ME_VIBRATO_DEPTH:
		if (opt_nrpn_vibrato) {
			channel[ch].param_vibrato_depth = current_event->a - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,"Vibrato Depth (CH:%d VAL:%d)",ch,current_event->a - 64);
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;

	  case ME_VIBRATO_DELAY:
		if (opt_nrpn_vibrato) {
			channel[ch].param_vibrato_delay = current_event->a - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,"Vibrato Delay (CH:%d VAL:%d)",ch,current_event->a - 64);
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;

	    /* RPNs */
	  case ME_NRPN_LSB:
		if(!get_rx(ch, RX_NRPN)) break;
		  channel[ch].rpn_7f7f_flag = 0;
	    channel[ch].lastlrpn = current_event->a;
	    channel[ch].nrpn = 1;
	    break;
	  case ME_NRPN_MSB:
		if(!get_rx(ch, RX_NRPN)) break;
		  channel[ch].rpn_7f7f_flag = 0;
	    channel[ch].lastmrpn = current_event->a;
	    channel[ch].nrpn = 1;
	    break;
	  case ME_RPN_LSB:
		if(!get_rx(ch, RX_RPN)) break;
		  channel[ch].rpn_7f7f_flag = 0;
	    channel[ch].lastlrpn = current_event->a;
	    channel[ch].nrpn = 0;
	    break;
	  case ME_RPN_MSB:
		if(!get_rx(ch, RX_RPN)) break;
		  channel[ch].rpn_7f7f_flag = 0;
	    channel[ch].lastmrpn = current_event->a;
	    channel[ch].nrpn = 0;
	    break;
	  case ME_RPN_INC:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		if(channel[ch].rpnmap[i] < 127)
		    channel[ch].rpnmap[i]++;
		update_rpn_map(ch, i, 0);
	    }
	    break;
	case ME_RPN_DEC:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		if(channel[ch].rpnmap[i] > 0)
		    channel[ch].rpnmap[i]--;
		update_rpn_map(ch, i, 0);
	    }
	    break;
	  case ME_DATA_ENTRY_MSB:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		channel[ch].rpnmap[i] = current_event->a;
		update_rpn_map(ch, i, 0);
	//	  update_channel_filter(ch);// elion add.
//		  recompute_voice_filter(ch);// elion add.
	    }
	    break;
	  case ME_DATA_ENTRY_LSB:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		channel[ch].rpnmap_lsb[i] = current_event->a;
	    }
	    break;

	  case ME_REVERB_EFFECT:
		  if (opt_reverb_control) {
			if (ISDRUMCHANNEL(ch) && get_reverb_level(ch) != current_event->a) {
				if(channel[ch].drum_effect_flag)
					channel[ch].drum_effect_flag |= 0x4; // update effect
			}
			set_reverb_level(ch, current_event->a);
		  }
	    break;

	  case ME_CHORUS_EFFECT:
		if(opt_chorus_control == 1) {
			if (ISDRUMCHANNEL(ch) && channel[ch].chorus_level != current_event->a) {
				if(channel[ch].drum_effect_flag)
					channel[ch].drum_effect_flag |= 0x4; // update effect
			}
			channel[ch].chorus_level = current_event->a;
		} else {
			channel[ch].chorus_level = -opt_chorus_control;
		}

		if(current_event->a) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send (CH:%d LEVEL:%d)",ch,current_event->a);
		}
		break;

	  case ME_TREMOLO_EFFECT:
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Tremolo Send (CH:%d LEVEL:%d)",ch,current_event->a);
		break;

	  case ME_CELESTE_EFFECT:
		if(opt_delay_control) {
			if (ISDRUMCHANNEL(ch) && channel[ch].delay_level != current_event->a) {
				if(channel[ch].drum_effect_flag)
					channel[ch].drum_effect_flag |= 0x4; // update effect
			}
			channel[ch].delay_level = current_event->a;
			if (play_system_mode == XG_SYSTEM_MODE) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Send (CH:%d LEVEL:%d)",ch,current_event->a);
			} else {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Variation Send (CH:%d LEVEL:%d)",ch,current_event->a);
			}
		}
	    break;

	  case ME_ATTACK_TIME:
	  	if(!opt_tva_attack) { break; }
		set_envelope_time(ch, current_event->a, EG_ATTACK);
		break;

	  case ME_DECAY_TIME:
	  	if(!opt_tva_decay) { break; }
		set_envelope_time(ch, current_event->a, EG_DECAY);
		break;

	  case ME_RELEASE_TIME:
	  	if(!opt_tva_release) { break; }
		set_envelope_time(ch, current_event->a, EG_RELEASE);
		break;

	  case ME_PHASER_EFFECT:
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Phaser Send (CH:%d LEVEL:%d)",ch,current_event->a);
		break;

	  case ME_RANDOM_PAN:
	    channel[ch].panning = int_rand(128);
	    channel[ch].pan_random = 1;
	    break;

	  case ME_SET_PATCH:
	    i = channel[ch].special_sample = current_event->a;
	    if(special_patch[i] != NULL)
		special_patch[i]->sample_offset = 0;		
	    break;

	  case ME_TEMPO:
	    current_play_tempo = ch +
		current_event->b * 256 + current_event->a * 65536;
	    break;

	  case ME_RESET:
	    change_system_mode(current_event->a);
	    reset_midi(0);
	    break;

	  case ME_PATCH_OFFS:
	    i = channel[ch].special_sample;
	    if(special_patch[i] != NULL)
		special_patch[i]->sample_offset =
		    (current_event->a | 256 * current_event->b);
	    break;

	  case ME_WRD:
	    wrd_midi_event(ch, current_event->a | 256 * current_event->b);
	    break;

	  case ME_SHERRY:
	    wrd_sherry_event(ch |
			     (current_event->a<<8) |
			     (current_event->b<<16));
	    break;

	  case ME_DRUMPART:
		if(play_system_mode == XG_SYSTEM_MODE && current_event->a)
			channel[ch].bank_msb = 127; // Drum kit
	    if(midi_drumpart_change(ch, current_event->a))
		midi_program_change(ch, channel[ch].program);	
	    break;

	  case ME_KEYSHIFT:
	    channel[ch].key_shift = (int)current_event->a - 0x40;
	    break;

	case ME_KEYSIG:
		if (opt_init_keysig != 8)
			break;
		current_keysig = current_event->a + current_event->b * 16;
		break;

	case ME_MASTER_TUNING:
		set_master_tuning((current_event->b << 8) | current_event->a);
		adjust_pitch(ch);
		break;

	case ME_SCALE_TUNING:
		channel[ch].scale_tuning[current_event->a] = current_event->b;
		adjust_pitch(ch);
		break;

	case ME_BULK_TUNING_DUMP:
		set_single_note_tuning(ch, current_event->a, current_event->b, 0);
		break;

	case ME_SINGLE_NOTE_TUNING:
		set_single_note_tuning(ch, current_event->a, current_event->b, 0);
		break;

	case ME_TEMPER_KEYSIG:
		current_temper_keysig = (current_event->a + 8) % 32 - 8;
		temper_adj = ((current_event->a + 8) & 0x20) ? 1 : 0;
		adjust_pitch(ch);
		break;

	case ME_TEMPER_TYPE:
		channel[ch].temper_type = current_event->a;
		adjust_pitch(ch);
		break;

	case ME_MASTER_TEMPER_TYPE:
		for (i = 0; i < MAX_CHANNELS; i++)
			channel[i].temper_type = current_event->a;
		adjust_pitch(ch);
		break;

	case ME_USER_TEMPER_ENTRY:
		set_user_temper_entry(ch, current_event->a, current_event->b);
		adjust_pitch(ch);
		break;

	  case ME_SYSEX_LSB:
	    process_sysex_event(ME_SYSEX_LSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_MSB:
	    process_sysex_event(ME_SYSEX_MSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_GS_LSB:
	    process_sysex_event(ME_SYSEX_GS_LSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_GS_MSB:
	    process_sysex_event(ME_SYSEX_GS_MSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_XG_LSB:
	    process_sysex_event(ME_SYSEX_XG_LSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_XG_MSB:
	    process_sysex_event(ME_SYSEX_XG_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_SD_LSB:
		process_sysex_event(ME_SYSEX_SD_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_SD_MSB:
		process_sysex_event(ME_SYSEX_SD_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_SD_HSB:
		process_sysex_event(ME_SYSEX_SD_HSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_EOT:
	    current_sample = current_event->time;
	    playmidi_seek_flag = 0;
	    return;
	}
#ifndef SUPPRESS_CHANNEL_LAYER
			}
		}
		current_event->channel = orig_ch;
#endif
	current_event++;
    }
    wrd_midi_event(WRD_END_SKIP, WRD_NOARG);

    playmidi_seek_flag = 0;
    if(current_event != event_list)
	current_event--;
    current_sample = until_time;
}
///r
static void skip_to(int32 until_time)
{
  int ch;

  trace_flush();
  current_event = NULL;

  if (current_sample > until_time)
    current_sample=0;
///r
//  change_system_mode(DEFAULT_SYSTEM_MODE);
  reset_midi(0);

  buffered_count=0;
  buffer_pointer=common_buffer;
  current_event=event_list;
  current_play_tempo = 500000; /* 120 BPM */

  if (until_time)
    seek_forward(until_time);
  ctl_mode_event(CTLE_RESET, 0, 0, 0);
  for(ch = 0; ch < MAX_CHANNELS; ch++){
      channel[ch].lasttime = current_sample;
	  redraw_controllers(ch);
	  ctl_prog_event(ch);
  }
  trace_offset(until_time);
#ifdef SUPPORT_SOUNDSPEC
  soundspec_update_wave(NULL, 0);
#endif /* SUPPORT_SOUNDSPEC */
}

static int32 sync_restart(int only_trace_ok)
{
    int32 cur;

    cur = current_trace_samples();
    if(cur == -1)
    {
	if(only_trace_ok)
	    return -1;
	cur = current_sample;
    }
    aq_flush(1);
    skip_to(cur);
    return cur;
}

static int playmidi_change_rate(int32 rate, int restart)
{
    int arg;

    if(rate == play_mode->rate)
	return 1; /* Not need to change */

    if(rate < MIN_OUTPUT_RATE || rate > MAX_OUTPUT_RATE)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Out of sample rate: %d", rate);
	return -1;
    }

    if(restart)
    {
	if((midi_restart_time = current_trace_samples()) == -1)
	    midi_restart_time = current_sample;
    }
    else
	midi_restart_time = 0;

    arg = (int)rate;
    if(play_mode->acntl(PM_REQ_RATE, &arg) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Can't change sample rate to %d", rate);
	return -1;
    }

    aq_flush(1);
    aq_setup();
    aq_set_soft_queue(-1.0, -1.0);
    free_instruments(1);
#ifdef SUPPORT_SOUNDSPEC
    soundspec_reinit();
#endif /* SUPPORT_SOUNDSPEC */
    return 0;
}

void playmidi_output_changed(int play_state)
{
    if(target_play_mode == NULL)
	return;
    if(play_mode != NULL && play_mode->close_output)
	play_mode->close_output();
    play_mode = target_play_mode;

    if(play_state == 0)
    {
	/* Playing */
	if((midi_restart_time = current_trace_samples()) == -1)
	    midi_restart_time = current_sample;
    }
    else /* Not playing */
	midi_restart_time = 0;

    if(play_state != 2)
    {
	aq_flush(1);
	aq_setup();
	aq_set_soft_queue(-1.0, -1.0);
	clear_magic_instruments();
    }
    free_instruments(1);
#ifdef SUPPORT_SOUNDSPEC
    soundspec_reinit();
#endif /* SUPPORT_SOUNDSPEC */
    target_play_mode = NULL;
}

int check_apply_control(void)
{
    int rc;
    ptr_size_t val = 0;

    if(file_from_stdin)
	return RC_NONE;
    rc = ctl->read(&val);
    switch(rc)
    {
      case RC_CHANGE_VOLUME:
	if (val>0 || output_amplification > -val)
	    output_amplification += val;
	else
	    output_amplification=0;
	if (output_amplification > MAX_AMPLIFICATION)
	    output_amplification=MAX_AMPLIFICATION;
//	adjust_amplification();
	change_output_volume(output_amplification);
	ctl_mode_event(CTLE_MASTER_VOLUME, 0, output_amplification, 0);
	break;
      case RC_SYNC_RESTART:
	aq_flush(1);
	break;
      case RC_TOGGLE_PAUSE:
	play_pause_flag = !play_pause_flag;
	ctl_pause_event(play_pause_flag, 0);
	return RC_NONE;
      case RC_TOGGLE_SNDSPEC:
#ifdef SUPPORT_SOUNDSPEC
	if(view_soundspec_flag)
	    close_soundspec();
	else
	    open_soundspec();
	if(view_soundspec_flag || ctl_speana_flag)
	    soundspec_update_wave(NULL, -1);
	return RC_NONE;
      case RC_TOGGLE_CTL_SPEANA:
	ctl_speana_flag = !ctl_speana_flag;
	if(view_soundspec_flag || ctl_speana_flag)
	    soundspec_update_wave(NULL, -1);
#endif /* SUPPORT_SOUNDSPEC */
	return RC_NONE;
      case RC_CHANGE_RATE:
	if(playmidi_change_rate(val, 0))
	    return RC_NONE;
	return RC_RELOAD;
      case RC_OUTPUT_CHANGED:
	playmidi_output_changed(1);
	return RC_RELOAD;
    }
    return rc;
}

static void voice_increment(int n)
{
    int i;
    for(i = 0; i < n; i++)
    {
	if(voices == max_voices)
	    break;
	voice[voices].status = VOICE_FREE;
	voice[voices].temper_instant = 0;
	voices++;
    }
    if(n > 0)
	ctl_mode_event(CTLE_MAXVOICES, 1, voices, 0);
}

static void voice_decrement(int n)
{
    int i, j, lowest;
    int32 lv, v;

    /* decrease voice */
    for(i = 0; i < n && voices > 0; i++)
    {
	voices--;
	if(voice[voices].status == VOICE_FREE)
	    continue;	/* found */

	for(j = 0; j < voices; j++)
	    if(voice[j].status == VOICE_FREE)
		break;
	if(j != voices)
	{
	    swap_voices(&voice[j], &voice[voices]);
	    continue;	/* found */
	}

	/* Look for the decaying note with the lowest volume */
	lv = 0x7FFFFFFF;
	lowest = -1;
	for(j = 0; j <= voices; j++)
	{
	    if(voice[j].status & ~(VOICE_ON | VOICE_DIE))
	    {
		v = voice[j].left_mix;
		if((voice[j].panned==PANNED_MYSTERY) &&
		   (voice[j].right_mix > v))
		    v = voice[j].right_mix;
		if(v < lv)
		{
		    lv = v;
		    lowest = j;
		}
	    }
	}

	if(lowest != -1)
	{
	    cut_notes++;
	    free_voice(lowest);
	    ctl_note_event(lowest);
	    swap_voices(&voice[lowest], &voice[voices]);
	}
	else
	    lost_notes++;
    }
    if(upper_voices > voices)
	upper_voices = voices;
    if(n > 0)
	ctl_mode_event(CTLE_MAXVOICES, 1, voices, 0);
}

/* EAW -- do not throw away good notes, stop decrementing */
static void voice_decrement_conservative(int n)
{
    int i, j, lowest, finalnv;
    int32 lv, v;

    /* decrease voice */
    finalnv = voices - n;
    for(i = 1; i <= n && voices > 0; i++)
    {
	if(voice[voices-1].status == VOICE_FREE) {
	    voices--;
	    continue;	/* found */
	}

	for(j = 0; j < finalnv; j++)
	    if(voice[j].status == VOICE_FREE)
		break;
	if(j != finalnv)
	{
	    swap_voices(&voice[j], &voice[voices - 1]);
	    voices--;
	    continue;	/* found */
	}

	/* Look for the decaying note with the lowest volume */
	lv = 0x7FFFFFFF;
	lowest = -1;
	for(j = 0; j < voices; j++)
	{
	    if(voice[j].status & ~(VOICE_ON | VOICE_DIE) &&
	       !(voice[j].sample->note_to_use &&
	         ISDRUMCHANNEL(voice[j].channel)))
	    {
		v = voice[j].left_mix;
		if((voice[j].panned==PANNED_MYSTERY) &&
		   (voice[j].right_mix > v))
		    v = voice[j].right_mix;
		if(v < lv)
		{
		    lv = v;
		    lowest = j;
		}
	    }
	}

	if(lowest != -1)
	{
	    voices--;
	    cut_notes++;
	    free_voice(lowest);
	    ctl_note_event(lowest);
	    swap_voices(&voice[lowest], &voice[voices]);
	}
	else break;
    }
    if(upper_voices > voices)
	upper_voices = voices;
}

void restore_voices(int save_voices)
{
#ifdef REDUCE_VOICE_TIME_TUNING
    static int old_voices = -1;
    if(old_voices == -1 || save_voices)
	old_voices = voices;
    else if (voices < old_voices)
	voice_increment(old_voices - voices);
    else
	voice_decrement(voices - old_voices);
#endif /* REDUCE_VOICE_TIME_TUNING */
}
	
///r
static int apply_controls(void)
{
    int rc, i, jump_flag = 0;
    ptr_size_t val = 0, cur;
    FLOAT_T r;
    ChannelBitMask tmp_chbitmask;

    /* ASCII renditions of CD player pictograms indicate approximate effect */
    do
    {
	switch(rc=ctl->read(&val))
	{
	  case RC_STOP:
	  case RC_QUIT:		/* [] */
	  case RC_LOAD_FILE:
	  case RC_NEXT:		/* >>| */
	  case RC_REALLY_PREVIOUS: /* |<< */
	  case RC_TUNE_END:	/* skip */
	    aq_flush(1);
	    return rc;

	  case RC_CHANGE_VOLUME:
	    if (val>0 || output_amplification > -val)
		output_amplification += val;
	    else
		output_amplification=0;
	    if (output_amplification > MAX_AMPLIFICATION)
		output_amplification=MAX_AMPLIFICATION;
	 //   adjust_amplification();
	 //   for (i=0; i<upper_voices; i++)
		//if (voice[i].status != VOICE_FREE)
		//{
		//    recompute_amp(i);
		//    apply_envelope_to_amp(i);
		//}
		change_output_volume(output_amplification);
	    ctl_mode_event(CTLE_MASTER_VOLUME, 0, output_amplification, 0);
	    continue;

	  case RC_CHANGE_REV_EFFB:
	  case RC_CHANGE_REV_TIME:
	    reverb_rc_event(rc, val);
	    sync_restart(0);
	    continue;

	  case RC_PREVIOUS:	/* |<< */
	    aq_flush(1);
	    if (current_sample < 2*play_mode->rate)
		return RC_REALLY_PREVIOUS;
	    return RC_RESTART;

	  case RC_RESTART:	/* |<< */
	    if(play_pause_flag)
	    {
		midi_restart_time = 0;
		ctl_pause_event(1, 0);
		continue;
	    }
	    aq_flush(1);
	    skip_to(0);
	    ctl_updatetime(0);
	    jump_flag = 1;
		midi_restart_time = 0;
	    continue;

	  case RC_JUMP:
	    if(play_pause_flag)
	    {
		midi_restart_time = val;
		ctl_pause_event(1, val);
		continue;
	    }
	    aq_flush(1);
	    if (val >= sample_count)
		return RC_TUNE_END;
	    skip_to(val);
	    ctl_updatetime(val);
	    return rc;

	  case RC_FORWARD:	/* >> */
	    if(play_pause_flag)
	    {
		midi_restart_time += val;
		if(midi_restart_time > sample_count)
		    midi_restart_time = sample_count;
		ctl_pause_event(1, midi_restart_time);
		continue;
	    }
	    cur = current_trace_samples();
	    aq_flush(1);
	    if(cur == -1)
		cur = current_sample;
	    if(val + cur >= sample_count)
		return RC_TUNE_END;
	    skip_to(val + cur);
	    ctl_updatetime(val + cur);
	    return RC_JUMP;

	  case RC_BACK:		/* << */
	    if(play_pause_flag)
	    {
		midi_restart_time -= val;
		if(midi_restart_time < 0)
		    midi_restart_time = 0;
		ctl_pause_event(1, midi_restart_time);
		continue;
	    }
	    cur = current_trace_samples();
	    aq_flush(1);
	    if(cur == -1)
		cur = current_sample;
	    if(cur > val)
	    {
		skip_to(cur - val);
		ctl_updatetime(cur - val);
	    }
	    else
	    {
		skip_to(0);
		ctl_updatetime(0);
		midi_restart_time = 0;
	    }
	    return RC_JUMP;

	  case RC_TOGGLE_PAUSE:
	    if(play_pause_flag)
	    {
		play_pause_flag = 0;
		skip_to(midi_restart_time);
#ifdef USE_TRACE_TIMER
		if(ctl->trace_playing)
			start_trace_timer();
#endif
	    }
	    else
	    {
#ifdef USE_TRACE_TIMER
		if(ctl->trace_playing)
			stop_trace_timer();
#endif
		midi_restart_time = current_trace_samples();
		if(midi_restart_time == -1)
		    midi_restart_time = current_sample;
		aq_flush(1);
		play_pause_flag = 1;
	    }
	    ctl_pause_event(play_pause_flag, midi_restart_time);
	    jump_flag = 1;
	    continue;

	  case RC_KEYUP:
	  case RC_KEYDOWN:
	    note_key_offset += val;
	    current_freq_table += val;
	    current_freq_table -= floor(current_freq_table * DIV_12) * 12;
	    current_temper_freq_table += val;
	    current_temper_freq_table -=
	    		floor(current_temper_freq_table * DIV_12) * 12;
	    if(sync_restart(1) != -1)
		jump_flag = 1;
	    ctl_mode_event(CTLE_KEY_OFFSET, 0, note_key_offset, 0);
	    continue;

	  case RC_SPEEDUP:
	    r = 1.0;
	    for(i = 0; i < val; i++)
		r *= SPEED_CHANGE_RATE;
	    sync_restart(0);
	    midi_time_ratio /= r;
	    current_sample = (int32)(current_sample / r + 0.5);
	    trace_offset(current_sample);
	    jump_flag = 1;
	    ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
	    continue;

	  case RC_SPEEDDOWN:
	    r = 1.0;
	    for(i = 0; i < val; i++)
		r *= SPEED_CHANGE_RATE;
	    sync_restart(0);
	    midi_time_ratio *= r;
	    current_sample = (int32)(current_sample * r + 0.5);
	    trace_offset(current_sample);
	    jump_flag = 1;
	    ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
	    continue;

	  case RC_VOICEINCR:
	    restore_voices(0);
	    voice_increment(val);
	    if(sync_restart(1) != -1)
		jump_flag = 1;
	    restore_voices(1);
	    continue;

	  case RC_VOICEDECR:
	    restore_voices(0);
	    if(sync_restart(1) != -1)
	    {
		voices -= val;
		if(voices < 0)
		    voices = 0;
		jump_flag = 1;
	    }
	    else
		voice_decrement(val);
	    restore_voices(1);
	    continue;

	  case RC_TOGGLE_DRUMCHAN:
	    midi_restart_time = current_trace_samples();
	    if(midi_restart_time == -1)
		midi_restart_time = current_sample;
	    SET_CHANNELMASK(drumchannel_mask, val);
	    SET_CHANNELMASK(current_file_info->drumchannel_mask, val);
	    if(IS_SET_CHANNELMASK(drumchannels, val))
	    {
		UNSET_CHANNELMASK(drumchannels, val);
		UNSET_CHANNELMASK(current_file_info->drumchannels, val);
	    }
	    else
	    {
		SET_CHANNELMASK(drumchannels, val);
		SET_CHANNELMASK(current_file_info->drumchannels, val);
	    }
	    aq_flush(1);
	    return RC_RELOAD;

	  case RC_TOGGLE_SNDSPEC:
#ifdef SUPPORT_SOUNDSPEC
	    if(view_soundspec_flag)
		close_soundspec();
	    else
		open_soundspec();
	    if(view_soundspec_flag || ctl_speana_flag)
	    {
		sync_restart(0);
		soundspec_update_wave(NULL, -1);
	    }
#endif /* SUPPORT_SOUNDSPEC */
	    continue;

	  case RC_TOGGLE_CTL_SPEANA:
#ifdef SUPPORT_SOUNDSPEC
	    ctl_speana_flag = !ctl_speana_flag;
	    if(view_soundspec_flag || ctl_speana_flag)
	    {
		sync_restart(0);
		soundspec_update_wave(NULL, -1);
	    }
#endif /* SUPPORT_SOUNDSPEC */
	    continue;

	  case RC_SYNC_RESTART:
	    sync_restart(val);
	    jump_flag = 1;
	    continue;

	  case RC_RELOAD:
	    midi_restart_time = current_trace_samples();
	    if(midi_restart_time == -1)
		midi_restart_time = current_sample;
	    aq_flush(1);
	    return RC_RELOAD;

	  case RC_CHANGE_RATE:
	    if(playmidi_change_rate(val, 1))
		return RC_NONE;
	    return RC_RELOAD;

	  case RC_OUTPUT_CHANGED:
	    playmidi_output_changed(0);
	    return RC_RELOAD;

	case RC_TOGGLE_MUTE:
		TOGGLE_CHANNELMASK(channel_mute, val);
		sync_restart(0);
		jump_flag = 1;
		ctl_mode_event(CTLE_MUTE, 0,
				val, (IS_SET_CHANNELMASK(channel_mute, val)) ? 1 : 0);
		continue;

	case RC_SOLO_PLAY:
		COPY_CHANNELMASK(tmp_chbitmask, channel_mute);
		FILL_CHANNELMASK(channel_mute);
		UNSET_CHANNELMASK(channel_mute, val);
		if (! COMPARE_CHANNELMASK(tmp_chbitmask, channel_mute)) {
			sync_restart(0);
			jump_flag = 1;
			for (i = 0; i < MAX_CHANNELS; i++)
				ctl_mode_event(CTLE_MUTE, 0, i, 1);
			ctl_mode_event(CTLE_MUTE, 0, val, 0);
		}
		continue;

	case RC_MUTE_CLEAR:
		COPY_CHANNELMASK(tmp_chbitmask, channel_mute);
		CLEAR_CHANNELMASK(channel_mute);
		if (! COMPARE_CHANNELMASK(tmp_chbitmask, channel_mute)) {
			sync_restart(0);
			jump_flag = 1;
			for (i = 0; i < MAX_CHANNELS; i++)
				ctl_mode_event(CTLE_MUTE, 0, i, 0);
		}
		continue;
	}
	if(intr)
	    return RC_QUIT;
	if(play_pause_flag)
	    usleep(300000);
    } while (rc != RC_NONE || play_pause_flag);
    return jump_flag ? RC_JUMP : RC_NONE;
}
///r
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static inline void mix_signal(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PD(&dest[i], _mm256_load_pd(&src[i]));
		MM256_LS_ADD_PD(&dest[i + 4], _mm256_load_pd(&src[i + 4]));
	}
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static inline void mix_signal(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8)
		MM256_LS_ADD_PS(&dest[i], _mm256_load_ps(&src[i]));
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static inline void mix_signal(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PD(&dest[i], _mm_load_pd(&src[i]));
		MM_LS_ADD_PD(&dest[i + 2], _mm_load_pd(&src[i + 2]));
		MM_LS_ADD_PD(&dest[i + 4], _mm_load_pd(&src[i + 4]));
		MM_LS_ADD_PD(&dest[i + 6], _mm_load_pd(&src[i + 6]));
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static inline void mix_signal(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PS(&dest[i], _mm_load_ps(&src[i]));
		MM_LS_ADD_PS(&dest[i + 4], _mm_load_ps(&src[i + 4]));
	}
}
#else /* floating-point implementation */
static inline void mix_signal(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;
	for (i = 0; i < count; i++) {
		dest[i] += src[i];
	}
}
#endif

// mix and clear
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static inline void mix_signal_clear(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8){
		MM256_LS_ADD_PD(&dest[i], _mm256_load_pd(&src[i]));
		MM256_LS_ADD_PD(&dest[i + 4], _mm256_load_pd(&src[i + 4]));
	}
	memset(src, 0, sizeof(DATA_T) * count);
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static inline void mix_signal_clear(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8)
		MM256_LS_ADD_PS(&dest[i], _mm256_load_ps(&src[i]));
	memset(src, 0, sizeof(DATA_T) * count);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static inline void mix_signal_clear(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PD(&dest[i], _mm_load_pd(&src[i]));
		MM_LS_ADD_PD(&dest[i + 2], _mm_load_pd(&src[i + 2]));
		MM_LS_ADD_PD(&dest[i + 4], _mm_load_pd(&src[i + 4]));
		MM_LS_ADD_PD(&dest[i + 6], _mm_load_pd(&src[i + 6]));
	}
	memset(src, 0, sizeof(DATA_T) * count);
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static inline void mix_signal_clear(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;	
	for(i = 0; i < count; i += 8){
		MM_LS_ADD_PS(&dest[i], _mm_load_ps(&src[i]));
		MM_LS_ADD_PS(&dest[i + 4], _mm_load_ps(&src[i + 4]));
	}
	memset(src, 0, sizeof(DATA_T) * count);
}
#else /* floating-point implementation */
static inline void mix_signal_clear(DATA_T *dest, DATA_T *src, int32 count)
{
	int32 i;

	for (i = 0; i < count; i++)
		dest[i] += src[i];
	memset(src, 0, sizeof(DATA_T) * count);
}
#endif

#ifdef __BORLANDC__
static int is_insertion_effect_xg(int ch)
#else
inline static int is_insertion_effect_xg(int ch)
#endif
{
	int i;
	for (i = 0; i < XG_INSERTION_EFFECT_NUM; i++) {
		if (insertion_effect_xg[i].part == ch) {
			return 1;
		}
	}
	for (i = 0; i < XG_VARIATION_EFFECT_NUM; i++) {
		if (variation_effect_xg[i].connection == XG_CONN_INSERTION
			&& variation_effect_xg[i].part == ch) {
			return 1;
		}
	}
	return 0;
}

#ifdef __BORLANDC__
static int is_mfx_effect_sd(int ch)
#else
inline static int is_mfx_effect_sd(int ch)
#endif
{
	if(channel[ch].sd_output_assign != 1){
		if(!mfx_effect_sd[channel[ch].sd_output_mfx_select].type) // ==NULL
			return 0;
		if(*mfx_effect_sd[channel[ch].sd_output_mfx_select].type > 0)
			return 1;
	}
	return 0;
}

/*
リアルタイムモード時 無音シンセ処理スキップ
ボイス数0状態で無音と推定 ボイス数0継続中X秒経過でバッファチェックフラグ
 (継続カウント数は下位ビット チェック回数は上位ビット チェックフラグは下位ビット0
 (経過時間は毎回変えるのはロングディレイ音のときのチェック抜け防止
無音確認したらスキップフラグセット (スキップフラグは -1
 (無音判定閾値は24bits出力相当で0.5bit以下
VST等もあるのでエフェクトの有無による無音判定変更はできない	
前回バッファは利用できない aq_add()で上書き
*/

#define CDSS_CHECK_MAX 8 // must (CDSS_CHECK_BIT + CDSS_CHECK_MAX(bit) + 1) < 31BIT
#define CDSS_CHECK_BIT 24 // sample rate max < 400kHz

static int compute_data_midi_skip(int32 count, int32 cnt)
{
	int32 i;
	const FLOAT_T cdss_check_sec[CDSS_CHECK_MAX + 1] = 
	{ 0.25, 0.3616, 0.3535, 0.1875, 0.3333, 0.2, 0.3141, 0.175, 0.25, }; // MAX8基準 2sec
	const uint32 cdss_check_mask = (0xFFFFFFFF << CDSS_CHECK_BIT); // 0xFFF00000
	const DATA_T thres = (double)((1 << (MAX_BITS - GUARD_BITS - 1)) - 1) / (double)(1U << (23 + 1)); // 24bit出力0.5ビット相当
	const uint32 cdss_count_mask = (1 << CDSS_CHECK_BIT) - 1; // 0xFFFFFF
	const int32 cdss_check_add = (1 << CDSS_CHECK_BIT); // 0x1000000
	const int32 cdss_check_max = cdss_check_add * (CDSS_CHECK_MAX + 1);

	if(count){ // ボイス数チェック , 継続カウント
		if(upper_voices){ // ボイス数チェック
			compute_skip_count = 1; // reset (バッファチェック回避
			return 0; // no skip
		}
		if(compute_skip_count < 0) // 処理スキップ
			return 1; // skip
		if((compute_skip_count += count) < 
			(cdss_check_sec[compute_skip_count >> CDSS_CHECK_BIT] * play_mode->rate)) // 継続カウント
			return 0; // no skip
		compute_skip_count &= cdss_check_mask; // count 0 // buffer check on
		return 0; // no skip
	}else{ // バッファチェック
		if(compute_skip_count & cdss_count_mask) // !=0
			return 0; // no skip
		for(i = 0; i < cnt; i += 3){ // L/R 
#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT)
			if(fabs((DATA_T)buffer_pointer[i]) > thres){
#else // DATA_T_INT32
			if(abs((DATA_T)buffer_pointer[i]) > thres){
#endif
				compute_skip_count = 1; // reset (バッファチェック回避
				return 0; // no skip
			}
		}
		if((compute_skip_count += cdss_check_add) < cdss_check_max) // next check
			return 0; // no skip
		compute_skip_count = -1; // set skip flag
		return 1; // skip
	}
}

// source mix
static inline void mix_ch_signal_source(DATA_T *src, int ch, int count)
{
	Channel *cp = channel + ch;
	int i;
	FLOAT_T left_mix = cp->vol_env.vol[0], right_mix = cp->vol_env.vol[1];
	
	if (play_mode->encoding & PE_MONO) {
		/* Mono output. */
		if(opt_mix_envelope > 0){
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)) // i==0 で通る
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
				*src++ *= cp->mix_env.vol[0];
			}
		}else{
			for (i = 0; i < count; i++) {
				*src++ *= left_mix;
			}
		}
	}else{		
		if(opt_mix_envelope >= 4){
			
// multi 4sample * 2ch	
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
			__m128d vevolx;
			__m256d vevol, vsp, vsp1, vsp2;
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){ // i==0 で通る
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevolx = _mm_loadu_pd(cp->mix_env.vol);
#else
					vevolx = _mm_cvtps_pd(_mm_load_ps(cp->mix_env.vol));
#endif
					vevol = MM256_SET2X_PD(vevolx, vevolx);
				}
				MM256_LSU_MUL_PD(src, vevol);
				src += 4;
				MM256_LSU_MUL_PD(src, vevol);
				src += 4;
			}	
	
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
			__m128 vevolx;
			__m256 vevol;
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){ // i==0 で通る
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevolx = _mm_cvtpd_ps(_mm_loadu_pd(cp->mix_env.vol));
#else
					vevolx = _mm_loadu_ps(cp->mix_env.vol);
#endif
					vevolx = _mm_shuffle_ps(vevolx, vevolx, 0x44);
					vevol = MM256_SET2X_PS(vevolx, vevolx);
				}
				MM256_LSU_MUL_PS(src, vevol);
				src += 8;
			}

#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
			__m128d vevol;
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){ // i==0 で通る
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevol = _mm_loadu_pd(cp->mix_env.vol);
#else
					vevol = _mm_cvtps_pd(_mm_load_ps(cp->mix_env.vol));
#endif
				}
				MM_LSU_MUL_PD(src, vevol);
				src += 2;
				MM_LSU_MUL_PD(src, vevol);
				src += 2;
				MM_LSU_MUL_PD(src, vevol);
				src += 2;
				MM_LSU_MUL_PD(src, vevol);
				src += 2;
			}

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)			
			__m128 vevol;
			__m128 vsp;
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i += 4) {
				if(!(i & mix_env_mask)){ // i==0 で通る
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
#if (USE_X86_EXT_INTRIN >= 3)
					vevol = _mm_cvtpd_ps(_mm_loadu_pd(cp->mix_env.vol));
#else
					vevol = _mm_set_ps(0, 0, (float)cp->mix_env.vol[1], (float)cp->mix_env.vol[0]);
#endif
#else
					vevol = _mm_loadu_ps(cp->mix_env.vol);
#endif
					vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
				}
				MM_LSU_MUL_PS(src, vevol);
				src += 4;
				MM_LSU_MUL_PS(src, vevol);
				src += 4;
			}

#else // ! USE_X86_EXT_INTRIN
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)) // i==0 で通る
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
				*src++ *= cp->mix_env.vol[0]; *src++ *= cp->mix_env.vol[1];
			}
#endif // USE_X86_EXT_INTRIN

		}else if(opt_mix_envelope > 0){

// single 1sample * 2ch
#if (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
			__m128d vevol;
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
					vevol = _mm_loadu_pd(cp->mix_env.vol);
#else
					vevol = _mm_cvtps_pd(_mm_load_ps(cp->mix_env.vol));
#endif
				}
				MM_LSU_MUL_PD(src, vevol);
				src += 2;
			}
			
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)			
			__m128 vevol;
			__m128 vsp;
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask)){
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
#if defined(FLOAT_T_DOUBLE)	
#if (USE_X86_EXT_INTRIN >= 3)
					vevol = _mm_cvtpd_ps(_mm_loadu_pd(cp->mix_env.vol));
#else
					vevol = _mm_set_ps(0, 0, (float)cp->mix_env.vol[1], (float)cp->mix_env.vol[0]);
#endif
#else
					vevol = _mm_loadu_ps(cp->mix_env.vol);
#endif
					vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
				}
				vsp = _mm_mul_ps(_mm_loadu_ps(src), vevol);
				*(src++) = MM_EXTRACT_F32(vsp,0);
				*(src++) = MM_EXTRACT_F32(vsp,1);	
			}

#else // ! USE_X86_EXT_INTRIN
			reset_envelope2(&cp->mix_env, left_mix, right_mix, ENVELOPE_KEEP);
			for (i = 0; i < count; i++) {
				if(!(i & mix_env_mask))
					compute_envelope2(&cp->mix_env, opt_mix_envelope);
				*src++ *= cp->mix_env.vol[0]; *src++ *= cp->mix_env.vol[1];
			}
#endif // USE_X86_EXT_INTRIN

		}else{
			for (i = 0; i < count; i++) {
				*src++ *= left_mix; *src++ *= right_mix;
			}
		}
	}
}

// drum efx mix level
static inline void mix_dfx_signal_source(DATA_T *src, int ch, int count)
{
	Channel *cp = channel + ch;
	int i;
	FLOAT_T left_mix = cp->vol_env.vol[0], right_mix = cp->vol_env.vol[1];
	
	if (play_mode->encoding & PE_MONO) {
		/* Mono output. */
		for (i = 0; i < count; i++) {
			*src++ *= left_mix;
		}
	}else{

// multi 4sample * 2ch	
#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
		__m128d vevolx;
		__m256d vevol;
#if defined(FLOAT_T_DOUBLE)	
		vevolx = _mm_loadu_pd(cp->vol_env.vol);
#else
		vevolx = _mm_cvtps_pd(_mm_load_ps(cp->vol_env.vol);
#endif
		vevol = MM256_SET2X_PD(vevolx, vevolx);
		for (i = 0; i < count; i += 4) {
			MM256_LSU_MUL_PD(src, vevol);
			src += 4;
			MM256_LSU_MUL_PD(src, vevol);
			src += 4;
		}	

#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
		__m128d vevol;
#if defined(FLOAT_T_DOUBLE)	
		vevol = _mm_loadu_pd(cp->vol_env.vol);
#else
		vevol = _mm_cvtps_pd(_mm_load_ps(cp->vol_env.vol));
#endif
		for (i = 0; i < count; i += 4) {
			MM_LSU_MUL_PD(src, vevol);
			src += 2;
			MM_LSU_MUL_PD(src, vevol);
			src += 2;
			MM_LSU_MUL_PD(src, vevol);
			src += 2;
			MM_LSU_MUL_PD(src, vevol);
			src += 2;
		}	
	
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
		__m128 vevolx;
		__m256 vevol;
#if defined(FLOAT_T_DOUBLE)	
		vevolx = _mm_cvtpd_ps(_mm_loadu_pd(cp->vol_env.vol));
#else
		vevolx = _mm_loadu_ps(cp->vol_env.vol);
#endif
		vevolx = _mm_shuffle_ps(vevolx, vevolx, 0x44);
		vevol = MM256_SET2X_PS(vevolx, vevolx);
		for (i = 0; i < count; i += 4) {
			MM256_LSU_MUL_PS(src, vevol);
			src += 8;
		}

#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)			
		__m128 vevol;	
#if defined(FLOAT_T_DOUBLE)	
#if (USE_X86_EXT_INTRIN >= 3)
		vevol = _mm_cvtpd_ps(_mm_loadu_pd(cp->vol_env.vol));
#else
		vevol = _mm_set_ps(0, 0, (float)cp->vol_env.vol[1], (float)cp->vol_env.vol[0]);
#endif
#else
		vevol = _mm_loadu_ps(cp->vol_env.vol);
#endif
		vevol = _mm_shuffle_ps(vevol, vevol, 0x44);
		for (i = 0; i < count; i += 4) {
			MM_LSU_MUL_PS(src, vevol);
			src += 4;
			MM_LSU_MUL_PS(src, vevol);
			src += 4;
		}
		
#else // ! USE_X86_EXT_INTRIN
		for (i = 0; i < count; i++) {
			*src++ *= left_mix;
			*src++ *= right_mix;
		}
#endif // USE_X86_EXT_INTRIN
	}
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static inline void mix_signal_level(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	int32 i;	
	__m256d send_level = _mm256_set1_pd((FLOAT_T)level * DIV_127);
	for(i = 0; i < count; i += 8){
		MM256_LS_FMA_PD(&dest[i], _mm256_load_pd(&src[i]), send_level);
		MM256_LS_FMA_PD(&dest[i + 4], _mm256_load_pd(&src[i + 4]), send_level);
	}
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static inline void mix_signal_level(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	int32 i;	
	__m256 send_level = _mm256_set1_ps((FLOAT_T)level * DIV_127);
	for(i = 0; i < count; i += 8)
		MM256_LS_FMA_PS(&dest[i], _mm256_load_ps(&src[i]), send_level);
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static inline void mix_signal_level(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	int32 i;
	__m128d send_level = _mm_set1_pd((FLOAT_T)level * DIV_127);
	for(i = 0; i < count; i += 8){
		MM_LS_FMA_PD(&dest[i], _mm_load_pd(&src[i]), send_level);
		MM_LS_FMA_PD(&dest[i + 2], _mm_load_pd(&src[i + 2]), send_level);
		MM_LS_FMA_PD(&dest[i + 4], _mm_load_pd(&src[i + 4]), send_level);
		MM_LS_FMA_PD(&dest[i + 6], _mm_load_pd(&src[i + 6]), send_level);
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static inline void mix_signal_level(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
	int32 i;	
	__m128 send_level = _mm_set1_ps((FLOAT_T)level * DIV_127);
	for(i = 0; i < count; i += 8){
		MM_LS_FMA_PS(&dest[i], _mm_load_ps(&src[i]), send_level);
		MM_LS_FMA_PS(&dest[i + 4], _mm_load_ps(&src[i + 4]), send_level);
	}
}
#else /* floating-point implementation */
static inline void mix_signal_level(DATA_T *dest, DATA_T *src, int32 count, int32 level)
{
    int32 i;
	FLOAT_T send_level = (FLOAT_T)level * DIV_127;

    for(i = 0; i < count; i++)
		dest[i] += src[i] * send_level;
}
#endif


enum _send_type {
	SEND_OUTPUT = 0, // dry : common_buffer_pointer
	SEND_EFFECT, // reverb, chorus, delay, gs eq : set func
	SEND_GS_INS, // gs ins : insertion_effect_buffer
	SEND_SD_MFX, // sd mfx : mfx_effect_buffer
};


/* do_compute_data_midi() with DSP Effect */
static void do_compute_data_midi(int32 count)
{
	int i, j, ch, note, uv = upper_voices, max_ch = current_file_info->max_channel;
	int channel_effect, channel_reverb, channel_chorus, channel_delay, channel_eq, flg_ch_vst;
	int stereo = ! (play_mode->encoding & PE_MONO);
	int32 cnt = count * ((stereo) ? 2 : 1);
	int32 n = cnt * sizeof(DATA_T); /* in bytes */	
	struct DrumPartEffect *de;
	int channel_drumfx[MAX_CHANNELS];
	int channel_silent[MAX_CHANNELS];
	int channel_send[MAX_CHANNELS];
	
	/* output buffer clear */
	memset(buffer_pointer, 0, n); // must

	if(opt_realtime_playing)
		if(compute_data_midi_skip(count, cnt)){	// silent skip
			current_sample += count;
			return;
		}
	
	// clear flag
	memset(channel_drumfx, 0, sizeof(channel_drumfx));	
	memset(channel_silent, 0, sizeof(channel_silent));
	memset(channel_send, 0, sizeof(channel_send));
	
	if(max_ch >= MAX_CHANNELS) max_ch = MAX_CHANNELS - 1;

	/* are effects valid? / don't supported in mono */
	if(!stereo){
		channel_reverb = channel_chorus = channel_delay = 0;
	}else{
		channel_reverb = ((opt_reverb_control > 0 && opt_reverb_control & 0x1)
				|| (opt_reverb_control < 0 && opt_reverb_control & 0x80));
		channel_chorus = opt_chorus_control;
		channel_delay = opt_delay_control > 0;
	}
	/* is EQ valid? */
	channel_eq = opt_eq_control && (eq_status_gs.low_gain != 0x40 || eq_status_gs.high_gain != 0x40
		|| play_system_mode == XG_SYSTEM_MODE);
	
	/* is ch vst valid? */
#ifdef VST_LOADER_ENABLE		// elion add.
	flg_ch_vst = (hVSTHost != NULL && use_vst_channel) ? 1 : 0;
#else
	flg_ch_vst = 0;
#endif // VST_LOADER_ENABLE

	channel_effect = !flg_ch_vst && (stereo && (channel_reverb || channel_chorus
			|| channel_delay || channel_eq || opt_insertion_effect));

	for(i = 0; i < uv; i++) {
		if(voice[i].status != VOICE_FREE) {
			channel[voice[i].channel].lasttime = current_sample + count;
		}
	}

	/* appropriate buffers for channels */
	if(channel_effect) {
		for(i = 0; i <= max_ch; i++) {
			if(!IS_SET_CHANNELMASK(channel_mute, i))
				channel_silent[i] = 1;			
			if(flg_ch_vst){
				channel_send[i] = SEND_OUTPUT;
			} else if(opt_insertion_effect && channel[i].insertion_effect) { // GS INS
				channel_send[i] = SEND_GS_INS;
			} else if(opt_insertion_effect && (play_system_mode == SD_SYSTEM_MODE) && is_mfx_effect_sd(i)) { // SD MFX
				channel_send[i] = SEND_SD_MFX;
			} else if(!flg_ch_vst && opt_drum_effect && ISDRUMCHANNEL(i)	) {
				make_drum_effect(i);
				channel_drumfx[i] = 1;
				channel_send[i] = SEND_EFFECT;
			} else if((play_system_mode == XG_SYSTEM_MODE && is_insertion_effect_xg(i))
				|| channel[i].eq_gs
				|| channel[i].reverb_level > 0
				|| channel[i].chorus_level > 0
				|| channel[i].delay_level > 0
				|| channel[i].eq_xg.valid
				|| channel[i].dry_level != 127) {
				channel_send[i] = SEND_EFFECT;
			} else {
				channel_send[i] = SEND_OUTPUT;
			}
			/* clear buffers of drum-part effect */
			if (opt_drum_effect && ISDRUMCHANNEL(i)){
				for (j = 0; j < channel[i].drum_effect_num; j++) 
					if (channel[i].drum_effect[j].buf != NULL) 
						memset(channel[i].drum_effect[j].buf, 0, n);
			}
		}
	}else{
		for(i = 0; i <= max_ch; i++)
			if(!IS_SET_CHANNELMASK(channel_mute, i))
				channel_silent[i] = 1;
	}

	for (i = 0; i < uv; i++) {
		if (voice[i].status != VOICE_FREE) {
			int8 flag;
			ch = voice[i].channel;
			
			if(channel_silent[ch]) {
				int dfx_flg = 0, dfx_num = 0;
				if(channel_drumfx[ch]){
					for (j = 0; j < channel[ch].drum_effect_num; j++){
						if (channel[ch].drum_effect[j].note == voice[i].note){
							dfx_flg = 1; dfx_num = j;
						}						
					}
				}
				if(dfx_flg){
					mix_voice(channel[ch].drum_effect[dfx_num].buf, i, count);
				}else{
					mix_voice(ch_buffer[ch], i, count);
				}
				if(voice[i].status == VOICE_FREE){
					ctl_note_event(i);
#ifdef VOICE_EFFECT
					uninit_voice_effect(i);
#endif
				}
			} else {
				free_voice(i);
				ctl_note_event(i);
#ifdef VOICE_EFFECT
				uninit_voice_effect(i);
#endif
			}
		}
	}

	while(uv > 0 && voice[uv - 1].status == VOICE_FREE)	{uv--;}
	upper_voices = uv;

	
	if(!channel_effect){
// channel VST
#ifdef VST_LOADER_ENABLE		// elion add.
		if (flg_ch_vst) {
			// ch mixer		
			//if(adjust_panning_immediately){
			//	for(i = 0; i <= max_ch; i++) {
			//		Channel *cp = channel + i;
			//		reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
			//		compute_envelope2(&cp->vol_env, count);
			//		mix_ch_signal_source(ch_buffer[i], i, count);
			//	}
			//}
			do_channel_vst(buffer_pointer, ch_buffer, cnt, max_ch);
			/* clear buffers of channnel buffer */
			for(i = 0; i <= max_ch; i++)
				memset(ch_buffer[i], 0, n); // clear
		}else 
#endif // VST_LOADER_ENABLE
// all effect off
		{
			// ch mixer		
			//if(adjust_panning_immediately){
			//	for(i = 0; i <= max_ch; i++) {
			//		Channel *cp = channel + i;
			//		reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
			//		compute_envelope2(&cp->vol_env, count);
			//		mix_ch_signal_source(ch_buffer[i], i, count);
			//	}
			//}
			for(i = 0; i <= max_ch; i++) {
				DATA_T *p = ch_buffer[i];
	
				mix_signal(buffer_pointer, p, cnt);
				/* clear buffers of channnel buffer */
				memset(p, 0, n); // clear
			}
		}
	}else switch(play_system_mode){
	case XG_SYSTEM_MODE:	/* XG */
		for(i = 0; i <= max_ch; i++) {	/* system effects */
			DATA_T *p = ch_buffer[i];
			int silent = channel_silent[i];
			int subtime = current_sample - channel[i].lasttime;
			int thru;
			int32 efx_delay_out, delay_out = 0;
			Channel *cp = channel + i;
		
			if(silent && (subtime < delay_out_count[1]) ){
				int eq_flg = 0;
				if(!ISDRUMCHANNEL(i) ){
					if(channel_eq && channel[i].eq_xg.valid){ /* part EQ*/
						do_ch_eq_xg(p, cnt, &(channel[i].eq_xg));
						eq_flg++;
					}
				}else if(channel_drumfx[i] ){
					for (j = 0; j < channel[i].drum_effect_num; j++) {
						de = &(channel[i].drum_effect[j]);
						if(channel_eq && de->eq_xg->valid){	/* drum effect part EQ */
							do_ch_eq_xg(de->buf, cnt, de->eq_xg);
							eq_flg++;
						}
						mix_signal(p, de->buf, cnt); // drum_efx_buf mix to ch_buff
					}
				}
				if(eq_flg)
					delay_out += delay_out_count[1];		
			}
			/* insertion effect */
			if(opt_insertion_effect){
				for (j = 0; j < XG_INSERTION_EFFECT_NUM; j++) {
					int ch = insertion_effect_xg[j].part;
					if(i != ch || !channel_silent[i])
						continue;
					efx_delay_out = delay_out_count[delay_out_type_xg[insertion_effect_xg[j].type_msb][insertion_effect_xg[j].type_lsb]];
					if(subtime < (delay_out + efx_delay_out)){
						do_insertion_effect_xg(p, cnt, &insertion_effect_xg[j]);
						delay_out += efx_delay_out;
					}
				}
			}
			/* variation effect (ins mode) */
			if(opt_delay_control){	
				for (j = 0; j < XG_VARIATION_EFFECT_NUM; j++) {
					int ch = variation_effect_xg[j].part;
					if(i != ch || !channel_silent[i] || variation_effect_xg[j].connection != XG_CONN_INSERTION)
						continue;				
					efx_delay_out = delay_out_count[delay_out_type_xg[variation_effect_xg[j].type_msb][variation_effect_xg[j].type_lsb]];
					if(subtime < (delay_out + efx_delay_out)){
						do_insertion_effect_xg(p, cnt, &variation_effect_xg[j]);
						delay_out += efx_delay_out;
					}
				}
			}
			// ch mixer			
			if(adjust_panning_immediately){
				reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
				compute_envelope2(&cp->vol_env, count);
				mix_ch_signal_source(p, i, count);
				if(channel_drumfx[i]){
					for (j = 0; j < channel[i].drum_effect_num; j++) {
						mix_dfx_signal_source(channel[i].drum_effect[j].buf, i, cnt); // drum_efx_buf mix level
					}
				}
			}			
			// send effect
			thru = silent && (subtime < delay_out);
			if(channel_send[i] == SEND_EFFECT){
				if(thru) {
					if (!is_insertion_effect_xg(i) && opt_drum_effect && ISDRUMCHANNEL(i) && channel[i].drum_effect_num) {
						for (j = 0; j < channel[i].drum_effect_num; j++) {
							de = &(channel[i].drum_effect[j]);
							if (de->reverb_send > 0)
								set_ch_reverb(de->buf, cnt, de->reverb_send);
							if (de->chorus_send > 0)
								set_ch_chorus(de->buf, cnt, de->chorus_send);
							if (de->delay_send > 0)
								set_ch_delay(de->buf, cnt, de->delay_send);
						}
					} else {
						if(channel_chorus && channel[i].chorus_level > 0)
							set_ch_chorus(p, cnt, channel[i].chorus_level);
						if(channel_delay && channel[i].delay_level > 0)
							set_ch_delay(p, cnt, channel[i].delay_level);
						if(channel_reverb && channel[i].reverb_level > 0)
							set_ch_reverb(p, cnt, channel[i].reverb_level);
					}
					if(channel[i].dry_level == 127)
						mix_signal(buffer_pointer, p, cnt);
					else if(channel[i].dry_level) // lv != 0
						mix_signal_level(buffer_pointer, p, cnt, channel[i].dry_level);
				
				}				
			}else if(channel_send[i] == SEND_OUTPUT){
				mix_signal(buffer_pointer, p, cnt);
			}
			/* clear buffers of channnel buffer */
			memset(p, 0, n); // clear
		}	
		/* mixing signal and applying system effects */ 
		if(channel_delay) {do_variation_effect1_xg(buffer_pointer, cnt);}
		if(channel_chorus) {do_ch_chorus_xg(buffer_pointer, cnt);}
		if(channel_reverb) {do_ch_reverb_xg(buffer_pointer, cnt);}
		if(multi_eq_xg.valid) {do_multi_eq_xg(buffer_pointer, cnt);}
		break;
	case GM2_SYSTEM_MODE:	/* GM2 */
		{
		int mfx_lasttime[SD_MFX_EFFECT_NUM] = {INT_MIN / 2, INT_MIN / 2, INT_MIN / 2,};
		int mfx_silent[SD_MFX_EFFECT_NUM] = {0, 0, 0,};
		
		for(i = 0; i <= max_ch; i++) {	/* system effects */
			DATA_T *p = ch_buffer[i];
			int silent = channel_silent[i];
			int subtime = current_sample - channel[i].lasttime;
			Channel *cp = channel + i;
			
			// drum effect buffer mix
			if(channel_drumfx[i])
				for (j = 0; j < channel[i].drum_effect_num; j++)
					mix_signal(p, channel[i].drum_effect[j].buf, cnt);
			// send effect			
			if(channel_send[i] == SEND_SD_MFX){
				if(channel[i].lasttime > mfx_lasttime[cp->sd_output_mfx_select])
					mfx_lasttime[cp->sd_output_mfx_select] = channel[i].lasttime;
				mfx_silent[cp->sd_output_mfx_select] += silent;
				set_ch_mfx_sd(p, cnt, cp->sd_output_mfx_select, cp->sd_dry_send_level);
			}else if(channel_send[i] == SEND_EFFECT){
				if(silent && (subtime < delay_out_count[0])) 
				{
					if (opt_drum_effect && ISDRUMCHANNEL(i) && channel[i].drum_effect_num) {
						for (j = 0; j < channel[i].drum_effect_num; j++) {
							de = &(channel[i].drum_effect[j]);
							if (de->reverb_send > 0)
								set_ch_reverb(de->buf, cnt, de->reverb_send);
							if (de->chorus_send > 0)
								set_ch_chorus(de->buf, cnt, de->chorus_send);
						}
					} else {
						if(channel_chorus && channel[i].chorus_level > 0)
							set_ch_chorus(p, cnt, channel[i].chorus_level);
						if(channel_reverb && channel[i].reverb_level > 0)
							set_ch_reverb(p, cnt, channel[i].reverb_level);
					}
					mix_signal(buffer_pointer, p, cnt);
				}
			}else if(channel_send[i] == SEND_OUTPUT){
				mix_signal(buffer_pointer, p, cnt);
			}
			/* clear buffers of channnel buffer */
			memset(p, 0, n); // clear
		}
		if(opt_insertion_effect){
			for(j = 0; j < SD_MFX_EFFECT_NUM; j++){
				if(mfx_silent[j]){
					int32 efx_delay_out = delay_out_count[delay_out_type_sd[*(mfx_effect_sd[j].type)]];
					if((current_sample - mfx_lasttime[j]) < efx_delay_out) {
						do_mfx_effect_sd(buffer_pointer, cnt, j);			
					}
				}
			}
		}
		/* mixing signal and applying system effects */ 
		if(channel_chorus) {do_ch_chorus(buffer_pointer, cnt);}
		if(channel_reverb) {do_ch_reverb(buffer_pointer, cnt);}
		if(multi_eq_sd.valid) {do_multi_eq_sd(buffer_pointer, cnt);}
		}
		break;
	case SD_SYSTEM_MODE:	/* SD */
		{
		int mfx_lasttime[SD_MFX_EFFECT_NUM] = {INT_MIN / 2, INT_MIN / 2, INT_MIN / 2,};
		int mfx_silent[SD_MFX_EFFECT_NUM] = {0, 0, 0,};
		
		for(i = 0; i <= max_ch; i++) {	/* system effects */
			DATA_T *p = ch_buffer[i];
			int silent = channel_silent[i];
			int subtime = current_sample - channel[i].lasttime;
			Channel *cp = channel + i;
			
			// drum effect buffer mix
			if(channel_drumfx[i])
				for (j = 0; j < channel[i].drum_effect_num; j++)
					mix_signal(p, channel[i].drum_effect[j].buf, cnt);
			// send effect			
			if(channel_send[i] == SEND_SD_MFX){
				if(channel[i].lasttime > mfx_lasttime[cp->sd_output_mfx_select])
					mfx_lasttime[cp->sd_output_mfx_select] = channel[i].lasttime;
				mfx_silent[cp->sd_output_mfx_select] += silent;
				set_ch_mfx_sd(p, cnt, cp->sd_output_mfx_select, cp->sd_dry_send_level);
			}else if(channel_send[i] == SEND_EFFECT){
				if(silent && (subtime < delay_out_count[0])) 
				{
					if (opt_drum_effect && ISDRUMCHANNEL(i) && channel[i].drum_effect_num) {
						for (j = 0; j < channel[i].drum_effect_num; j++) {
							de = &(channel[i].drum_effect[j]);
							if (de->reverb_send > 0)
								set_ch_reverb(de->buf, cnt, de->reverb_send);
							if (de->chorus_send > 0)
								set_ch_chorus(de->buf, cnt, de->chorus_send);
						}
					} else {
						if(channel_chorus && channel[i].chorus_level > 0)
							set_ch_chorus(p, cnt, channel[i].chorus_level);
						if(channel_reverb && channel[i].reverb_level > 0)
							set_ch_reverb(p, cnt, channel[i].reverb_level);
					}
					mix_signal(buffer_pointer, p, cnt);
				}
			}else if(channel_send[i] == SEND_OUTPUT){
				mix_signal(buffer_pointer, p, cnt);
			}
			/* clear buffers of channnel buffer */
			memset(p, 0, n); // clear
		}
		if(opt_insertion_effect){
			for(j = 0; j < SD_MFX_EFFECT_NUM; j++){
				if(mfx_silent[j]){
					int32 efx_delay_out = delay_out_count[delay_out_type_sd[*(mfx_effect_sd[j].type)]];
					if((current_sample - mfx_lasttime[j]) < efx_delay_out) {
						do_mfx_effect_sd(buffer_pointer, cnt, j);			
					}
				}
			}
		}
		/* mixing signal and applying system effects */ 
		if(channel_chorus) {do_ch_chorus_sd(buffer_pointer, cnt);}
		if(channel_reverb) {do_ch_reverb_sd(buffer_pointer, cnt);}
		if(multi_eq_sd.valid) {do_multi_eq_sd(buffer_pointer, cnt);}
		}
		break;
	default:	/* GM & GS */
		{
		int insertion_lasttime = INT_MIN / 2;
		int insertion_silent = 0;

		for(i = 0; i <= max_ch; i++) {	/* system effects */
			DATA_T *p = ch_buffer[i];
			int silent = channel_silent[i];
			int subtime = current_sample - channel[i].lasttime;
			Channel *cp = channel + i;
			
			// drum effect buffer mix
			if(channel_drumfx[i])
				for (j = 0; j < channel[i].drum_effect_num; j++)
					mix_signal(p, channel[i].drum_effect[j].buf, cnt);
			// ch mixer
			//if(adjust_panning_immediately){
			//	reset_envelope2(&cp->vol_env, cp->ch_amp[0], cp->ch_amp[1], ENVELOPE_KEEP);
			//	compute_envelope2(&cp->vol_env, count);
			//	mix_ch_signal_source(p, i, count);
			//	if(channel_drumfx[i]){
			//		for (j = 0; j < channel[i].drum_effect_num; j++) {
			//			mix_dfx_signal_source(channel[i].drum_effect[j].buf, i, cnt); // drum_efx_buf mix level
			//		}
			//	}
			//}
			// send effect
			if(channel_send[i] == SEND_GS_INS){
				if(channel[i].lasttime > insertion_lasttime)
					insertion_lasttime = channel[i].lasttime;
				insertion_silent += silent;
				set_ch_insertion_gs(p, cnt);
			}else if(channel_send[i] == SEND_EFFECT){
				if(silent && (subtime < delay_out_count[0])) {
					if (opt_drum_effect && ISDRUMCHANNEL(i) && channel[i].drum_effect_num) {
						for (j = 0; j < channel[i].drum_effect_num; j++) {
							de = &(channel[i].drum_effect[j]);
							if (de->reverb_send > 0)
								set_ch_reverb(de->buf, cnt, de->reverb_send);
							if (de->chorus_send > 0)
								set_ch_chorus(de->buf, cnt, de->chorus_send);
							if (de->delay_send > 0)
								set_ch_delay(de->buf, cnt, de->delay_send);
						}
					} else {
						if(channel_chorus && channel[i].chorus_level > 0)
							set_ch_chorus(p, cnt, channel[i].chorus_level);
						if(channel_delay && channel[i].delay_level > 0)
							set_ch_delay(p, cnt, channel[i].delay_level);
						if(channel_reverb && channel[i].reverb_level > 0)
							set_ch_reverb(p, cnt, channel[i].reverb_level);
					}
					if(channel_eq && channel[i].eq_gs)	
						set_ch_eq_gs(p, cnt);
					else
						mix_signal(buffer_pointer, p, cnt);
				}
			}else if(channel_send[i] == SEND_OUTPUT){
				mix_signal(buffer_pointer, p, cnt);
			}
			/* clear buffers of channnel buffer */
			memset(p, 0, n); // clear
		}
		if(opt_insertion_effect && insertion_silent){
			int32 efx_delay_out = delay_out_count[delay_out_type_gs[insertion_effect_gs.type_msb][insertion_effect_gs.type_lsb]];
			if((current_sample - insertion_lasttime) < efx_delay_out) {
				/* insertion effect */ /* sending insertion effect voice to channel effect, eq, dry */
				do_insertion_effect_gs(buffer_pointer, cnt, channel_eq);			
			}
		}
		/* mixing signal and applying system effects */ 
		if(channel_eq) {
			do_ch_eq_gs(buffer_pointer, cnt);
		}
		if(channel_chorus) {do_ch_chorus(buffer_pointer, cnt);}
		if(channel_delay) {do_ch_delay(buffer_pointer, cnt);}
		if(channel_reverb) {do_ch_reverb(buffer_pointer, cnt);}
		}
		break;
	}
///r
	// elion add.
	// move effect.c do_effect()
	//mix_compressor(buffer_pointer, cnt);
    do_effect(buffer_pointer, count);

	if(opt_realtime_playing)
		compute_data_midi_skip(0, cnt); // silent skip , buffer check
	current_sample += count;
}


static void do_compute_data_wav(int32 count)
{
	int i, stereo, samples, req_size, act_samples, v;

	stereo = !(play_mode->encoding & PE_MONO);
	samples = (stereo ? (count * 2) : count);
	req_size = samples * 2; /* assume 16bit */

	act_samples = tf_read(wav_buffer, 1, req_size, current_file_info->pcm_tf) / 2;
	for(i = 0; i < act_samples; i++) {
		v = (uint16)LE_SHORT(wav_buffer[i]);
		buffer_pointer[i] = (int32)((v << 16) | (v ^ 0x8000)) / 4; /* 4 : level down */
	}
	for(; i < samples; i++)
		buffer_pointer[i] = 0;

	current_sample += count;
}

static void do_compute_data_aiff(int32 count)
{
	int i, stereo, samples, req_size, act_samples, v;

	stereo = !(play_mode->encoding & PE_MONO);
	samples = (stereo ? (count * 2) : count);
	req_size = samples * 2; /* assume 16bit */

	act_samples = tf_read(wav_buffer, 1, req_size, current_file_info->pcm_tf) / 2;
	for(i = 0; i < act_samples; i++) {
		v = (uint16)BE_SHORT(wav_buffer[i]);
		buffer_pointer[i] = (int32)((v << 16) | (v ^ 0x8000)) / 4; /* 4 : level down */
	}
	for(; i < samples; i++)
		buffer_pointer[i] = 0;

	current_sample += count;
}


static void do_compute_data(int32 count)
{
    switch(current_file_info->pcm_mode)
    {
      case PCM_MODE_NON:
#ifdef MULTI_THREAD_COMPUTE
		if(compute_thread_ready)
    		do_compute_data_midi_thread(count);
		else
#endif
    	do_compute_data_midi(count);
      	break;
      case PCM_MODE_WAV:
    	do_compute_data_wav(count);
        break;
      case PCM_MODE_AIFF:
    	do_compute_data_aiff(count);
        break;
      case PCM_MODE_AU:
        break;
      case PCM_MODE_MP3:
        break;
    }    
}

static int check_midi_play_end(MidiEvent *e, int len)
{
    int i, type;

    for(i = 0; i < len; i++)
    {
	type = e[i].type;
	if(type == ME_NOTEON || type == ME_LAST || type == ME_WRD || type == ME_SHERRY)
	    return 0;
	if(type == ME_EOT)
	    return i + 1;
    }
    return 0;
}

static int compute_data(int32 count);
static int midi_play_end(void)
{
///r
    int i, rc = RC_TUNE_END;//, time;

    check_eot_flag = 0;

    if(opt_realtime_playing && current_sample == 0)
    {
	reset_voices();
	return RC_TUNE_END;
    }

///r
    /* add play time */
    if(opt_realtime_playing)
		rc = compute_data((int32)(play_mode->rate * ADD_PLAY_TIME/2));
    else if(add_play_time)
		rc = compute_data(add_play_count);
//	time = play_mode->rate * MAX_DIE_TIME / 1000; // time ms

    if(upper_voices > 0)
    {
		int fadeout_cnt;

		rc = compute_data(play_mode->rate);
		if(RC_IS_SKIP_FILE(rc))
			goto midi_end;

		for(i = 0; i < upper_voices; i++)
			if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
			finish_note(i);
		if(opt_realtime_playing)
			fadeout_cnt = 3;
		else
			fadeout_cnt = 6;
		for(i = 0; i < fadeout_cnt && upper_voices > 0; i++)
		{
			rc = compute_data(play_mode->rate / 2);
			if(RC_IS_SKIP_FILE(rc))
			goto midi_end;
		}

		/* kill voices */
		kill_all_voices();
///r
//		rc = compute_data(MAX_DIE_TIME); // sample num
		rc = compute_data(ramp_out_count);
		if(RC_IS_SKIP_FILE(rc))
			goto midi_end;
		upper_voices = 0;
    }

    /* clear reverb echo sound */
    init_ch_reverb();
    for(i = 0; i < MAX_CHANNELS; i++)
    {
	channel[i].reverb_level = -1;
    }
///r
    /* add silent time */
    if(opt_realtime_playing){
		rc = compute_data((int32)(play_mode->rate * ADD_SILENT_TIME/2));
		if(RC_IS_SKIP_FILE(rc))
			goto midi_end;
	}else if(add_silent_time){
		rc = compute_data(add_silent_count);
	//	rc = compute_data((int32)(play_mode->rate * PLAY_INTERLEAVE_SEC));
		if(RC_IS_SKIP_FILE(rc))
			goto midi_end;
	}

    compute_data(0); /* flush buffer to device */

    if(ctl->trace_playing)
    {
	rc = aq_flush(0); /* Wait until play out */	
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
    }
    else
    {
	trace_flush();
	rc = aq_soft_flush();
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
    }

  midi_end:
    if(RC_IS_SKIP_FILE(rc))
	aq_flush(1);

    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Playing time: ~%d seconds",
	      current_sample/play_mode->rate+2);
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Notes cut: %d",
	      cut_notes);
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Notes lost totally: %d",
	      lost_notes);
    if(RC_IS_SKIP_FILE(rc))
	return rc;
    return RC_TUNE_END;
}

///r
static void reduce_control(void)
{
	int i, filled, s_filled , q_size , rate, nv;
	
	if(!reduce_voice_threshold || opt_realtime_playing || !(play_mode->flag & PF_CAN_TRACE))
		return;

	/* use Auto voice reduce  &  reduce quality */
	filled = aq_filled();
	s_filled = aq_soft_filled();
	q_size = aq_get_dev_queuesize();
	rate = (int)(((double)(filled + s_filled) / q_size) * 100 + 0.5);

	if(aq_fill_buffer_flag || q_size <= 0)
		return;

	/* fall back to linear interpolation when queue < 100% */
	if (reduce_quality_threshold){
		if (rate < reduce_quality_threshold)
			reduce_quality_flag = 1;
		else  if(rate > reduce_quality_threshold * 1.15)
			reduce_quality_flag = 0;
	}

#ifdef REDUCE_VOICE_TIME_TUNING
	if(!reduce_voice_threshold)
		return;
	/* Auto voice reduce implementation by Masanao Izumo */
	/* Reduce voices if there is not enough audio device buffer */
	/* Calculate rate as it is displayed in ncurs_c.c */
	/* The old method of calculating rate resulted in very low values
		when using the new high order interplation methods on "slow"
		CPUs when the queue was being drained WAY too quickly.  This
		caused premature voice reduction under Linux, even if the queue
		was over 2000%, leading to major voice lossage. */
	for(i = nv = 0; i < upper_voices; i++)
		if(voice[i].status != VOICE_FREE)
			nv++;
	if(reduce_polyphony_threshold) {
		/* calculate ok_nv, the "optimum" max polyphony */
//	  if (auto_reduce_polyphony && rate < reduce_polyphony_threshold) {
		if (rate < reduce_polyphony_threshold) {
			reduce_polyphony_flag = 1;
			/* average in current nv */
			if ((rate == old_rate && nv > min_bad_nv) || (rate >= old_rate && rate < (reduce_polyphony_threshold * 0.25))) {
				ok_nv_total += nv;
				ok_nv_counts++;				
			/* increase polyphony when it is too low */
			}else if (nv == voices && (rate > old_rate)) {
	          	ok_nv_total += nv + 1;
	          	ok_nv_counts++;				
			/* reduce polyphony when loosing buffer */
			}else if (rate < (reduce_polyphony_threshold * 0.85) && (rate < old_rate)) {
				ok_nv_total += min_bad_nv;
	    		ok_nv_counts++;
			}else
				goto NO_RESCALE_NV;
			/* rescale ok_nv stuff every 1 seconds */
			if (current_sample >= ok_nv_sample && ok_nv_counts > 1)
			{
				ok_nv_total >>= 1;
				ok_nv_counts >>= 1;
				ok_nv_sample = current_sample + (play_mode->rate);
			}
			NO_RESCALE_NV:;
		} else
			reduce_polyphony_flag = 0;
	}

	/* EAW -- if buffer is < 75%, start reducing some voices to
		try to let it recover.  This really helps a lot, preserves
		decent sound, and decreases the frequency of lost ON notes */
	///r
	if (rate < reduce_voice_threshold) {
		reduce_voice_flag = 1;	

		if(rate <= old_rate) {
			int kill_nv, temp_nv;

			/* set bounds on "good" and "bad" nv */
			if (rate > (reduce_voice_threshold * 0.5) && nv < min_bad_nv) {
				min_bad_nv = nv;
				if (max_good_nv < min_bad_nv)
					max_good_nv = min_bad_nv;
			}
			/* EAW -- count number of !ON voices */
			/* treat chorus notes as !ON */
			for(i = kill_nv = 0; i < upper_voices; i++) {
				if(voice[i].status & VOICE_FREE || voice[i].cache != NULL)
					continue;		      
				if((voice[i].status & ~(VOICE_ON|VOICE_SUSTAINED) &&
					!(voice[i].status & ~(VOICE_DIE) && voice[i].sample->note_to_use)))
					kill_nv++;
			}
			/* EAW -- buffer is dangerously low, drasticly reduce
				voices to a hopefully "safe" amount */
			if (rate < (reduce_voice_threshold * 0.25)) {
				FLOAT_T n;
				/* calculate the drastic voice reduction */
				if(nv > kill_nv){ /* Avoid division by zero */
					n = (FLOAT_T) nv / (nv - kill_nv);
					temp_nv = (int)(nv - nv / (n + 1));
					/* reduce by the larger of the estimates */
					if (kill_nv < temp_nv && temp_nv < nv)
						kill_nv = temp_nv;
				}else
					kill_nv = nv - 1; /* do not kill all the voices */
			}else {
				/* the buffer is still high enough that we can throw
					fewer voices away; keep the ON voices, use the
					minimum "bad" nv as a floor on voice reductions */
				temp_nv = nv - min_bad_nv;
				if (kill_nv > temp_nv)
					kill_nv = temp_nv;
			}
			for(i = 0; i < kill_nv; i++)
				reduce_voice();
			/* lower max # of allowed voices to let the buffer recover */
		//	if (auto_reduce_polyphony) {
			if (reduce_polyphony_threshold) {
				temp_nv = nv - kill_nv;
				ok_nv = ok_nv_total / ok_nv_counts;
				/* decrease it to current nv left */
				if (voices > temp_nv && temp_nv > ok_nv)
					voice_decrement_conservative(voices - temp_nv);
				/* decrease it to ok_nv */
				else if (voices > ok_nv && temp_nv <= ok_nv)
					voice_decrement_conservative(voices - ok_nv);
				/* increase the polyphony */
				else if (voices < ok_nv)
					voice_increment(ok_nv - voices);
			}
			while(upper_voices > 0 && voice[upper_voices - 1].status == VOICE_FREE)
				upper_voices--;
		}
	}else {
		reduce_voice_flag = 0;
		/* set bounds on "good" and "bad" nv */
		if (rate > (reduce_voice_threshold * 1.15) && nv > max_good_nv) {
			max_good_nv = nv;
			if (min_bad_nv > max_good_nv)
				min_bad_nv = max_good_nv;
		}
		if (reduce_polyphony_threshold) {
			/* reset ok_nv stuff when out of danger */
			ok_nv_total = max_good_nv * ok_nv_counts;
			if (ok_nv_counts > 1) {
				ok_nv_total >>= 1;
				ok_nv_counts >>= 1;
			}
			/* restore max # of allowed voices to normal */
			restore_voices(0);
		}
	}
	old_rate = rate;
#endif
}



/* count=0 means flush remaining buffered data to output device, then
   flush the device itself */
///r
static int compute_data(int32 count)
{
	int rc;
	
	if (!count){
		if (buffered_count){
			do_compute_data(compute_buffer_size); // fixed size
#ifdef _DEBUG
			ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "output data (%d)", buffered_count);
#endif
#ifdef SUPPORT_SOUNDSPEC
			soundspec_update_wave(common_buffer, compute_buffer_size);
#endif /* SUPPORT_SOUNDSPEC */
			if(aq_add(common_buffer, compute_buffer_size) == -1)
				return RC_ERROR;
		}
		buffer_pointer=common_buffer;
		buffered_count=0;
		return RC_NONE;
    }
	count += buffered_count; // 前回の残りカウントを追加
	buffered_count = 0;
	if(current_event->type != ME_EOT)
		ctl_timestamp();
	if((rc = apply_controls()) != RC_NONE)
		return rc;
	while (count >= compute_buffer_size){
		do_compute_data(compute_buffer_size); // fixed size
		count -= compute_buffer_size;
#ifdef _DEBUG
		ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "output data (%d)", compute_buffer_size);
#endif
#ifdef SUPPORT_SOUNDSPEC
		soundspec_update_wave(common_buffer, compute_buffer_size);
#endif /* SUPPORT_SOUNDSPEC */
		if(aq_add(common_buffer, compute_buffer_size) == -1)
			return RC_ERROR;
		buffer_pointer=common_buffer;
    }
	reduce_control();
	buffered_count = (count < 0) ? 0 : count; // save
	/* check break signals */
	VOLATILE_TOUCH(intr);
	if(intr)
		return RC_QUIT;
	if(upper_voices == 0 && check_eot_flag){
		int i = check_midi_play_end(current_event, EOT_PRESEARCH_LEN);
		if(i > 1)
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Last %d MIDI events are ignored", i - 1);
		if(i > 0)
			return midi_play_end();
	}
	return RC_NONE;
}

int play_event(MidiEvent *ev){

    int32 i, j, cet;
    int k, l, ch, orig_ch, port_ch, offset, layered;

    if(play_mode->flag & PF_MIDI_EVENT)
	return play_mode->acntl(PM_REQ_MIDI, ev);
    if(!(play_mode->flag & PF_PCM_STREAM))
	return RC_NONE;

    current_event = ev;
    cet = MIDI_EVENT_TIME(ev);

    if(ctl->verbosity >= VERB_DEBUG_SILLY)
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Midi Event %d: %s %d %d %d", cet,
		  event_name(ev->type), ev->channel, ev->a, ev->b);

    if(cet > current_sample)
    {
	int rc;

#if ! defined(IA_WINSYN) && ! defined(IA_PORTMIDISYN) && ! defined(IA_W32G_SYN)
	if (midi_streaming != 0)
		if ((cet - current_sample) * 1000 / play_mode->rate > stream_max_compute) {
			kill_all_voices();
			/* reset_voices(); */
			/* ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
					"play_event: discard %d samples", cet - current_sample); */
			current_sample = cet;
		}
#endif	
	rc = compute_data(cet - current_sample);
	ctl_mode_event(CTLE_REFRESH, 0, 0, 0);
    if(rc == RC_JUMP)
	{
		ctl_timestamp();
		return RC_NONE;
	}
	if(rc != RC_NONE)
	    return rc;
	}

#ifndef SUPPRESS_CHANNEL_LAYER
	orig_ch = ev->channel;
	layered = ! IS_SYSEX_EVENT_TYPE(ev);
	for (k = 0; k < MAX_CHANNELS; k += 16) {
		port_ch = (orig_ch + k) % MAX_CHANNELS;
		offset = port_ch & ~0xf;
		for (l = offset; l < offset + 16; l++) {
			if (! layered && (k || l != offset))
				continue;
			if (layered) {
				if (! IS_SET_CHANNELMASK(channel[l].channel_layer, port_ch)
						|| channel[l].port_select != (orig_ch >> 4))
					continue;
				ev->channel = l;
			}
#endif
	ch = ev->channel;

	
	if(ev->type >= ME_TONE_BANK_MSB && ev->type <= ME_UNDEF_CTRL_CHNG)
		if(!get_rx(ch, RX_CONTROL_CHANGE)) 			
			ev->type = ME_NONE;
	
	process_channel_control_event(ev);
	
	if(opt_insertion_effect){		
		switch(play_system_mode) {
		case GS_SYSTEM_MODE:
			control_effect_gs(ev);
			break;
		case SD_SYSTEM_MODE:
			control_effect_sd(ev);
			break;
		default:
			break;
		}
	}

    switch(ev->type)
    {
	/* MIDI Events */
      case ME_NOTEOFF:
	if(!get_rx(ch, RX_NOTE_MESSAGE)) break;
	note_off(ev);
	break;

      case ME_NOTEON:
	if(!get_rx(ch, RX_NOTE_MESSAGE)) break;
	note_on(ev);
	break;

      case ME_KEYPRESSURE:
	if(!get_rx(ch, RX_POLY_PRESSURE)) break;
	adjust_pressure(ev);
	break;

      case ME_PROGRAM:
	if(!get_rx(ch, RX_PROGRAM_CHANGE)) break;
	midi_program_change(ch, ev->a);
	ctl_prog_event(ch);
	break;

      case ME_CHANNEL_PRESSURE:
	if(!get_rx(ch, RX_CH_PRESSURE)) break;
	adjust_channel_pressure(ev);
	control_effect_xg(ch);
	break;

      case ME_PITCHWHEEL:
	if(!get_rx(ch, RX_PITCH_BEND)) break;
	{
		int tmp = (int)ev->a + (int)ev->b * 128;
		channel[ch].pitchbend = tmp;
		channel[ch].bend.val = calc_bend_val(tmp);
	}
	/* Adjust pitch for notes already playing */
	adjust_pitch(ch);
	ctl_mode_event(CTLE_PITCH_BEND, 1, ch, channel[ch].pitchbend);
	control_effect_xg(ch);
	break;

	/* Controls */
      case ME_TONE_BANK_MSB:
	if(!get_rx(ch, RX_BANK_SELECT)) break;
	channel[ch].bank_msb = ev->a;
	break;

      case ME_TONE_BANK_LSB:
	if(!get_rx(ch, RX_BANK_SELECT_LSB)) break;
	channel[ch].bank_lsb = ev->a;
	break;

      case ME_MODULATION_WHEEL:
	if(!get_rx(ch, RX_MODULATION)) break;
	if(opt_modulation_wheel){
		channel[ch].mod.val = ev->a;
		update_modulation_wheel(ch);
		control_effect_xg(ch);
	}
	ctl_mode_event(CTLE_MOD_WHEEL, 1, ch, channel[ch].mod.val);
	break;

      case ME_MAINVOLUME:
	if(!get_rx(ch, RX_VOLUME)) break;
	channel[ch].volume = ev->a;
	adjust_volume(ch);
	ctl_mode_event(CTLE_VOLUME, 1, ch, ev->a);
	break;

      case ME_PAN:
	if(!get_rx(ch, RX_PANPOT)) break;
	channel[ch].panning = ev->a;
	channel[ch].pan_random = 0;
	if(adjust_panning_immediately && !channel[ch].pan_random)
	    adjust_panning(ch);
	ctl_mode_event(CTLE_PANNING, 1, ch, ev->a);
	break;

      case ME_EXPRESSION:
	if(!get_rx(ch, RX_EXPRESSION)) break;
	channel[ch].expression = ev->a;
	adjust_volume(ch);
	ctl_mode_event(CTLE_EXPRESSION, 1, ch, ev->a);
	break;

///r
      case ME_SUSTAIN:
	if(!get_rx(ch, RX_HOLD1)) break;		  
	if (channel[ch].damper_mode == 0) {	/* half-damper is not allowed. */
		if (ev->a >= 64) {
			if(channel[ch].sustain == 0){
				channel[ch].sustain = 127;
				update_redamper_controls(ch);
			}
		}else {
			channel[ch].sustain = 0;
		}
	}else{
		if(channel[ch].sustain != ev->a){
			channel[ch].sustain = ev->a;
			update_redamper_controls(ch);
		}
	}
	if(channel[ch].sustain == 0 && channel[ch].sostenuto == 0)
	    drop_sustain(ch);
	ctl_mode_event(CTLE_SUSTAIN, 1, ch, channel[ch].sustain);
#if 0 
    if (channel[ch].sustain == 0 && ev->a >= 64) {
		update_redamper_controls(ch);
	}
	channel[ch].sustain = ev->a;
	if (channel[ch].damper_mode == 0) {	/* half-damper is not allowed. */
		if (channel[ch].sustain >= 64) {channel[ch].sustain = 127;}
		else {channel[ch].sustain = 0;}
	}
	if(channel[ch].sustain == 0 && channel[ch].sostenuto == 0)
	    drop_sustain(ch);
	ctl_mode_event(CTLE_SUSTAIN, 1, ch, channel[ch].sustain);
#endif
	break;
///r
      case ME_SOSTENUTO:
	if(!get_rx(ch, RX_SOSTENUTO)) break;	
	channel[ch].sostenuto = (ev->a >= 64);
	update_sostenuto_controls(ch);
	if(channel[ch].sustain == 0 && channel[ch].sostenuto == 0)
	    drop_sustain(ch);
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Sostenuto %d", channel[ch].sostenuto);
	break;

      case ME_LEGATO_FOOTSWITCH:
	if(opt_portamento){
    channel[ch].legato = (ev->a >= 64);
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Legato Footswitch (CH:%d VAL:%d)", ch, ev->a);
	}
	break;

      case ME_HOLD2:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Hold2 - this function is not supported.");
	break;

      case ME_BREATH:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Breath - this function is not supported.");
	break;

      case ME_FOOT:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Foot - this function is not supported.");
	break;

      case ME_BALANCE:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Balance - this function is not supported.");
	break;

      case ME_PORTAMENTO_TIME_MSB:
	if(opt_portamento){
		channel[ch].portamento_time_msb = ev->a;
		update_portamento_time(ch);
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Portamento Time MSB (CH:%d VAL:%d)",ch,ev->a);
	}
	break;

      case ME_PORTAMENTO_TIME_LSB:
	if(opt_portamento){
		channel[ch].portamento_time_lsb = ev->a;
		update_portamento_time(ch);
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Portamento Time LSB (CH:%d VAL:%d)",ch,ev->a);
	}
	break;

      case ME_PORTAMENTO:
	if(!get_rx(ch, RX_PORTAMENTO)) break;	
	if(opt_portamento){
		channel[ch].portamento = (ev->a >= 64);
		if(!channel[ch].portamento)
			drop_portamento(ch);
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Portamento (CH:%d VAL:%d)",ch,ev->a);
	}
	break;

      case ME_PORTAMENTO_CONTROL:
	if(opt_portamento){
		channel[ch].portamento_control = ev->a;
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Portamento Control (CH:%d VAL:%d)",ch,ev->a);
	}
	break;

	  case ME_SOFT_PEDAL:
	if(!get_rx(ch, RX_SOFT)) break;	
		  //if(opt_lpf_def) {
			  channel[ch].soft_pedal = ev->a;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Soft Pedal (CH:%d VAL:%d)",ch,channel[ch].soft_pedal);
		 //}
		  break;

	  case ME_HARMONIC_CONTENT:
		  //if(opt_lpf_def) {
			  channel[ch].param_resonance = ev->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Harmonic Content (CH:%d VAL:%d)",ch,channel[ch].param_resonance);
		//	  update_channel_filter(ch);//elion add.
//			  recompute_voice_filter(ch);// elion add.
			  recompute_channel_filter(ch);
		  //}
		  break;

	  case ME_BRIGHTNESS:
		  //if(opt_lpf_def) {
			  channel[ch].param_cutoff_freq = ev->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Brightness (CH:%d VAL:%d)",ch,channel[ch].param_cutoff_freq);
		//	  update_channel_filter(ch);//elion add.
//			  recompute_voice_filter(ch);// elion add.
			  recompute_channel_filter(ch);
		  //}
		  break;

	  case ME_VIBRATO_RATE:
		if (opt_nrpn_vibrato) {
			channel[ch].param_vibrato_rate = ev->a - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,"Vibrato Rate (CH:%d VAL:%d)",ch,ev->a - 64);
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;

	  case ME_VIBRATO_DEPTH:
		if (opt_nrpn_vibrato) {
			channel[ch].param_vibrato_depth = ev->a - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,"Vibrato Depth (CH:%d VAL:%d)",ch,ev->a - 64);
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;

	  case ME_VIBRATO_DELAY:
		if (opt_nrpn_vibrato) {
			channel[ch].param_vibrato_delay = ev->a - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,"Vibrato Delay (CH:%d VAL:%d)",ch,ev->a - 64);
		//	adjust_pitch(ch);
			recompute_channel_lfo(ch);
			recompute_channel_pitch(ch);
		}
		break;

      case ME_DATA_ENTRY_MSB:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	if((i = last_rpn_addr(ch)) >= 0)
	{
	    channel[ch].rpnmap[i] = ev->a;
	    update_rpn_map(ch, i, 1);
	}
	break;

      case ME_DATA_ENTRY_LSB:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		channel[ch].rpnmap_lsb[i] = ev->a;
	    }
	break;

	case ME_REVERB_EFFECT:
		if (opt_reverb_control) {
			if (ISDRUMCHANNEL(ch) && get_reverb_level(ch) != ev->a) {
				if(channel[ch].drum_effect_flag)
					channel[ch].drum_effect_flag |= 0x4; // update effect
			}
			set_reverb_level(ch, ev->a);
			ctl_mode_event(CTLE_REVERB_EFFECT, 1, ch, get_reverb_level(ch));
		}
		break;

      case ME_CHORUS_EFFECT:
	if(opt_chorus_control)
	{
		if(opt_chorus_control == 1) {
			if (ISDRUMCHANNEL(ch) && channel[ch].chorus_level != ev->a) {
				if(channel[ch].drum_effect_flag)
					channel[ch].drum_effect_flag |= 0x4; // update effect
			}
			channel[ch].chorus_level = ev->a;
		} else {
			channel[ch].chorus_level = -opt_chorus_control;
		}
	    ctl_mode_event(CTLE_CHORUS_EFFECT, 1, ch, get_chorus_level(ch));
		if(ev->a) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send (CH:%d LEVEL:%d)",ch,ev->a);
		}
	}
	break;

      case ME_TREMOLO_EFFECT:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Tremolo Send (CH:%d LEVEL:%d)",ch,ev->a);
	break;

      case ME_CELESTE_EFFECT:
	if(opt_delay_control) {
		if (ISDRUMCHANNEL(ch) && channel[ch].delay_level != ev->a) {
			if(channel[ch].drum_effect_flag)
				channel[ch].drum_effect_flag |= 0x4; // update effect
		}
		channel[ch].delay_level = ev->a;
		if (play_system_mode == XG_SYSTEM_MODE) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Variation Send (CH:%d LEVEL:%d)",ch,ev->a);
		} else {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Send (CH:%d LEVEL:%d)",ch,ev->a);
		}
	}
	break;

	  case ME_ATTACK_TIME:
  	if(!opt_tva_attack) { break; }
	set_envelope_time(ch, ev->a, EG_ATTACK);
	break;

	  case ME_DECAY_TIME:
  	if(!opt_tva_decay) { break; }
	set_envelope_time(ch, ev->a, EG_DECAY);
	break;

	  case ME_RELEASE_TIME:
  	if(!opt_tva_release) { break; }
	set_envelope_time(ch, ev->a, EG_RELEASE);
	break;

      case ME_PHASER_EFFECT:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Phaser Send (CH:%d LEVEL:%d)",ch,ev->a);
	break;

      case ME_RPN_INC:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	if((i = last_rpn_addr(ch)) >= 0)
	{
	    if(channel[ch].rpnmap[i] < 127)
		channel[ch].rpnmap[i]++;
	    update_rpn_map(ch, i, 1);
	}
	break;

      case ME_RPN_DEC:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	if((i = last_rpn_addr(ch)) >= 0)
	{
	    if(channel[ch].rpnmap[i] > 0)
		channel[ch].rpnmap[i]--;
	    update_rpn_map(ch, i, 1);
	}
	break;

      case ME_NRPN_LSB:
	if(!get_rx(ch, RX_NRPN)) break;
	channel[ch].rpn_7f7f_flag = 0;
	channel[ch].lastlrpn = ev->a;
	channel[ch].nrpn = 1;
	break;

      case ME_NRPN_MSB:
	if(!get_rx(ch, RX_NRPN)) break;
	channel[ch].rpn_7f7f_flag = 0;
	channel[ch].lastmrpn = ev->a;
	channel[ch].nrpn = 1;
	break;

      case ME_RPN_LSB:
	if(!get_rx(ch, RX_RPN)) break;
	channel[ch].rpn_7f7f_flag = 0;
	channel[ch].lastlrpn = ev->a;
	channel[ch].nrpn = 0;
	break;

      case ME_RPN_MSB:
	if(!get_rx(ch, RX_RPN)) break;
	channel[ch].rpn_7f7f_flag = 0;
	channel[ch].lastmrpn = ev->a;
	channel[ch].nrpn = 0;
	break;

      case ME_ALL_SOUNDS_OFF:
	all_sounds_off(ch);
	break;

      case ME_RESET_CONTROLLERS:
	reset_controllers(ch);
	redraw_controllers(ch);
	break;

      case ME_ALL_NOTES_OFF:
	all_notes_off(ch);
	break;

      case ME_MONO:
	channel[ch].mono = 1;
	all_notes_off(ch);
	break;

      case ME_POLY:
	channel[ch].mono = 0;
	all_notes_off(ch);
	break;

	/* TiMidity Extensionals */
      case ME_RANDOM_PAN:
	channel[ch].panning = int_rand(128);
	channel[ch].pan_random = 1;
	if(adjust_panning_immediately && !channel[ch].pan_random)
	    adjust_panning(ch);
	break;

      case ME_SET_PATCH:
	i = channel[ch].special_sample = current_event->a;
	if(special_patch[i] != NULL)
	    special_patch[i]->sample_offset = 0;
	ctl_prog_event(ch);
	break;

      case ME_TEMPO:
	current_play_tempo = ch + ev->b * 256 + ev->a * 65536;
	ctl_mode_event(CTLE_TEMPO, 1, current_play_tempo, 0);
	break;

      case ME_CHORUS_TEXT:
      case ME_LYRIC:
      case ME_MARKER:
      case ME_INSERT_TEXT:
      case ME_TEXT:
      case ME_KARAOKE_LYRIC:
	i = ev->a | ((int)ev->b << 8);
	ctl_mode_event(CTLE_LYRIC, 1, i, 0);
	break;

      case ME_GSLCD:
	i = ev->a | ((int)ev->b << 8);
	ctl_mode_event(CTLE_GSLCD, 1, i, 0);
	break;

      case ME_MASTER_VOLUME:
	master_volume_ratio = (int32)ev->a + 256 * (int32)ev->b;
	adjust_master_volume();
	break;

      case ME_RESET:
	change_system_mode(ev->a);
	reset_midi(1);
	break;

      case ME_PATCH_OFFS:
	i = channel[ch].special_sample;
	if(special_patch[i] != NULL)
	    special_patch[i]->sample_offset =
		(current_event->a | 256 * current_event->b);
	break;

      case ME_WRD:
	push_midi_trace2(wrd_midi_event,
			 ch, current_event->a | (current_event->b << 8));
	break;

      case ME_SHERRY:
	push_midi_trace1(wrd_sherry_event,
			 ch | (current_event->a<<8) | (current_event->b<<16));
	break;

      case ME_DRUMPART:
		if(play_system_mode == XG_SYSTEM_MODE && current_event->a)
			channel[ch].bank_msb = 127; // Drum kit
	if(midi_drumpart_change(ch, current_event->a))
	{
	    /* Update bank information */
	    midi_program_change(ch, channel[ch].program);
	    ctl_mode_event(CTLE_DRUMPART, 1, ch, ISDRUMCHANNEL(ch));
	    ctl_prog_event(ch);
	}
	break;

      case ME_KEYSHIFT:
	i = (int)current_event->a - 0x40;
	if(i != channel[ch].key_shift)
	{
	    all_sounds_off(ch);
	    channel[ch].key_shift = (int8)i;
	}
	break;

	case ME_KEYSIG:
		if (opt_init_keysig != 8)
			break;
		current_keysig = (current_event->a + current_event->b * 16)&0xff;
		ctl_mode_event(CTLE_KEYSIG, 1, current_keysig, 0);
		if (opt_force_keysig != 8) {
			i = current_keysig - ((current_keysig < 8) ? 0 : 16), j = 0;
			while (i != opt_force_keysig && i != opt_force_keysig + 12)
				i += (i > 0) ? -5 : 7, j++;
			while (abs(j - note_key_offset) > 7)
				j += (j > note_key_offset) ? -12 : 12;
			if (abs(j - key_adjust) >= 12)
				j += (j > key_adjust) ? -12 : 12;
			note_key_offset = j;
			kill_all_voices();
			ctl_mode_event(CTLE_KEY_OFFSET, 1, note_key_offset, 0);
		}
		i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
		while (i != 7)
			i += (i < 7) ? 5 : -7, j++;
		j += note_key_offset, j -= floor(j * DIV_12) * 12;
		current_freq_table = j;
		adjust_pitch(ch);
		break;

	case ME_MASTER_TUNING:
		set_master_tuning((ev->b << 8) | ev->a);
		adjust_all_pitch();
		break;

	case ME_SCALE_TUNING:
		resamp_cache_refer_alloff(ch, current_event->time);
		channel[ch].scale_tuning[current_event->a] = current_event->b;
		adjust_pitch(ch);
		break;

	case ME_BULK_TUNING_DUMP:
		set_single_note_tuning(ch, current_event->a, current_event->b, 0);
		break;

	case ME_SINGLE_NOTE_TUNING:
		set_single_note_tuning(ch, current_event->a, current_event->b, 1);
		break;

	case ME_TEMPER_KEYSIG:
		current_temper_keysig = (current_event->a + 8) % 32 - 8;
		temper_adj = ((current_event->a + 8) & 0x20) ? 1 : 0;
		ctl_mode_event(CTLE_TEMPER_KEYSIG, 1, current_event->a, 0);
		i = current_temper_keysig + ((current_temper_keysig < 8) ? 7 : -9);
		j = 0;
		while (i != 7)
			i += (i < 7) ? 5 : -7, j++;
		j += note_key_offset, j -= floor(j * DIV_12) * 12;
		current_temper_freq_table = j;
		if (current_event->b)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
				//	recompute_freq(i);
				}
		adjust_pitch(ch);
		break;

	case ME_TEMPER_TYPE:
		channel[ch].temper_type = current_event->a;
		ctl_mode_event(CTLE_TEMPER_TYPE, 1, ch, channel[ch].temper_type);
		if (temper_type_mute) {
			if (temper_type_mute & (1 << (current_event->a
					- ((current_event->a >= 0x40) ? 0x3c : 0)))) {
				SET_CHANNELMASK(channel_mute, ch);
				ctl_mode_event(CTLE_MUTE, 1, ch, 1);
			} else {
				UNSET_CHANNELMASK(channel_mute, ch);
				ctl_mode_event(CTLE_MUTE, 1, ch, 0);
			}
		}
		if (current_event->b)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
				//	recompute_freq(i);
				}
		adjust_pitch(ch);
		break;

	case ME_MASTER_TEMPER_TYPE:
		for (i = 0; i < MAX_CHANNELS; i++) {
			channel[i].temper_type = current_event->a;
			ctl_mode_event(CTLE_TEMPER_TYPE, 1, i, channel[i].temper_type);
		}
		if (temper_type_mute) {
			if (temper_type_mute & (1 << (current_event->a
					- ((current_event->a >= 0x40) ? 0x3c : 0)))) {
				FILL_CHANNELMASK(channel_mute);
				for (i = 0; i < MAX_CHANNELS; i++)
					ctl_mode_event(CTLE_MUTE, 1, i, 1);
			} else {
				CLEAR_CHANNELMASK(channel_mute);
				for (i = 0; i < MAX_CHANNELS; i++)
					ctl_mode_event(CTLE_MUTE, 1, i, 0);
			}
		}
		if (current_event->b)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
				//	recompute_freq(i);
				}
		adjust_pitch(ch);
		break;

	case ME_USER_TEMPER_ENTRY:
		set_user_temper_entry(ch, current_event->a, current_event->b);
		adjust_pitch(ch);
		break;

	case ME_SYSEX_LSB:
		process_sysex_event(ME_SYSEX_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_MSB:
		process_sysex_event(ME_SYSEX_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_GS_LSB:
		process_sysex_event(ME_SYSEX_GS_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_GS_MSB:
		process_sysex_event(ME_SYSEX_GS_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_XG_LSB:
		process_sysex_event(ME_SYSEX_XG_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_XG_MSB:
		process_sysex_event(ME_SYSEX_XG_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_SD_LSB:
		process_sysex_event(ME_SYSEX_SD_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_SD_MSB:
		process_sysex_event(ME_SYSEX_SD_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_SD_HSB:
		process_sysex_event(ME_SYSEX_SD_HSB,ch,current_event->a,current_event->b);
	    break;

	case ME_NOTE_STEP:
		i = ev->a + ((ev->b & 0x0f) << 8);
		j = ev->b >> 4;
		ctl_mode_event(CTLE_METRONOME, 1, i, j);
		if (readmidi_wrd_mode)
			wrdt->update_events();
		break;
		
	case ME_CUEPOINT:
		set_cuepoint(ch, current_event->a, current_event->b);
		break;

	case ME_LOOP_EXPANSION_START:
		play_mode->acntl(PM_REQ_LOOP_START, (void *)current_event->time);
		break;

	case ME_LOOP_EXPANSION_END:
		play_mode->acntl(PM_REQ_LOOP_END, (void *)current_event->time);
		break;

	case ME_EOT:
		ctl_mode_event(CTLE_CURRENT_TIME_END, 0, (long)(current_sample / (midi_time_ratio * play_mode->rate)), 0);
		return midi_play_end();
    }
#ifndef SUPPRESS_CHANNEL_LAYER
		}
	}
	ev->channel = orig_ch;
#endif

    return RC_NONE;
}

static void set_master_tuning(int tune)
{
	if (tune & 0x4000)	/* 1/8192 semitones + 0x2000 | 0x4000 */
		tune = (tune & 0x3FFF) - 0x2000;
	else if (tune & 0x8000)	/* 1 semitones | 0x8000 */
		tune = ((tune & 0x7F) - 0x40) << 13;
	else	/* millisemitones + 0x400 */
		tune = (((tune - 0x400) << 13) + 500) / 1000;
	master_tuning = tune;
}

static void set_single_note_tuning(int part, int a, int b, int rt)
{
	static int tp;	/* tuning program number */
	static int kn;	/* MIDI key number */
	static int st;	/* the nearest equal-tempered semitone */
	double f, fst;	/* fraction of semitone */
	int i;
	
	switch (part) {
	case 0:
		tp = a;
		break;
	case 1:
		kn = a, st = b;
		break;
	case 2:
		if (st == 0x7f && a == 0x7f && b == 0x7f)	/* no change */
			break;
		f = 440 * pow(2.0, (st - 69) * DIV_12);
		fst = pow(2.0, (a << 7 | b) / 196608.0);
		freq_table_tuning[tp][kn] = f * fst * 1000 + 0.5;
		if (rt)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
				//	recompute_freq(i);
					recompute_voice_pitch(i);
				}
		break;
	}
}

static void set_user_temper_entry(int part, int a, int b)
{
	static int tp;		/* temperament program number */
	static int ll;		/* number of formula */
	static int fh, fl;	/* applying pitch bit mask (forward) */
	static int bh, bl;	/* applying pitch bit mask (backward) */
	static int aa, bb;	/* fraction (aa/bb) */
	static int cc, dd;	/* power (cc/dd)^(ee/ff) */
	static int ee, ff;
	static int ifmax, ibmax, count;
	static double rf[11], rb[11];
	int i, j, k, l, n, m;
	double ratio[12], f, sc;
	
	switch (part) {
	case 0:
		for (i = 0; i < 11; i++)
			rf[i] = rb[i] = 1;
		ifmax = ibmax = 0;
		count = 0;
		tp = a, ll = b;
		break;
	case 1:
		fh = a, fl = b;
		break;
	case 2:
		bh = a, bl = b;
		break;
	case 3:
		aa = a, bb = b;
		break;
	case 4:
		cc = a, dd = b;
		break;
	case 5:
		ee = a, ff = b;
		for (i = 0; i < 11; i++) {
			if (((fh & 0xf) << 7 | fl) & 1 << i) {
				rf[i] *= (double) aa / bb
						* pow((double) cc / dd, (double) ee / ff);
				if (ifmax < i + 1)
					ifmax = i + 1;
			}
			if (((bh & 0xf) << 7 | bl) & 1 << i) {
				rb[i] *= (double) aa / bb
						* pow((double) cc / dd, (double) ee / ff);
				if (ibmax < i + 1)
					ibmax = i + 1;
			}
		}
		if (++count < ll)
			break;
		ratio[0] = 1;
		for (i = n = m = 0; i < ifmax; i++, m = n) {
			n += (n > 4) ? -5 : 7;
			ratio[n] = ratio[m] * rf[i];
			if (ratio[n] > 2)
				ratio[n] /= 2;
		}
		for (i = n = m = 0; i < ibmax; i++, m = n) {
			n += (n > 6) ? -7 : 5;
			ratio[n] = ratio[m] / rb[i];
			if (ratio[n] < 1)
				ratio[n] *= 2;
		}
		sc = 27 / ratio[9] / 16;	/* syntonic comma */
		for (i = 0; i < 12; i++)
			for (j = -1; j < 11; j++) {
				f = 440 * pow(2.0, (i - 9) * DIV_12 + j - 5);
				for (k = 0; k < 12; k++) {
					l = i + j * 12 + k;
					if (l < 0 || l >= 128)
						continue;
					if (! (fh & 0x40)) {	/* major */
						freq_table_user[tp][i][l] =
								f * ratio[k] * 1000 + 0.5;
						freq_table_user[tp][i + 36][l] =
								f * ratio[k] * sc * 1000 + 0.5;
					}
					if (! (bh & 0x40)) {	/* minor */
						freq_table_user[tp][i + 12][l] =
								f * ratio[k] * sc * 1000 + 0.5;
						freq_table_user[tp][i + 24][l] =
								f * ratio[k] * 1000 + 0.5;
					}
				}
			}
		break;
	}
}

static void set_cuepoint(int part, int a, int b)
{
	static int a0 = 0, b0 = 0;

	if (part == 0) {
		a0 = a, b0 = b;
		return;
	}
	ctl_mode_event(CTLE_CUEPOINT, 1, a0 << 24 | b0 << 16 | a << 8 | b, 0);
}

static int play_midi(MidiEvent *eventlist, int32 samples)
{
    int rc;
    static int play_count = 0;

    if (play_mode->id_character == 'M') {
		int cnt, err;

		err = convert_mod_to_midi_file(eventlist);

		play_count = 0;
		cnt = free_global_mblock();	/* free unused memory */
		if(cnt > 0)
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
				  "%d memory blocks are free", cnt);
		if (err) return RC_ERROR;
		return RC_TUNE_END;
    }
	
    sample_count = samples;	

    event_list = eventlist;
    lost_notes = cut_notes = 0;
    check_eot_flag = 1;

    wrd_midi_event(-1, -1); /* For initialize */

    reset_midi(0);
 ///r	
	// for opt_amp_compensation , see play_midi_prescan() adjust_amplification()
	mainvolume_max = 0x7f;
	compensation_ratio = 1.0;

	if(!opt_realtime_playing
	//	&& allocate_cache_size > 0
		&& !IS_CURRENT_MOD_FILE
		&& (play_mode->flag&PF_PCM_STREAM))
    {
		play_midi_prescan(eventlist);
		reset_midi(0);
    }

    rc = aq_flush(0);
    if(RC_IS_SKIP_FILE(rc))
	return rc;

#ifdef USE_TRACE_TIMER
    if(ctl->trace_playing)
		start_trace_timer();
#endif
	
    skip_to(midi_restart_time);

    if(midi_restart_time > 0) { /* Need to update interface display */
		int i;
		for(i = 0; i < MAX_CHANNELS; i++)
			redraw_controllers(i);
    }
    rc = RC_NONE;
	
///r
#if 0
	if(midi_streaming == 0 && add_silent_time){
		sample_count += add_silent_count;
		compute_data(add_silent_count);
	}
#endif
    for(;;)
    {
		midi_restart_time = 1;
		rc = play_event(current_event);
		if(rc != RC_NONE)
			break;
		if (midi_restart_time)    /* don't skip the first event if == 0 */
			current_event++;
    }
#ifdef USE_TRACE_TIMER
    if(ctl->trace_playing)
		stop_trace_timer();
#endif

    if(play_count++ > 3)
    {
		int cnt;
		play_count = 0;
		cnt = free_global_mblock();	/* free unused memory */
		if(cnt > 0)
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
				  "%d memory blocks are free", cnt);
    }
    return rc;
}

static void read_header_wav(struct timidity_file* tf)
{
    char buff[44];
    tf_read( buff, 1, 44, tf);
}

static int read_header_aiff(struct timidity_file* tf)
{
    char buff[5]="    ";
    int i;
    
    for( i=0; i<100; i++ ){
    	buff[0]=buff[1]; buff[1]=buff[2]; buff[2]=buff[3];
    	tf_read( &buff[3], 1, 1, tf);
    	if( strcmp(buff,"SSND")==0 ){
            /*SSND chunk found */
    	    tf_read( &buff[0], 1, 4, tf);
    	    tf_read( &buff[0], 1, 4, tf);
	    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "aiff header read OK.");
	    return 0;
    	}
    }
    /*SSND chunk not found */
    return -1;
}

static int load_pcm_file_wav()
{
    char *filename;

    if(strcmp(pcm_alternate_file, "auto") == 0)
    {
	filename = safe_malloc(strlen(current_file_info->filename)+5);
	strcpy(filename, current_file_info->filename);
	strcat(filename, ".wav");
    }
    else if(strlen(pcm_alternate_file) >= 5 &&
	    strncasecmp(pcm_alternate_file + strlen(pcm_alternate_file) - 4,
			".wav", 4) == 0)
	filename = safe_strdup(pcm_alternate_file);
    else
	return -1;

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "wav filename: %s", filename);
    current_file_info->pcm_tf = open_file(filename, 0, OF_SILENT);
    if( current_file_info->pcm_tf ){
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open successed.");
	read_header_wav(current_file_info->pcm_tf);
	current_file_info->pcm_filename = filename;
	current_file_info->pcm_mode = PCM_MODE_WAV;
	return 0;
    }else{
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open failed.");
	safe_free(filename);
	current_file_info->pcm_filename = NULL;
	return -1;
    }
}

static int load_pcm_file_aiff()
{
    char *filename;

    if(strcmp(pcm_alternate_file, "auto") == 0)
    {
	filename = safe_malloc(strlen(current_file_info->filename)+6);
	strcpy(filename, current_file_info->filename);
	strcat( filename, ".aiff");
    }
    else if(strlen(pcm_alternate_file) >= 6 &&
	    strncasecmp(pcm_alternate_file + strlen(pcm_alternate_file) - 5,
			".aiff", 5) == 0)
	filename = safe_strdup(pcm_alternate_file);
    else
	return -1;

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "aiff filename: %s", filename);
    current_file_info->pcm_tf = open_file(filename, 0, OF_SILENT);
    if( current_file_info->pcm_tf ){
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open successed.");
	read_header_aiff(current_file_info->pcm_tf);
	current_file_info->pcm_filename = filename;
	current_file_info->pcm_mode = PCM_MODE_AIFF;
	return 0;
    }else{
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open failed.");
	safe_free(filename);
	current_file_info->pcm_filename = NULL;
	return -1;
    }
}

static void load_pcm_file()
{
    if( load_pcm_file_wav()==0 ) return; /*load OK*/
    if( load_pcm_file_aiff()==0 ) return; /*load OK*/
}

static int play_midi_load_file(char *fn,
			       MidiEvent **event,
			       int32 *nsamples)
{
    int rc;
    struct timidity_file *tf;
    int32 nevents;

    *event = NULL;

    if(!strcmp(fn, "-"))
	file_from_stdin = 1;
    else
	file_from_stdin = 0;

    ctl_mode_event(CTLE_NOW_LOADING, 0, (ptr_size_t)fn, 0);
    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "MIDI file: %s", fn);
    if((tf = open_midi_file(fn, 1, OF_VERBOSE)) == NULL)
    {
	ctl_mode_event(CTLE_LOADING_DONE, 0, -1, 0);
	return RC_ERROR;
    }

    *event = NULL;
    rc = check_apply_control();
    if(RC_IS_SKIP_FILE(rc))
    {
	close_file(tf);
	ctl_mode_event(CTLE_LOADING_DONE, 0, 1, 0);
	return rc;
    }

    *event = read_midi_file(tf, &nevents, nsamples, fn);
    close_file(tf);

    if(*event == NULL)
    {
	ctl_mode_event(CTLE_LOADING_DONE, 0, -1, 0);
	return RC_ERROR;
    }

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
	      "%d supported events, %d samples, time %d:%02d",
	      nevents, *nsamples,
	      *nsamples / play_mode->rate / 60,
	      (*nsamples / play_mode->rate) % 60);

    current_file_info->pcm_mode = PCM_MODE_NON; /*initialize*/
    if(pcm_alternate_file != NULL &&
       strcmp(pcm_alternate_file, "none") != 0 &&
       (play_mode->flag&PF_PCM_STREAM))
	load_pcm_file();

    if(!IS_CURRENT_MOD_FILE &&
       (play_mode->flag&PF_PCM_STREAM))
    {
	/* FIXME: Instruments is not need for pcm_alternate_file. */
    }
    else
	clear_magic_instruments();	/* Clear load markers */

    ctl_mode_event(CTLE_LOADING_DONE, 0, 0, 0);

    return RC_NONE;
}

int play_midi_file(char *fn)
{
    int i, j, rc;
    static int last_rc = RC_NONE;
    MidiEvent *event = NULL;
    int32 nsamples;

    /* Set current file information */
    current_file_info = get_midi_file_info(fn, 1);

    rc = check_apply_control();
    if(RC_IS_SKIP_FILE(rc) && rc != RC_RELOAD)
	return rc;

    /* Reset key & speed each files */
    current_keysig = (opt_init_keysig == 8) ? 0 : opt_init_keysig;
    note_key_offset = key_adjust;
    midi_time_ratio = tempo_adjust;
	for (i = 0; i < MAX_CHANNELS; i++) {
		for (j = 0; j < 12; j++)
			channel[i].scale_tuning[j] = 0;
		channel[i].prev_scale_tuning = 0;
		channel[i].temper_type = 0;
	}
    CLEAR_CHANNELMASK(channel_mute);
	if (temper_type_mute & 1)
		FILL_CHANNELMASK(channel_mute);

    /* Reset restart offset */
    midi_restart_time = 0;

#ifdef REDUCE_VOICE_TIME_TUNING
    /* Reset voice reduction stuff */
    min_bad_nv = 256;
    max_good_nv = 1;
    ok_nv_total = 32;
    ok_nv_counts = 1;
    ok_nv = 32;
    ok_nv_sample = 0;
    old_rate = -1;
///r
//    reduce_quality_flag = no_4point_interpolation;
    reduce_quality_flag = 0;
	reduce_voice_flag = 0;
	reduce_polyphony_flag = 0;

    restore_voices(0);
#endif

	ctl_mode_event(CTLE_METRONOME, 0, 0, 0);
	ctl_mode_event(CTLE_KEYSIG, 0, current_keysig, 0);
	ctl_mode_event(CTLE_TEMPER_KEYSIG, 0, 0, 0);
	ctl_mode_event(CTLE_KEY_OFFSET, 0, note_key_offset, 0);
	i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
	while (i != 7)
		i += (i < 7) ? 5 : -7, j++;
	j += note_key_offset, j -= floor(j * DIV_12) * 12;
	current_freq_table = j;
	ctl_mode_event(CTLE_TEMPO, 0, current_play_tempo, 0);
	ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
	for (i = 0; i < MAX_CHANNELS; i++) {
		ctl_mode_event(CTLE_TEMPER_TYPE, 0, i, channel[i].temper_type);
		ctl_mode_event(CTLE_MUTE, 0, i, temper_type_mute & 1);
	}
  play_reload: /* Come here to reload MIDI file */
    rc = play_midi_load_file(fn, &event, &nsamples);
    if(RC_IS_SKIP_FILE(rc))
	goto play_end; /* skip playing */

    init_mblock(&playmidi_pool);
    ctl_mode_event(CTLE_PLAY_START, 0, nsamples, 0);
    play_mode->acntl(PM_REQ_PLAY_START, NULL);
    rc = play_midi(event, nsamples);
    play_mode->acntl(PM_REQ_PLAY_END, NULL);
    ctl_mode_event(CTLE_PLAY_END, 0, 0, 0);
    reuse_mblock(&playmidi_pool);

    for(i = 0; i < MAX_CHANNELS; i++)
	memset(channel[i].drums, 0, sizeof(channel[i].drums));

  play_end:
    if(current_file_info->pcm_tf){
    	close_file(current_file_info->pcm_tf);
    	current_file_info->pcm_tf = NULL;
    	safe_free( current_file_info->pcm_filename );
    	current_file_info->pcm_filename = NULL;
    }
    
    if(wrdt->opened)
	wrdt->end();

    if(free_instruments_afterwards)
    {
	int cnt;
	free_instruments(0);
	cnt = free_global_mblock(); /* free unused memory */
	if(cnt > 0)
	    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "%d memory blocks are free",
		      cnt);
    }

    free_special_patch(-1);

    safe_free(event);
    if(rc == RC_RELOAD)
	goto play_reload;

    if(rc == RC_ERROR)
    {
	if(current_file_info->file_type == IS_OTHER_FILE)
	    current_file_info->file_type = IS_ERROR_FILE;
	if(last_rc == RC_REALLY_PREVIOUS)
	    return RC_REALLY_PREVIOUS;
    }
    last_rc = rc;
    return rc;
}

int dumb_pass_playing_list(int number_of_files, char *list_of_files[])
{
    #ifndef CFG_FOR_SF
    int i = 0;

    for(;;)
    {
	switch(play_midi_file(list_of_files[i]))
	{
	  case RC_REALLY_PREVIOUS:
	    if(i > 0)
		i--;
	    break;

	  default: /* An error or something */
	  case RC_NEXT:
	    if(i < number_of_files-1)
	    {
		i++;
		break;
	    }
	    aq_flush(0);

	    if(!(ctl->flags & CTLF_LIST_LOOP))
		return 0;
	    i = 0;
	    break;

	    case RC_QUIT:
		return 0;
	}
    }
    #endif
}

void default_ctl_lyric(int lyricid)
{
    char *lyric;

    lyric = event2string(lyricid);
    if(lyric != NULL)
	ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s", lyric + 1);
}

void ctl_mode_event(int type, int trace, ptr_size_t arg1, ptr_size_t arg2)
{
    CtlEvent ce;
    ce.type = type;
    ce.v1 = arg1;
    ce.v2 = arg2;
    if(trace && ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_note_event(int noteID)
{
    CtlEvent ce;

    ce.type = CTLE_NOTE;
    ce.v1 = voice[noteID].status;
    ce.v2 = voice[noteID].channel;
    ce.v3 = voice[noteID].note;
    ce.v4 = voice[noteID].velocity;
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}
///r
static void ctl_note_event2(uint8 channel, uint8 note, uint8 status, uint8 velocity)
{
    CtlEvent ce;

    ce.type = CTLE_NOTE;
    ce.v1 = status;
    ce.v2 = channel;
    ce.v3 = note;
    ce.v4 = velocity;
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_timestamp(void)
{
    long i, secs, voices;
    CtlEvent ce;
    static int last_secs = -1, last_voices = -1;

    secs = (long)(current_sample / (midi_time_ratio * play_mode->rate));
    for(i = voices = 0; i < upper_voices; i++)
	if(voice[i].status != VOICE_FREE)
	    voices++;
    if(secs == last_secs && voices == last_voices)
	return;
    ce.type = CTLE_CURRENT_TIME;
    ce.v1 = last_secs = secs;
    ce.v2 = last_voices = voices;
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_updatetime(int32 samples)
{
    long secs;
    secs = (long)(samples / (midi_time_ratio * play_mode->rate));
    ctl_mode_event(CTLE_CURRENT_TIME, 0, secs, 0);
    ctl_mode_event(CTLE_REFRESH, 0, 0, 0);
}

static void ctl_prog_event(int ch)
{
    CtlEvent ce;
    int bank, prog;

    if(IS_CURRENT_MOD_FILE)
    {
	bank = 0;
	prog = channel[ch].special_sample;
    }
    else
    {
	bank = channel[ch].bank;
	prog = channel[ch].program;
    }

    ce.type = CTLE_PROGRAM;
    ce.v1 = ch;
    ce.v2 = prog;
    ce.v3 = (ptr_size_t)channel_instrum_name(ch);
    ce.v4 = (bank |
	     (channel[ch].bank_lsb << 8) |
	     (channel[ch].bank_msb << 16));
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_pause_event(int pause, int32 s)
{
    long secs;
    secs = (long)(s / (midi_time_ratio * play_mode->rate));
    ctl_mode_event(CTLE_PAUSE, 0, pause, secs);
}
///r
char *channel_instrum_name(int ch)
{
    char *comm;
    int bank, prog;
	int elm = 0;

    if(ISDRUMCHANNEL(ch)) {
		bank = channel[ch].bank;
		prog = 0;
		instrument_map_no_mapped(channel[ch].mapID, &bank, &prog);
		if (drumset[bank] == NULL) return " ----";
		if(drumset[bank]->tone[prog][elm] == NULL) return " ----";
		comm = drumset[bank]->tone[prog][elm]->comment;
		if (comm == NULL) return " ----";
		return comm;
    }

    if(channel[ch].program == SPECIAL_PROGRAM)
	return "Special Program";

    if(IS_CURRENT_MOD_FILE)
    {
	int pr;
	pr = channel[ch].special_sample;
	if(pr > 0 &&
	   special_patch[pr] != NULL &&
	   special_patch[pr]->name != NULL)
	    return special_patch[pr]->name;
	return "MOD";
    }

    bank = channel[ch].bank;
    prog = channel[ch].program;
    instrument_map(channel[ch].mapID, &bank, &prog);

	if (tonebank[bank] == NULL) {alloc_instrument_bank(0, bank);}
	if (tonebank[bank]->tone[prog][elm]){
		if (tonebank[bank]->tone[prog][elm]->name) {
			comm = tonebank[bank]->tone[prog][elm]->comment;
		//	if (comm == NULL) {comm = tonebank[bank]->tone[prog][elm]->name;}
			if (comm == NULL) {comm = " ----";}
			return comm;
		}
	}
	if (tonebank[0]->tone[prog][elm]){
	    comm = tonebank[0]->tone[prog][elm]->comment;
	//	if (comm == NULL) {comm = tonebank[0]->tone[prog][elm]->name;}
		if (comm == NULL) {comm = " ----";}
	}else{
		comm = " ----";
	}	
    return comm;
}


/*
 * For MIDI stream player.
 */
void playmidi_stream_init(void)
{
    int i;
    static int first = 1;

    note_key_offset = key_adjust;
    midi_time_ratio = tempo_adjust;
    CLEAR_CHANNELMASK(channel_mute);
	if (temper_type_mute & 1)
		FILL_CHANNELMASK(channel_mute);
    midi_restart_time = 0;
    if(first)
    {
	first = 0;
        init_mblock(&playmidi_pool);
	current_file_info = get_midi_file_info("TiMidity", 1);
    midi_streaming=1;
    }
    else
        reuse_mblock(&playmidi_pool);

    /* Fill in current_file_info */
    current_file_info->readflag = 1;
    safe_free(current_file_info->seq_name);
    current_file_info->seq_name = safe_strdup("TiMidity server");
    safe_free(current_file_info->karaoke_title);
    safe_free(current_file_info->first_text);
    current_file_info->karaoke_title = current_file_info->first_text = NULL;
    current_file_info->mid = 0x7f;
    current_file_info->hdrsiz = 0;
    current_file_info->format = 0;
    current_file_info->tracks = 0;
    current_file_info->divisions = 192; /* ?? */
    current_file_info->time_sig_n = 4; /* 4/ */
    current_file_info->time_sig_d = 4; /* /4 */
    current_file_info->time_sig_c = 24; /* clock */
    current_file_info->time_sig_b = 8;  /* q.n. */
    current_file_info->samples = 0;
    current_file_info->max_channel = MAX_CHANNELS;
    current_file_info->compressed = 0;
    current_file_info->midi_data = NULL;
    current_file_info->midi_data_size = 0;
    current_file_info->file_type = IS_OTHER_FILE;

    current_play_tempo = 500000;
    check_eot_flag = 0;

    /* Setup default drums */
	COPY_CHANNELMASK(current_file_info->drumchannels, default_drumchannels);
	COPY_CHANNELMASK(current_file_info->drumchannel_mask, default_drumchannel_mask);
    for(i = 0; i < MAX_CHANNELS; i++)
	memset(channel[i].drums, 0, sizeof(channel[i].drums));
    change_system_mode(DEFAULT_SYSTEM_MODE);
    reset_midi(0);

    playmidi_tmr_reset();
}

void playmidi_tmr_reset(void)
{
    int i;

    aq_flush(0);
    if(ctl->id_character != 'N')
        current_sample = 0;
    buffered_count = 0;
    buffer_pointer = common_buffer;
    for(i = 0; i < MAX_CHANNELS; i++)
		channel[i].lasttime = -M_32BIT;
}


void playmidi_stream_free(void)
{
	reuse_mblock(&playmidi_pool);
}



/*! initialize Part EQ (XG) */
void init_part_eq_xg(struct part_eq_xg *p)
{
	p->bass = 0x40;
	p->treble = 0x40;	
	p->mid_bass = 0x40;
	p->mid_treble = 0x40;
	p->bass_freq = 0x0C;
	p->treble_freq = 0x36;
	p->mid_bass_freq = 0x22;
	p->mid_treble_freq = 0x2E;	
	p->bass_q = 0x07;
	p->treble_q = 0x07;
	p->mid_bass_q = 0x07;
	p->mid_treble_q = 0x07;
	p->bass_shape= 0x0; // def shelving
	p->treble_shape = 0x0; // def shelving
	p->valid = 0;	
	recompute_part_eq_xg(p);
}

static FLOAT_T clip_part_eq_gain(int in)
{
	if(in < 0)
		in = 0;
	else if(in > 127)
		in = 127;
	in -= 0x40;
	return (FLOAT_T)in * DIV_64 * 12.0; // -12.0~+12.0
}

static FLOAT_T clip_part_eq_q(int in)
{
	if(in < 1)
		in = 1;
	else if(in > 120)
		in = 120;	
	return (FLOAT_T)in * DIV_10; // 0.1 ~ 12.0
}

/*! recompute Part EQ (XG) */
void recompute_part_eq_xg(struct part_eq_xg *p)
{
	FLOAT_T freq, gain, q;
	int8 shape, valid = 0;

	if(p->bass != 0x40) {
		++valid;
		if     (p->bass_freq < 0x04)        p->bass_freq = 0x04;
		else if(p->bass_freq > 0x28)        p->bass_freq = 0x28;
		freq = eq_freq_table_xg[p->bass_freq];
		gain = clip_part_eq_gain(p->bass);
		q = clip_part_eq_q(p->bass_q);
		shape = p->bass_shape ? FILTER_PEAKING : FILTER_SHELVING_LOW;
		init_sample_filter2(&(p->basss), freq, gain, q, shape);
	} else {
		init_sample_filter2(&(p->basss), 0, 0, 0, FILTER_NONE);
	}
	if(p->treble != 0x40) {
		++valid;
		if     (p->treble_freq < 0x1C)      p->treble_freq = 0x1C;
		else if(p->treble_freq > 0x3A)      p->treble_freq = 0x3A;
		freq = eq_freq_table_xg[p->treble_freq];
		gain = clip_part_eq_gain(p->treble);
		q = clip_part_eq_q(p->treble_q);
		shape = p->treble_shape ? FILTER_PEAKING : FILTER_SHELVING_HI;
		init_sample_filter2(&(p->trebles), freq, gain, q, shape);
	} else {
		init_sample_filter2(&(p->trebles), 0, 0, 0, FILTER_NONE);
	}
	if(p->mid_bass != 0x40) {
		++valid;
		if     (p->mid_bass_freq < 0x0E)    p->mid_bass_freq = 0x0E;
		else if(p->mid_bass_freq > 0x36)    p->mid_bass_freq = 0x36;
		freq = eq_freq_table_xg[p->mid_bass_freq];
		gain = clip_part_eq_gain(p->mid_bass);
		q = clip_part_eq_q(p->mid_bass_q);
		init_sample_filter2(&(p->mid_basss), freq, gain, q, FILTER_PEAKING);
	} else {
		init_sample_filter2(&(p->mid_basss), 0, 0, 0, FILTER_NONE);
	}
	if(p->mid_treble != 0x40) {
		++valid;
		if     (p->mid_treble_freq < 0x0E)  p->mid_treble_freq = 0x0E;
		else if(p->mid_treble_freq > 0x36)  p->mid_treble_freq = 0x36;
		freq = eq_freq_table_xg[p->mid_treble_freq];
		gain = clip_part_eq_gain(p->mid_treble);
		q = clip_part_eq_q(p->mid_treble_q);
		init_sample_filter2(&(p->mid_trebles), freq, gain, q, FILTER_PEAKING);
	} else {
		init_sample_filter2(&(p->mid_trebles), 0, 0, 0, FILTER_NONE);
	}
	p->valid = valid ? 1 : 0;
}

static void init_midi_controller(midi_controller *p)
{
	p->mode = 0;
	p->num = -1;
	p->val = 0;
	p->pitch = 0;
	p->cutoff = 0;
	p->amp = 0.0;
	p->lfo1_rate = p->lfo2_rate = 0;
	p->lfo1_pitch_depth = p->lfo2_pitch_depth = 0;
	p->lfo1_tvf_depth = p->lfo2_tvf_depth = 0;
	p->lfo1_tva_depth = p->lfo2_tva_depth = 0;
}

static inline float get_midi_controller_amp(midi_controller *p)
{
	return (float)p->val * DIV_127 * p->amp; // -1.0 ~ +1.0
}

static inline float get_midi_controller_filter_cutoff(midi_controller *p)
{
	return ((float)p->val * DIV_127 * (float)p->cutoff);
}

static inline int32 get_midi_controller_pitch(midi_controller *p)
{
	return ((int32)(p->val * p->pitch) << 6);
}

static inline float get_midi_controller_lfo1_pitch_depth(midi_controller *p)
{
//	return (int16)((float)p->val * (float)p->lfo1_pitch_depth * (1.0f * DIV_127 * 256.0 / 400.0));
	return (float)p->val * DIV_127 * (float)p->lfo1_pitch_depth; // return cent
}

static inline float get_midi_controller_lfo1_amp_depth(midi_controller *p)
{
	return (float)p->val * DIV_127 * (float)p->lfo1_tva_depth; // * (1.0f);
}

static inline float get_midi_controller_lfo1_filter_depth(midi_controller *p)
{
	return ((float)p->val * DIV_127 * (float)p->lfo1_tvf_depth);
}

static inline float get_midi_controller_lfo2_pitch_depth(midi_controller *p)
{
//	return (int16)((float)p->val * (float)p->lfo2_pitch_depth * (1.0f * DIV_127 * 256.0 / 400.0));
	return (float)p->val * DIV_127 * (float)p->lfo2_pitch_depth; // return cent
}

static inline float get_midi_controller_lfo2_amp_depth(midi_controller *p)
{
	return (float)p->val * DIV_127 * (float)p->lfo2_tva_depth; // * (1.0f);
}

static inline float get_midi_controller_lfo2_filter_depth(midi_controller *p)
{
	return (float)p->val * DIV_127 * (float)p->lfo2_tvf_depth;
}


static void init_rx(int ch)
{
	channel[ch].rx = 0xFFFFFFFF;	/* all on */
}

static void set_rx(int ch, int32 rx, int flag)
{
	if(ch > MAX_CHANNELS) {return;}
	if(flag) {channel[ch].rx |= rx;}
	else {channel[ch].rx &= ~rx;}
}

static int32 get_rx(int ch, int32 rx)
{
	if(ch > MAX_CHANNELS) {return 0;}
	return (channel[ch].rx & rx);
}

static void init_rx_drum(struct DrumParts *p)
{
	p->rx = 0xFFFFFFFF;	/* all on */
}

static void set_rx_drum(struct DrumParts *p, int32 rx, int flag)
{
	if(flag) {p->rx |= rx;}
	else {p->rx &= ~rx;}
}

static int32 get_rx_drum(struct DrumParts *p, int32 rx)
{
	return (p->rx & rx);
}

int32 get_current_play_tempo(void)
{
	return current_play_tempo;	// 500000=120bpm
}




#ifdef MULTI_THREAD_COMPUTE
#include "thread_playmidi.c"
#endif



