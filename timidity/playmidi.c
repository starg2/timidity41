/*

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
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

    playmidi.c -- random stuff in need of rearrangement

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"

#ifndef __WIN32__
#include <unistd.h>
#else
#include <windows.h>
extern VOLATILE int intr;
#endif /* __WIN32__ */

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
#include "reverb.h"
#include "wrd.h"

#ifdef SOLARIS
/* shut gcc warning up */
int usleep(unsigned int useconds);
#endif

#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */

#include "tables.h"

#define PLAY_INTERLEAVE_SEC 1.0
#define MIDITRACE_OUTPUT_FRAGMENTS 32
#define DEFAULT_PORTAMENTO_TIME 2560
#define PORTAMENTO_TIME_TUNING (1.0 / 4096.0)
#define PORTAMENTO_CONTROL_RATIO 100
#define DEFAULT_CHORUS_DELAY1 0.02
#define DEFAULT_CHORUS_DELAY2 0.003
#define CHORUS_OPPOSITE_THRESHOLD    32
#define CHORUS_VELOCITY_TUNING1      0.7
#define CHORUS_VELOCITY_TUNING2      0.6
#define EOT_PRESEARCH_LEN 32
#define SPEED_CHANGE_RATE 1.0594630943592953  /* 2^(1/12) */

static int prescanning_flag;
static int32 midi_restart_time = 0;
Channel channel[MAX_CHANNELS];
Voice voice[MAX_VOICES];
int32 current_play_tempo = 500000;
int32 play_tempo_offset = 0;
int opt_realtime_playing = 0;
static MBlockList playmidi_pool;
int check_eot_flag;
int special_tonebank = -1;
int default_tonebank = 0;
int playmidi_seek_flag = 0;
static int play_pause_flag = 0;
static int file_from_stdin;
static struct ReverbControls reverb_ctls[MAX_CHANNELS];
static void set_reverb_level(int ch, int level);
static int make_rvid_flag = 0;
static int data_output_count;
#define fragment_start_count (play_mode->rate)

/* Ring voice id for each notes */
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
int opt_reverb_control = 0;
#endif /* REVERB_CONTROL_ALLOW */

#ifdef CHORUS_CONTROL_ALLOW
int opt_chorus_control = 1;
#else
int opt_chorus_control = 0;
#endif /* CHORUS_CONTROL_ALLOW */

#ifdef GM_CHANNEL_PRESSURE_ALLOW
int opt_channel_pressure = 1;
#else
int opt_channel_pressure = 0;
#endif /* GM_CHANNEL_PRESSURE_ALLOW */

#ifdef OVERLAP_VOICE_ALLOW
int opt_overlap_voice_allow = 1;
#else
int opt_overlap_voice_allow = 0;
#endif /* OVERLAP_VOICE_ALLOW */

int voices=DEFAULT_VOICES, upper_voices;

int32
    control_ratio=0,
    amplification=DEFAULT_AMPLIFICATION;

static FLOAT_T
    master_volume;
static int32 master_volume_ratio = 0xFFFF;
ChannelBitMask default_drumchannel_mask;
ChannelBitMask default_drumchannels;
ChannelBitMask drumchannel_mask;
ChannelBitMask drumchannels;
int adjust_panning_immediately=0;
double envelope_modify_rate = 1.0;

static int32 lost_notes, cut_notes;
static int32 common_buffer[AUDIO_BUFFER_SIZE*2], /* stereo samples */
             *buffer_pointer;
static int32 buffered_count;
static char *channel_buffer = NULL; /* MAX_CHANNELS*AUDIO_BUFFER_SIZE*8 */


static MidiEvent *event_list;
static MidiEvent *current_event;
static int32 sample_count;
int32 current_sample;
int note_key_offset = 0;
FLOAT_T midi_time_ratio = 1.0;

#ifdef PRESENCE_HACK
static void presence_resample(int32 count);
extern double presence_delay_msec;
#endif /* PRESENCE_HACK */

static void update_portamento_controls(int ch);
static void update_rpn_map(int ch, int addr, int update_now);
static void ctl_prog_event(int ch);
static void ctl_timestamp(void);

static char *event_name(int type)
{
#define EVENT_NAME(X) case X: return #X
    switch(type)
    {
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
	EVENT_NAME(ME_MAINVOLUME);
	EVENT_NAME(ME_PAN);
	EVENT_NAME(ME_EXPRESSION);
	EVENT_NAME(ME_SUSTAIN);
	EVENT_NAME(ME_PORTAMENTO_TIME);
	EVENT_NAME(ME_PORTAMENTO);
	EVENT_NAME(ME_DATA_ENTRY_MSB);
#if 0
	EVENT_NAME(ME_DATA_ENTRY_LSB);
	EVENT_NAME(ME_SUSTENUTO);
	EVENT_NAME(ME_SOFT_PEDAL);
	EVENT_NAME(ME_HARMONIC_CONTENT);
	EVENT_NAME(ME_RELEASE_TIME);
	EVENT_NAME(ME_ATTACK_TIME);
	EVENT_NAME(ME_BRIGHTNESS);
#endif
	EVENT_NAME(ME_REVERB_EFFECT);
	EVENT_NAME(ME_CHORUS_EFFECT);
#if 0
	EVENT_NAME(ME_VARIATION_EFFECT);
#endif
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
	EVENT_NAME(ME_RANDOM_PAN);
	EVENT_NAME(ME_SET_PATCH);
	EVENT_NAME(ME_DRUMPART);
	EVENT_NAME(ME_KEYSHIFT);
#if 0
	EVENT_NAME(ME_VOLUME_ONOFF);
#endif
	EVENT_NAME(ME_TEMPO);
	EVENT_NAME(ME_CHORUS_TEXT);
	EVENT_NAME(ME_LYRIC);
	EVENT_NAME(ME_MARKER);
	EVENT_NAME(ME_INSERT_TEXT);
	EVENT_NAME(ME_TEXT);
	EVENT_NAME(ME_KARAOKE_LYRIC);
	EVENT_NAME(ME_MASTER_VOLUME);
	EVENT_NAME(ME_RESET);
	EVENT_NAME(ME_NOTE_STEP);
	EVENT_NAME(ME_PATCH_OFFS);
	EVENT_NAME(ME_WRD);
	EVENT_NAME(ME_TIMESIG);
	EVENT_NAME(ME_LAST);
	EVENT_NAME(ME_EOT);
    }
    return "Unknown";
#undef EVENT_NAME
}

static void adjust_amplification(void)
{
    master_volume = (double)(amplification) / 100.0 *
	((double)master_volume_ratio * (1.0/0xFFFF));
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

static void reset_voices(void)
{
    int i;
    for(i = 0; i < MAX_VOICES; i++)
	voice[i].status = VOICE_FREE;
    upper_voices = 0;
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

static void kill_note(int i)
{
    voice[i].status = VOICE_DIE;
    if(!prescanning_flag)
	ctl_note_event(i);
}

static void kill_all_voices(void)
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
    int i;

    if(note == -1)
    {
	for(i = 0; i < 128; i++)
	    if(d[i] != NULL)
	    {
		d[i]->drum_panning = NO_PANNING;
		memset(d[i]->drum_envelope_rate, 0,
		       sizeof(d[i]->drum_envelope_rate));
	    }
    }
    else
    {
	d[note]->drum_panning = NO_PANNING;
	memset(d[note]->drum_envelope_rate, 0,
	       sizeof(d[note]->drum_envelope_rate));
    }
}

/* Process the Reset All Controllers event */
static void reset_controllers(int c)
{
    /* Some standard says, although the SCC docs say 0. */
    
  if(play_system_mode == XG_SYSTEM_MODE)
      channel[c].volume = 100;
  else
      channel[c].volume = 90;

  channel[c].expression=127; /* SCC-1 does this. */
  channel[c].sustain=0;
  channel[c].pitchbend=0x2000;
  channel[c].pitchfactor=0; /* to be computed */
  channel[c].modulation_wheel = 0;
  channel[c].portamento_time = DEFAULT_PORTAMENTO_TIME;
  channel[c].porta_control_ratio = 0;
  channel[c].portamento = 0;
  channel[c].last_note_fine = -1;
  reset_drum_controllers(channel[c].drums, -1);

  channel[c].vibrato_ratio = 0;
  channel[c].vibrato_depth = 0;
  channel[c].vibrato_delay = 0;
  memset(channel[c].envelope_rate, 0, sizeof(channel[c].envelope_rate));
  update_portamento_controls(c);
  set_reverb_level(c, 0);
  if(opt_chorus_control <= 1)
      channel[c].chorus_level = 0;
  else
      channel[c].chorus_level = opt_chorus_control;
  channel[c].mono = 0;
}

static void redraw_controllers(int c)
{
    ctl_mode_event(CTLE_VOLUME, 1, c, channel[c].volume);
    ctl_mode_event(CTLE_EXPRESSION, 1, c, channel[c].expression);
    ctl_mode_event(CTLE_SUSTAIN, 1, c, channel[c].sustain);
    ctl_mode_event(CTLE_MOD_WHEEL, 1, c, channel[c].modulation_wheel);
    ctl_mode_event(CTLE_PITCH_BEND, 1, c, channel[c].pitchbend);
    ctl_prog_event(c);
    ctl_mode_event(CTLE_CHORUS_EFFECT, 1, c, channel[c].chorus_level);
    ctl_mode_event(CTLE_REVERB_EFFECT, 1, c,
		   channel[c].rb ? channel[c].rb->level : 0);
}

static void reset_midi(int playing)
{
    int i;
    for(i = 0; i < MAX_CHANNELS; i++)
    {
	reset_controllers(i);
	/* The rest of these are unaffected by the Reset All Controllers
	   event */
	channel[i].program = default_program[i];
	channel[i].panning = NO_PANNING;
	/* tone bank or drum set */
	if(ISDRUMCHANNEL(i))
	{
	    channel[i].bank = 0;
	    channel[i].altassign = drumset[0]->alt;
	}
	else
	{
	    if(special_tonebank >= 0)
		channel[i].bank = special_tonebank;
	    else
		channel[i].bank = default_tonebank;
	}
	channel[i].bank_lsb = channel[i].bank_msb = -1;
	update_rpn_map(i, RPN_ADDR_FFFF, 0);
	channel[i].special_sample = 0;
	channel[i].key_shift = 0;
	channel[i].mapID = get_default_mapID(i);
    }
    if(playing)
	kill_all_voices();
    else
	reset_voices();
#ifdef PRESENCE_HACK
    presence_resample(0);
#endif /* PRESENCE_HACK */
    master_volume_ratio = 0xFFFF;
    adjust_amplification();
    if(current_file_info)
    {
	memcpy(&drumchannels, &current_file_info->drumchannels,
	       sizeof(ChannelBitMask));
	memcpy(&drumchannel_mask, &current_file_info->drumchannel_mask,
	       sizeof(ChannelBitMask));
    }
    else
    {
	memcpy(&drumchannels, &default_drumchannels, sizeof(ChannelBitMask));
	memcpy(&drumchannel_mask, &default_drumchannel_mask,
	       sizeof(drumchannels));
    }
}

void recompute_freq(int v)
{
  int ch=voice[v].channel;
  int
    sign=(voice[v].sample_increment < 0), /* for bidirectional loops */
    pb=channel[ch].pitchbend;
  double a;
  int32 tuning = 0;

  if(!voice[v].sample->sample_rate)
      return;

  if(!opt_modulation_wheel)
      voice[v].modulation_wheel = 0;
  if(!opt_portamento)
      voice[v].porta_control_ratio = 0;

  voice[v].vibrato_control_ratio = voice[v].orig_vibrato_control_ratio;
  if(voice[v].vibrato_control_ratio || voice[v].modulation_wheel > 0)
  {
      /* This instrument has vibrato. Invalidate any precomputed
         sample_increments. */
      int i;
      if(voice[v].modulation_wheel > 0)
      {
	  voice[v].vibrato_control_ratio =
	      (int32)(play_mode->rate * MODULATION_WHEEL_RATE
		      / (2.0 * VIBRATO_SAMPLE_INCREMENTS));
	  voice[v].vibrato_delay = 0;
      }
      for(i = 0; i < VIBRATO_SAMPLE_INCREMENTS; i++)
	  voice[v].vibrato_sample_increment[i] = 0;
      voice[v].cache = NULL;
  }

  /* fine: [0..128] => [-256..256]
   * 1 coarse = 256 fine (= 1 note)
   * 1 fine = 2^5 tuning
   */
  tuning = (((int32)channel[ch].rpnmap[RPN_ADDR_0001] - 0x40)
	    + 64 * ((int32)channel[ch].rpnmap[RPN_ADDR_0002] - 0x40)) << 7;

  if(!voice[v].porta_control_ratio)
  {
      if(tuning == 0 && pb == 0x2000)
	  voice[v].frequency = voice[v].orig_frequency;
      else
      {
	  pb -= 0x2000;
	  if(!(channel[ch].pitchfactor))
	  {
	      /* Damn. Somebody bent the pitch. */
	      int32 i = (int32)pb * channel[ch].rpnmap[RPN_ADDR_0000] + tuning;
	      if(i >= 0)
		  channel[ch].pitchfactor =
		      bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F];
	      else
	      {
		  i = -i;
		  channel[ch].pitchfactor = 1.0 /
		      (bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F]);
	      }
	  }
	  voice[v].frequency =
	      (int32)((double)voice[v].orig_frequency
		      * channel[ch].pitchfactor);
	  if(voice[v].frequency != voice[v].orig_frequency)
	      voice[v].cache = NULL;
      }
  }
  else /* Portament */
  {
      int32 i;
      FLOAT_T pf;

      pb -= 0x2000;
      i = pb * channel[ch].rpnmap[RPN_ADDR_0000]
	  + (voice[v].porta_pb << 5) + tuning;
      if(i >= 0)
	  pf = bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F];
      else
      {
	  i = -i;
	  pf = 1.0 / (bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F]);
      }

      voice[v].frequency = (int32)((double)(voice[v].orig_frequency) * pf);
      voice[v].cache = NULL;
  }

  a = TIM_FSCALE(((double)voice[v].sample->sample_rate * voice[v].frequency) /
		 ((double)voice[v].sample->root_freq * play_mode->rate),
		 FRACTION_BITS) + 0.5;

#ifdef ABORT_AT_FATAL
  if((int32)a == 0)
  {
      fprintf(stderr, "Invalid sample increment a=%e %ld %ld %ld %ld%s\n",
	      a, (long)voice[v].sample->sample_rate, (long)voice[v].frequency,
	      (long)voice[v].sample->root_freq, (long)play_mode->rate,
	      voice[v].cache ? " (Cached)" : "");
      abort();
  }
#endif /* ABORT_AT_FATAL */

  if(sign)
      a = -a; /* need to preserve the loop direction */

  voice[v].sample_increment = (int32)a;
}

static void recompute_amp(int v)
{
    FLOAT_T tempamp;

    tempamp = ((FLOAT_T)master_volume *
	       voice[v].velocity *
	       voice[v].sample->volume *
	       channel[voice[v].channel].volume *
	       channel[voice[v].channel].expression); /* 21 bits */

    if(!(play_mode->encoding & PE_MONO))
    {
	if(voice[v].panning > 60 && voice[v].panning < 68)
	{
	    voice[v].panned = PANNED_CENTER;
	    voice[v].left_amp = TIM_FSCALENEG(tempamp, 21);
	}
	else if (voice[v].panning < 5)
	{
	    voice[v].panned = PANNED_LEFT;
	    voice[v].left_amp = TIM_FSCALENEG(tempamp, 20);
	}
	else if(voice[v].panning > 123)
	{
	    voice[v].panned = PANNED_RIGHT;
	    /* left_amp will be used */
	    voice[v].left_amp =  TIM_FSCALENEG(tempamp, 20);
	}
	else
	{
	    voice[v].panned = PANNED_MYSTERY;
	    voice[v].left_amp = TIM_FSCALENEG(tempamp, 27);
	    voice[v].right_amp = voice[v].left_amp * voice[v].panning;
	    voice[v].left_amp *= (FLOAT_T)(127 - voice[v].panning);
	}
    }
    else
    {
	voice[v].panned = PANNED_CENTER;
	voice[v].left_amp = TIM_FSCALENEG(tempamp, 21);
    }
}

static Instrument *play_midi_load_instrument(int dr, int bk, int prog)
{
    ToneBank **bank = ((dr) ? drumset : tonebank);
    Instrument *ip;

    if(bank[bk] == NULL)
	bk = 0;

    if((ip = bank[bk]->tone[prog].instrument) == MAGIC_LOAD_INSTRUMENT)
	ip = bank[bk]->tone[prog].instrument = load_instrument(dr, bk, prog);
    if(ip == NULL && bk != 0)
    {
	/* Instrument is not found.
	   Retry to load the instrument from bank 0 */

	if((ip = bank[0]->tone[prog].instrument) == MAGIC_LOAD_INSTRUMENT)
	    ip = bank[0]->tone[prog].instrument = load_instrument(dr, 0, prog);
	if(ip != NULL)
	    bank[bk]->tone[prog].instrument = ip;
    }

    return ip;
}

/* Only one instance of a note can be playing on a single channel. */
static int find_voice(MidiEvent *e)
{
  int i, j, lowest=-1, note, ch, status_check, mono_check;
  int32 lv, v;
  AlternateAssign *altassign;

  note = MIDI_EVENT_NOTE(e);
  ch = e->channel;

  if(opt_overlap_voice_allow)
      status_check = (VOICE_OFF | VOICE_SUSTAINED);
  else
      status_check = 0xFF;
  mono_check = channel[ch].mono;
  altassign = find_altassign(channel[ch].altassign, note);

  i = upper_voices;
  for(j = 0; j < i; j++)
      if(voice[j].status == VOICE_FREE)
      {
	  lowest = j; /* lower volume */
	  break;
      }

  for(j = 0; j < i; j++)
  {
      if(voice[j].status != VOICE_FREE &&
	 voice[j].channel == ch &&
	 (((voice[j].status & status_check) && voice[j].note == note) ||
	  mono_check ||
	  (altassign &&
	   (voice[j].note == note || find_altassign(altassign, voice[j].note)))
	  ))
	  kill_note(j);
  }

  if(lowest != -1)
  {
      /* Found a free voice. */
      if(upper_voices <= lowest)
	  upper_voices = lowest + 1;
      return lowest;
  }

  if(i < voices)
      return upper_voices++;

  lv = 0x7FFFFFFF;
  /* Look for the decaying note with the lowest volume */
  for(j = 0; j < i; j++)
    {
      if(voice[j].status & ~(VOICE_ON | VOICE_DIE))
	{
	  v=voice[j].left_mix;
	  if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	      v=voice[j].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=j;
	    }
	}
    }

  if (lowest != -1)
    {
      /* This can still cause a click, but if we had a free voice to
	 spare for ramping down this note, we wouldn't need to kill it
	 in the first place... Still, this needs to be fixed. Perhaps
	 we could use a reserve of voices to play dying notes only. */

      cut_notes++;
      /*fprintf(stderr, "cuts\n");*/
      voice[lowest].status=VOICE_FREE;
      if(!prescanning_flag)
	  ctl_note_event(lowest);
      return lowest;
    }

  lost_notes++;
  lv = voice[0].left_mix;
  if(voice[0].panned == PANNED_MYSTERY && voice[0].right_mix > lv)
      lv = voice[0].right_mix;
  lowest = 0;
  for(j = 1; j < i; j++)
  {
      v = voice[j].left_mix;
      if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	  v = voice[j].right_mix;
      if(v < lv)
      {
	  lv = v;
	  lowest = j;
      }
  }

  voice[lowest].status = VOICE_FREE;
  if(!prescanning_flag)
      ctl_note_event(lowest);
  return lowest;
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
	if(voice[i].status & ~(VOICE_ON | VOICE_DIE))
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
	voice[lowest].status = VOICE_FREE;
	ctl_note_event(lowest);
    }
    return lowest;
}

static int select_play_sample(Sample *splist, int nsp,
			      int note, int *vlist, MidiEvent *e)
{
    int32 f, cdiff, diff;
    Sample *sp, *closest;
    int i, j, nv, vel;

    f = freq_table[note];
    if(nsp == 1)
    {
	j = vlist[0] = find_voice(e);
	voice[j].orig_frequency = f;
	voice[j].sample = splist;
	voice[j].status = VOICE_ON;
	return 1;
    }

    vel = e->b;

    nv = 0;
    for(i = 0, sp = splist; i < nsp; i++, sp++)
	if(sp->low_vel <= vel && sp->high_vel >= vel &&
	   sp->low_freq <= f && sp->high_freq >= f)
	{
	    j = vlist[nv] = find_voice(e);
	    voice[j].orig_frequency = f;
	    voice[j].sample = sp;
	    voice[j].status = VOICE_ON;
	    nv++;
	}
    if(nv == 0)
    {
	cdiff = 0x7FFFFFFF;
	closest = sp = splist;
	for(i = 0; i < nsp; i++, sp++)
	{
	    diff = sp->root_freq - f;
	    if(diff < 0) diff = -diff;
	    if(diff < cdiff)
	    {
		cdiff = diff;
		closest = sp;
	    }
	}
	j = vlist[nv] = find_voice(e);
	voice[j].orig_frequency = f;
	voice[j].sample = closest;
	voice[j].status = VOICE_ON;
	nv++;
    }
    return nv;
}

static int find_samples(MidiEvent *e, int *vlist)
{
	Instrument *ip;
	int i, nv, note, ch, prog, bk;

	ch = e->channel;
	if(channel[ch].special_sample > 0)
	{
	    SpecialPatch *s;

	    s = special_patch[channel[ch].special_sample];
	    if(s == NULL)
	    {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
			  "Strange: Special patch %d is not installed",
			  channel[ch].special_sample);
		return 0;
	    }
	    return select_play_sample(s->sample, s->samples, e->a, vlist, e);
	}

	bk = channel[ch].bank;
	if(ISDRUMCHANNEL(ch))
	{
	    note = e->a;
	    instrument_map(channel[ch].mapID, &bk, &note);
	    if(!(ip = play_midi_load_instrument(1, bk, note)))
		return 0;	/* No instrument? Then we can't play. */

	    if(ip->type == INST_GUS && ip->samples != 1)
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
			  "Strange: percussion instrument with %d samples!",
			  ip->samples);
	    if(ip->sample->note_to_use)
		note = ip->sample->note_to_use;
	    i = vlist[0] = find_voice(e);
	    voice[i].orig_frequency = freq_table[note];
	    voice[i].sample = ip->sample;
	    voice[i].status = VOICE_ON;
	    return 1;
	}

	prog = channel[ch].program;
	if(prog == SPECIAL_PROGRAM)
	    ip = default_instrument;
	else
	{
	    instrument_map(channel[ch].mapID, &bk, &prog);
	    if(!(ip = play_midi_load_instrument(0, bk, prog)))
		return 0;	/* No instrument? Then we can't play. */
	}

	if(ip->sample->note_to_use)
	    note = ip->sample->note_to_use + note_key_offset;
	else
	    note = e->a + note_key_offset;
	if(note < 0)
	    note = 0;
	else if(note > 127)
	    note = 127;

	nv = select_play_sample(ip->sample, ip->samples, note, vlist, e);

	/* Replace the sample if the sample is cached. */
	if(!prescanning_flag)
	{
	    if(ip->sample->note_to_use)
		note = MIDI_EVENT_NOTE(e);

	    for(i = 0; i < nv; i++)
	    {
		int j;

		j = vlist[i];
		if(!opt_realtime_playing && allocate_cache_size > 0 &&
		   !channel[ch].portamento)
		{
		    voice[j].cache = resamp_cache_fetch(voice[j].sample, note);
		    if(voice[j].cache) /* cache hit */
			voice[j].sample = voice[j].cache->resampled;
		}
		else
		    voice[j].cache = NULL;
	    }
	}

	return nv;
}

static void start_note(MidiEvent *e, int i, int vid, int cnt)
{
  int j, ch, note;

  ch = e->channel;
  note = MIDI_EVENT_NOTE(e);
  voice[i].status=VOICE_ON;
  voice[i].channel=ch;
  voice[i].note=note;
  voice[i].velocity=e->b;
  j = channel[ch].special_sample;
  if(j == 0 || special_patch[j] == NULL)
      voice[i].sample_offset = 0;
  else
  {
      voice[i].sample_offset =
	  special_patch[j]->sample_offset << FRACTION_BITS;
      if(voice[i].sample->modes & MODES_LOOPING)
      {
	  if(voice[i].sample_offset > voice[i].sample->loop_end)
	      voice[i].sample_offset = voice[i].sample->loop_start;
      }
      else if(voice[i].sample_offset > voice[i].sample->data_length)
      {
	  voice[i].status = VOICE_FREE;
	  return;
      }
  }
  voice[i].sample_increment=0; /* make sure it isn't negative */
  voice[i].modulation_wheel=channel[ch].modulation_wheel;
  voice[i].delay=0;
  voice[i].vid=vid;

  voice[i].tremolo_phase=0;
  voice[i].tremolo_phase_increment=voice[i].sample->tremolo_phase_increment;
  voice[i].tremolo_sweep=voice[i].sample->tremolo_sweep_increment;
  voice[i].tremolo_sweep_position=0;

  voice[i].vibrato_sweep=voice[i].sample->vibrato_sweep_increment;
  voice[i].vibrato_sweep_position=0;

  if(opt_nrpn_vibrato &&
     channel[ch].vibrato_ratio && voice[i].vibrato_depth > 0)
  {
      voice[i].vibrato_control_ratio = channel[ch].vibrato_ratio;
      voice[i].vibrato_depth = channel[ch].vibrato_depth;
      voice[i].vibrato_delay = channel[ch].vibrato_delay;
  }
  else
  {
      voice[i].vibrato_control_ratio = voice[i].sample->vibrato_control_ratio;
      voice[i].vibrato_depth = voice[i].sample->vibrato_depth;
      voice[i].vibrato_delay = 0;
  }
  voice[i].orig_vibrato_control_ratio = voice[i].sample->vibrato_control_ratio;

  voice[i].vibrato_control_counter=voice[i].vibrato_phase=0;
  for (j=0; j<VIBRATO_SAMPLE_INCREMENTS; j++)
    voice[i].vibrato_sample_increment[j]=0;

  /* Pan */
  if(ISDRUMCHANNEL(ch) &&
     channel[ch].drums[note] != NULL &&
     channel[ch].drums[note]->drum_panning != NO_PANNING)
      voice[i].panning = channel[ch].drums[note]->drum_panning;
  else if(channel[ch].panning != NO_PANNING)
      voice[i].panning = channel[ch].panning;
  else
      voice[i].panning = voice[i].sample->panning;

  if(channel[ch].portamento && !channel[ch].porta_control_ratio)
  {
      update_portamento_controls(ch);
      if(!channel[ch].porta_control_ratio)
	  channel[ch].portamento = 0;
  }

  if(channel[ch].porta_control_ratio)
  {
      if(channel[ch].last_note_fine == -1)
      {
	  /* first on */
	  channel[ch].last_note_fine = voice[i].note * 256;
	  channel[ch].porta_control_ratio = 0;
      }
      else
      {
	  voice[i].porta_control_ratio = channel[ch].porta_control_ratio;
	  voice[i].porta_dpb = channel[ch].porta_dpb;
	  voice[i].porta_pb = channel[ch].last_note_fine - voice[i].note * 256;
	  if(voice[i].porta_pb == 0)
	      voice[i].porta_control_ratio = 0;
      }
  }
  if(cnt == 0)
      channel[ch].last_note_fine = voice[i].note * 256;

  recompute_freq(i);
  recompute_amp(i);
  if (voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
      voice[i].envelope_stage=0;
      voice[i].envelope_volume=0;
      voice[i].control_counter=0;
      recompute_envelope(i);
      apply_envelope_to_amp(i);
    }
  else
    {
      voice[i].envelope_increment=0;
      apply_envelope_to_amp(i);
    }

  voice[i].timeout = -1;
  if(cnt == 0 && channel[ch].rb != NULL && channel[ch].rb->lastin == 0)
      channel[ch].rb->lastin = current_sample + 1;
  if(!prescanning_flag)
      ctl_note_event(i);
}

static void finish_note(int i)
{
  if (voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* We need to get the envelope out of Sustain stage. */
      /* Note that voice[i].envelope_stage < 3 */
      voice[i].status=VOICE_OFF;
      voice[i].envelope_stage=3;
      recompute_envelope(i);
      apply_envelope_to_amp(i);
      ctl_note_event(i);
    }
  else
    {
      /* Set status to OFF so resample_voice() will let this voice out
         of its loop, if any. In any case, this voice dies when it
         hits the end of its data (ofs>=data_length). */
	if(voice[i].status != VOICE_OFF)
	{
	    voice[i].status=VOICE_OFF;
	    ctl_note_event(i);
	}
    }
}

static void new_chorus_voice(int v, int level)
{
    int cv, ch;
    uint8 vol;

    if((cv = find_free_voice()) == -1)
	return;
    ch = voice[v].channel;

    vol = voice[v].velocity;
    voice[cv] = voice[v];
    voice[v].velocity  = (uint8)(vol * CHORUS_VELOCITY_TUNING1);
    voice[cv].velocity = (uint8)(vol * CHORUS_VELOCITY_TUNING2);
    if(channel[ch].pitchbend + level < 0x2000)
        voice[cv].orig_frequency *= bend_fine[level];
    else
	voice[cv].orig_frequency /= bend_fine[level];
    voice[cv].cache = NULL;

    /* set panning & delay */
    if(play_mode->encoding & PE_MONO)
	voice[cv].delay = 0;
    else
    {
	double delay;

	if(voice[cv].panned == PANNED_CENTER)
	{
	    static int cpan[MAX_CHANNELS];
	    voice[cv].panning = 32 + cpan[ch];
	    cpan[ch] = (((cpan[ch] + 1) & 63));
	    delay = DEFAULT_CHORUS_DELAY2;
	}
	else
	{
	    int panning = voice[cv].panning;

	    if(panning < CHORUS_OPPOSITE_THRESHOLD)
	    {
		voice[cv].panning = 127;
		delay = DEFAULT_CHORUS_DELAY1;
	    }
	    else if(panning > 127 - CHORUS_OPPOSITE_THRESHOLD)
	    {
		voice[cv].panning = 0;
		delay = DEFAULT_CHORUS_DELAY1;
	    }
	    else
	    {
		voice[cv].panning = (panning < 64 ? 0 : 127);
		delay = DEFAULT_CHORUS_DELAY2;
	    }
	}
	voice[cv].delay = (int)(play_mode->rate * delay);
    }

    recompute_amp(v);
    apply_envelope_to_amp(v);
    recompute_amp(cv);
    apply_envelope_to_amp(cv);
    recompute_freq(cv);
}

static void note_on(MidiEvent *e)
{
    int i, nv, v, ch;
    int vlist[32];
    int vid;

    if((nv = find_samples(e, vlist)) == 0)
	return;
    vid = new_vidq(e->channel, MIDI_EVENT_NOTE(e));
    ch = e->channel;
    for(i = 0; i < nv; i++)
    {
	v = vlist[i];
	start_note(e, v, vid, nv - i - 1);
	if(channel[ch].chorus_level && voice[v].sample->sample_rate)
	    new_chorus_voice(v, channel[ch].chorus_level);
    }
}

static void set_voice_timeout(Voice *vp, int ch, int note)
{
    int prog;
    ToneBank *bank;

    if(channel[ch].special_sample > 0)
	return;

    if(ISDRUMCHANNEL(ch))
    {
	prog = note;
	bank = drumset[(int)channel[ch].bank];
	if(bank == NULL)
	    bank = drumset[0];
    }
    else
    {
	if((prog = channel[ch].program) == SPECIAL_PROGRAM)
	    return;
	bank = tonebank[(int)channel[ch].bank];
	if(bank == NULL)
	    bank = tonebank[0];
    }

    if(bank->tone[prog].loop_timeout > 0)
	vp->timeout = (int32)(bank->tone[prog].loop_timeout
			      * play_mode->rate * midi_time_ratio
			      + current_sample);
}

static void note_off(MidiEvent *e)
{
  int uv = upper_voices, i;
  int ch, note, vid, sustain;

  ch = e->channel;
  note = MIDI_EVENT_NOTE(e);
  if((vid = last_vidq(ch, note)) == -1)
      return;
  sustain = channel[ch].sustain;
  for(i = 0; i < uv; i++)
      if(voice[i].status==VOICE_ON &&
	 voice[i].channel==ch &&
	 voice[i].note==note &&
	 voice[i].vid==vid)
      {
	  if(sustain)
	  {
	      voice[i].status=VOICE_SUSTAINED;
	      set_voice_timeout(voice + i, ch, note);
	      ctl_note_event(i);
	  }
	  else
	      finish_note(i);
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
	    set_voice_timeout(voice + i, c, voice[i].note);
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

static void adjust_pressure(MidiEvent *e)
{
    int i, uv = upper_voices;
    int note, ch, vel;

    note = MIDI_EVENT_NOTE(e);
    ch = e->channel;

    vel = e->b;
    for(i = 0; i < uv; i++)
    if(voice[i].status == VOICE_ON &&
       voice[i].channel == ch &&
       voice[i].note == note)
    {
	voice[i].velocity = vel;
	recompute_amp(i);
	apply_envelope_to_amp(i);
    }
}

static void adjust_channel_pressure(MidiEvent *e)
{
    if(opt_channel_pressure)
    {
	int i, uv = upper_voices;
	int ch, pressure;

	ch = e->channel;
	pressure = e->a;
	for(i = 0; i < uv; i++)
	{
	    if(voice[i].status == VOICE_ON && voice[i].channel == ch)
	    {
		voice[i].velocity = pressure;
		recompute_amp(i);
		apply_envelope_to_amp(i);
	    }
	}
    }
}

static void adjust_panning(int c)
{
  int i, uv = upper_voices, pan = channel[c].panning;
  for(i = 0; i < uv; i++)
    if ((voice[i].channel==c) &&
	(voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
      {
	voice[i].panning = pan;
	recompute_amp(i);
	apply_envelope_to_amp(i);
      }
}

static void play_midi_setup_drums(int ch, int note)
{
    channel[ch].drums[note] = (struct DrumParts *)
	new_segment(&playmidi_pool, sizeof(struct DrumParts));
    reset_drum_controllers(channel[ch].drums, note);
}

static void adjust_drum_panning(int ch, int note)
{
    int i, uv = upper_voices, pan;

    if(!ISDRUMCHANNEL(ch) || channel[ch].drums[note] == NULL)
	return;

    pan = channel[ch].drums[note]->drum_panning;
    if(pan == 0xFF)
	return;

    for(i = 0; i < uv; i++)
	if(voice[i].channel == ch &&
	   voice[i].note == note &&
	   (voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
	{
	    voice[i].panning = pan;
	    recompute_amp(i);
	    apply_envelope_to_amp(i);
	}
}

static void drop_sustain(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status==VOICE_SUSTAINED && voice[i].channel==c)
      finish_note(i);
}

static void adjust_pitchbend(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status!=VOICE_FREE && voice[i].channel==c)
	recompute_freq(i);
}

static void adjust_volume(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].channel==c &&
	(voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
      {
	recompute_amp(i);
	apply_envelope_to_amp(i);
      }
}

static void set_reverb_level(int ch, int level)
{
    struct ReverbControls *rb;

    rb = &reverb_ctls[ch];
    rb->lastin = 0;
    rb->level = level;
    rb->rvid = ch;
    make_rvid_flag = 0;

    if(level == 0 || !opt_reverb_control)
	channel[ch].rb = NULL;
    else
	channel[ch].rb = rb;
}

static void make_rvid(void)
{
    int i, j, lv, maxrv;

    for(maxrv = MAX_CHANNELS - 1; maxrv >= 0; maxrv--)
    {
	if(reverb_ctls[maxrv].level != 0)
	    break;
	reverb_ctls[maxrv].rvid = maxrv;
    }

    /* collect same reverb level */
    reverb_ctls[0].rvid = 0;
    for(i = 1; i <= maxrv; i++)
    {
	lv = reverb_ctls[i].level;
	reverb_ctls[i].rvid = i;
	if(lv == 0)
	    continue;
	for(j = 0; j < i; j++)
	{
	    if(reverb_ctls[j].level == lv)
	    {
		reverb_ctls[i].rvid = j;
		break;
	    }
	}
    }
}

static void adjust_master_volume(void)
{
  int i, uv = upper_voices;
  adjust_amplification();
  for(i = 0; i < uv; i++)
      if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
      {
	  recompute_amp(i);
	  apply_envelope_to_amp(i);
      }
}

int midi_drumpart_change(int ch, int isdrum)
{
    if(IS_SET_CHANNELMASK(drumchannel_mask, ch))
	return 0;
    if(isdrum)
	SET_CHANNELMASK(drumchannels, ch);
    else
	UNSET_CHANNELMASK(drumchannels, ch);
    return 1;
}

static void midi_program_change(int ch, int prog)
{
    int newbank, dr;

    dr = (int)ISDRUMCHANNEL(ch);
    if(dr)
	newbank = channel[ch].program;
    else
	newbank = channel[ch].bank;

    switch(play_system_mode)
    {
      case GS_SYSTEM_MODE: /* GS */
	switch(channel[ch].bank_lsb)
	{
	  case 0:	/* No change */
	    break;
	  case 1:
	    channel[ch].mapID = (ISDRUMCHANNEL(ch) ? SC_55_TONE_MAP
				 : SC_55_DRUM_MAP);
	    break;
	  case 2:
	    channel[ch].mapID = (ISDRUMCHANNEL(ch) ? SC_88_TONE_MAP
				 : SC_88_DRUM_MAP);
	    break;
	  case 3:
	    channel[ch].mapID = (ISDRUMCHANNEL(ch) ? SC_88PRO_TONE_MAP
				 : SC_88PRO_DRUM_MAP);
	    break;
	  default:
	    break;
	}
	if(channel[ch].bank_msb >= 0)
	    newbank = channel[ch].bank_msb;
	break;

      case XG_SYSTEM_MODE: /* XG */
	switch(channel[ch].bank_msb)
	{
	  case 0: /* Normal */
	    midi_drumpart_change(ch, 0);
	    channel[ch].mapID = XG_NORMAL_MAP;
	    break;
	  case 64: /* SFX voice */
	    midi_drumpart_change(ch, 0);
	    channel[ch].mapID = XG_SFX64_MAP;
	    break;
	  case 126: /* SFX kit */
	    midi_drumpart_change(ch, 1);
	    channel[ch].mapID = XG_SFX126_MAP;
	    break;
	  case 127: /* Drumset */
	    midi_drumpart_change(ch, 1);
	    channel[ch].mapID = XG_DRUM_MAP;
	    break;
	  default:
	    break;
	}
	if(channel[ch].bank_lsb >= 0)
	    newbank = channel[ch].bank_lsb;
	break;

      default:
	if(channel[ch].bank_msb >= 0)
	    newbank = channel[ch].bank_msb;
	break;
    }

    if(dr)
    {
	channel[ch].bank = prog; /* newbank is ignored */
	if(drumset[prog] == NULL)
	    channel[ch].altassign = drumset[0]->alt;
	else
	    channel[ch].altassign = drumset[prog]->alt;
    }
    else
    {
	if(special_tonebank >= 0)
	    newbank = special_tonebank;
	channel[ch].bank = newbank;
	channel[ch].altassign = NULL;
    }
    channel[ch].program = prog;
}

static void play_midi_prescan(MidiEvent *ev)
{
    int i;

    prescanning_flag = 1;
    change_system_mode(DEFAULT_SYSTEM_MODE);
    reset_midi(0);
    resamp_cache_reset();

    while(ev->type != ME_EOT)
    {
	int ch;

	ch = ev->channel;
	switch(ev->type)
	{
	  case ME_NOTEON:
	    if(channel[ch].portamento_time == 0 ||
	       channel[ch].portamento == 0)
	    {
		int nv;
		int vlist[32];
		Voice *vp;

		nv = find_samples(ev, vlist);
		for(i = 0; i < nv; i++)
		{
		    vp = voice + vlist[i];
		    start_note(ev, vlist[i], 0, nv - i - 1);
		    resamp_cache_refer_on(vp, ev->time);
		    vp->status = VOICE_FREE;
		}
	    }
	    break;

	  case ME_NOTEOFF:
	    resamp_cache_refer_off(ch, MIDI_EVENT_NOTE(ev), ev->time);
	    break;

	  case ME_PORTAMENTO_TIME:
	    channel[ch].portamento_time = ev->a + ev->b * 128;
	    break;

	  case ME_PORTAMENTO:
	    channel[ch].portamento = ev->a;

	  case ME_RESET_CONTROLLERS:
	    reset_controllers(ch);
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

	  case ME_PITCHWHEEL:
	  case ME_ALL_NOTES_OFF:
	  case ME_ALL_SOUNDS_OFF:
	  case ME_MONO:
	  case ME_POLY:
	    resamp_cache_refer_alloff(ch, ev->time);
	    break;

	  case ME_DRUMPART:
	    if(midi_drumpart_change(ch, ev->a))
		midi_program_change(ch, channel[ch].program);
	    break;

	  case ME_KEYSHIFT:
	    resamp_cache_refer_alloff(ch, ev->time);
	    channel[ch].key_shift = (int)ev->a - 0x40;
	    break;
	}
	ev++;
    }

    for(i = 0; i < MAX_CHANNELS; i++)
	resamp_cache_refer_alloff(i, ev->time);
    resamp_cache_create();
    prescanning_flag = 0;
}

static int32 midi_cnv_vib_rate(int rate)
{
    return (int32)((double)play_mode->rate * MODULATION_WHEEL_RATE
		   / (midi_time_table[rate] *
		      2.0 * VIBRATO_SAMPLE_INCREMENTS));
}

static int midi_cnv_vib_depth(int depth)
{
    return (int)(depth * VIBRATO_DEPTH_TUNING);
}

static int32 midi_cnv_vib_delay(int delay)
{
    return (int32)(midi_time_table[delay]);
}

static int last_rpn_addr(int ch)
{
    int lsb, msb, addr, i;
    struct rpn_tag_map_t *addrmap;
    struct rpn_tag_map_t
    {
	int addr, mask, tag;
    };
    static struct rpn_tag_map_t rpn_addr_map[] =
    {
	{0x0000, 0xFFFF, RPN_ADDR_0000},
	{0x0001, 0xFFFF, RPN_ADDR_0001},
	{0x0002, 0xFFFF, RPN_ADDR_0002},
	{0x7F7F, 0xFFFF, RPN_ADDR_7F7F},
	{0xFFFF, 0xFFFF, RPN_ADDR_FFFF},
	{-1, -1}
    };
    static struct rpn_tag_map_t nrpn_addr_map[] =
    {
	{0x0108, 0xFFFF, NRPN_ADDR_0108},
	{0x0109, 0xFFFF, NRPN_ADDR_0109},
	{0x010A, 0xFFFF, NRPN_ADDR_010A},
	{0x0120, 0xFFFF, NRPN_ADDR_0120},
	{0x0121, 0xFFFF, NRPN_ADDR_0121},
	{0x0163, 0xFFFF, NRPN_ADDR_0163},
	{0x0164, 0xFFFF, NRPN_ADDR_0164},
	{0x0166, 0xFFFF, NRPN_ADDR_0166},
	{0x1400, 0xFF00, NRPN_ADDR_1400},
	{0x1500, 0xFF00, NRPN_ADDR_1500},
	{0x1600, 0xFF00, NRPN_ADDR_1600},
	{0x1700, 0xFF00, NRPN_ADDR_1700},
	{0x1800, 0xFF00, NRPN_ADDR_1800},
	{0x1900, 0xFF00, NRPN_ADDR_1900},
	{0x1A00, 0xFF00, NRPN_ADDR_1A00},
	{0x1C00, 0xFF00, NRPN_ADDR_1C00},
	{0x1D00, 0xFF00, NRPN_ADDR_1D00},
	{0x1E00, 0xFF00, NRPN_ADDR_1E00},
	{0x1F00, 0xFF00, NRPN_ADDR_1F00},
	{-1, -1, 0}
    };

    lsb = channel[ch].lastlrpn;
    msb = channel[ch].lastmrpn;
    addr = (msb << 8 | lsb);
    if(channel[ch].nrpn)
	addrmap = nrpn_addr_map;
    else
	addrmap = rpn_addr_map;
    for(i = 0; addrmap[i].addr != -1; i++)
	if(addrmap[i].addr == (addr & addrmap[i].mask))
	    return addrmap[i].tag;
    return -1;
}

static void update_channel_freq(int ch)
{
    int i, uv = upper_voices;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE && voice[i].channel == ch)
	    recompute_freq(i);
}

static void update_rpn_map(int ch, int addr, int update_now)
{
    int note, val, drumflag;

    val = channel[ch].rpnmap[addr];
    drumflag = 0;
    switch(addr)
    {
      case NRPN_ADDR_0108: /* Vibrato Rate */
	channel[ch].vibrato_ratio = midi_cnv_vib_rate(val);
	if(update_now)
	    update_channel_freq(ch);
	break;
      case NRPN_ADDR_0109: /* Vibrato Depth */
	channel[ch].vibrato_depth = midi_cnv_vib_depth(val);
	if(update_now)
	    update_channel_freq(ch);
	break;
      case NRPN_ADDR_010A: /* Vibrato Delay */
	channel[ch].vibrato_delay = midi_cnv_vib_delay(val);
	if(update_now)
	    update_channel_freq(ch);
	break;
      case NRPN_ADDR_0120:	/* Filter cutoff frequency */
	break;
      case NRPN_ADDR_0121:	/* Filter Resonance */
	break;
      case NRPN_ADDR_0163:	/* Attack Time */
	break;
      case NRPN_ADDR_0164:	/* EG Decay Time */
	break;
      case NRPN_ADDR_0166:	/* EG Release Time */
	break;
      case NRPN_ADDR_1400:	/* Drum Filter Cutoff (XG) */
	if(play_system_mode != GS_SYSTEM_MODE)
	    drumflag = 1;
	break;
      case NRPN_ADDR_1500:	/* Drum Filter Resonance (XG) */
	if(play_system_mode != GS_SYSTEM_MODE)
	    drumflag = 1;
	break;
      case NRPN_ADDR_1600:	/* Drum EG Attack Time (XG) */
	if(play_system_mode != GS_SYSTEM_MODE)
	    drumflag = 1;
	break;
      case NRPN_ADDR_1700:	/* Drum EG Decay Time (XG) */
	if(play_system_mode != GS_SYSTEM_MODE)
	    drumflag = 1;
	break;
      case NRPN_ADDR_1800:	/* Coarse Pitch of Drum (GS)
				   Fine Pitch of Drum (XG) */
	drumflag = 1;
	break;
      case NRPN_ADDR_1900:	/* Coarse Pitch of Drum (XG) */
	if(play_system_mode != GS_SYSTEM_MODE)
	    drumflag = 1;
	break;
      case NRPN_ADDR_1A00:	/* Level of Drum */
	drumflag = 1;
	break;
      case NRPN_ADDR_1C00:	/* Panpot of Drum */
	drumflag = 1;
	note = channel[ch].lastlrpn;
	if(channel[ch].drums[note] == NULL)
	    play_midi_setup_drums(ch, note);
	if(val == 0)
	    val = int_rand(128);
	channel[ch].drums[note]->drum_panning = val;
	if(update_now)
	    adjust_drum_panning(ch, note);
	break;
      case NRPN_ADDR_1D00:	/* Reverb Send Level of Drum */
	drumflag = 1;
	break;
      case NRPN_ADDR_1E00:	/* Chorus Send Level of Drum */
	drumflag = 1;
	break;
      case NRPN_ADDR_1F00:	/* Variation Send Level of Drum */
	drumflag = 1;
	break;
      case RPN_ADDR_0000: /* Pitch bend sensitivity */
	channel[ch].pitchfactor = 0;
	break;
      case RPN_ADDR_0001:
	channel[ch].pitchfactor = 0;
	break;
      case RPN_ADDR_0002:
	channel[ch].pitchfactor = 0;
	break;
      case RPN_ADDR_7F7F: /* RPN reset */
	channel[ch].rpn_7f7f_flag = 1;
	break;
      case RPN_ADDR_FFFF: /* RPN initialize */
	/* All reset to defaults */
	channel[ch].rpn_7f7f_flag = 0;
	memset(channel[ch].rpnmap, 0, sizeof(channel[ch].rpnmap));
	channel[ch].lastlrpn = channel[ch].lastmrpn = 0;
	channel[ch].nrpn = 0;
	channel[ch].rpnmap[RPN_ADDR_0000] = 2;
	channel[ch].rpnmap[RPN_ADDR_0001] = 0x40;
	channel[ch].rpnmap[RPN_ADDR_0002] = 0x40;
	channel[ch].pitchfactor = 0;
	break;
    }
    if(drumflag && midi_drumpart_change(ch, 1) && update_now)
    {
	midi_program_change(ch, channel[ch].program);
	ctl_prog_event(ch);
    }
}

static void seek_forward(int32 until_time)
{
    int32 i;

    playmidi_seek_flag = 1;
    wrd_midi_event(WRD_START_SKIP, WRD_NOARG);
    while(MIDI_EVENT_TIME(current_event) < until_time)
    {
	int ch;

	ch = current_event->channel;
	switch(current_event->type)
	{
	  case ME_PITCHWHEEL:
	    channel[ch].pitchbend = current_event->a + current_event->b * 128;
	    channel[ch].pitchfactor=0;
	    break;

	  case ME_MAINVOLUME:
	    channel[ch].volume = current_event->a;
	    break;

	  case ME_MASTER_VOLUME:
	    master_volume_ratio =
		(int32)current_event->a + 256 * (int32)current_event->b;
	    break;

	  case ME_PAN:
	    channel[ch].panning=current_event->a;
	    break;

	  case ME_EXPRESSION:
	    channel[ch].expression=current_event->a;
	    break;

	  case ME_PROGRAM:
	    midi_program_change(ch, current_event->a);
	    break;

	  case ME_SUSTAIN:
	    channel[ch].sustain=current_event->a;
	    break;

	  case ME_RESET_CONTROLLERS:
	    reset_controllers(ch);
	    break;

	  case ME_TONE_BANK_MSB:
	    channel[ch].bank_msb = current_event->a;
	    break;

	  case ME_TONE_BANK_LSB:
	    channel[ch].bank_lsb = current_event->a;
	    break;

	  case ME_MODULATION_WHEEL:
	    channel[ch].modulation_wheel =
		midi_cnv_vib_depth(current_event->a);
	    break;

	  case ME_PORTAMENTO_TIME:
	    channel[ch].portamento_time =
		current_event->a + current_event->b * 128;
	    break;

	  case ME_PORTAMENTO:
	    channel[ch].portamento = current_event->a;
	    break;

	  case ME_MONO:
	    channel[ch].mono = 1;
	    break;

	  case ME_POLY:
	    channel[ch].mono = 0;
	    break;

	    /* RPNs */
	  case ME_NRPN_LSB:
	    channel[ch].lastlrpn = current_event->a;
	    channel[ch].nrpn = 1;
	    break;
	  case ME_NRPN_MSB:
	    channel[ch].lastmrpn = current_event->a;
	    channel[ch].nrpn = 1;
	    break;
	  case ME_RPN_LSB:
	    channel[ch].lastlrpn = current_event->a;
	    channel[ch].nrpn = 0;
	    break;
	  case ME_RPN_MSB:
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
		channel[ch].rpnmap[i] =
		    current_event->a;
		update_rpn_map(ch, i, 0);
	    }
	    break;

	  case ME_REVERB_EFFECT:
	    set_reverb_level(ch, current_event->a);
	    break;

	  case ME_CHORUS_EFFECT:
	    if(opt_chorus_control)
	    {
		if(opt_chorus_control == 1)
		    channel[ch].chorus_level = current_event->a;
		else
		    channel[ch].chorus_level = opt_chorus_control;
	    }
	    break;

	  case ME_RANDOM_PAN:
	    channel[ch].panning = int_rand(128);
	    break;

	  case ME_SET_PATCH:
	    i = channel[ch].special_sample = current_event->a;
	    if(special_patch[i] != NULL)
		special_patch[i]->sample_offset = 0;
	    break;

	  case ME_TEMPO:
	    current_play_tempo = ch +
		current_event->b * 256 + current_event->a * 65536;
	    play_tempo_offset = current_event->time;
	    break;

	  case ME_RESET:
	    change_system_mode(current_event->a);
	    reset_midi(0);
	    break;

	  case ME_PATCH_OFFS:
	    if(special_patch[ch] != NULL)
		special_patch[ch]->sample_offset =
		    (current_event->a | 256 * current_event->b);
	    break;

	  case ME_WRD:
	    wrd_midi_event(ch, current_event->a | 256 * current_event->b);
	    break;

	  case ME_DRUMPART:
	    if(midi_drumpart_change(ch, current_event->a))
		midi_program_change(ch, channel[ch].program);
	    break;

	  case ME_KEYSHIFT:
	    channel[ch].key_shift = (int)current_event->a - 0x40;
	    break;

	  case ME_EOT:
	    current_sample = current_event->time;
	    playmidi_seek_flag = 0;
	    return;
	}
	current_event++;
    }
    wrd_midi_event(WRD_END_SKIP, WRD_NOARG);

    playmidi_seek_flag = 0;
    if(current_event != event_list)
	current_event--;
    current_sample = until_time;
}

static void skip_to(int32 until_time)
{
  int ch;

  trace_flush();
  current_event = NULL;

  if (current_sample > until_time)
    current_sample=0;

  init_reverb(play_mode->rate);
  change_system_mode(DEFAULT_SYSTEM_MODE);
  reset_midi(0);

  buffered_count=0;
  buffer_pointer=common_buffer;
  current_event=event_list;
  current_play_tempo = 500000; /* 120 BPM */
  play_tempo_offset = 0;
  if (until_time)
    seek_forward(until_time);
  for(ch = 0; ch < MAX_CHANNELS; ch++)
      if(channel[ch].rb != NULL)
	  channel[ch].rb->lastin = 0;

  ctl_mode_event(CTLE_RESET, 0, 0, 0);
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
    play_mode->purge_output();
    data_output_count = 0;
    skip_to(cur);
    return cur;
}

int check_apply_control(void)
{
    int rc;
    int32 val;

    if(file_from_stdin)
	return RC_NONE;
    rc = ctl->read(&val);
    switch(rc)
    {
      case RC_CHANGE_VOLUME:
	if (val>0 || amplification > -val)
	    amplification += val;
	else
	    amplification=0;
	if (amplification > MAX_AMPLIFICATION)
	    amplification=MAX_AMPLIFICATION;
	adjust_amplification();
	ctl_mode_event(CTLE_MASTER_VOLUME, 0, amplification, 0);
	break;
      case RC_SYNC_RESTART:
	play_mode->purge_output();
	data_output_count = 0;
	trace_flush();
	break;
      case RC_TOGGLE_PAUSE:
	play_pause_flag = !play_pause_flag;
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
	return RC_NONE;
#endif /* SUPPORT_SOUNDSPEC */
    }
    return rc;
}

static void voice_increment(int n)
{
    int i;
    for(i = 0; i < n; i++)
    {
	if(voices == MAX_VOICES)
	    break;
	voice[voices++].status = VOICE_FREE;
    }
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
	    voice[j] = voice[voices];
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
	    voice[lowest].status = VOICE_FREE;
	    ctl_note_event(lowest);
	    voice[lowest] = voice[voices];
	}
	else
	    lost_notes++;
    }
    if(upper_voices > voices)
	upper_voices = voices;
}

static int apply_controls(void)
{
    int rc, i, jump_flag = 0;
    int32 val, cur;
    FLOAT_T r;

    /* ASCII renditions of CD player pictograms indicate approximate effect */
    do
    {
	switch(rc=ctl->read(&val))
	{
	  case RC_QUIT:		/* [] */
	  case RC_LOAD_FILE:
	  case RC_NEXT:		/* >>| */
	  case RC_REALLY_PREVIOUS: /* |<< */
	    play_mode->purge_output();
	    data_output_count = 0;
	    trace_flush();
	    return rc;

	  case RC_CHANGE_VOLUME:
	    if (val>0 || amplification > -val)
		amplification += val;
	    else
		amplification=0;
	    if (amplification > MAX_AMPLIFICATION)
		amplification=MAX_AMPLIFICATION;
	    adjust_amplification();
	    for (i=0; i<upper_voices; i++)
		if (voice[i].status != VOICE_FREE)
		{
		    recompute_amp(i);
		    apply_envelope_to_amp(i);
		}
	    ctl_mode_event(CTLE_MASTER_VOLUME, 0, amplification, 0);
	    continue;

	  case RC_CHANGE_REV_EFFB:
	  case RC_CHANGE_REV_TIME:
	    reverb_rc_event(rc, val);
	    sync_restart(0);
	    continue;

	  case RC_PREVIOUS:	/* |<< */
	    play_mode->purge_output();
	    data_output_count = 0;
	    if (current_sample < 2*play_mode->rate)
		return RC_REALLY_PREVIOUS;
	    return RC_RESTART;

	  case RC_RESTART:	/* |<< */
	    play_mode->purge_output();
	    data_output_count = 0;
	    skip_to(0);
	    jump_flag = 1;
	    continue;

	  case RC_JUMP:
	    play_mode->purge_output();
	    data_output_count = 0;
	    if (val >= sample_count)
		return RC_NEXT;
	    skip_to(val);
	    return rc;

	  case RC_FORWARD:	/* >> */
	    cur = current_trace_samples();
	    play_mode->purge_output();
	    data_output_count = 0;
	    if(cur == -1)
		cur = current_sample;
	    if(val + cur >= sample_count)
		return RC_NEXT;
	    skip_to(val + cur);
	    return RC_JUMP;

	  case RC_BACK:		/* << */
	    cur = current_trace_samples();
	    play_mode->purge_output();
	    data_output_count = 0;
	    if(cur == -1)
		cur = current_sample;
	    if(cur > val)
		skip_to(cur - val);
	    else
		skip_to(0);
	    return RC_JUMP;

	  case RC_TOGGLE_PAUSE:
	    if(play_pause_flag)
	    {
		play_pause_flag = 0;
		skip_to(midi_restart_time);
	    }
	    else
	    {
		midi_restart_time = current_trace_samples();
		if(midi_restart_time == -1)
		    midi_restart_time = current_sample;
		play_mode->purge_output();
		data_output_count = 0;
		play_pause_flag = 1;
	    }
	    jump_flag = 1;
	    continue;

	  case RC_KEYUP:
	  case RC_KEYDOWN:
	    note_key_offset += val;
	    if(sync_restart(1) != -1)
		jump_flag = 1;
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
	    continue;

	  case RC_VOICEINCR:
	    voice_increment(val);
	    if(sync_restart(1) != -1)
		jump_flag = 1;
	    continue;

	  case RC_VOICEDECR:
	    if(sync_restart(1) != -1)
	    {
		voices -= val;
		if(voices < 0)
		    voices = 0;
		jump_flag = 1;
		continue;
	    }
	    voice_decrement(val);
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
	    play_mode->purge_output();
	    data_output_count = 0;
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
	    play_mode->purge_output();
	    data_output_count = 0;
	    return RC_RELOAD;
	}
	if(play_pause_flag)
#ifdef HAVE_USLEEP
	    usleep(300000);
#else
	sleep(1);
#endif
    } while (rc != RC_NONE || play_pause_flag);
    return jump_flag ? RC_JUMP : RC_NONE;
}

static void do_compute_data(int32 count)
{
    int i, uv, stereo, n;
    int32 *vpblist[MAX_CHANNELS];
    int vc[MAX_CHANNELS];

    play_mode->play_loop();
    trace_loop();
    stereo = !(play_mode->encoding & PE_MONO);
    n = (stereo ? (count * 8) : (count * 4)); /* in bytes */
    memset(buffer_pointer, 0, n);

    if(!make_rvid_flag)
    {
	make_rvid();
	make_rvid_flag = 1;
    }

    if(stereo)
    {
	int chbufidx;

	chbufidx = 0;
	for(i = 0; i < MAX_CHANNELS; i++)
	{
	    vc[i] = 0;
	    if(channel[i].rb != NULL && channel[i].rb->lastin != 0)
	    {
		if(channel_buffer == NULL)
		    channel_buffer =
			(char *)safe_malloc(MAX_CHANNELS*AUDIO_BUFFER_SIZE*8);
		if(channel[i].rb->rvid < i)
		    vpblist[i] = vpblist[channel[i].rb->rvid];
		else
		{
		    vpblist[i] = (int32 *)(channel_buffer + chbufidx);
		    chbufidx += n;
		}
	    }
	    else
		vpblist[i] = buffer_pointer;
	}
	if(chbufidx)
	    memset(channel_buffer, 0, chbufidx);
    }

    uv = upper_voices;
    for(i = 0; i < uv; i++)
    {
	if(voice[i].status != VOICE_FREE)
	{
	    int32 *vpb;

	    if(stereo)
	    {
		int ch = voice[i].channel;
		vpb = vpblist[ch];
		vc[ch] = 1;
	    }
	    else
		vpb = buffer_pointer;
	    mix_voice(vpb, i, count);
	    if(voice[i].timeout > 0 &&
	       voice[i].timeout < current_sample &&
	       voice[i].status == VOICE_SUSTAINED)
		finish_note(i); /* timeout (See also "#extension timeout" line
				   in *.cfg file */
	}
    }

    while(uv > 0 && voice[uv - 1].status == VOICE_FREE)
	uv--;
    upper_voices = uv;

    if(stereo)
    {
	if(opt_reverb_control || do_reverb_flag)
	{
	    int k;
	    int32 *p;

	    k = 2 * count;
	    for(i = 0; i < MAX_CHANNELS; i++)
	    {
		p = vpblist[i];
		if(p != buffer_pointer)
		{
		    if(vc[i])
			channel[i].rb->lastin = current_sample + 1;
		    else if(current_sample - channel[i].rb->lastin >
			    REVERB_MAX_DELAY_OUT)
		    {
			channel[i].rb->lastin = 0;
			continue;
		    }
		    if(channel[i].rb->rvid == i)
			set_ch_reverb(p, k, channel[i].rb->level);
		}
	    }

	    if(opt_reverb_control)
	    {
		set_ch_reverb(buffer_pointer, k, DEFAULT_REVERB_SEND_LEVEL);
		do_ch_reverb(buffer_pointer, k);
	    }
	    else if(do_reverb_flag)
		do_reverb(buffer_pointer, k);
	}
    }
    else if(do_reverb_flag)
	do_mono_reverb(buffer_pointer, count);

    current_sample += count;
}

#ifdef PRESENCE_HACK
extern int presence_balance;
static void presence_resample(int32 count)
{
    static int32 prev[AUDIO_BUFFER_SIZE * 2];
    int32 save[AUDIO_BUFFER_SIZE * 2];
    int32 pi, i, backoff;
    int32 *buff;
    int b;

    if(count == 0)
    {
	memset(prev, 0, sizeof(prev));
	return;
    }

    if(play_mode->encoding & PE_MONO)
	return;
    if(presence_balance == 0 || presence_balance == 1 ||
       presence_balance == 2)
	b = presence_balance;
    else
	return;

    count *= 2;
    backoff = 2 * (int)(play_mode->rate * presence_delay_msec / 1000.0);
    if(backoff == 0)
	return;
    if(backoff > count)
	backoff = count;
    buff = common_buffer;

    if(count < AUDIO_BUFFER_SIZE * 2)
    {
	memset(buff + count, 0, 4 * (AUDIO_BUFFER_SIZE * 2 - count));
	count = AUDIO_BUFFER_SIZE * 2;
    }

    memcpy(save, buff, 4 * count);
    pi = count - backoff;

    if(b == 2)
    {
	static int turn_counter = 0, tc;
	static int status;
	static double rate0, rate1, dr;
	int32 *p;

#define SIDE_CONTI_SEC 10
#define CHANGE_SEC     2.0

	if(turn_counter == 0)
	{
	    turn_counter = SIDE_CONTI_SEC * play_mode->rate;

	    /*  status: 0 -> 2 -> 3 -> 1 -> 4 -> 5 -> 0 -> ...
	     *  status left   right
	     *  0      -      +		(right)
	     *  1      +      -		(left)
	     *  2     -> +    +		(right -> center)
	     *  3      +     -> -	(center -> left)
	     *  4     -> -    -		(left -> center)
	     *  5      -     -> +	(center -> right)
	     */
	    status = 0;
	    tc = 0;
	}
	p = prev;
	for(i = 0; i < count; i += 2, pi += 2)
	{
	    if(i < backoff)
		p = prev;
	    else if(p == prev)
	    {
		pi = 0;
		p = save;
	    }

	    if(status < 2)
		buff[i+status] = p[pi+status];
	    else if(status < 4)
	    {
		int32 v, j;
		j = (status & 1);

		v = (int32)(rate0 * buff[i+j] + rate1 * p[pi+j]);
		buff[i+j] = v;
		rate0 += dr; rate1 -= dr;
	    }
	    else
	    {
		int32 v, j, k;
		j = (status & 1);
		k = !j;

		v = (int32)(rate0 * buff[i+j] + rate1 * p[pi+j]);
		buff[i+j] = v;
		buff[i+k] = p[pi+k];
		rate0 += dr; rate1 -= dr;
	    }

	    tc++;
	    if(tc == turn_counter)
	    {
		tc = 0;

		switch(status)
		{
		  case 0:
		    status = 2;
		    turn_counter = (CHANGE_SEC/2.0) * play_mode->rate;
		    rate0 = 0.0;
		    rate1 = 1.0;
		    dr = 1.0 / turn_counter;
		    break;

		  case 2:
		    status = 3;
		    rate0 = 1.0;
		    rate1 = 0.0;
		    dr = -1.0 / turn_counter;
		    break;

		  case 3:
		    status = 1;
		    turn_counter = SIDE_CONTI_SEC * play_mode->rate;
		    break;

		  case 1:
		    status = 4;
		    turn_counter = (CHANGE_SEC/2.0) * play_mode->rate;
		    rate0 = 1.0;
		    rate1 = 0.0;
		    dr = -1.0 / turn_counter;
		    break;

		  case 4:
		    status = 5;
		    turn_counter = (CHANGE_SEC/2.0) * play_mode->rate;
		    rate0 = 0.0;
		    rate1 = 1.0;
		    dr = 1.0 / turn_counter;
		    break;

		  case 5:
		    status = 0;
		    turn_counter = SIDE_CONTI_SEC * play_mode->rate;
		    break;
		}
	    }
	}
    }
    else
    {
	for(i = 0; i < backoff; i += 2, pi += 2)
	    buff[b+i] = prev[b+pi];
	for(pi = 0; i < count; i += 2, pi += 2)
	    buff[b+i] = save[b+pi];
    }

    memcpy(prev + count - backoff, save + count - backoff, 4 * backoff);
}
#endif /* PRESENCE_HACK */

static int check_midi_play_end(MidiEvent *e, int len)
{
    int i, type;

    for(i = 0; i < len; i++)
    {
	type = e[i].type;
	if(type == ME_NOTEON || type == ME_LAST || type == ME_WRD)
	    return 0;
	if(type == ME_EOT)
	    return i + 1;
    }
    return 0;
}

static int compute_data(int32 count);
static int midi_play_end(void)
{
    int i, rc = RC_TUNE_END;

    check_eot_flag = 0;
    if(upper_voices > 0)
    {
	rc = compute_data(play_mode->rate);
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;

	for(i = 0; i < upper_voices; i++)
	    if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
		finish_note(i);
	for(i = 0; i < MIDITRACE_OUTPUT_FRAGMENTS && upper_voices > 0; i++)
	{
	    trace_loop();
	    rc = compute_data(play_mode->rate * 3
			      / MIDITRACE_OUTPUT_FRAGMENTS);
	    play_mode->play_loop();
	    if(RC_IS_SKIP_FILE(rc))
		goto midi_end;
	}

	/* kill voices */
	kill_all_voices();
	rc = compute_data(MAX_DIE_TIME);
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
	upper_voices = 0;
    }

    /* clear reverb echo sound */
    init_reverb(play_mode->rate);
    for(i = 0; i < MAX_CHANNELS; i++)
	channel[i].rb = NULL;

    /* output null sound */
    rc = compute_data((int32)(play_mode->rate * PLAY_INTERLEAVE_SEC));
    if(RC_IS_SKIP_FILE(rc))
	goto midi_end;

    compute_data(0); /* flush buffer to device */

    if(ctl->trace_playing)
    {
	rc = play_mode->flush_output(); /* Wait until play out */
	data_output_count = 0;
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
    }
    else
    {
	trace_flush();
	while(play_mode->play_loop())
	{
	    int rc;
	    usleep(100000);
	    rc = check_apply_control();
	    if(RC_IS_SKIP_FILE(rc))
		goto midi_end;
	}
    }

  midi_end:
    if(RC_IS_SKIP_FILE(rc))
    {
	play_mode->purge_output();
	data_output_count = 0;
    }

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

/* count=0 means flush remaining buffered data to output device, then
   flush the device itself */
static int compute_data(int32 count)
{
  int rc;

  if (!count)
    {
      if (buffered_count)
      {
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		    "output data (%d)", buffered_count);

#ifdef SUPPORT_SOUNDSPEC
	  soundspec_update_wave(common_buffer, buffered_count);
#endif /* SUPPORT_SOUNDSPEC */

#ifdef PRESENCE_HACK
	  presence_resample(buffered_count);
#endif /* PRESENCE_HACK */

	  play_mode->output_data(common_buffer, buffered_count);
      }
      buffer_pointer=common_buffer;
      buffered_count=0;
      return RC_NONE;
    }

  while ((count+buffered_count) >= AUDIO_BUFFER_SIZE)
    {
      int i;

      do_compute_data(AUDIO_BUFFER_SIZE-buffered_count);
      count -= AUDIO_BUFFER_SIZE-buffered_count;
      ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		"output data (%d)", AUDIO_BUFFER_SIZE);

#ifdef SUPPORT_SOUNDSPEC
      soundspec_update_wave(common_buffer, AUDIO_BUFFER_SIZE);
#endif /* SUPPORT_SOUNDSPEC */

#ifdef PRESENCE_HACK
      presence_resample(AUDIO_BUFFER_SIZE);
#endif /* PRESENCE_HACK */

    if(trace_loop() && data_output_count > fragment_start_count)
    {
	  int i;
	  int32 *bufp;
	  static int j = 0, nfragm, d, ch;

	  if(j == 0)
	  {
	      int r;

	      r = play_mode->rate / MIDITRACE_OUTPUT_FRAGMENTS;
	      for(j = 256; j < r && j < AUDIO_BUFFER_SIZE; j <<= 1)
		  ;
	      nfragm = AUDIO_BUFFER_SIZE / j;
	      if(play_mode->encoding & PE_MONO)
		  ch = 1;
	      else
		  ch = 2;
	      d = ch * AUDIO_BUFFER_SIZE / nfragm;
	  }

	  bufp = common_buffer;
	  for(i = 0; i < nfragm; i++, bufp += d)
	  {
	      play_mode->output_data(bufp, d / ch);
	      if(trace_loop() == 0)
	      {
		  bufp += d;
		  i++;
		  break;
	      }
	  }
	  for(; i < nfragm; i++, bufp += d)
	      play_mode->output_data(bufp, d / ch);
      }
      else
	  play_mode->output_data(common_buffer, AUDIO_BUFFER_SIZE);
      data_output_count += AUDIO_BUFFER_SIZE;

      buffer_pointer=common_buffer;
      buffered_count=0;
      if(current_event->type != ME_EOT)
	  ctl_timestamp();
      if((rc = apply_controls()) != RC_NONE)
	  return rc;

#ifdef __WIN32__
      /* check break signals */
      VOLATILE_TOUCH(intr);
      if(intr)
	  return RC_QUIT;
#endif

      if(upper_voices == 0 && check_eot_flag &&
	 (i = check_midi_play_end(current_event, EOT_PRESEARCH_LEN)) > 0)
      {
	  if(i > 1)
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
			"%d MIDI events are ignored", i - 1);
	  return midi_play_end();
      }
    }
  if (count>0)
    {
      do_compute_data(count);
      buffered_count += count;
      buffer_pointer += (play_mode->encoding & PE_MONO) ? count : count*2;
      play_mode->play_loop();
      trace_loop();
    }
  return RC_NONE;
}

static void update_modulation_wheel(int ch, int val)
{
    int i, uv = upper_voices;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE && voice[i].channel == ch)
	{
	    /* Set/Reset mod-wheel */
	    voice[i].modulation_wheel = val;
	    voice[i].vibrato_delay = 0;
	    recompute_freq(i);
	}
}

static void drop_portamento(int ch)
{
    int i, uv = upper_voices;

    channel[ch].portamento = 0;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = 0;
	    recompute_freq(i);
	}
    channel[ch].last_note_fine = -1;
}

static void update_portamento_controls(int ch)
{
    if(channel[ch].portamento_time < 128)
    {
	channel[ch].portamento_time = 0;
	drop_portamento(ch);
    }
    else
    {
	double mt, dc;
	int d;

	mt = midi_time_table[(channel[ch].portamento_time >> 7) & 0x7F]
	    * PORTAMENTO_TIME_TUNING;
	dc = play_mode->rate * mt;
	d = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	d++;
	channel[ch].porta_control_ratio = (int)(d * dc + 0.5);
	channel[ch].porta_dpb = d;
    }
}

static void update_portamento_time(int ch)
{
    int i, uv = upper_voices;
    int dpb;
    int32 ratio;

    update_portamento_controls(ch);
    dpb = channel[ch].porta_dpb;
    ratio = channel[ch].porta_control_ratio;

    for(i = 0; i < uv; i++)
    {
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = ratio;
	    voice[i].porta_dpb = dpb;
	    recompute_freq(i);
	}
    }
}

int default_play_event(void *p)
{
    MidiEvent *ev = (MidiEvent *)p;
    int ch;
    int32 i, cet;

    current_event = ev;
    cet = MIDI_EVENT_TIME(ev);
    if(ctl->verbosity >= VERB_DEBUG_SILLY)
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Midi Event %d: %s %d %d %d", cet,
		  event_name(ev->type), ev->channel, ev->a, ev->b);
    if(cet > current_sample)
    {
	int rc;

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

    ch = ev->channel;
    switch(ev->type)
    {
	/* MIDI Events */
      case ME_NOTEOFF:
	note_off(ev);
	break;

      case ME_NOTEON:
	note_on(ev);
	break;

      case ME_KEYPRESSURE:
	adjust_pressure(ev);
	break;

      case ME_PROGRAM:
	midi_program_change(ch, ev->a);
	ctl_prog_event(ch);
	break;

      case ME_CHANNEL_PRESSURE:
	adjust_channel_pressure(ev);
	break;

      case ME_PITCHWHEEL:
	channel[ch].pitchbend = ev->a + ev->b * 128;
	channel[ch].pitchfactor = 0;
	/* Adjust pitch for notes already playing */
	adjust_pitchbend(ch);
	ctl_mode_event(CTLE_PITCH_BEND, 1, ch, channel[ch].pitchbend);
	break;

	/* Controls */
      case ME_TONE_BANK_MSB:
	channel[ch].bank_msb = ev->a;
	break;

      case ME_TONE_BANK_LSB:
	channel[ch].bank_lsb = ev->a;
	break;

      case ME_MODULATION_WHEEL:
	channel[ch].modulation_wheel =
	    midi_cnv_vib_depth(ev->a);
	update_modulation_wheel(ch, channel[ch].modulation_wheel);
	ctl_mode_event(CTLE_MOD_WHEEL, 1, ch, channel[ch].modulation_wheel);
	break;

      case ME_MAINVOLUME:
	channel[ch].volume = ev->a;
	adjust_volume(ch);
	ctl_mode_event(CTLE_VOLUME, 1, ch, ev->a);
	break;

      case ME_PAN:
	channel[ch].panning = ev->a;
	if(adjust_panning_immediately)
	    adjust_panning(ch);
	ctl_mode_event(CTLE_PANNING, 1, ch, ev->a);
	break;

      case ME_EXPRESSION:
	channel[ch].expression = ev->a;
	adjust_volume(ch);
	ctl_mode_event(CTLE_EXPRESSION, 1, ch, ev->a);
	break;

      case ME_SUSTAIN:
	channel[ch].sustain = ev->a;
	if(!ev->a)
	    drop_sustain(ch);
	ctl_mode_event(CTLE_SUSTAIN, 1, ch, ev->a);
	break;

      case ME_PORTAMENTO_TIME:
	channel[ch].portamento_time = ev->a + ev->b * 128;
	update_portamento_time(ch);
	break;

      case ME_PORTAMENTO:
	channel[ch].portamento = ev->a;
	if(!ev->a)
	    drop_portamento(ch);
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

      case ME_REVERB_EFFECT:
	set_reverb_level(ch, ev->a);
	ctl_mode_event(CTLE_REVERB_EFFECT, 1, ch,
		       channel[ch].rb ? channel[ch].rb->level : 0);
	break;

      case ME_CHORUS_EFFECT:
	if(opt_chorus_control)
	{
	    if(opt_chorus_control == 1)
		channel[ch].chorus_level = ev->a;
	    else
		channel[ch].chorus_level = opt_chorus_control;
	    ctl_mode_event(CTLE_CHORUS_EFFECT, 1, ch, channel[ch].chorus_level);
	}
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
	channel[ch].lastlrpn = ev->a;
	channel[ch].nrpn = 1;
	break;

      case ME_NRPN_MSB:
	channel[ch].lastmrpn = ev->a;
	channel[ch].nrpn = 1;
	break;

      case ME_RPN_LSB:
	channel[ch].lastlrpn = ev->a;
	channel[ch].nrpn = 0;
	break;

      case ME_RPN_MSB:
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
	if(adjust_panning_immediately)
	    adjust_panning(ch);
	ctl_mode_event(CTLE_PANNING, 1, ch, channel[ch].panning);
	break;

      case ME_SET_PATCH:
	i = channel[ch].special_sample = current_event->a;
	if(special_patch[i] != NULL)
	    special_patch[i]->sample_offset = 0;
	ctl_prog_event(ch);
	break;

      case ME_TEMPO:
	current_play_tempo = ch + ev->b * 256 + ev->a * 65536;
	play_tempo_offset = ev->time;
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

      case ME_MASTER_VOLUME:
	master_volume_ratio = (int32)ev->a + 256 * (int32)ev->b;
	adjust_master_volume();
	break;

      case ME_RESET:
	change_system_mode(ev->a);
	reset_midi(1);
	break;

      case ME_PATCH_OFFS:
	if(special_patch[ch] != NULL)
	    special_patch[ch]->sample_offset =
		(current_event->a | 256 * current_event->b);
	break;

      case ME_WRD:
	push_midi_trace2(wrd_midi_event,
			 ch, current_event->a | (current_event->b << 8));
	break;

      case ME_DRUMPART:
	if(midi_drumpart_change(ch, current_event->a))
	{
	    /* Update bank information */
	    midi_program_change(ch, channel[ch].program);
	}
	ctl_prog_event(ch);
	break;

      case ME_KEYSHIFT:
	i = (int)current_event->a - 0x40;
	if(i != channel[ch].key_shift)
	{
	    all_sounds_off(ch);
	    channel[ch].key_shift = (int8)i;
	}
	break;

      case ME_NOTE_STEP:
	i = ev->a | ((int)ev->b << 8);
	ctl_mode_event(CTLE_METRONOME, 1, i, 0);
	if(readmidi_wrd_flag)
	    wrdt->update_events();
	break;

      case ME_EOT:
	return midi_play_end();
    }

    return RC_NONE;
}

/* parameter "events" was declared but never referenced */
static int play_midi(MidiEvent *eventlist, int32 samples)
{
    int rc;
    static int play_count = 0;

    rc = play_mode->flush_output();
    data_output_count = 0;
    if(RC_IS_SKIP_FILE(rc))
	return rc;

    sample_count = samples;
    event_list = eventlist;
    lost_notes = cut_notes = 0;
    skip_to(midi_restart_time);

    check_eot_flag = 1;
    for(;;)
    {
	rc = play_mode->play_event(current_event);
	if(current_event->type == ME_EOT)
	    break;
	if(rc != RC_NONE)
	    break;
	current_event++;
    }

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

int play_midi_file(char *fn)
{
    MidiEvent *event;
    int32 events, samples;
    int rc = RC_NONE, i;
    static int last_rc = RC_NONE;
    struct timidity_file *tf;
    static int init_flag = 0;

    if(!strcmp(fn, "-"))
	file_from_stdin = 1;
    else
	file_from_stdin = 0;

    rc = check_apply_control();
    if(RC_IS_SKIP_FILE(rc))
	return rc;

    if(!init_flag)
    {
	init_flag = 1;
	for(i = 0; i < MAX_CHANNELS; i++)
	    memset(channel[i].drums, 0, sizeof(channel[i].drums));

	/* Memory initialize at first call.
	 * It needs for optimization of memory alignment.
	 */
	if(allocate_cache_size > 0)
	    resamp_cache_reset();
    }

    init_mblock(&playmidi_pool);
    reset_midi(0);

  play_restart:
    ctl_mode_event(CTLE_NOW_LOADING, 0, (long)fn, 0);
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MIDI file: %s", fn);
    if((tf = open_midi_file(fn, 1, OF_VERBOSE)) == NULL)
    {
	if(last_rc == RC_REALLY_PREVIOUS)
	    return RC_REALLY_PREVIOUS;
	return RC_ERROR;
    }

    event = NULL;
    rc = check_apply_control();
    if(RC_IS_SKIP_FILE(rc))
	goto play_end; /* skip playing */

    event = read_midi_file(tf, &events, &samples, fn);
    close_file(tf);

    if(!event)
    {
	if(last_rc == RC_REALLY_PREVIOUS)
	    return RC_REALLY_PREVIOUS;
	return RC_ERROR;
    }

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
	      "%d supported events, %d samples, time %d:%02d", events, samples,
	      samples / play_mode->rate / 60,
	      (samples / play_mode->rate) % 60);

    ctl_mode_event(CTLE_PLAY_START, 0, samples, 0);
    ctl_mode_event(CTLE_MASTER_VOLUME, 0, amplification, 0);

     if(!opt_realtime_playing && !IS_CURRENT_MOD_FILE
        && (play_mode->flag&PF_NEED_INSTRUMENTS))
	load_missing_instruments(&rc);
    ctl_mode_event(CTLE_LOADING_DONE, 0, 0, 0);

    if(RC_IS_SKIP_FILE(rc))
    {
	/* Interupted instrument loading */
	clear_magic_load_instruments();
	goto play_end; /* skip playing */
    }

    note_key_offset = 0;
    midi_time_ratio = 1.0;

    if(!opt_realtime_playing && allocate_cache_size > 0
       && !IS_CURRENT_MOD_FILE && (play_mode->flag&PF_NEED_INSTRUMENTS))
	play_midi_prescan(event);

    wrd_midi_event(-1, -1); /* For initialize */
    rc = play_midi(event, samples);

    reuse_mblock(&playmidi_pool);
    for(i = 0; i < MAX_CHANNELS; i++)
	memset(channel[i].drums, 0, sizeof(channel[i].drums));

  play_end:
    ctl_mode_event(CTLE_PLAY_END, 0, 0, 0);
    if(wrdt->opened)
	wrdt->end();
    if(free_instruments_afterwards)
    {
	int cnt;
	free_instruments();
	cnt = free_global_mblock(); /* free unused memory */
	if(cnt > 0)
	    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "%d memory blocks are free",
		      cnt);
    }
    free_special_patch(-1);
    if(event != NULL)
	free(event);
    if(rc == RC_RELOAD)
	goto play_restart;
    midi_restart_time = 0;
    if(rc == RC_ERROR && last_rc == RC_REALLY_PREVIOUS)
	return RC_REALLY_PREVIOUS;
    last_rc = rc;
    return rc;
}

void dumb_pass_playing_list(int number_of_files, char *list_of_files[])
{
    int i=0;

    for (;;)
	{
	  switch(play_midi_file(list_of_files[i]))
	    {
	    case RC_REALLY_PREVIOUS:
		if (i>0)
		    i--;
		break;

	    default: /* An error or something */
	    case RC_NEXT:
		if (i<number_of_files-1)
		    {
			i++;
			break;
		    }
		play_mode->flush_output();
		data_output_count = 0;
		return;

	    case RC_QUIT:
		return;
	    }
	}
}

void default_ctl_lyric(int lyricid)
{
    char *lyric;

    lyric = event2string(lyricid);
    if(lyric != NULL)
	ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s", lyric + 1);
}

void ctl_mode_event(int type, int trace, long arg1, long arg2)
{
    CtlEvent ce;
    ce.type = type;
    ce.v1 = arg1;
    ce.v2 = arg2;
    if(trace && ctl->trace_playing && !midi_trace.flush_flag)
	push_midi_trace_ce(ctl->event, &ce);
    else if(ctl->event != NULL)
	ctl->event(&ce);
}

void ctl_note_event(int noteID)
{
    CtlEvent ce;
    ce.type = CTLE_NOTE;
    ce.v1 = voice[noteID].status;
    ce.v2 = voice[noteID].channel;
    ce.v3 = voice[noteID].note;
    ce.v4 = voice[noteID].velocity;
    if(ctl->trace_playing && !midi_trace.flush_flag)
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
    if(ctl->trace_playing && !midi_trace.flush_flag)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_prog_event(int ch)
{
    CtlEvent ce;
    ce.type = CTLE_PROGRAM;
    ce.v1 = ch;
    ce.v2 = channel[ch].program;
    ce.v3 = (long)channel_instrum_name(ch);
    if(ctl->trace_playing && !midi_trace.flush_flag)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

char *channel_instrum_name(int ch)
{
    char *comm;
    int bank;

    if(ISDRUMCHANNEL(ch))
	return "";
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
    if(tonebank[bank] == NULL)
	bank = 0;
    comm = tonebank[bank]->tone[channel[ch].program].comment;
    if(comm == NULL)
	comm = tonebank[0]->tone[channel[ch].program].comment;
    return comm;
}
