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

    load_mod.c: Module loader for TiMidity
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

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

typedef struct _mod_sample_info
{
    char name[22];		/* Sample name */
    uint16 len;			/* Sample length */
    uint8 finetune;		/* 0..7  => 0..7, 8..15 => -8..-1 */
    uint8 volume;		/* 0..64 */
    uint16 loop_start;		/* Loop point for sample */
    uint16 loop_len;		/* Loop length for sample */
} mod_sample_info;

#define SETMIDIEVENT(e, at, t, ch, pa, pb) \
    { (e).time = (at); (e).type = (t); \
      (e).channel = (uint8)(ch); (e).a = (uint8)(pa); (e).b = (uint8)(pb); }

#define MIDIEVENT(at, t, ch, pa, pb) \
    { MidiEvent event; SETMIDIEVENT(event, at, t, ch, pa, pb); \
      readmidi_add_event(&event); }

static int mod_cnv_pattern(struct timidity_file *tf, int nch, int nptn,
			   uint8 *poslist, int npos,
			   mod_sample_info *sinfo);
static int install_mod_instruments(struct timidity_file *tf,
				   mod_sample_info *sinfo);

static struct
{
    char *magic;
    int  ch;
} mod_magics[] =
{
    {"M.K.", 4}, /* Protracker */
    {"M!K!", 4}, /* Protracker */
    {"FLT4", 4}, /* Startracker */
    {"FLT8", 8}, /* Startracker */
    {"4CHN", 4}, /* Fasttracker */
    {"6CHN", 6}, /* Fasttracker */
    {"8CHN", 8}, /* Fasttracker */
    {"CD81", 8}, /* Atari Oktalyzer */
    {"OKTA", 8}, /* Atari Oktalyzer */
    {"16CN", 16},/* Taketracker */
    {"32CN", 32},/* Taketracker */
    {"    ", 4}, /* 15-instrument */
    {0, 0}
};

int load_mod(struct timidity_file *tf)
{
    char buff[32];
    char songname[20];
    int i;
    mod_sample_info sinfo[31];
    int nptn;
    uint8 poslist[128];
    int nch;

    /* Song name (Must be end with '\0' */
    tf_read(songname, 1, 20, tf);
    songname[19] = '\0'; /* for safty */

    /* Information for sample */
    if(tf_read(sinfo, sizeof(mod_sample_info), 31, tf) != 31)
	return 1;
    for(i = 0; i < 31; i++)
    {
	sinfo[i].finetune &= 0xF;
	if(sinfo[i].finetune >= 8)
	    sinfo[i].finetune -= 16;
	if(sinfo[i].volume > 64)
	    sinfo[i].volume = 64; /* 0..64 */
	sinfo[i].len = 2 * BE_SHORT(sinfo[i].len);
	sinfo[i].loop_start = 2 * BE_SHORT(sinfo[i].loop_start);
	sinfo[i].loop_len = 2 * BE_SHORT(sinfo[i].loop_len);
    }

    /* Song length */
    if(tf_getc(tf) == EOF)
	return 1;

    /* I don't know what this is */
    if(tf_getc(tf) == EOF)
	return 1;

    /* Song positions */
    if(tf_read(poslist, 1, 128, tf) != 128)
	return 1;

    nptn = 0;
    for(i = 0; i < 128; i++)
	if(nptn < poslist[i])
	    nptn = poslist[i];
    nptn++;

    /* magic */
    if(tf_read(buff, 1, 4, tf) != 4)
	return 1;
    buff[4] = '\0';
    nch = -1;
    for(i = 0; mod_magics[i].magic; i++)
	if(strncmp(buff, mod_magics[i].magic, 4) == 0)
	{
	    nch = mod_magics[i].ch;
	    break;
	}
    if(nch == -1)
	return 1; /* Not a MOD file */

    current_file_info->file_type = IS_MOD_FILE;
    if(current_file_info->seq_name == NULL)
	current_file_info->seq_name = safe_strdup(songname);
    if(mod_cnv_pattern(tf, nch, nptn, poslist, nptn, sinfo))
	return 2;
    if(install_mod_instruments(tf, sinfo))
	return 2;
    return 0;
}

static int install_mod_instruments(struct timidity_file *tf,
				   mod_sample_info *sinfo)
{
    int i, j;

    for(i = 0; i < 31; i++)
    {
	sample_t *dat;
	Sample *sp;
	char name[23];

	if(sinfo[i].len == 0)
	    continue;

	ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		  "MOD Sample %d (%.22s):len=%d ls=%d le=%d tune=%d v=%d",
		  i, sinfo[i].name,
		  sinfo[i].len, sinfo[i].loop_start, sinfo[i].loop_len,
		  sinfo[i].finetune, sinfo[i].volume);

	special_patch[i + 1] =
	    (SpecialPatch *)safe_malloc(sizeof(SpecialPatch));
	special_patch[i + 1]->type = INST_MOD;
	special_patch[i + 1]->samples = 1;
	special_patch[i + 1]->sample = sp =
	    (Sample *)safe_malloc(sizeof(Sample));
	memset(sp, 0, sizeof(Sample));
	memcpy(name, sinfo[i].name, 22);
	name[22] = '\0';
	code_convert(name, NULL, 23, NULL, "ASCII");
	if(name[0] == '\0')
	    special_patch[i + 1]->name = NULL;
	else
	    special_patch[i + 1]->name = safe_strdup(name);
	special_patch[i + 1]->sample_offset = 0;

#ifdef LOOKUP_HACK
	dat = (sample_t *)safe_malloc(sinfo[i].len);
	tf_read(dat, 1, sinfo[i].len, tf);
#else
	dat = (sample_t *)safe_malloc(sinfo[i].len * 2);
	tf_read(dat, 1, sinfo[i].len, tf);
	for(j = sinfo[i].len - 1; j >= 0; j--)
	    dat[j] = 256 * ((int8 *)dat)[j];
#endif /* LOOKUP_HACK */
	sp->data = dat;
	sp->data_alloced = 1;
	sp->data_length = sinfo[i].len;
	sp->loop_start = sinfo[i].loop_start;
	sp->loop_end   = sinfo[i].loop_start + sinfo[i].loop_len;
	sp->data_length <<= FRACTION_BITS;
	sp->loop_start <<= FRACTION_BITS;
	sp->loop_end <<= FRACTION_BITS;
	if(sinfo[i].loop_len > 2)
	    sp->modes = MODES_LOOPING;

	sp->sample_rate = (int32)(NTSC_RATE * 16);
	sp->low_freq = 0;
	sp->high_freq = 0x7fffffff;
	sp->root_freq = freq_table[MOD_BASE_NOTE + 48];
	sp->volume = 1.0;
	sp->panning = 64;
	sp->low_vel = 0;
	sp->high_vel = 127;
    }
    return 0;
}

static int mod_cnv_pattern(struct timidity_file *tf, int nch, int nptn,
			   uint8 *poslist, int npos,
			   mod_sample_info *sinfo)
{
    MBlockList pool;
    uint8 **ptns, *pt;
    int32 at, i, j;
    int play_speed;
    int songpos, ptpos, ptlen;
    int v;
    double r;

    int jumpflag, jump;
    int jumpcnt;
    int current_tempo = 125;

    init_mblock(&pool);
    ptns = (uint8 **)new_segment(&pool, nptn * sizeof(uint8 *));
    ptlen = 4 * 64 * nch;
    for(i = 0; i < nptn; i++)
    {
	ptns[i] = (uint8 *)new_segment(&pool, ptlen);
	if(tf_read(ptns[i], 1, ptlen, tf) != ptlen)
	{
	    reuse_mblock(&pool);
	    return 1;
	}
    }

    ModV = (ModVoice *)new_segment(&pool, nch * sizeof(ModVoice));

    readmidi_set_track(0, 1);

    at = 0;
    current_file_info->divisions = 24;
    play_speed = 6;

    mod_change_tempo(0, current_tempo);
    MIDIEVENT(0, ME_TEMPO, 0x00, 0x07, 0x53); /* 125 BPM */
    for(v = 0; v < nch; v++)
    {
	ModV[v].sample = 0;
	ModV[v].noteon = -1;
	ModV[v].period = 0;
	ModV[v].tune = 0;
	ModV[v].vol = ModV[v].lastvol = 64;
	ModV[v].retrig = 0;
	ModV[v].start = ModV[v].starttmp = 0;

	MIDIEVENT(0, ME_SET_PATCH, v, 1, 0);
	MIDIEVENT(0, ME_PAN, v, (v & 1) ? 127 : 0, 0);
	MIDIEVENT(0, ME_MONO, v, 0, 0);
	MIDIEVENT(0, ME_RPN_LSB, v, 0, 0);
	MIDIEVENT(0, ME_RPN_MSB, v, 0, 0);
	MIDIEVENT(0, ME_DATA_ENTRY_MSB, v, MOD_BEND_SENSITIVE, 0);
    }

    jumpflag = jump = jumpcnt = 0;
    for(songpos = 0; songpos < npos; songpos++)
    {
	pt = ptns[poslist[songpos]];
	ptpos = 0;
	i = 0;
	if(jumpflag)
	{
	    if(jumpflag == 0x0d)
		i = jump;
	    else if(jumpflag == 0x0b)
	    {
		songpos = jump;
		pt = ptns[poslist[songpos]];
	    }
	    jumpflag = jump = 0;
	}

	for(; i < 64; i++) /* 16 q.n. */
	{
	    jumpflag = 0;
	    jump = 0;
	    for(v = 0; v < nch; v++)
	    {
		int sample, period, efx, arg;

		sample = (pt[ptpos] & 0xF0) | (pt[ptpos + 2] >> 4);
		period = ((pt[ptpos] & 0x0F) << 8) | pt[ptpos + 1];
		efx = pt[ptpos + 2] & 0x0F;
		arg = pt[ptpos + 3];

		if(sample > 0)
		{
		    sample--;
		    if(sample > 31 || sinfo[sample].len == 0)
		    {
			if(ModV[v].noteon != -1)
			    MIDIEVENT(at, ME_NOTEOFF, v, ModV[v].noteon, 0);
			ModV[v].noteon = -1;
			period = 0;
		    }
		    else
		    {
			if(ModV[v].sample != sample)
			{
			    ModV[v].sample = sample;
			    MIDIEVENT(at, ME_SET_PATCH,
				      v, ModV[v].sample + 1, 0);
			    ModV[v].start = 0;
			}
			ModV[v].vol = sinfo[sample].volume;
		    }
		}

		ModV[v].efx = efx;
		ModV[v].arg = arg;
		ModV[v].skipon = 0;
		switch(efx)
		{
		  case 0x0b:	/* Jump */
		    if(jumpcnt < 2)
		    {
			if(arg < npos)
			{
			    jumpflag = 0x0b;
			    jump = arg;
			    if(arg <= songpos)
				jumpcnt++;
			    songpos--;
			}
		    }
		    break;

		  case 0x0d:	/* Break */
		    jumpflag = 0x0d;
		    jump = (arg & 0x0f) + ((arg >> 4) & 0x0f) * 10;
		    if(jump > 63)
			jump = 0;
		    break;

		  case 0x0f: /* speed change */
		    if(arg >= 0x20)
		    {
			if(current_tempo != arg)
			{
			    current_tempo = arg;
			    mod_change_tempo(at, current_tempo);
			}
		    }
		    else if(arg > 0)
			play_speed = arg;
		    break;

		  default:
		    mod_new_effect(v, period, efx, arg);
		    break;
		}

		if(ModV[v].lastvol != ModV[v].vol)
		{
		    int vol;
		    ModV[v].lastvol = ModV[v].vol;
		    vol = ModV[v].vol * 2;
		    if(vol > 127)
			vol = 127;
		    MIDIEVENT(at, ME_EXPRESSION, v, vol, 0);
		}

		if(period && !ModV[v].skipon)
		    mod_start_note(at, v, period);

		ptpos += 4;

		if(jumpflag)
		    break;
	    }

	    for(j = 0; j < play_speed - 1; j += 2, at += 2)
		for(v = 0; v < nch; v++)
		    mod_update_effect(at, v, 2);
	    if(j < play_speed)
	    {
		for(v = 0; v < nch; v++)
		    mod_update_effect(at, v, 1);
		at++;
	    }
	    if(jumpflag)
		break;
	}
    }

    r = 0.0;
    for(v = 0; v < nch; v++)
	if(ModV[v].sample != -1 && ModV[v].noteon != -1)
	{
	    double sec;

	    sec = sinfo[ModV[v].sample].len * (1.0 / NTSC_RATE)
		* (freq_table[MOD_BASE_NOTE] / freq_table[ModV[v].noteon]);
	    if(r < sec)
		r = sec;
	}
    i = (int32)(r * (48 / (60.0/125.0)));
    at += i;
    for(v = 0; v < nch; v++)
	MIDIEVENT(at, ME_ALL_NOTES_OFF, v, 0, 0);
    reuse_mblock(&pool);
    return 0;
}
