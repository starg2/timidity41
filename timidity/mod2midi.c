/*
   TiMidity++ -- MIDI to WAVE converter and player
   Copyright (C) 1999,2000 Masanao Izumo <mo@goice.co.jp>
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   mod2midi.c

   Sample info -> MIDI event conversion

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
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
#include "tables.h"
#include "mod.h"
#include "output.h"
#include "controls.h"
#include "unimod.h"
#include "mod2midi.h"


#define SETMIDIEVENT(e, at, t, ch, pa, pb) \
    { (e).time = (at); (e).type = (t); \
      (e).channel = (uint8)(ch); (e).a = (uint8)(pa); (e).b = (uint8)(pb); }

#define MIDIEVENT(at, t, ch, pa, pb) \
    { MidiEvent event; SETMIDIEVENT(event, at, t, ch, pa, pb); \
      readmidi_add_event(&event); }

/*
		   Clock
   SampleRate := ----------
		   Period
 */


#define NTSC_CLOCK 3579545.25
#define NTSC_RATE (NTSC_CLOCK/428)	/* <-- 428 = period for c2 */

#define PAL_CLOCK 3546894.6
#define PAL_RATE (PAL_CLOCK/428)

#define MOD_NOTE_OFFSET 60

#define MOD_BEND_SENSITIVE 60

typedef struct _ModVoice
  {
    int sample;			/* current sample ID */
    int noteon;			/* (-1 means OFF status) */
    int period;			/* current frequency */
    int tuneon;			/* note fine tune */
    int pan;			/* current panning */
    int vol;			/* current volume */
    int start;			/* sample start */
  }
ModVoice;

static void mod_change_tempo (int32 at, int bpm);
static int mod_pitch_bend (int tune);
static int period2note (int period, int *finetune);

static ModVoice ModV[MOD_NUM_VOICES];
static int at;

static int period_table[84] =
{
/*       C     C#    D     D#    E     F     F#    G     G#    A    A#   B  */
/* 0 */ 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906,
/* 1 */ 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
/* 2 */ 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
/* 3 */ 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
/* 4 */ 107, 101, 95, 90, 85, 80, 75, 71, 67, 63, 60, 56,
/* 5 */ 53, 50, 47, 45, 42, 40, 37, 35, 33, 31, 30, 28,
/* 6 */ 27, 25, 24, 22, 21, 20, 19, 18, 17, 16, 15, 14
};

void
mod_change_tempo (int32 at, int bpm)
{
  int32 tempo;
  int c, a, b;

  tempo = 60000000 / bpm;
  c = (tempo & 0xff);
  a = ((tempo >> 8) & 0xff);
  b = ((tempo >> 16) & 0xff);
  MIDIEVENT (at, ME_TEMPO, c, b, a);
}

int
mod_pitch_bend(int tune)
{
  int bend;

  bend = tune * (8192 / 256) / MOD_BEND_SENSITIVE + 8192;
  if (bend <= 0)
    bend = 1;
  else if (bend >= 2 * 8192)
    bend = 2 * 8192 - 1;

  return bend;
}

int
period2note (int period, int *finetune)
{
  int note;
  int l, r, m;

  if (period < 14 || period > 1712)
    return -1;

  /* bin search */
  l = 0;
  r = 84;
  while (l < r)
    {
      m = (l + r) / 2;
      if (period_table[m] >= period)
	l = m + 1;
      else
	r = m;
    }
  note = l - 1;

  /*
   * 83 >= note >= 0
   * period_table[note] >= period > period_table[note + 1]
   */

  if (period_table[note] == period)
    {
      *finetune = 0;
    }
  else
    {
      /* fine tune completion */
      *finetune = (int) (256.0 *
		     (period_table[note] - period) /
		     (period_table[note] - period_table[note + 1]));
    }

  return note + MOD_NOTE_OFFSET;
}

/********** Interface to mod.c */

void
Voice_SetVolume (UBYTE v, UWORD vol)
{
  if (v >= MOD_NUM_VOICES)
    return;

  if (vol != ModV[v].vol) {
    ModV[v].vol = vol;
    vol = vol > 254 ? 127 : vol >> 1;
    MIDIEVENT (at, ME_EXPRESSION, v, vol, 0);
  }
}

void
Voice_SetPeriod (UBYTE v, ULONG period)
{
  int tune, new_noteon, bend;

  if (v >= MOD_NUM_VOICES)
    return;

  ModV[v].period = period;
  new_noteon = period2note (ModV[v].period, &tune);
  if (new_noteon < 0)
    {
      return;
    }

  if (ModV[v].noteon != new_noteon)
    MIDIEVENT(at, ME_NOTEOFF, v, ModV[v].noteon, 0);

  if (ModV[v].tuneon != tune)
    {
      ModV[v].tuneon = tune;
      bend = mod_pitch_bend(tune);
      MIDIEVENT (at, ME_PITCHWHEEL, v, bend & 0x7F, (bend >> 7) & 0x7F);
    }
 
  if (ModV[v].noteon != new_noteon)
    {
      ModV[v].noteon = new_noteon;
      MIDIEVENT(at, ME_NOTEON, v, ModV[v].noteon, 0x7f);
    }
}

void
Voice_SetPanning (UBYTE v, ULONG pan)
{
  if (v >= MOD_NUM_VOICES)
    return;
  if (pan == PAN_SURROUND)
    pan = PAN_CENTER; /* :-( */

  if (pan != ModV[v].pan) {
    ModV[v].pan = pan;
    MIDIEVENT(at, ME_PAN, v, pan * 127 / PAN_RIGHT, 0);
  }
}

void
Voice_Play (UBYTE v, SAMPLE * s, ULONG start)
{
  int tune, new_noteon, new_sample, bend;
  if (v >= MOD_NUM_VOICES)
    return;

  new_noteon = period2note (ModV[v].period, &tune);
  new_sample = s->id;

  ModV[v].noteon = new_noteon;
  if (ModV[v].noteon < 0)
    {
      return;
    }

  if (ModV[v].sample != new_sample)
    {
      ModV[v].sample = new_sample;
      MIDIEVENT(at, ME_SET_PATCH, v, ModV[v].sample, 0);
    }

  if (ModV[v].start != start)
    {
      int a, b;
      ModV[v].start = start;
      a = (ModV[v].start & 0xff);
      b = ((ModV[v].start >> 8) & 0xff);
      MIDIEVENT (at, ME_PATCH_OFFS, v, a, b);
    }

  if (ModV[v].tuneon != tune)
    {
      ModV[v].tuneon = tune;
      bend = mod_pitch_bend(tune);
      MIDIEVENT (at, ME_PITCHWHEEL, v, bend & 0x7F, (bend >> 7) & 0x7F);
    }

  MIDIEVENT (at, ME_NOTEON, v, ModV[v].noteon, 0x7f);
}

void
Voice_Stop (UBYTE v)
{
  if (v >= MOD_NUM_VOICES)
    return;

  if (ModV[v].noteon != -1)
    {
      MIDIEVENT (at, ME_NOTEOFF, v, ModV[v].noteon, 0);
      ModV[v].noteon = -1;
    }
}

BOOL
Voice_Stopped (UBYTE v)
{
  return (v >= MOD_NUM_VOICES) ? 1 : (ModV[v].noteon == -1);
}

void
Voice_TickDone ()
{
  at++;
}

void
Voice_NewTempo (UWORD bpm, UWORD sngspd)
{
  mod_change_tempo(at, bpm);
}

void
Voice_EndPlaying ()
{
  int v;

  at += 48 / (60.0/125.0);		/* 1 second */
  for(v = 0; v < MOD_NUM_VOICES; v++)
    MIDIEVENT(at, ME_ALL_NOTES_OFF, v, 0, 0);
}

void
Voice_StartPlaying ()
{
  int v;

  readmidi_set_track(0, 1);

  current_file_info->divisions = 24;

  for(v = 0; v < MOD_NUM_VOICES; v++)
    {
	ModV[v].sample = -1;
	ModV[v].noteon = -1;
	ModV[v].period = 0;
	ModV[v].tuneon = 0;
	ModV[v].pan = (v & 1) ? 127 : 0;
	ModV[v].vol = 64;
	ModV[v].start = 0;

	MIDIEVENT(0, ME_PAN, v, ModV[v].pan, 0);
        MIDIEVENT(0, ME_SET_PATCH, v, 1, 0);
        MIDIEVENT(0, ME_MAINVOLUME, v, 127, 0);
	MIDIEVENT(0, ME_RPN_LSB, v, 0, 0);
	MIDIEVENT(0, ME_RPN_MSB, v, 0, 0);
	MIDIEVENT(0, ME_DATA_ENTRY_MSB, v, MOD_BEND_SENSITIVE, 0);
	MIDIEVENT(0, ME_DRUMPART, v, 0, 0);
    }

  at = 1;
}

/* convert from 8bit value to fractional offset (15.15) */
static int32 env_offset(int offset)
{
    return (int32)offset << (7+15);
}

/* calculate ramp rate in fractional unit;
 * diff = 8bit, time = msec
 */
static int32 env_rate(int diff, double msec)
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


void load_module_samples (SAMPLE * s, int numsamples)
{
    int i;

    for(i = 1; numsamples--; i++, s++)
    {
	Sample *sp;
	char name[23];

	if(!s->data)
	    continue;

	ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		  "MOD Sample %d (%.22s)", i, s->samplename);

	special_patch[i] =
	    (SpecialPatch *)safe_malloc(sizeof(SpecialPatch));
	special_patch[i]->type = INST_MOD;
	special_patch[i]->samples = 1;
	special_patch[i]->sample = sp =
	    (Sample *)safe_malloc(sizeof(Sample));
	memset(sp, 0, sizeof(Sample));
	strncpy(name, s->samplename, 22);
	name[22] = '\0';
	code_convert(name, NULL, 23, NULL, "ASCII");
	if(name[0] == '\0')
	    special_patch[i]->name = NULL;
	else
	    special_patch[i]->name = safe_strdup(name);
	special_patch[i]->sample_offset = 0;

	sp->data = (sample_t *)s->data;
	sp->data_alloced = 1;
	sp->data_length = s->length;
	sp->loop_start = s->loopstart;
	sp->loop_end   = s->loopend;

	/* Stereo instruments (SF_STEREO) are dithered by libunimod into mono */
	sp->modes = MODES_UNSIGNED;
	if (s->flags & SF_SIGNED)  sp->modes ^= MODES_UNSIGNED;
	if (s->flags & SF_LOOP)    sp->modes ^= MODES_LOOPING;
	if (s->flags & SF_BIDI)    sp->modes ^= MODES_PINGPONG;
	if (s->flags & SF_REVERSE) sp->modes ^= MODES_REVERSE;
	if (s->flags & SF_16BITS)  sp->modes ^= MODES_16BIT;

	/* libunimod sets *both* SF_LOOP and SF_BIDI/SF_REVERSE */
	if (sp->modes & (MODES_PINGPONG | MODES_REVERSE))
	  sp->modes &= ~MODES_LOOPING;

	/* envelope (0,1:attack, 2:sustain, 3,4,5:release) */
	sp->modes |= MODES_ENVELOPE;
	/* attack */
	sp->envelope_offset[0] = env_offset(255);
	sp->envelope_rate[0]   = env_rate(255, 0.0);	/* fastest */
	sp->envelope_offset[1] = sp->envelope_offset[0];
	sp->envelope_rate[1]   = 0; /* skip this stage */
	/* sustain */
	sp->envelope_offset[2] = sp->envelope_offset[1];
	sp->envelope_rate[2]   = 0;
	/* release */
	sp->envelope_offset[3] = env_offset(0);
	sp->envelope_rate[3]   = env_rate(255, 200.0);	/* 200 msec */
	sp->envelope_offset[4] = sp->envelope_offset[3];
	sp->envelope_rate[4]   = 0; /* skip this stage */
	sp->envelope_offset[5] = sp->envelope_offset[4];
	sp->envelope_rate[5]   = 0; /* skip this stage, then the voice is
				       disappeared */

	sp->sample_rate = ((int32)PAL_RATE) >> s->divfactor;
	sp->low_freq = 0;
	sp->high_freq = 0x7fffffff;
	sp->root_freq = freq_table[MOD_NOTE_OFFSET];
	sp->volume = 1.0;		/* I guess it should use globvol... */
	sp->panning = s->panning == PAN_SURROUND ? 64 : s->panning * 128 / 255;
	sp->low_vel = 0;
	sp->high_vel = 127;
	sp->data_length <<= FRACTION_BITS;
	sp->loop_start <<= FRACTION_BITS;
	sp->loop_end <<= FRACTION_BITS;

	s->data = NULL;		/* Avoid free-ing */
	s->id = i;
    }
}

/* ex:set ts=4: */
