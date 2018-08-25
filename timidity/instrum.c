/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>
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

    instrum.c

    Code to load and unload GUS-compatible instrument patches.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <math.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "resample.h"
#include "tables.h"
#include "filter.h"
#include "quantity.h"
#include "freq.h"
#include "support.h"
#include "dls.h"
#include "sfz.h"

#define INSTRUMENT_HASH_SIZE 128
struct InstrumentCache
{
    char *name;
    int panning, amp, note_to_use, strip_loop, strip_envelope, strip_tail;
    Instrument *ip;
    struct InstrumentCache *next;
};
static struct InstrumentCache *instrument_cache[INSTRUMENT_HASH_SIZE];

/* Some functions get aggravated if not even the standard banks are
   available. */
static ToneBank standard_tonebank, standard_drumset;
ToneBank
  *tonebank[128 + MAP_BANK_COUNT] = {&standard_tonebank};
ToneBank
  *drumset[128 + MAP_BANK_COUNT] = {&standard_drumset};

/* bank mapping (mapped bank) */
struct bank_map_elem {
	int16 used, mapid;
	int bankno;
};
static struct bank_map_elem map_bank[MAP_BANK_COUNT], map_drumset[MAP_BANK_COUNT];
static int map_bank_counter;

/* This is a special instrument, used for all melodic programs */
Instrument *default_instrument=0;
SpecialPatch *special_patch[NSPECIAL_PATCH];
int progbase = 0;
struct inst_map_elem
{
    int set, elem, mapped;
};

static struct inst_map_elem *inst_map_table[NUM_INST_MAP][128];

/* This is only used for tracks that don't specify a program */
int default_program[MAX_CHANNELS];
///r
int special_program[MAX_CHANNELS];


char *default_instrument_name = NULL;

int antialiasing_allowed=0;
#ifdef FAST_DECAY
int fast_decay=1;
#else
int fast_decay=0;
#endif
int opt_print_fontname = 0;

/*Pseudo Reverb*/
int32 modify_release;

/** below three functinos are imported from sndfont.c **/

/* convert from 8bit value to fractional offset (15.15) */
static int32 to_offset(int offset)
{
	return (int32)offset << (7+15);
}

/* calculate ramp rate in fractional unit;
 * diff = 8bit, time = msec
 */
static int32 calc_rate(int diff, double msec)
{
    double rate;

    if(msec < 6)
	msec = 6;
    if(diff == 0)
	diff = 255;
    diff <<= (7+15);
    rate = ((double)diff / play_mode->rate) * control_ratio * 1000.0 / msec;
    if(fast_decay)
	rate *= 2;
    return (int32)rate;
}
/*End of Pseudo Reverb*/

void free_instrument(Instrument *ip)
{
	Sample *sp;
	int i;
	extern void free_pcm_sample_file(Instrument*); /* from smplfile.c */
#ifdef INT_SYNTH
	extern void free_int_synth_file(Instrument *ip);
#endif
#ifdef ENABLE_DLS
	extern void free_dls_file(Instrument *ip);
#endif
#ifdef ENABLE_SFZ
	extern void free_sfz_file(Instrument *ip);
#endif

	if (!ip) return;

	for (i=0; i<ip->samples; i++) {
		sp=&(ip->sample[i]);
		if(sp->data_alloced) {
			sp->data_alloced = 0;
			safe_free(sp->data);
			sp->data = NULL;
		}
	}
	safe_free(ip->sample);
	ip->sample = NULL;

	switch(ip->type){
	case INST_PCM:
		free_pcm_sample_file(ip);
		break;
#ifdef INT_SYNTH
	case INST_MMS:
	case INST_SCC:
		free_int_synth_file(ip);
		break;
#endif
#ifdef ENABLE_SFZ
	case INST_SFZ:
		free_sfz_file(ip);
		break;
#endif
#ifdef ENABLE_DLS
	case INST_DLS:
		free_dls_file(ip);
		break;
#endif
	}
	safe_free(ip);
}
///r
void clear_magic_instruments(void)
{
    int i, j;
	int elm = 0;

    for(j = 0; j < 128 + map_bank_counter; j++)
    {
	if(tonebank[j])
	{
	    ToneBank *bank = tonebank[j];
	    for(i = 0; i < 128; i++)
			for(elm = 0; elm < MAX_ELEMENT; elm++)
				if(bank->tone[i][elm])
					if(IS_MAGIC_INSTRUMENT(bank->tone[i][elm]->instrument))
						bank->tone[i][elm]->instrument = NULL;
	}
	if(drumset[j])
	{
	    ToneBank *bank = drumset[j];
	    for(i = 0; i < 128; i++)
			for(elm = 0; elm < MAX_ELEMENT; elm++)
				if(bank->tone[i][elm])
					if(IS_MAGIC_INSTRUMENT(bank->tone[i][elm]->instrument))
						bank->tone[i][elm]->instrument = NULL;
	}
    }
}

#define GUS_ENVRATE_MAX (int32)(0x3FFFFFFF >> 9)

static int32 convert_envelope_rate(uint8 rate)
{
  int32 r;

  r=3-((rate>>6) & 0x3);
  r*=3;
  r = (int32)(rate & 0x3f) << r; /* 6.9 fixed point */

  /* 15.15 fixed point. */
  r = r * 44100 / play_mode->rate * control_ratio * (1 << fast_decay);
  if(r > GUS_ENVRATE_MAX) {r = GUS_ENVRATE_MAX;}
  return (r << 9);
}

static int32 convert_envelope_offset(uint8 offset)
{
  /* This is not too good... Can anyone tell me what these values mean?
     Are they GUS-style "exponential" volumes? And what does that mean? */

  /* 15.15 fixed point */
  return offset << (7+15);
}

static int32 convert_gus_lfo_sweep(uint8 sweep)
{
	if (!sweep)
		return 0;
	return 1000 * sweep / 38; // SWEEP_TUNING
}

static int32 convert_gus_lfo_rate(uint8 rate)
{
	if (!rate)
		return 0;
	return 1000 * rate / 38; // TREMOLO_RATE_TUNING, VIBRATO_RATE_TUNING
}

static void reverse_data(int16 *sp, int32 ls, int32 le)
{
  int16 s, *ep=sp+le;
  int32 i;
  sp+=ls;
  le-=ls;
  le/=2;
  for(i = 0; i < le; i++)
  {
      s=*sp;
      *sp++=*ep;
      *ep--=s;
  }
}

static int name_hash(char *name)
{
    unsigned int addr = 0;

    while(*name)
	addr += *name++;
    return addr % INSTRUMENT_HASH_SIZE;
}

static Instrument *search_instrument_cache(char *name,
				int panning, int amp, int note_to_use,
				int strip_loop, int strip_envelope,
				int strip_tail)
{
    struct InstrumentCache *p;

    for(p = instrument_cache[name_hash(name)]; p != NULL; p = p->next)
    {
	if(strcmp(p->name, name) != 0)
	    return NULL;
	if(p->panning == panning &&
	   p->amp == amp &&
	   p->note_to_use == note_to_use &&
	   p->strip_loop == strip_loop &&
	   p->strip_envelope == strip_envelope &&
	   p->strip_tail == strip_tail)
	    return p->ip;
    }
    return NULL;
}

static void store_instrument_cache(Instrument *ip,
				   char *name,
				   int panning, int amp, int note_to_use,
				   int strip_loop, int strip_envelope,
				   int strip_tail)
{
    struct InstrumentCache *p;
    int addr;

    addr = name_hash(name);
    p = (struct InstrumentCache *)safe_malloc(sizeof(struct InstrumentCache));
    p->next = instrument_cache[addr];
    instrument_cache[addr] = p;
    p->name = name;
    p->panning = panning;
    p->amp = amp;
    p->note_to_use = note_to_use;
    p->strip_loop = strip_loop;
    p->strip_envelope = strip_envelope;
    p->strip_tail = strip_tail;
    p->ip = ip;
}

static int32 adjust_tune_freq(int32 val, float tune)
{
	if (! tune)
		return val;
	return val / pow(2.0, tune * DIV_12);
}

static int16 adjust_scale_tune(int16 val)
{
	return 1024 * (double) val / 100 + 0.5;
}

static int16 adjust_fc(int16 val)
{
	if (val < 0 || val > play_mode->rate / 2) {
		return 0;
	} else {
		return val;
	}
}

static int16 adjust_reso(int16 val)
{
	if (val < 0 || val > 960) {
		return 0;
	} else {
		return val;
	}
}

static int32 to_rate(int rate)
{
	return (rate) ? (int32) (0x200 * pow(2.0, rate / 17.0)
			* 44100 / play_mode->rate * control_ratio) << fast_decay : 0;
}

#if 0
static int32 to_control(int control)
{
	return (int32) (0x2000 / pow(2.0, control / 31.0));
}
#endif

///r
static void apply_bank_parameter(Instrument *ip, ToneBankElement *tone)
{
	int i, j, k;
	Sample *sp;

	// timidity.c read_config_file() "soundfont" // init tonebank で reinit_tone_bank_element()
	// soundfontコマンドのときtonebank初期状態がall0になるのを回避

	/* amp tuning */
	if (tone->amp != -1) {
		for (i = 0; i < ip->samples; i++)
#if 1 // float (timidity.c set_gus_patchconf_opts()
			ip->sample[i].cfg_amp = (FLOAT_T)tone->amp * DIV_12BIT;
#else
			ip->sample[i].cfg_amp = (FLOAT_T)tone->amp * DIV_100;
#endif
	}
	/* normalize volume */
	if (tone->amp_normalize == 1) {
		FLOAT_T volume_max = 0;
		for (i = 0; i < ip->samples; i++)
			if (volume_max < ip->sample[i].volume)
				volume_max = ip->sample[i].volume;
		if (volume_max != 0){
			volume_max = 1.0 / volume_max; // div to mul
			for (i = 0; i < ip->samples; i++)
				ip->sample[i].volume *= volume_max;
		}
	}
	/* panning */
	if (tone->def_pan != -1) {
		int pan = ((int) tone->def_pan & 0x7f);
		for (i = 0; i < ip->samples; i++) {
			ip->sample[i].def_pan = pan;
		}
	}
	/* sample panning */
	if ((tone->sample_pan != -1 || tone->sample_width != -1)) {
		FLOAT_T span = (tone->sample_pan == -1) ? 
			0.0 : ((FLOAT_T)(tone->sample_pan - 200) * DIV_200); // def 0 , offset 200
		FLOAT_T swid = (tone->sample_width == -1) ? 
			1.0 : ((FLOAT_T)(tone->sample_width - 800) * DIV_100); // def 100 , offset 800
		for (i = 0; i < ip->samples; i++) {
			FLOAT_T panning = ip->sample[i].sample_pan;
			panning *= swid;
			if(panning > 0.5)
				panning = 0.5;
			else if(panning < -0.5)
				panning = -0.5;
			panning += span;
			if(panning > 0.5)
				panning = 0.5;
			else if(panning < -0.5)
				panning = -0.5;
			ip->sample[i].sample_pan = panning;
		}
	}
	/* note to use */
	if (tone->note != -1)
		for (i = 0; i < ip->samples; i++){
			ip->sample[i].root_key = tone->note & 0x7f;
			ip->sample[i].root_freq = freq_table[tone->note & 0x7f];
		}
	/* filter key-follow */
	if (tone->key_to_fc != 0)
		for (i = 0; i < ip->samples; i++)
			ip->sample[i].key_to_fc = tone->key_to_fc;
	/* filter velocity-follow */
	if (tone->vel_to_fc != 0)
		for (i = 0; i < ip->samples; i++)
			ip->sample[i].vel_to_fc = tone->vel_to_fc;
	/* resonance velocity-follow */
	if (tone->vel_to_resonance != 0)
		for (i = 0; i < ip->samples; i++){
			sp = &ip->sample[i];
			ip->sample[i].vel_to_resonance = tone->vel_to_resonance;
		}
	/* strip tail */
	if (tone->strip_tail == 1)
		for (i = 0; i < ip->samples; i++){
			ip->sample[i].data_length = ip->sample[i].loop_end;
		}
	/* lpf_type */
	if (tone->lpf_type != -1)	
		for (i = 0; i < ip->samples; i++){
			ip->sample[i].lpf_type = tone->lpf_type;
		}
	/* keep_voice */
	if(tone->keep_voice != 0)
		for (i = 0; i < ip->samples; i++){
			ip->sample[i].keep_voice = tone->keep_voice;
		}

	
	/* sample_lokey/hikey/lovel/hivel */
	if (tone->sample_lokeynum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->sample_lokeynum == 1) {
				if (tone->sample_lokey[0] >= 0)
					sp->low_key = tone->sample_lokey[0] & 0x7f;
			} else if (i < tone->sample_lokeynum) {
				if (tone->sample_lokey[i] >= 0)
					sp->low_key = tone->sample_lokey[i] & 0x7f;
			}
		}
	if (tone->sample_hikeynum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->sample_hikeynum == 1) {
				if (tone->sample_hikey[0] >= 0)
					sp->high_key = tone->sample_hikey[0] & 0x7f;
			} else if (i < tone->sample_hikeynum) {
				if (tone->sample_hikey[i] >= 0)
					sp->high_key = tone->sample_hikey[i] & 0x7f;
			}
		}
	if (tone->sample_lovelnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->sample_lovelnum == 1) {
				if (tone->sample_lovel[0] >= 0)
					sp->low_vel = tone->sample_lovel[0];
			} else if (i < tone->sample_lovelnum) {
				if (tone->sample_lovel[i] >= 0)
					sp->low_vel = tone->sample_lovel[i];
			}
		}
	if (tone->sample_hivelnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->sample_hivelnum == 1) {
				if (tone->sample_hivel[0] >= 0)
					sp->high_vel = tone->sample_hivel[0];
			} else if (i < tone->sample_hivelnum) {
				if (tone->sample_hivel[i] >= 0)
					sp->high_vel = tone->sample_hivel[i];
			}
		}
	/* after sample_lokey/hikey/lovel/hivel */
	if (tone->lokey != -1){
		int32 key = tone->lokey & 0x7f;
		for (i = 0; i < ip->samples; i++){			
			sp = &ip->sample[i];
			if(sp->low_key < key)
				sp->low_key = key;
		}
	}
	if (tone->hikey != -1){
		int32 key = tone->hikey & 0x7f;
		for (i = 0; i < ip->samples; i++){
			sp = &ip->sample[i];
			if(sp->high_key > key)
				sp->high_key = key;
		}
	}
	if (tone->lovel != -1)
		for (i = 0; i < ip->samples; i++){
			sp = &ip->sample[i];
			if(sp->low_vel < tone->lovel)
				sp->low_vel = tone->lovel;
		}
	if (tone->hivel != -1)
		for (i = 0; i < ip->samples; i++){
			sp = &ip->sample[i];
			if(sp->high_vel > tone->hivel)
				sp->high_vel = tone->hivel;
		}

	if (tone->tunenum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tunenum == 1) {
			//	sp->root_freq = adjust_tune_freq(sp->root_freq, tone->tune[0]);
				sp->tune *= pow(2.0, (double)tone->tune[0] * DIV_12);
			} else if (i < tone->tunenum) {
			//	sp->root_freq = adjust_tune_freq(sp->root_freq, tone->tune[i]);
				sp->tune *= pow(2.0, (double)tone->tune[i] * DIV_12);
			}
		}
	if (tone->envratenum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->envratenum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->envrate[0][j] >= 0)
						sp->envelope_rate[j] = to_rate(tone->envrate[0][j]);
			} else if (i < tone->envratenum) {
				for (j = 0; j < 6; j++)
					if (tone->envrate[i][j] >= 0)
						sp->envelope_rate[j] = to_rate(tone->envrate[i][j]);
			}
		}
	if (tone->envofsnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->envofsnum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->envofs[0][j] >= 0)
						sp->envelope_offset[j] = to_offset(tone->envofs[0][j]);
			} else if (i < tone->envofsnum) {
				for (j = 0; j < 6; j++)
					if (tone->envofs[i][j] >= 0)
						sp->envelope_offset[j] = to_offset(tone->envofs[i][j]);
			}
		}
///r
	if (tone->tremnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tremnum == 1) {
				if (IS_QUANTITY_DEFINED(tone->trem[0][0]))
					sp->tremolo_sweep = quantity_to_int(&tone->trem[0][0], 0);
				if (IS_QUANTITY_DEFINED(tone->trem[0][1]))
					sp->tremolo_freq = quantity_to_int(&tone->trem[0][1], 0);
				if (IS_QUANTITY_DEFINED(tone->trem[0][2]))
					sp->tremolo_to_amp = quantity_to_int(&tone->trem[0][2], 0);
			} else if (i < tone->tremnum) {
				if (IS_QUANTITY_DEFINED(tone->trem[i][0]))
					sp->tremolo_sweep = quantity_to_int(&tone->trem[i][0], 0);
				if (IS_QUANTITY_DEFINED(tone->trem[i][1]))
					sp->tremolo_freq = quantity_to_int(&tone->trem[i][1], 0);
				if (IS_QUANTITY_DEFINED(tone->trem[i][2]))
					sp->tremolo_to_amp = quantity_to_int(&tone->trem[i][2], 0);
			}
		}		
	if (tone->tremdelaynum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tremdelaynum == 1)
				sp->tremolo_delay = tone->tremdelay[0];
			else if (i < tone->tremdelaynum)
				sp->tremolo_delay = tone->tremdelay[i];
		}
	if (tone->tremsweepnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tremsweepnum == 1)
				sp->tremolo_sweep = tone->tremsweep[0];
			else if (i < tone->tremsweepnum)
				sp->tremolo_sweep = tone->tremsweep[i];
		}
	if (tone->tremfreqnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tremfreqnum == 1)
				sp->tremolo_freq = tone->tremfreq[0];
			else if (i < tone->tremfreqnum)
				sp->tremolo_freq = tone->tremfreq[i];
		}
	if (tone->tremampnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tremampnum == 1)
				sp->tremolo_to_amp = tone->tremamp[0];
			else if (i < tone->tremampnum)
				sp->tremolo_to_amp = tone->tremamp[i];
		}
	if (tone->trempitchnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->trempitchnum == 1)
				sp->tremolo_to_pitch = tone->trempitch[0];
			else if (i < tone->trempitchnum)
				sp->tremolo_to_pitch = tone->trempitch[i];
		}
	if (tone->tremfcnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->tremfcnum == 1)
				sp->tremolo_to_fc = tone->tremfc[0];
			else if (i < tone->tremfcnum)
				sp->tremolo_to_fc = tone->tremfc[i];
		}

	if (tone->vibnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibnum == 1) {
				if (IS_QUANTITY_DEFINED(tone->vib[0][0]))
					sp->vibrato_sweep = quantity_to_int(&tone->vib[0][0], 0);
				if (IS_QUANTITY_DEFINED(tone->vib[0][1]))
					sp->vibrato_freq = quantity_to_int(&tone->vib[0][1], 0);
				if (IS_QUANTITY_DEFINED(tone->vib[0][2]))
					sp->vibrato_to_pitch = quantity_to_int(&tone->vib[0][2], 0);
			} else if (i < tone->vibnum) {
				if (IS_QUANTITY_DEFINED(tone->vib[i][0]))
					sp->vibrato_sweep = quantity_to_int(&tone->vib[i][0], 0);
				if (IS_QUANTITY_DEFINED(tone->vib[i][1]))
					sp->vibrato_freq = quantity_to_int(&tone->vib[i][1], 0);
				if (IS_QUANTITY_DEFINED(tone->vib[i][2]))
					sp->vibrato_to_pitch = quantity_to_int(&tone->vib[i][2], 0);
			}
		}
	if (tone->vibdelaynum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibdelaynum == 1)
				sp->vibrato_delay = tone->vibdelay[0];
			else if (i < tone->vibdelaynum)
				sp->vibrato_delay = tone->vibdelay[i];
		}
	if (tone->vibsweepnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibsweepnum == 1)
				sp->vibrato_sweep = tone->vibsweep[0];
			else if (i < tone->vibsweepnum)
				sp->vibrato_sweep = tone->vibsweep[i];
		}
	if (tone->vibfreqnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibfreqnum == 1)
				sp->vibrato_freq = tone->vibfreq[0];
			else if (i < tone->vibfreqnum)
				sp->vibrato_freq = tone->vibfreq[i];
		}
	if (tone->vibampnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibampnum == 1)
				sp->vibrato_to_amp = tone->vibamp[0];
			else if (i < tone->vibampnum)
				sp->vibrato_to_amp = tone->vibamp[i];
		}
	if (tone->vibpitchnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibpitchnum == 1)
				sp->vibrato_to_pitch = tone->vibpitch[0];
			else if (i < tone->vibpitchnum)
				sp->vibrato_to_pitch = tone->vibpitch[i];
		}
	if (tone->vibfcnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->vibfcnum == 1)
				sp->vibrato_to_fc = tone->vibfc[0];
			else if (i < tone->vibfcnum)
				sp->vibrato_to_fc = tone->vibfc[i];
		}
	if (tone->sclnotenum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->sclnotenum == 1)
				sp->scale_freq = tone->sclnote[0];
			else if (i < tone->sclnotenum)
				sp->scale_freq = tone->sclnote[i];
		}
	if (tone->scltunenum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->scltunenum == 1)
				sp->scale_factor = adjust_scale_tune(tone->scltune[0]);
			else if (i < tone->scltunenum)
				sp->scale_factor = adjust_scale_tune(tone->scltune[i]);
		}
	if (tone->modenvratenum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->modenvratenum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->modenvrate[0][j] >= 0)
						sp->modenv_rate[j] = to_rate(tone->modenvrate[0][j]);
			} else if (i < tone->modenvratenum) {
				for (j = 0; j < 6; j++)
					if (tone->modenvrate[i][j] >= 0)
						sp->modenv_rate[j] = to_rate(tone->modenvrate[i][j]);
			}
		}
	if (tone->modenvofsnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->modenvofsnum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->modenvofs[0][j] >= 0)
						sp->modenv_offset[j] =
								to_offset(tone->modenvofs[0][j]);
			} else if (i < tone->modenvofsnum) {
				for (j = 0; j < 6; j++)
					if (tone->modenvofs[i][j] >= 0)
						sp->modenv_offset[j] =
								to_offset(tone->modenvofs[i][j]);
			}
		}
	if (tone->envkeyfnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->envkeyfnum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->envkeyf[0][j] != -1)
						sp->envelope_keyf[j] = tone->envkeyf[0][j];
			} else if (i < tone->envkeyfnum) {
				for (j = 0; j < 6; j++)
					if (tone->envkeyf[i][j] != -1)
						sp->envelope_keyf[j] = tone->envkeyf[i][j];
			}
		}
	if (tone->envvelfnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->envvelfnum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->envvelf[0][j] != -1)
						sp->envelope_velf[j] = tone->envvelf[0][j];
			} else if (i < tone->envvelfnum) {
				for (j = 0; j < 6; j++)
					if (tone->envvelf[i][j] != -1)
						sp->envelope_velf[j] = tone->envvelf[i][j];
			}
		}
	if (tone->modenvkeyfnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->modenvkeyfnum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->modenvkeyf[0][j] != -1)
						sp->modenv_keyf[j] = tone->modenvkeyf[0][j];
			} else if (i < tone->modenvkeyfnum) {
				for (j = 0; j < 6; j++)
					if (tone->modenvkeyf[i][j] != -1)
						sp->modenv_keyf[j] = tone->modenvkeyf[i][j];
			}
		}
	if (tone->modenvvelfnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->modenvvelfnum == 1) {
				for (j = 0; j < 6; j++)
					if (tone->modenvvelf[0][j] != -1)
						sp->modenv_velf[j] = tone->modenvvelf[0][j];
			} else if (i < tone->modenvvelfnum) {
				for (j = 0; j < 6; j++)
					if (tone->modenvvelf[i][j] != -1)
						sp->modenv_velf[j] = tone->modenvvelf[i][j];
			}
		}
	if (tone->modpitchnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->modpitchnum == 1)
				sp->modenv_to_pitch = tone->modpitch[0];
			else if (i < tone->modpitchnum)
				sp->modenv_to_pitch = tone->modpitch[i];
		}
	if (tone->modfcnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->modfcnum == 1)
				sp->modenv_to_fc = tone->modfc[0];
			else if (i < tone->modfcnum)
				sp->modenv_to_fc = tone->modfc[i];
		}
	if (tone->fcnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->fcnum == 1)
				sp->cutoff_freq = adjust_fc(tone->fc[0]);
			else if (i < tone->fcnum)
				sp->cutoff_freq = adjust_fc(tone->fc[i]);
		}
	if (tone->resonum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->resonum == 1) {
				sp->resonance = adjust_reso(tone->reso[0]);
			} else if (i < tone->resonum) {
				sp->resonance = adjust_reso(tone->reso[i]);
			}
		}
///r
	if (tone->fclownum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->fclownum == 1)
				sp->cutoff_low_limit = tone->fclow[0];
			else if (i < tone->fclownum)
				sp->cutoff_low_limit = tone->fclow[i];
		}
	if (tone->fclowkeyfnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->fclowkeyfnum == 1)
				sp->cutoff_low_keyf = tone->fclowkeyf[0];
			else if (i < tone->fclowkeyfnum)
				sp->cutoff_low_keyf = tone->fclowkeyf[i];
		}
	if (tone->fcmulnum) // after fc
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->fcmulnum == 1 && sp->cutoff_freq)
				sp->cutoff_freq *= pow((FLOAT_T)2.0, (FLOAT_T)tone->fcmul[0] * DIV_1200);
			else if (i < tone->fcmulnum && sp->cutoff_freq)
				sp->cutoff_freq *= pow((FLOAT_T)2.0, (FLOAT_T)tone->fcmul[i] * DIV_1200);
		}
	if (tone->fcaddnum) // after fc
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->fcaddnum == 1 && sp->cutoff_freq)
				sp->cutoff_freq += tone->fcadd[0];
			else if (i < tone->fcaddnum && sp->cutoff_freq)
				sp->cutoff_freq += tone->fcadd[i];
		}
	if (tone->pitenvnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->pitenvnum == 1) {
				if (tone->pitenv[0][0] != 0)
					sp->pitch_envelope[0] = tone->pitenv[0][0]; // cent init
				if (tone->pitenv[0][1] != 0)
					sp->pitch_envelope[1] = tone->pitenv[0][1]; // cent attack
				if (tone->pitenv[0][2] >= 1)
					sp->pitch_envelope[2] = tone->pitenv[0][2]; // ms attack
				if (tone->pitenv[0][3] != 0)
					sp->pitch_envelope[3] = tone->pitenv[0][3]; // cent decay1
				if (tone->pitenv[0][4] >= 1)
					sp->pitch_envelope[4] = tone->pitenv[0][4]; // ms decay1
				if (tone->pitenv[0][5] != 0)
					sp->pitch_envelope[5] = tone->pitenv[0][5]; // cent decay2
				if (tone->pitenv[0][6] >= 1)
					sp->pitch_envelope[6] = tone->pitenv[0][6]; // ms decay2
				if (tone->pitenv[0][7] != 0)
					sp->pitch_envelope[7] = tone->pitenv[0][7]; // cent release
				if (tone->pitenv[0][8] >= 1)
					sp->pitch_envelope[8] = tone->pitenv[0][8]; // ms release
			} else if (i < tone->pitenvnum) {
				if (tone->pitenv[i][0] != 0)
					sp->pitch_envelope[0] = tone->pitenv[i][0]; // cent
				if (tone->pitenv[i][1] != 0)
					sp->pitch_envelope[1] = tone->pitenv[i][1]; // cent
				if (tone->pitenv[i][2] >= 1)
					sp->pitch_envelope[2] = tone->pitenv[i][2]; // ms
				if (tone->pitenv[i][3] != 0)
					sp->pitch_envelope[3] = tone->pitenv[i][3]; // cent
				if (tone->pitenv[i][4] >= 1)
					sp->pitch_envelope[4] = tone->pitenv[i][4]; // ms
				if (tone->pitenv[i][5] != 0)
					sp->pitch_envelope[5] = tone->pitenv[i][5]; // cent
				if (tone->pitenv[i][6] >= 1)
					sp->pitch_envelope[6] = tone->pitenv[i][6]; // ms
				if (tone->pitenv[i][7] != 0)
					sp->pitch_envelope[7] = tone->pitenv[i][7]; // cent
				if (tone->pitenv[i][8] >= 1)
					sp->pitch_envelope[8] = tone->pitenv[i][8]; // ms
			}
		}
	if (tone->hpfnum)
		for (i = 0; i < ip->samples; i++) {
			sp = &ip->sample[i];
			if (tone->hpfnum == 1) {
				for (j = 0; j < 3; j++)
					sp->hpf[j] = tone->hpf[0][j];
			} else if (i < tone->hpfnum) {
				for (j = 0; j < 3; j++)
					sp->hpf[j] = tone->hpf[i][j];
			}
		}
	for (k = 0; k < VOICE_EFFECT_NUM; k++){
		if (tone->vfxnum[k])
			for (i = 0; i < ip->samples; i++) {
				sp = &ip->sample[i];
				if (tone->vfxnum[k] == 1) {
					for (j = 0; j < VOICE_EFFECT_PARAM_NUM; j++)
						sp->vfx[k][j] = tone->vfx[k][0][j];
				} else if (i < tone->vfxnum[k]) {
					for (j = 0; j < VOICE_EFFECT_PARAM_NUM; j++)
						sp->vfx[k][j] = tone->vfx[k][i][j];
				}
			}
	}
}

#define READ_CHAR(thing) { \
		uint8 tmpchar; \
		\
		if (tf_read(&tmpchar, 1, 1, tf) != 1) \
			goto fail; \
		thing = tmpchar; \
}
#define READ_SHORT(thing) { \
		uint16 tmpshort; \
		\
		if (tf_read(&tmpshort, 2, 1, tf) != 1) \
			goto fail; \
		thing = LE_SHORT(tmpshort); \
}
#define READ_LONG(thing) { \
		int32 tmplong; \
		\
		if (tf_read(&tmplong, 4, 1, tf) != 1) \
			goto fail; \
		thing = LE_LONG(tmplong); \
}

/* If panning or note_to_use != -1, it will be used for all samples,
 * instead of the sample-specific values in the instrument file.
 *
 * For note_to_use, any value < 0 or > 127 will be forced to 0.
 *
 * For other parameters, 1 means yes, 0 means no, other values are
 * undefined.
 *
 * TODO: do reverse loops right
 */
static Instrument *load_gus_instrument(char *name,
		ToneBank *bank, int dr, int prog, char *infomsg, int elm)
{
	ToneBankElement *tone;
	int amp, panning, note_to_use; // c191-
	int strip_envelope, strip_loop, strip_tail;
	Instrument *ip;
	struct timidity_file *tf;
	uint8 tmp[FILEPATH_MAX], fractions;
	Sample *sp;
	int i, j, noluck = 0;
	int32 low_freq, high_freq, root_freq;

	
	if (! name)
		return 0;
	if (infomsg != NULL)
		ctl->cmsg(CMSG_INFO, VERB_NOISY, "%s: %s", infomsg, name);
	else
		ctl->cmsg(CMSG_INFO, VERB_NOISY, "Loading instrument %s", name);
	if (!bank) {
		tone = NULL;
	//	amp = note_to_use = panning = -1;
		strip_envelope = strip_loop = strip_tail = 0;
	}else if (bank->tone[prog][elm] == NULL) {
		tone = NULL;
	//	amp = note_to_use = panning = -1;
		strip_envelope = strip_loop = strip_tail = 0;
	}else{
		tone = bank->tone[prog][elm];		
		amp = panning = -1;
	//	amp = tone->amp;
	//	note_to_use = (tone->note != -1) ? tone->note : ((dr) ? prog : -1);
		note_to_use = (dr) ? prog : -1; // c191
	//	panning = tone->def_pan;	
		amp = panning = -1; // 
		strip_envelope = (tone->strip_envelope != -1)
				? tone->strip_envelope : ((dr) ? 1 : -1);
		strip_loop = (tone->strip_loop != -1)
				? tone->strip_loop : ((dr) ? 1 : -1);
		strip_tail = tone->strip_tail;
	}
	amp = note_to_use = panning = -1;
	if (tone && tone->tunenum == 0
			&& tone->envratenum == 0 && tone->envofsnum == 0
			&& tone->tremnum == 0 && tone->vibnum == 0
			&& tone->sclnotenum == 0 && tone->scltunenum == 0
			&& tone->modenvratenum == 0 && tone->modenvofsnum == 0
			&& tone->envkeyfnum == 0 && tone->envvelfnum == 0
			&& tone->modenvkeyfnum == 0 && tone->modenvvelfnum == 0
			&& tone->trempitchnum == 0 && tone->tremfcnum == 0
			&& tone->modpitchnum == 0 && tone->modfcnum == 0
			&& tone->fcnum == 0 && tone->resonum == 0)
		if ((ip = search_instrument_cache(name, panning, amp, note_to_use,
				strip_loop, strip_envelope, strip_tail)) != NULL) {
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * Cached");
			return ip;
		}
	/* Open patch file */
	if (! (tf = open_file_r(name, 2, OF_NORMAL))) {
#ifdef PATCH_EXT_LIST
		size_t name_len, ext_len;
		static char *patch_ext[] = PATCH_EXT_LIST;
#endif
		
		noluck = 1;
#ifdef PATCH_EXT_LIST
		name_len = strlen(name);
		/* Try with various extensions */
		for (i = 0; patch_ext[i]; i++) {
			ext_len = strlen(patch_ext[i]);
			if (name_len + ext_len < FILEPATH_MAX) {
				if (name_len >= ext_len && strcmp(name + name_len - ext_len,
						patch_ext[i]) == 0)
					continue;	/* duplicated ext. */
				strcpy((char *) tmp, name);
				strcat((char *) tmp, patch_ext[i]);
				if ((tf = open_file_r((char *) tmp, 1, OF_NORMAL))) {
					noluck = 0;
					break;
				}
			}
		}
#endif
	}
	if (noluck) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"Instrument `%s' can't be found.", name);
		return 0;
	}
	/* Read some headers and do cursory sanity checks. There are loads
	 * of magic offsets.  This could be rewritten...
	 */
	tmp[0] = tf_getc(tf);
	if (tmp[0] == '\0') {
		/* for Mac binary */
		skip(tf, 127);
		tmp[0] = tf_getc(tf);
	}
	if ((tf_read(tmp + 1, 1, 238, tf) != 238)
			|| (memcmp(tmp, "GF1PATCH110\0ID#000002", 22)
			&& memcmp(tmp, "GF1PATCH100\0ID#000002", 22))) {
			/* don't know what the differences are */
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: not an instrument", name);
		close_file(tf);
		return 0;
	}
	/* instruments.  To some patch makers, 0 means 1 */
	if (tmp[82] != 1 && tmp[82] != 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"Can't handle patches with %d instruments", tmp[82]);
		close_file(tf);
		return 0;
	}
	if (tmp[151] != 1 && tmp[151] != 0) {	/* layers.  What's a layer? */
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"Can't handle instruments with %d layers", tmp[151]);
		close_file(tf);
		return 0;
	}
	ip = (Instrument *) safe_malloc(sizeof(Instrument));
	ip->instname = NULL;
	ip->type = INST_GUS;
	ip->samples = tmp[198];
	ip->sample = (Sample *) safe_malloc(sizeof(Sample) * ip->samples);
	memset(ip->sample, 0, sizeof(Sample) * ip->samples);
	for (i = 0; i < ip->samples; i++) {
		skip(tf, 7);	/* Skip the wave name */
		if (tf_read(&fractions, 1, 1, tf) != 1) {
fail:
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Error reading sample %d", i);
			for (j = 0; j < i; j++) {
				safe_free(ip->sample[j].data);
				ip->sample[j].data = NULL;
			}
			safe_free(ip->sample);
			safe_free(ip);
			close_file(tf);
			return 0;
		}
		sp = &(ip->sample[i]);
		sp->low_vel = 0;
		sp->high_vel = 127;
///r
		sp->cutoff_freq = 20000;
		sp->cutoff_low_limit = -1;
		sp->cutoff_low_keyf = 0; // cent
		sp->resonance = 0;
		sp->tremolo_delay = sp->vibrato_delay = 0;
		sp->vibrato_to_amp = sp->vibrato_to_fc = 0;
		sp->tremolo_to_pitch = sp->tremolo_to_fc = 0;
		sp->modenv_to_pitch = sp->modenv_to_fc = 0;
		sp->vel_to_fc = sp->key_to_fc = sp->vel_to_resonance = 0;
		sp->envelope_velf_bpo = sp->modenv_velf_bpo = 64;
		sp->vel_to_fc_threshold = 0;
		sp->key_to_fc_bpo = 60;
		sp->envelope_delay = sp->modenv_delay = 0;
		sp->inst_type = INST_GUS;
		sp->sample_type = SF_SAMPLETYPE_MONO;
		sp->sf_sample_link = -1;
		sp->sf_sample_index = 0;
		sp->lpf_type = -1;
		sp->keep_voice = 0;
		sp->hpf[0] = -1; // opt_hpf_def
		sp->hpf[1] = 10;
		sp->hpf[2] = 0;
		sp->def_pan = 64;
		sp->sample_pan = 0.0;
		sp->pitch_envelope[0] = 0; // 0cent init
		sp->pitch_envelope[1] = 0; // 0cent atk
		sp->pitch_envelope[2] = 0; // 125ms atk
		sp->pitch_envelope[3] = 0; // 0cent dcy1
		sp->pitch_envelope[4] = 0; // 125ms dcy1
		sp->pitch_envelope[5] = 0; // 0cent dcy2
		sp->pitch_envelope[6] = 0; // 125ms dcy3
		sp->pitch_envelope[7] = 0; // 0cent rls
		sp->pitch_envelope[8] = 0; // 125ms rls

		memset(sp->envelope_velf, 0, sizeof(sp->envelope_velf));
		memset(sp->envelope_keyf, 0, sizeof(sp->envelope_keyf));
		memset(sp->modenv_velf, 0, sizeof(sp->modenv_velf));
		memset(sp->modenv_keyf, 0, sizeof(sp->modenv_keyf));
		memset(sp->modenv_rate, 0, sizeof(sp->modenv_rate));
		memset(sp->modenv_offset, 0, sizeof(sp->modenv_offset));
///r
		sp->modenv_offset[0] = to_offset(250);
		sp->modenv_rate[0] = to_rate(255);
		sp->modenv_offset[1] = to_offset(254);
		sp->modenv_rate[1] = to_rate(1);
		sp->modenv_offset[2] = to_offset(253);
		sp->modenv_rate[2] = to_rate(1);
		sp->modenv_offset[3] = 0;
		sp->modenv_rate[3] = to_rate(64);
		sp->modenv_offset[4] = 0;
		sp->modenv_rate[4] = to_rate(64);
		sp->modenv_offset[5] = 0;
		sp->modenv_rate[5] = to_rate(64);

		READ_LONG(sp->data_length);
		READ_LONG(sp->loop_start);
		READ_LONG(sp->loop_end);
		READ_SHORT(sp->sample_rate);
		READ_LONG(low_freq);
		READ_LONG(high_freq);
		READ_LONG(root_freq);	
		skip(tf, 2);	/* Why have a "root frequency" and then "tuning"?? */
		READ_CHAR(tmp[0]);
		/* convert freq to key */	
		{
			int32 freq1, freq2;
			int k;			
					
			sp->low_key = 0;
			sp->high_key = 127;
			sp->root_key = 60;
			for(k = 0; k < 128; k++){
				if(k == 0){
					freq1 = 0;
					freq2 = freq_table[k + 1];
				}else if(k == 127){
					freq1 = freq_table[k];
					freq2 = 0x7fffffff;
				}else{
					freq1 = freq_table[k];
					freq2 = freq_table[k + 1];
				}					
				if(low_freq >= freq1 && low_freq < freq2)
					sp->low_key = k;
				if(high_freq >= freq1 && high_freq < freq2)
					sp->high_key = k;
				if(root_freq >= freq1 && root_freq < freq2)
					sp->root_key = k;
			}
#if 1 // c219 ルートキー周波数とtuneを分離
			sp->root_freq = freq_table[sp->root_key];
			sp->tune = (FLOAT_T)sp->root_freq / (FLOAT_T)root_freq;			
#else // root_freqはルートキー周波数(freq_table[sp->root_key])との比(tune)が含まれている
			sp->root_freq = root_freq;	
			sp->tune = 1.0;
#endif
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Rate=%d LK=%d HK=%d RK=%d RF=%d Tune=%f",
					sp->sample_rate, sp->low_key, sp->high_key, sp->root_key, sp->root_freq, sp->tune);
		}
#if 1 // c191-
		sp->def_pan = ((tmp[0] - ((tmp[0] < 8) ? 7 : 8)) * 63) / 7 + 64;
#else
		if (panning == -1)
			/* 0x07 and 0x08 are both center panning */
			sp->def_pan = ((tmp[0] - ((tmp[0] < 8) ? 7 : 8)) * 63) / 7 + 64;
		else
			sp->def_pan = (uint8) (panning & 0x7f);
#endif

		/* envelope, tremolo, and vibrato */
		if (tf_read(tmp, 1, 18, tf) != 18)
			goto fail;
		if (! tmp[13] || ! tmp[14]) {
			sp->tremolo_sweep = sp->tremolo_freq = 0;
			sp->tremolo_to_amp = 0;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * no tremolo");
		} else {
			sp->tremolo_sweep = convert_gus_lfo_sweep(tmp[12]);
			sp->tremolo_freq = convert_gus_lfo_rate(tmp[13]);
			sp->tremolo_to_amp = (int32)tmp[14] * 10000 / 255; // max255 to max10000
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
					" * tremolo: sweep %d(ms), freq %d(mHz), depth %d(0.01%)",
					sp->tremolo_sweep, sp->tremolo_freq, sp->tremolo_to_amp);
		}
		if (! tmp[16] || ! tmp[17]) {
			sp->vibrato_sweep = sp->vibrato_freq = 0;
			sp->vibrato_to_pitch = 0;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * no vibrato");
		} else {
			sp->vibrato_freq = convert_gus_lfo_rate(tmp[16]);
			sp->vibrato_sweep = convert_gus_lfo_sweep(tmp[15]);
			sp->vibrato_to_pitch = tmp[17];
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
					" * vibrato: sweep %d(ms), freq %d(mHz), depth %d(cent)",
					sp->vibrato_sweep, sp->vibrato_freq, sp->vibrato_to_pitch);
		}
		READ_CHAR(sp->modes);
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * mode: 0x%02x", sp->modes);
		READ_SHORT(sp->scale_freq);
		READ_SHORT(sp->scale_factor);
		skip(tf, 36);	/* skip reserved space */
		/* Mark this as a fixed-pitch instrument if such a deed is desired. */
		sp->note_to_use = (note_to_use != -1) ? (uint8) note_to_use : 0;
		/* seashore.pat in the Midia patch set has no Sustain.  I don't
		 * understand why, and fixing it by adding the Sustain flag to
		 * all looped patches probably breaks something else.  We do it
		 * anyway.
		 */
		if (sp->modes & MODES_LOOPING)
			sp->modes |= MODES_SUSTAIN;
		/* Strip any loops and envelopes we're permitted to */
		if ((strip_loop == 1) && (sp->modes & (MODES_SUSTAIN | MODES_LOOPING
				| MODES_PINGPONG | MODES_REVERSE))) {
			sp->modes &= ~(MODES_SUSTAIN | MODES_LOOPING
					| MODES_PINGPONG | MODES_REVERSE);
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
					" - Removing loop and/or sustain");
		}
		if (strip_envelope == 1) {
			if (sp->modes & MODES_ENVELOPE)
				ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Removing envelope");
			sp->modes &= ~MODES_ENVELOPE;
		} else if (strip_envelope != 0) {
			/* Have to make a guess. */
			if (! (sp->modes & (MODES_LOOPING
					| MODES_PINGPONG | MODES_REVERSE))) {
				/* No loop? Then what's there to sustain?
				 * No envelope needed either...
				 */
				sp->modes &= ~(MODES_SUSTAIN|MODES_ENVELOPE);
				ctl->cmsg(CMSG_INFO, VERB_DEBUG,
						" - No loop, removing sustain and envelope");
			} else if (! memcmp(tmp, "??????", 6) || tmp[11] >= 100) {
				/* Envelope rates all maxed out?
				 * Envelope end at a high "offset"?
				 * That's a weird envelope.  Take it out.
				 */
				sp->modes &= ~MODES_ENVELOPE;
				ctl->cmsg(CMSG_INFO, VERB_DEBUG,
						" - Weirdness, removing envelope");
			} else if (! (sp->modes & MODES_SUSTAIN)) {
				/* No sustain? Then no envelope.  I don't know if this is
				 * justified, but patches without sustain usually don't need
				 * the envelope either... at least the Gravis ones.  They're
				 * mostly drums.  I think.
				 */
				sp->modes &= ~MODES_ENVELOPE;
				ctl->cmsg(CMSG_INFO, VERB_DEBUG,
						" - No sustain, removing envelope");
			}
		}
		{		
#if 1 // limit ADSR
			double limit = (double)control_ratio * (double)0x3FFFFFFF / ((double)play_mode->rate * 0.02);
			int32 tmp_rate, tmp_rate2, limit_rate = limit < 1.0 ? 1 : (int32)limit;
			sp->envelope_rate[0] = convert_envelope_rate(tmp[0]);
			sp->envelope_offset[0] = to_offset(255);
			tmp_rate = convert_envelope_rate(tmp[1]);
			sp->envelope_rate[1] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[1] = convert_envelope_offset(tmp[1 + 6]);
			tmp_rate = convert_envelope_rate(tmp[2]);
			sp->envelope_rate[2] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[2] = convert_envelope_offset(tmp[2 + 6]);	
			if(sp->envelope_offset[2] > sp->envelope_offset[1])
				sp->envelope_offset[2] = sp->envelope_offset[1];
			tmp_rate = convert_envelope_rate(tmp[3]);
			sp->envelope_rate[3] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[3] = to_offset(0);
			sp->envelope_rate[5] = sp->envelope_rate[4] = sp->envelope_rate[3];
			sp->envelope_offset[5] = sp->envelope_offset[4] = sp->envelope_offset[3];
#else
			double limit = (double)control_ratio * (double)0x3FFFFFFF / ((double)play_mode->rate * 0.02);
			int32 tmp_rate, tmp_rate2, limit_rate = limit < 1.0 ? 1 : (int32)limit;
			sp->envelope_rate[0] = convert_envelope_rate(tmp[0]);
			sp->envelope_offset[0] = convert_envelope_offset(tmp[0 + 6]);
			tmp_rate = convert_envelope_rate(tmp[1]);
			sp->envelope_rate[1] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[1] = convert_envelope_offset(tmp[1 + 6]);
			tmp_rate = convert_envelope_rate(tmp[2]);
			sp->envelope_rate[2] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[2] = convert_envelope_offset(tmp[2 + 6]);		
			tmp_rate = convert_envelope_rate(tmp[3]);
			sp->envelope_rate[3] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[3] = convert_envelope_offset(tmp[3 + 6]);
			tmp_rate = convert_envelope_rate(tmp[4]);
			sp->envelope_rate[4] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[4] = convert_envelope_offset(tmp[4 + 6]);
			tmp_rate = convert_envelope_rate(tmp[5]);
			sp->envelope_rate[5] = (tmp_rate > limit_rate) ? limit_rate : tmp_rate;
			sp->envelope_offset[5] = convert_envelope_offset(tmp[5 + 6]);
#endif
		}
		/* this envelope seems to give reverb like effects to most patches
		 * use the same method as soundfont
		 */
		if (modify_release) {
			sp->envelope_rate[3] = calc_rate(255, modify_release);
			sp->envelope_offset[3] = 0;
			sp->envelope_rate[5] = sp->envelope_rate[4] = sp->envelope_rate[3];
			sp->envelope_offset[5] = sp->envelope_offset[4] = sp->envelope_offset[3];
		}
		/* Then read the sample data */
		sp->data = (sample_t *) safe_large_malloc(sp->data_length + sizeof(sample_t) * 128);
		memset(sp->data, 0, sp->data_length + sizeof(sample_t) * 128);
		sp->data_alloced = 1;
///r
		sp->data_type = SAMPLE_TYPE_INT16;
		if ((j = tf_read(sp->data, 1, sp->data_length, tf))
				!= sp->data_length) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"Too small this patch length: %d < %d",
					j, sp->data_length);
			goto fail;
		}
		if (! (sp->modes & MODES_16BIT)) {	/* convert to 16-bit data */
			int32 i;
			uint16 *tmp;
			uint8 *cp = (uint8 *) sp->data;
			
			tmp = (uint16 *) safe_large_malloc(sp->data_length * 2 + sizeof(sample_t) * 128);
		//	memset(tmp, 0, sp->data_length * 2 + sizeof(sample_t) * 128);
			for (i = 0; i < sp->data_length; i++)
				tmp[i] = (uint16) cp[i] << 8;
			sp->data = (sample_t *) tmp;
			safe_free(cp);
			sp->data_length *= 2;
			sp->loop_start *= 2;
			sp->loop_end *= 2;
		}
#ifndef LITTLE_ENDIAN
		else {	/* convert to machine byte order */
			int32 i;
			int16 *tmp = (int16 *) sp->data, s;
			
			for (i = 0; i < sp->data_length / 2; i++)
				s = LE_SHORT(tmp[i]), tmp[i] = s;
		}
#endif
		if (sp->modes & MODES_UNSIGNED) {	/* convert to signed data */
			int32 i = sp->data_length / 2;
			int16 *tmp = (int16 *) sp->data;
			
			while (i--)
				*tmp++ ^= 0x8000;
		}
		/* Reverse loops and pass them off as normal loops */
		if (sp->modes & MODES_REVERSE) {
			/* The GUS apparently plays reverse loops by reversing the
			 * whole sample.  We do the same because the GUS does not SUCK.
			 */
			int32 t;
			
			reverse_data((int16 *) sp->data, 0, sp->data_length / 2);
			t = sp->loop_start;
			sp->loop_start = sp->data_length - sp->loop_end;
			sp->loop_end = sp->data_length - t;
			sp->modes &= ~MODES_REVERSE;
			sp->modes |= MODES_LOOPING;	/* just in case */
			ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Reverse loop in %s", name);
		}
		/* If necessary do some anti-aliasing filtering */
		if (antialiasing_allowed)
			antialiasing((int16 *) sp->data, sp->data_length / 2, sp->sample_rate, play_mode->rate);

#ifdef ADJUST_SAMPLE_VOLUMES
		// // c191-
		//if (amp != -1){
		//	sp->volume = (double) amp / 100; // -c190
		//	sp->volume = 1.0;
		//}else 
		{
			/* Try to determine a volume scaling factor for the sample.
			 * This is a very crude adjustment, but things sound more
			 * balanced with it.  Still, this should be a runtime option.
			 */
			int32 i, a, maxamp = 0;
			int16 *tmp = (int16 *) sp->data;
			
			for (i = 0; i < sp->data_length / 2; i++)
				if ((a = abs(tmp[i])) > maxamp)
					maxamp = a;
			sp->volume = (double)32768.0 / (double) maxamp;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
					" * volume comp: %f", sp->volume);
		}
		sp->cfg_amp = 1.0;
#else
	//	sp->volume = (amp != -1) ? (double) amp / 100 : 1.0; // -c190
		sp->volume = 1.0; // c191-
		sp->cfg_amp = 1.0;
#endif
		/* These are in bytes.  Convert into samples. */
		sp->data_length /= 2;
		sp->loop_start /= 2;
		sp->loop_end /= 2;
		/* The sample must be padded out by 2 extra sample, so that
		 * round off errors in the offsets used in interpolation will not
		 * cause a "pop" by reading random data beyond data_length
		 */
	///r
	//	sp->data[sp->data_length] = sp->data[sp->data_length + 1] = 0;
		memset(&sp->data[sp->data_length], 0, sizeof(sample_t) * 128);
		/* Remove abnormal loops which cause pop noise
		 * in long sustain stage
		 */
		if (! (sp->modes & MODES_LOOPING)) {
			sp->loop_start = sp->data_length - 1;
			sp->loop_end = sp->data_length;
			sp->data[sp->data_length - 1] = 0;
		}
		/* Then fractional samples */
		sp->data_length <<= FRACTION_BITS;
		sp->loop_start <<= FRACTION_BITS;
		sp->loop_end <<= FRACTION_BITS;
		/* Adjust for fractional loop points. This is a guess.  Does anyone
		 * know what "fractions" really stands for?
		 */
		sp->loop_start |= (fractions & 0x0f) << (FRACTION_BITS - 4);
		sp->loop_end |= ((fractions >> 4) & 0x0f) << (FRACTION_BITS - 4);
		/* If this instrument will always be played on the same note,
		 * and it's not looped, we can resample it now.
		 */
///r
		if (opt_pre_resamplation && sp->note_to_use && ! (sp->modes & MODES_LOOPING))
			pre_resample(sp);
		
#ifdef LOOKUP_HACK
		squash_sample_16to8(sp);
#endif
		if (strip_tail == 1) {
			/* Let's not really, just say we did. */
			sp->data_length = sp->loop_end;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Stripping tail");
		}
	}
	close_file(tf);
	store_instrument_cache(ip, name, panning, amp, note_to_use,
			strip_loop, strip_envelope, strip_tail);
	return ip;
}

#ifdef LOOKUP_HACK
/*! Squash the 16-bit data into 8 bits. */
void squash_sample_16to8(Sample *sp)
{
	uint8 *gulp, *ulp;

	int l = sp->data_length >> FRACTION_BITS;

	gulp = ulp = (uint8 *)safe_malloc(l + 1);

	switch(sp->data_type){
	case SAMPLE_TYPE_INT16:
		{
		int16 *swp;
		swp = (int16 *)sp->data;
		while (l--)
			*ulp++ = (*swp++ >> 8) & 0xff;
		}
		break;
	case SAMPLE_TYPE_INT32:
		{
		int32 *swp;
		swp = (int32 *)sp->data;
		while (l--)
			*ulp++ = (*swp++ >> 24) & 0xff;
		}
		break;
	case SAMPLE_TYPE_FLOAT:
		{
		float *swp = (float *)sp->data;
		while (l--)
			*ulp++ = *swp++ * 127.0;
		}
		break;
	case SAMPLE_TYPE_DOUBLE:
		{
		double *swp = (double *)sp->data;
		while (l--)
			*ulp++ = *swp++ * 127.0;
		}
		break;
	default:
		ctl->cmsg(CMSG_INFO, VERB_NORMAL, "invalid squash data_type %d", data_type);
		break;
	}
	safe_free(sp->data);
	sp->data = (sample_t *)gulp;
}
#endif

///r
int opt_load_all_instrument = 0;

void load_all_instrument(void)
{
	int i, j, k, l, elm;
	ToneBankElement *tone;
	Instrument *ip = NULL;
	
	if(!opt_load_all_instrument)
		return;
	free_instruments_afterwards = 0;
	init_load_soundfont(); // load_instrument()より前
	
	for(i= 0; i < 128 + MAP_BANK_COUNT; i++){
		if(!tonebank[i] || !tonebank[i]->tone)
			continue; // standard bank
		for(j= 0; j < 128; j++){
			if(!tonebank[i]->tone[j])
				continue; // error
			if(tonebank[i]->tone[j][0] == NULL)
				continue;
			for(elm= 0; elm <= tonebank[i]->tone[j][0]->element_num; elm++){
				if(tonebank[i]->tone[j][elm] == NULL)
					break;
				if( !((ip = tonebank[i]->tone[j][elm]->instrument) == NULL))
					break;
				ip = load_instrument(0, i, j, elm);
				if (ip == NULL || IS_MAGIC_INSTRUMENT(ip)){
					tonebank[i]->tone[j][elm]->instrument = MAGIC_ERROR_INSTRUMENT;
					break;
				}					
				tonebank[i]->tone[j][elm]->instrument = ip;
				
			}
		}
	}	
	for(i= 0; i < 128 + MAP_BANK_COUNT; i++){
		if(!drumset[i] || !drumset[i]->tone)
			continue; // standard bank
		for(j= 0; j < 128; j++){
			if(!drumset[i]->tone[j])
				continue; // error
			if(drumset[i]->tone[j][0] == NULL)
				continue;
			for(elm= 0; elm < drumset[i]->tone[j][0]->element_num; elm++){
				if(drumset[i]->tone[j][elm] == NULL)
					break;
				if(drumset[i]->tone[j][elm]->instrument != NULL)
					break;
				ip = load_instrument(1, i, j, elm);
				if(ip == NULL)
					break;
				drumset[i]->tone[j][elm]->instrument;
			}
		}
	}
}

///r
/*
サンプルロードの経路

起動時に全音色読込 opt_load_all_instrument
load_all_instrument()
SMFロード時 (c209無効 c211削除
readmidi.c SMFロード時read_midi_file() groom_list() NOTE_ONでマーク(MAGIC_LOAD_INSTRUMENT)したものをinstrum.c fill_bank()でロード
再生前プリスキャン(イベントだけ仮再生)のNOTE_ONでロード (playerの場合 ここで全部ロードしたい・・
playmidi.c play_midi_prescan()のNOTE_ONでfind_samples()でロード
再生中のNOTE_ONでロード
playmidi.c play_midi()
一部 プログラムチェンジ ドラムパート (主にSynthの場合
playmidi.c midi_program_change()
*/
Instrument *load_instrument(int dr, int b, int prog, int elm)
{
	ToneBank *bank = ((dr) ? drumset[b] : tonebank[b]);
	Instrument *ip;
	int i, j, font_bank, font_preset, font_keynote;
#ifdef INT_SYNTH
	extern Instrument *extract_scc_file(char *, int);
	extern Instrument *extract_mms_file(char *, int);
#endif
	extern Instrument *extract_sample_file(char *);

	int pan, panning;
	char infomsg[256];
	
#ifndef CFG_FOR_SF
	if (play_system_mode == GS_SYSTEM_MODE && (b == 64 || b == 65)) {
		if (! dr)	/* User Instrument */
			recompute_userinst(b, prog, elm);
		else {		/* User Drumset */
			ip = recompute_userdrum(b, prog, elm);
			if (ip != NULL) {
				return ip;
			}
		}
    }
#endif
	if(bank->tone[prog][elm] == NULL) // error
		return NULL;
	switch(bank->tone[prog][elm]->instype){
	case 0: /* load GUS/patch file */
		if (! dr)
			sprintf(infomsg, "Tonebank %d %d", b, prog + progbase);
		else
			sprintf(infomsg, "Drumset %d %d(%s)",
					b + progbase, prog, note_name[prog % 12]);
		infomsg[256 - 1] = '\0';
		ip = load_gus_instrument(bank->tone[prog][elm]->name,
				bank, dr, prog, infomsg, elm);
		break;
	case 1: /* Font extention */
		font_bank = bank->tone[prog][elm]->font_bank;
		font_preset = bank->tone[prog][elm]->font_preset;
		font_keynote = bank->tone[prog][elm]->font_keynote;
		ip = extract_soundfont(bank->tone[prog][elm]->name, font_bank, font_preset, font_keynote);
		break;
	case 2: /* Sample extension */		
		ip = extract_sample_file(bank->tone[prog][elm]->name);
		break;
#ifdef INT_SYNTH
	case 3: /* mms extention */	
		ip = extract_mms_file(bank->tone[prog][elm]->name, bank->tone[prog][elm]->is_preset);
		break;
	case 4: /* scc extention */
		ip = extract_scc_file(bank->tone[prog][elm]->name, bank->tone[prog][elm]->is_preset);
		break;
#endif
#ifdef ENABLE_SFZ
	case 5: /* sfz extension */
		ip = extract_sfz_file(bank->tone[prog][elm]->name);
		break;
#endif
#ifdef ENABLE_DLS
	case 6: /* dls extension */
		font_bank = bank->tone[prog][elm]->font_bank;
		font_preset = bank->tone[prog][elm]->font_preset;
		font_keynote = bank->tone[prog][elm]->font_keynote;
		ip = extract_dls_file(bank->tone[prog][elm]->name, font_bank, font_preset, font_keynote);
		break;
#endif
	default:
		goto TONEBANK_INSTRUMENT_NULL;
		break;
	}
	
	if (ip == NULL) 
		goto TONEBANK_INSTRUMENT_NULL;

	i = (dr) ? 0 : prog;
	if(bank->tone[i][elm] == NULL) // error
		return ip;
	if (!bank->tone[i][elm]->name)
		bank->tone[i][elm]->name = safe_strdup(DYNAMIC_INSTRUMENT_NAME);
#if 1 // c210 CFG comm
	if(elm == 0){
	if (!bank->tone[i][elm]->comment) {
		bank->tone[i][elm]->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
		bank->tone[i][elm]->comment[0] = '\0';
	}
	if(!(dr && bank->tone[0][0] && bank->tone[0][0]->comment && bank->tone[0][0]->comment[0]
		|| bank->tone[i][elm]->comment[0]) ){
		if(ip->instname != NULL){
			strncpy(bank->tone[i][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);					
			if (opt_print_fontname && !dr && bank->tone[i][elm]->name && bank->tone[i][elm]->instype != 2) {
				// SF2/SCC/MMS
				const char *name = pathsep_strrchr(bank->tone[i][elm]->name);
				if(name)
					name ++;
				else
					name = bank->tone[i][elm]->name;
				strlcat(bank->tone[i][elm]->comment, " (", MAX_TONEBANK_COMM_LEN);
				strlcat(bank->tone[i][elm]->comment, name, MAX_TONEBANK_COMM_LEN);
				strlcat(bank->tone[i][elm]->comment, ")", MAX_TONEBANK_COMM_LEN);
			}
		}else if(bank->tone[i][elm]->name != NULL){
			strncpy(bank->tone[i][elm]->comment, bank->tone[i][elm]->name, MAX_TONEBANK_COMM_LEN - 1);
		}
		bank->tone[i][elm]->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
	}
	}
#else
	if (!bank->tone[i][elm]->comment) {
		bank->tone[i][elm]->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
		bank->tone[i][elm]->comment[0] = '\0';
	}
	if(bank->tone[i][elm]->instype > 0){
		if(ip->instname != NULL){
			if(dr) {
				if(!bank->tone[i][elm]->comment[0] || strcmp(bank->tone[i][elm]->comment, bank->tone[i][elm]->name) == 0) {
					strncpy(bank->tone[i][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);
				}
			}
			else
				strncpy(bank->tone[i][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);
			bank->tone[i][elm]->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
		}
		if (opt_print_fontname && !dr && bank->tone[i][elm]->name != NULL && bank->tone[i][elm]->instype != 2) {
			// SF2/SCC/MMS
			const char *name = pathsep_strrchr(bank->tone[i][elm]->name);
			if(name)
				name ++;
			else
				name = bank->tone[i][elm]->name;
			strlcat(bank->tone[i][elm]->comment, " (", MAX_TONEBANK_COMM_LEN);
			strlcat(bank->tone[i][elm]->comment, name, MAX_TONEBANK_COMM_LEN);
			strlcat(bank->tone[i][elm]->comment, ")", MAX_TONEBANK_COMM_LEN);
		}
	}
#endif
	apply_bank_parameter(ip, bank->tone[prog][elm]);
	return ip;
	
TONEBANK_INSTRUMENT_NULL:
	if (! dr) {
		font_bank = b;
		font_preset = prog;
		font_keynote = -1;
	} else {
		font_bank = 128;
		font_preset = b;
		font_keynote = prog;
	}
	/* preload soundfont */
	ip = load_soundfont_inst(0, font_bank, font_preset, font_keynote);
	if (ip != NULL) {
		if (bank->tone[prog][elm]->name == NULL) /* this should not be NULL to play the instrument */
			bank->tone[prog][elm]->name = safe_strdup(DYNAMIC_INSTRUMENT_NAME);
#if 1 // c210 CFG comm
		if(elm == 0){
		if (!bank->tone[prog][elm]->comment){
			bank->tone[prog][elm]->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
			bank->tone[prog][elm]->comment[0] = '\0';
		}		
		if(!(dr && bank->tone[0][0] && bank->tone[0][0]->comment && bank->tone[0][0]->comment[0]
			|| bank->tone[prog][elm]->comment[0]) ){
			if(ip->instname != NULL){
				strncpy(bank->tone[prog][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);				
				if (opt_print_fontname && !dr && bank->tone[prog][elm]->name && bank->tone[prog][elm]->instype != 2) {
					const char *name = pathsep_strrchr(bank->tone[prog][elm]->name);
					if(name)
						name ++;
					else
						name = bank->tone[prog][elm]->name;
					strlcat(bank->tone[prog][elm]->comment, " (", MAX_TONEBANK_COMM_LEN);
					strlcat(bank->tone[prog][elm]->comment, name, MAX_TONEBANK_COMM_LEN);
					strlcat(bank->tone[prog][elm]->comment, ")", MAX_TONEBANK_COMM_LEN);
				}
			}else if(bank->tone[prog][elm]->name != NULL){
				strncpy(bank->tone[prog][elm]->comment, bank->tone[prog][elm]->name, MAX_TONEBANK_COMM_LEN - 1);
			}
			bank->tone[prog][elm]->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
		}
		}
#else
		if (!bank->tone[prog][elm]->comment)
			bank->tone[prog][elm]->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
		strncpy(bank->tone[prog][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);
		bank->tone[prog][elm]->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
		if (opt_print_fontname && !dr && bank->tone[prog][elm]->instype == 1 && !bank->tone[prog][elm]->name[0]) {
			const char *name = pathsep_strrchr(bank->tone[prog][elm]->name);
			if(name)
				name ++;
			else
				name = bank->tone[prog][elm]->name;
			strlcat(bank->tone[prog][elm]->comment, " (", MAX_TONEBANK_COMM_LEN);
			strlcat(bank->tone[prog][elm]->comment, name, MAX_TONEBANK_COMM_LEN);
			strlcat(bank->tone[prog][elm]->comment, ")", MAX_TONEBANK_COMM_LEN);
		}
#endif
	} else { // ip == NULL
		/* no patch; search soundfont again */
		ip = load_soundfont_inst(1, font_bank, font_preset, font_keynote);
		if (ip != NULL) {
#if 1 // c210 CFG comm
			if(elm == 0){
			if(bank->tone[0][elm]){
				if (!bank->tone[0][elm]->comment){
					bank->tone[0][elm]->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
					bank->tone[0][elm]->comment[0] = '\0';
				}
				if(!bank->tone[0][elm]->comment[0]){
					if(ip->instname){
						strncpy(bank->tone[0][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);			
						if (opt_print_fontname && !dr && bank->tone[0][elm]->name && bank->tone[0][elm]->instype != 2) {
							const char *name = pathsep_strrchr(bank->tone[0][elm]->name);
							if(name)
								name ++;
							else
								name = bank->tone[0][elm]->name;
							strlcat(bank->tone[0][elm]->comment, " (", MAX_TONEBANK_COMM_LEN);
							strlcat(bank->tone[0][elm]->comment, name, MAX_TONEBANK_COMM_LEN);
							strlcat(bank->tone[0][elm]->comment, ")", MAX_TONEBANK_COMM_LEN);
						}
					}else if(bank->tone[0][elm]->name != NULL){
						strncpy(bank->tone[0][elm]->comment, bank->tone[0][elm]->name, MAX_TONEBANK_COMM_LEN - 1);
					}
					strncpy(bank->tone[0][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);
				}
			}
			}
#else
			if(bank->tone[0][elm]){
				if (!bank->tone[0][elm]->comment)
					bank->tone[0][elm]->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
				strncpy(bank->tone[0][elm]->comment, ip->instname, MAX_TONEBANK_COMM_LEN - 1);
				bank->tone[0][elm]->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
			}
#endif
		}
	}
	if (ip != NULL)
		apply_bank_parameter(ip, bank->tone[prog][elm]);
	return ip;
}

static void *safe_memdup(void *s, size_t size)
{
	return memcpy(safe_malloc(size), s, size);
}

/*! Copy ToneBankElement src to elm. The original elm is released. */
void copy_tone_bank_element(ToneBankElement *elm, const ToneBankElement *src)
{
	int i, j;
	
	free_tone_bank_element(elm);
	memcpy(elm, src, sizeof(ToneBankElement));
	if (elm->name)
		elm->name = safe_strdup(elm->name);
///r	
	if (elm->sample_lokeynum)
		elm->sample_lokey = (int16 *) safe_memdup(elm->sample_lokey,
				elm->sample_lokeynum * sizeof(int16));
	if (elm->sample_hikeynum)
		elm->sample_hikey = (int16 *) safe_memdup(elm->sample_hikey,
				elm->sample_hikeynum * sizeof(int16));
	if (elm->sample_lovelnum)
		elm->sample_lovel = (int16 *) safe_memdup(elm->sample_lovel,
				elm->sample_lovelnum * sizeof(int16));
	if (elm->sample_hivelnum)
		elm->sample_hivel = (int16 *) safe_memdup(elm->sample_hivel,
				elm->sample_hivelnum * sizeof(int16));
	if (elm->tunenum)
		elm->tune = (float *) safe_memdup(elm->tune,
				elm->tunenum * sizeof(float));
	if (elm->envratenum) {
		elm->envrate = (int **) safe_memdup(elm->envrate,
				elm->envratenum * sizeof(int *));
		for (i = 0; i < elm->envratenum; i++)
			elm->envrate[i] = (int *) safe_memdup(elm->envrate[i],
					6 * sizeof(int));
	}
	if (elm->envofsnum) {
		elm->envofs = (int **) safe_memdup(elm->envofs,
				elm->envofsnum * sizeof(int *));
		for (i = 0; i < elm->envofsnum; i++)
			elm->envofs[i] = (int *) safe_memdup(elm->envofs[i],
					6 * sizeof(int));
	}
	if (elm->tremnum) {
		elm->trem = (Quantity **) safe_memdup(elm->trem,
				elm->tremnum * sizeof(Quantity *));
		for (i = 0; i < elm->tremnum; i++)
			elm->trem[i] = (Quantity *) safe_memdup(elm->trem[i],
					3 * sizeof(Quantity));
	}
	if (elm->tremdelaynum)
		elm->tremdelay = (int16 *) safe_memdup(elm->tremdelay,
				elm->tremdelaynum * sizeof(int16));
	if (elm->tremsweepnum)
		elm->tremsweep = (int16 *) safe_memdup(elm->tremsweep,
				elm->tremsweepnum * sizeof(int16));
	if (elm->tremfreqnum)
		elm->tremfreq = (int16 *) safe_memdup(elm->tremfreq,
				elm->tremfreqnum * sizeof(int16));
	if (elm->tremampnum)
		elm->tremamp = (int16 *) safe_memdup(elm->tremamp,
				elm->tremampnum * sizeof(int16));
	if (elm->trempitchnum)
		elm->trempitch = (int16 *) safe_memdup(elm->trempitch,
				elm->trempitchnum * sizeof(int16));
	if (elm->tremfcnum)
		elm->tremfc = (int16 *) safe_memdup(elm->tremfc,
				elm->tremfcnum * sizeof(int16));
	if (elm->vibnum) {
		elm->vib = (Quantity **) safe_memdup(elm->vib,
				elm->vibnum * sizeof(Quantity *));
		for (i = 0; i < elm->vibnum; i++)
			elm->vib[i] = (Quantity *) safe_memdup(elm->vib[i],
					3 * sizeof(Quantity));
	}	
	if (elm->vibdelaynum)
		elm->vibdelay = (int16 *) safe_memdup(elm->vibdelay,
				elm->vibdelaynum * sizeof(int16));
	if (elm->vibsweepnum)
		elm->vibsweep = (int16 *) safe_memdup(elm->vibsweep,
				elm->vibsweepnum * sizeof(int16));
	if (elm->vibfreqnum)
		elm->vibfreq = (int16 *) safe_memdup(elm->vibfreq,
				elm->vibfreqnum * sizeof(int16));
	if (elm->vibampnum)
		elm->vibamp = (int16 *) safe_memdup(elm->vibamp,
				elm->vibampnum * sizeof(int16));
	if (elm->vibpitchnum)
		elm->vibpitch = (int16 *) safe_memdup(elm->vibpitch,
				elm->vibpitchnum * sizeof(int16));
	if (elm->vibfcnum)
		elm->vibfc = (int16 *) safe_memdup(elm->vibfc,
				elm->vibfcnum * sizeof(int16));
	if (elm->sclnotenum)
		elm->sclnote = (int16 *) safe_memdup(elm->sclnote,
				elm->sclnotenum * sizeof(int16));
	if (elm->scltunenum)
		elm->scltune = (int16 *) safe_memdup(elm->scltune,
				elm->scltunenum * sizeof(int16));
	if (elm->comment)
		elm->comment = (char *) safe_memdup(elm->comment,
				MAX_TONEBANK_COMM_LEN * sizeof(char));
	if (elm->modenvratenum) {
		elm->modenvrate = (int **) safe_memdup(elm->modenvrate,
				elm->modenvratenum * sizeof(int *));
		for (i = 0; i < elm->modenvratenum; i++)
			elm->modenvrate[i] = (int *) safe_memdup(elm->modenvrate[i],
					6 * sizeof(int));
	}
	if (elm->modenvofsnum) {
		elm->modenvofs = (int **) safe_memdup(elm->modenvofs,
				elm->modenvofsnum * sizeof(int *));
		for (i = 0; i < elm->modenvofsnum; i++)
			elm->modenvofs[i] = (int *) safe_memdup(elm->modenvofs[i],
					6 * sizeof(int));
	}
	if (elm->envkeyfnum) {
		elm->envkeyf = (int **) safe_memdup(elm->envkeyf,
				elm->envkeyfnum * sizeof(int *));
		for (i = 0; i < elm->envkeyfnum; i++)
			elm->envkeyf[i] = (int *) safe_memdup(elm->envkeyf[i],
					6 * sizeof(int));
	}
	if (elm->envvelfnum) {
		elm->envvelf = (int **) safe_memdup(elm->envvelf,
				elm->envvelfnum * sizeof(int *));
		for (i = 0; i < elm->envvelfnum; i++)
			elm->envvelf[i] = (int *) safe_memdup(elm->envvelf[i],
					6 * sizeof(int));
	}
	if (elm->modenvkeyfnum) {
		elm->modenvkeyf = (int **) safe_memdup(elm->modenvkeyf,
				elm->modenvkeyfnum * sizeof(int *));
		for (i = 0; i < elm->modenvkeyfnum; i++)
			elm->modenvkeyf[i] = (int *) safe_memdup(elm->modenvkeyf[i],
					6 * sizeof(int));
	}
	if (elm->modenvvelfnum) {
		elm->modenvvelf = (int **) safe_memdup(elm->modenvvelf,
				elm->modenvvelfnum * sizeof(int *));
		for (i = 0; i < elm->modenvvelfnum; i++)
			elm->modenvvelf[i] = (int *) safe_memdup(elm->modenvvelf[i],
					6 * sizeof(int));
	}
	if (elm->modpitchnum)
		elm->modpitch = (int16 *) safe_memdup(elm->modpitch,
				elm->modpitchnum * sizeof(int16));
	if (elm->modfcnum)
		elm->modfc = (int16 *) safe_memdup(elm->modfc,
				elm->modfcnum * sizeof(int16));
	if (elm->fcnum)
		elm->fc = (int16 *) safe_memdup(elm->fc,
				elm->fcnum * sizeof(int16));
	if (elm->resonum)
		elm->reso = (int16 *) safe_memdup(elm->reso,
				elm->resonum * sizeof(int16));
///r
	if (elm->fclownum)
		elm->fclow = (int16 *) safe_memdup(elm->fclow,
				elm->fclownum * sizeof(int16));
	if (elm->fclowkeyfnum)
		elm->fclowkeyf = (int16 *) safe_memdup(elm->fclowkeyf,
				elm->fclowkeyfnum * sizeof(int16));
	if (elm->fcmulnum)
		elm->fcmul = (int16 *) safe_memdup(elm->fcmul,
				elm->fcmulnum * sizeof(int16));
	if (elm->fcaddnum)
		elm->fcadd = (int16 *) safe_memdup(elm->fcadd,
				elm->fcaddnum * sizeof(int16));
	if (elm->pitenvnum) {
		elm->pitenv = (int **) safe_memdup(elm->pitenv,
				elm->pitenvnum * sizeof(int *));
		for (i = 0; i < elm->pitenvnum; i++)
			elm->pitenv[i] = (int *) safe_memdup(elm->pitenv[i],
					9 * sizeof(int));
	}
	if (elm->hpfnum) {
		elm->hpf = (int **) safe_memdup(elm->hpf, elm->hpfnum * sizeof(int *));
		for (i = 0; i < elm->hpfnum; i++)
			elm->hpf[i] = (int *) safe_memdup(elm->hpf[i], 3 * sizeof(int));
	}
	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		if (elm->vfxnum[i]) {
			elm->vfx[i] = (int **) safe_memdup(elm->vfx[i], elm->vfxnum[i] * sizeof(int *));
			for (j = 0; j < elm->vfxnum[i]; j++)
				elm->vfx[i][j] = (int *) safe_memdup(elm->vfx[i][j], VOICE_EFFECT_PARAM_NUM * sizeof(int));
		}
	}
}




///r
/*! Release ToneBank[128 + MAP_BANK_COUNT] */
static void free_tone_bank_list(ToneBank *tb[])
{
	int i, j;
	ToneBank *bank;
	int elm;
	
	//for (i = 0; i < 128 + map_bank_counter; i++)//del by Kobarin
	for(i = 0; i < 128 + MAP_BANK_COUNT; i++)//add by Kobarin(メモリリーク修正)
    {
		bank = tb[i];
		if (!bank)
			continue;
		for (j = 0; j < 128; j++)
			for(elm = 0; elm < MAX_ELEMENT; elm++){
				if(bank->tone[j][elm]){
					free_tone_bank_element(bank->tone[j][elm]);
					safe_free(bank->tone[j][elm]);
					bank->tone[j][elm] = NULL;
				}
			}

        {//added by Kobarin(メモリリーク修正)
            struct _AlternateAssign *del=bank->alt;
            struct _AlternateAssign *next;
            while(del){
                next=del->next;
                safe_free(del);
                del=next;
            }
            bank->alt = NULL;
        }//ここまで
		if (i > 0)
		{
			safe_free(bank);
			tb[i] = NULL;
		}
	}
}

/*! Release tonebank and drumset */
void free_tone_bank(void)
{
	free_tone_bank_list(tonebank);
	free_tone_bank_list(drumset);
}

/*! Release ToneBankElement. */
void free_tone_bank_element(ToneBankElement *elm)
{
	int i;

	if (!elm)
		return;
	elm->instype = 0;
	safe_free(elm->name);
	elm->name = NULL;
///r
	safe_free(elm->sample_lokey);
	elm->sample_lokey = NULL, elm->sample_lokeynum = 0;
	safe_free(elm->sample_hikey);
	elm->sample_hikey = NULL, elm->sample_hikeynum = 0;
	safe_free(elm->sample_lovel);
	elm->sample_lovel = NULL, elm->sample_lovelnum = 0;
	safe_free(elm->sample_hivel);
	elm->sample_hivel = NULL, elm->sample_hivelnum = 0;
	safe_free(elm->tune);
	elm->tune = NULL, elm->tunenum = 0;
	if (elm->envratenum)
		free_ptr_list(elm->envrate, elm->envratenum);
	elm->envrate = NULL, elm->envratenum = 0;
	if (elm->envofsnum)
		free_ptr_list(elm->envofs, elm->envofsnum);
	elm->envofs = NULL, elm->envofsnum = 0;
///r
	if (elm->tremnum)
		free_ptr_list(elm->trem, elm->tremnum);
	elm->trem = NULL, elm->tremnum = 0;	
	safe_free(elm->tremdelay);
	elm->tremdelay = NULL, elm->tremdelaynum = 0;
	safe_free(elm->tremsweep);
	elm->tremsweep = NULL, elm->tremsweepnum = 0;
	safe_free(elm->tremfreq);
	elm->tremfreq = NULL, elm->tremfreqnum = 0;
	safe_free(elm->tremamp);
	elm->tremamp = NULL, elm->tremampnum = 0;
	safe_free(elm->trempitch);
	elm->trempitch = NULL, elm->trempitchnum = 0;
	safe_free(elm->tremfc);
	elm->tremfc = NULL, elm->tremfcnum = 0;
	if (elm->vibnum)
		free_ptr_list(elm->vib, elm->vibnum);
	elm->vib = NULL, elm->vibnum = 0;
	safe_free(elm->vibdelay);
	elm->vibdelay = NULL, elm->vibdelaynum = 0;	
	safe_free(elm->vibsweep);
	elm->vibsweep = NULL, elm->vibsweepnum = 0;
	safe_free(elm->vibfreq);
	elm->vibfreq = NULL, elm->vibfreqnum = 0;
	safe_free(elm->vibamp);
	elm->vibamp = NULL, elm->vibampnum = 0;
	safe_free(elm->vibpitch);
	elm->vibpitch = NULL, elm->vibpitchnum = 0;
	safe_free(elm->vibfc);
	elm->vibfc = NULL, elm->vibfcnum = 0;
	safe_free(elm->sclnote);
	elm->sclnote = NULL, elm->sclnotenum = 0;
	safe_free(elm->scltune);
	elm->scltune = NULL, elm->scltunenum = 0;
	safe_free(elm->comment);
	elm->comment = NULL;
	if (elm->modenvratenum)
		free_ptr_list(elm->modenvrate, elm->modenvratenum);
	elm->modenvrate = NULL, elm->modenvratenum = 0;
	if (elm->modenvofsnum)
		free_ptr_list(elm->modenvofs, elm->modenvofsnum);
	elm->modenvofs = NULL, elm->modenvofsnum = 0;
	if (elm->envkeyfnum)
		free_ptr_list(elm->envkeyf, elm->envkeyfnum);
	elm->envkeyf = NULL, elm->envkeyfnum = 0;
	if (elm->envvelfnum)
		free_ptr_list(elm->envvelf, elm->envvelfnum);
	elm->envvelf = NULL, elm->envvelfnum = 0;
	if (elm->modenvkeyfnum)
		free_ptr_list(elm->modenvkeyf, elm->modenvkeyfnum);
	elm->modenvkeyf = NULL, elm->modenvkeyfnum = 0;
	if (elm->modenvvelfnum)
		free_ptr_list(elm->modenvvelf, elm->modenvvelfnum);
	elm->modenvvelf = NULL, elm->modenvvelfnum = 0;
	safe_free(elm->modpitch);
	elm->modpitch = NULL, elm->modpitchnum = 0;
	safe_free(elm->modfc);
	elm->modfc = NULL, elm->modfcnum = 0;
	safe_free(elm->fc);
	elm->fc = NULL, elm->fcnum = 0;
	safe_free(elm->reso);
	elm->reso = NULL, elm->resonum = 0;
///r
	safe_free(elm->fclow);
	elm->fclow = NULL, elm->fclownum = 0;
	safe_free(elm->fclowkeyf);
	elm->fclowkeyf = NULL, elm->fclowkeyfnum = 0;
	safe_free(elm->fcmul);
	elm->fcmul = NULL, elm->fcmulnum = 0;
	safe_free(elm->fcadd);
	elm->fcadd = NULL, elm->fcaddnum = 0;	
	if (elm->pitenvnum)
		free_ptr_list(elm->pitenv, elm->pitenvnum);
	elm->pitenv = NULL, elm->pitenvnum = 0;
	if (elm->hpfnum)
		free_ptr_list(elm->hpf, elm->hpfnum);
	elm->hpf = NULL, elm->hpfnum = 0;
	for(i = 0; i < VOICE_EFFECT_NUM; i++){
		if (elm->vfxnum[i]){
			free_ptr_list(elm->vfx[i], elm->vfxnum[i]);
		}
		elm->vfx[i] = NULL, elm->vfxnum[i] = 0;
	}
}

///r
void free_instruments(int reload_default_inst)
{
    int i = 128 + map_bank_counter, j;
    struct InstrumentCache *p;
    ToneBank *bank;
    Instrument *ip;
    struct InstrumentCache *default_entry;
    int default_entry_addr;
	int elm;

    clear_magic_instruments();

    /* Free soundfont/SCC/MMS instruments */
    while(i--)
    {
	/* Note that bank[*]->tone[j].instrument may pointer to
	   bank[0]->tone[j].instrument. See play_midi_load_instrument()
	   at playmidi.c for the implementation */

	if((bank = tonebank[i]) != NULL)
	    for(j = 127; j >= 0; j--)
			for(elm = 0; elm < MAX_ELEMENT; elm++)
			{
				if(bank->tone[j][elm] == NULL)
					continue;
				ip = bank->tone[j][elm]->instrument;
				if(ip && (ip->type == INST_SF2 || ip->type == INST_PCM || ip->type == INST_MMS || ip->type == INST_SCC || ip->type == INST_SFZ) &&
					(i == 0 || !tonebank[0]->tone[j][elm] || ip != tonebank[0]->tone[j][elm]->instrument) )
						free_instrument(ip);
				bank->tone[j][elm]->instrument = NULL;
			}
	if((bank = drumset[i]) != NULL)
	    for(j = 127; j >= 0; j--)
			for(elm = 0; elm < MAX_ELEMENT; elm++)
			{
				if(bank->tone[j][elm] == NULL)
					continue;
				ip = bank->tone[j][elm]->instrument;
				if(ip && (ip->type == INST_SF2 || ip->type == INST_PCM || ip->type == INST_MMS || ip->type == INST_SCC || ip->type == INST_SFZ) &&
				   (i == 0 || !drumset[0]->tone[j][elm] || ip != drumset[0]->tone[j][elm]->instrument) )
					free_instrument(ip);
				bank->tone[j][elm]->instrument = NULL;
			}
#if 0
		if ((drumset[i] != NULL) && (drumset[i]->alt != NULL)) {
			safe_free(drumset[i]->alt);
			drumset[i]->alt = NULL;
		}
#endif
    }

    /* Free GUS/patch instruments */
    default_entry = NULL;
    default_entry_addr = 0;
    for(i = 0; i < INSTRUMENT_HASH_SIZE; i++)
    {
	p = instrument_cache[i];
	while(p != NULL)
	{
	    if(!reload_default_inst && p->ip == default_instrument)
	    {
		default_entry = p;
		default_entry_addr = i;
		p = p->next;
	    }
	    else
	    {
		struct InstrumentCache *tmp;

		tmp = p;
		p = p->next;
		free_instrument(tmp->ip);
		safe_free(tmp);
	    }
	}
	instrument_cache[i] = NULL;
    }

    if(reload_default_inst)
	set_default_instrument(NULL);
    else if(default_entry)
    {
	default_entry->next = NULL;
	instrument_cache[default_entry_addr] = default_entry;
    }
#ifdef INT_SYNTH
	free_int_synth_preset();
#endif
}

void free_special_patch(int id)
{
    int i, j, start, end;

    if(id >= 0)
	start = end = id;
    else
    {
	start = 0;
	end = NSPECIAL_PATCH - 1;
    }

    for(i = start; i <= end; i++)
	if(special_patch[i] != NULL)
	{
	    Sample *sp;
	    int n;

	    if(special_patch[i]->name != NULL)
		safe_free(special_patch[i]->name);
			special_patch[i]->name = NULL;
	    n = special_patch[i]->samples;
	    sp = special_patch[i]->sample;
	    if(sp)
	    {
		for(j = 0; j < n; j++)
		    if(sp[j].data_alloced && sp[j].data)
			safe_free(sp[j].data);
		safe_free(sp);
	    }
	    safe_free(special_patch[i]);
	    special_patch[i] = NULL;
	}
}

int set_default_instrument(char *name)
{
    Instrument *ip;
    int i;
    static char *last_name;
	int elm = 0;

    if(name == NULL)
    {
	name = last_name;
	if(name == NULL)
	    return 0;
    }

    if(!(ip = load_gus_instrument(name, NULL, 0, 0, NULL, elm)))
		return -1;
    if(default_instrument)
		free_instrument(default_instrument);
    default_instrument = ip;
    for(i = 0; i < MAX_CHANNELS; i++)
		default_program[i] = SPECIAL_PROGRAM;
    last_name = name;

    return 0;
}

/*! search mapped bank.
    returns negative value indicating free bank if not found,
    0 if no free bank was available */
int find_instrument_map_bank(int dr, int map, int bk)
{
	struct bank_map_elem *bm;
	int i;
	
	if (map == INST_NO_MAP)
		return 0;
	bm = dr ? map_drumset : map_bank;
	for(i = 0; i < MAP_BANK_COUNT; i++)
	{
		if (!bm[i].used)
			return -(128 + i);
		else if (bm[i].mapid == map && bm[i].bankno == bk)
			return 128 + i;
	}
	return 0;
}

/*! allocate mapped bank if needed. returns -1 if allocation failed. */
int alloc_instrument_map_bank(int dr, int map, int bk)
{
	struct bank_map_elem *bm;
	int i;
	
	if (map == INST_NO_MAP)
	{
		alloc_instrument_bank(dr, bk);
		return bk;
	}
	i = find_instrument_map_bank(dr, map, bk);
	if (i == 0)
		return -1;
	if (i < 0)
	{
		i = -i - 128;
		bm = dr ? map_drumset : map_bank;
		bm[i].used = 1;
		bm[i].mapid = map;
		bm[i].bankno = bk;
		if (map_bank_counter < i + 1)
			map_bank_counter = i + 1;
		i += 128;
		alloc_instrument_bank(dr, i);
	}
	return i;
}


///r
static void init_tone_bank_element(ToneBankElement *tone)
{
	tone->note = -1;
	tone->strip_loop = -1;
	tone->strip_envelope = -1;
	tone->strip_tail = -1;
	tone->amp = -1;
	tone->amp_normalize = 0;
	tone->lokey = -1;
	tone->hikey = -1;
	tone->lovel = -1;
	tone->hivel = -1;
	tone->rnddelay = 0;
	tone->loop_timeout = 0;
	tone->legato = 0;
	tone->damper_mode = 0;
	tone->key_to_fc = 0;
	tone->vel_to_fc = 0;
	tone->reverb_send = DEFALT_REVERB_SEND; // def -1
	tone->chorus_send = DEFALT_CHORUS_SEND; // def -1
	tone->delay_send = DEFALT_DELAY_SEND; // def -1
	tone->lpf_type = -1;
	tone->rx_note_off = 1;
	tone->keep_voice = 0;
	tone->tva_level = -1;
	tone->play_note = -1;
	tone->element_num = 0;
	tone->def_pan = -1;
	tone->sample_pan = -1;
	tone->sample_width = -1;
}

///r
void reinit_tone_bank_element(ToneBankElement *tone)
{
	free_tone_bank_element(tone);
	init_tone_bank_element(tone);
}

///r
int alloc_tone_bank_element(ToneBankElement **tone_ptr)
{
	*tone_ptr = (ToneBankElement *)safe_malloc(sizeof(ToneBankElement));
	if(*tone_ptr == NULL){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			"alloc_tone_bank_element: ToneBankElement malloc error.");
		return 1; // error
	}
	memset(*tone_ptr, 0, sizeof(ToneBankElement));
	init_tone_bank_element(*tone_ptr);
	return 0;
}

///r
void alloc_instrument_bank(int dr, int bk)
{
    ToneBank *b;

    if(dr)
    {
	if((b = drumset[bk]) == NULL)
	{
	    b = drumset[bk] = (ToneBank *)safe_malloc(sizeof(ToneBank));
		if(drumset[bk] == NULL){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"alloc_instrument_bank: ToneBank malloc error. drumset");
			return;
		}
	    memset(b, 0, sizeof(ToneBank));
		if(b->tone[0][0] == NULL)
		{
			if(alloc_tone_bank_element(&b->tone[0][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"alloc_instrument_bank: ToneBankElement malloc error. drumset");
				return;
			}
		}
	}
    }
    else
    {
	if((b = tonebank[bk]) == NULL)
	{
	    b = tonebank[bk] = (ToneBank *)safe_malloc(sizeof(ToneBank));
		if(tonebank[bk] == NULL){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"alloc_instrument_bank: ToneBank malloc error. tonebank");
			return;
		}
	    memset(b, 0, sizeof(ToneBank));
		if(b->tone[0][0] == NULL)
		{
			if(alloc_tone_bank_element(&b->tone[0][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"alloc_instrument_bank: ToneBankElement malloc error. tonebank");
				return;
			}
		}
	}
    }
}


/* Instrument alias map - Written by Masanao Izumo */

int instrument_map_no_mapped(int mapID, int *set, int *elem)
{
    int s, e;
    struct inst_map_elem *p;

    if(mapID == INST_NO_MAP)
	return 0; /* No map */

    s = *set;
    e = *elem;
    p = inst_map_table[mapID][s];
    if(p != NULL)
    {
	*set = p[e].set;
	*elem = p[e].elem;
	return 1;
    }

    if(s != 0)
    {
	p = inst_map_table[mapID][0];
	if(p != NULL)
	{
	    *set = p[e].set;
	    *elem = p[e].elem;
	}
	return 2;
    }
    return 0;
}

int instrument_map(int mapID, int *set, int *elem)
{
    int s, e;
    struct inst_map_elem *p;

    if(mapID == INST_NO_MAP)
	return 0; /* No map */

    s = *set;
    e = *elem;
    p = inst_map_table[mapID][s];
    if(p != NULL && p[e].mapped)
    {
	*set = p[e].set;
	*elem = p[e].elem;
	return 1;
    }

    if(s != 0)
    {
	p = inst_map_table[mapID][0];
	if(p != NULL && p[e].mapped)
	{
	    *set = p[e].set;
	    *elem = p[e].elem;
	}
	return 2;
    }
    return 0;
}

void set_instrument_map(int mapID,
			int set_from, int elem_from,
			int set_to, int elem_to)
{
    struct inst_map_elem *p;

    p = inst_map_table[mapID][set_from];
    if(p == NULL)
    {
		p = (struct inst_map_elem *)
	    safe_malloc(128 * sizeof(struct inst_map_elem));
	    memset(p, 0, 128 * sizeof(struct inst_map_elem));
		inst_map_table[mapID][set_from] = p;
    }
    p[elem_from].set = set_to;
    p[elem_from].elem = elem_to;
	p[elem_from].mapped = 1;
}

void free_instrument_map(void)
{
  int i, j;

  for(i = 0; i < map_bank_counter; i++)
    map_bank[i].used = map_drumset[i].used = 0;
  /* map_bank_counter = 0; never shrinks rather than assuming tonebank was already freed */
  for (i = 0; i < NUM_INST_MAP; i++) {
    for (j = 0; j < 128; j++) {
      struct inst_map_elem *map;
      map = inst_map_table[i][j];
      if (map) {
	safe_free(map);
	inst_map_table[i][j] = NULL;
      }
    }
  }
}

/* Alternate assign - Written by Masanao Izumo */

AlternateAssign *add_altassign_string(AlternateAssign *old,
				      char **params, int n)
{
    int i, j;
    char *p;
    int beg, end;
    AlternateAssign *alt;

    if(n == 0)
	return old;
    if(!strcmp(*params, "clear")) {
	while(old) {
	    AlternateAssign *next;
	    next = old->next;
	    safe_free(old);
	    old = next;
	}
	params++;
	n--;
	if(n == 0)
	    return NULL;
    }

    alt = (AlternateAssign *)safe_malloc(sizeof(AlternateAssign));
    memset(alt, 0, sizeof(AlternateAssign));
    for(i = 0; i < n; i++) {
	p = params[i];
	if(*p == '-') {
	    beg = 0;
	    p++;
	} else
	    beg = atoi(p);
	if((p = strchr(p, '-')) != NULL) {
	    if(p[1] == '\0')
		end = 127;
	    else
		end = atoi(p + 1);
	} else
	    end = beg;
	if(beg > end) {
	    int t;
	    t = beg;
	    beg = end;
	    end = t;
	}
	if(beg < 0)
	    beg = 0;
	if(end > 127)
	    end = 127;
	for(j = beg; j <= end; j++)
	    alt->bits[(j >> 5) & 0x3] |= 1 << (j & 0x1F);
    }
    alt->next = old;
    return alt;
}

AlternateAssign *find_altassign(AlternateAssign *altassign, int note)
{
    AlternateAssign *p;
    uint32 mask;
    int idx;

    mask = 1 << (note & 0x1F);
    idx = (note >> 5) & 0x3;
    for(p = altassign; p != NULL; p = p->next)
	if(p->bits[idx] & mask)
	    return p;
    return NULL;
}
