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

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
/*
 * This is beta version of module player for TiMidity 
 */

#include <stdio.h>

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

#define SETMIDIEVENT(e, at, t, ch, pa, pb) \
    { (e).time = (at); (e).type = (t); \
      (e).channel = (uint8)(ch); (e).a = (uint8)(pa); (e).b = (uint8)(pb); }

#define MIDIEVENT(at, t, ch, pa, pb) \
    { MidiEvent event; SETMIDIEVENT(event, at, t, ch, pa, pb); \
      readmidi_add_event(&event); }

ModVoice *ModV;
extern int load_mod(struct timidity_file *tf);

static struct
{
    int (* loader)(struct timidity_file *tf);
    int type;
} mod_loaders[] =
{
    {load_mod, IS_MOD_FILE},
    {NULL, IS_OTHER_FILE}
};

static int period_table[60] =
{
/*       C     C#    D     D#    E     F     F#    G     G#    A    A#   B  */
/* 0 */ 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906,
/* 1 */  856,  808,  762,  720,  678,  640,  604,  570,  538,  508, 480, 453,
/* 2 */  428,  404,  381,  360,  339,  320,  302,  285,  269,  254, 240, 226,
/* 3 */  214,  202,  190,  180,  170,  160,  151,  143,  135,  127, 120, 113,
/* 4 */  107,  101,   95,   90,   85,   80,   75,   71,   67,   63,  60,  56
};

int load_module_file(struct timidity_file *tf, int mod_type)
{
    int i, err;

    for(i = 0; mod_loaders[i].loader; i++)
	if(mod_type == mod_loaders[i].type)
	{
	    err = mod_loaders[i].loader(tf);
	    if(err == 1)
	    {
		url_rewind(tf->url);
		if(tf_getc(tf) == 0)
		{
		    skip(tf, 127);
		    err = mod_loaders[i].loader(tf);
		}
	    }
	    return err;
	}
    return 1;
}


int get_module_type(char *fn)
{
    char *p;

    if(check_file_extension(fn, ".mod", 1))
	return IS_MOD_FILE;

    if((p = strchr(fn, '#')) != NULL)
	p++;
    else
	p = fn;
    if(strncmp(p, "mod.", 4) == 0 || strncmp(p, "MOD.", 4) == 0)
	return IS_MOD_FILE;

    return IS_OTHER_FILE;
}

void mod_change_tempo(int32 at, int bpm)
{
    int32 tempo;
    int c, a, b;

    tempo = 60000000/bpm;
    c = (tempo & 0xff);
    a = ((tempo >> 8) & 0xff);
    b = ((tempo >> 16) & 0xff);
    MIDIEVENT(at, ME_TEMPO, c, b, a);
}

static int mod_vol_slide(int vol, int slide)
{
    vol += slide >> 4;
    vol -= slide & 0x0f;
    if(vol < 0)
	return 0;
    if(vol > 64)
	return 64;
    return vol;
}

void mod_new_effect(int v, int period, int efx, int arg)
{
    switch(efx)
    {
      case MOD_EFX_PORTAMENT:
	if(ModV[v].noteon == -1)
	{
	    ModV[v].efx = 0;
	    break;
	}
	if(arg)
	    ModV[v].period_step = arg;
	if(period)
	    ModV[v].period_todo = period;
	ModV[v].skipon = 1;
	break;

      case MOD_EFX_SAMPOFFS:
	ModV[v].starttmp = arg * 256;
	break;

      case MOD_EFX_VOLSLIDE:
	ModV[v].vol = mod_vol_slide(ModV[v].vol, arg);
	break;

      case MOD_EFX_VOLCHNG:
	arg &= 0x3f;
	ModV[v].vol = arg;
	break;

      default:
	if(efx)
	{
	    ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
		      "Effect V%d 0x%02x 0x%02x period=%d (Ignored)",
		      v, efx, arg, period);
	}
	break;
    }
}

void mod_update_effect(int32 at, int v, int mul)
{
    int i;

    switch(ModV[v].efx)
    {
      case MOD_EFX_PORTAMENT:
	if(ModV[v].period > ModV[v].period_todo)
	{
	    i = ModV[v].period - ModV[v].period_step * mul;
	    if(i < ModV[v].period_todo)
		i = ModV[v].period_todo;
	    mod_period_move(at, v, i);
	}
	else if(ModV[v].period < ModV[v].period_todo)
	{
	    i = ModV[v].period + ModV[v].period_step * mul;
	    if(i > ModV[v].period_todo)
		i = ModV[v].period_todo;
	    mod_period_move(at, v, i);
	}
	break;

      case MOD_EFX_VOLSLIDE:
	for(i = 0; i < mul; i++)
	    ModV[v].vol = mod_vol_slide(ModV[v].vol, ModV[v].arg);
	if(ModV[v].lastvol != ModV[v].vol)
	{
	    int vol;
	    ModV[v].lastvol = ModV[v].vol;
	    vol = ModV[v].vol * 2;
	    if(vol > 127)
		vol = 127;
	    MIDIEVENT(at, ME_EXPRESSION, v, vol, 0);
	}
	break;
    }
}

void mod_pitchbend(int at, int v, int tune)
{
    int bend;

    if(ModV[v].tune != tune)
    {
	ModV[v].tune = tune;
	bend = tune * (8192 / 256) / MOD_BEND_SENSITIVE + 8192;
	if(bend <= 0)
	    bend = 1;
	else if(bend >= 2 * 8192)
	    bend = 2 * 8192 - 1;
	MIDIEVENT(at, ME_PITCHWHEEL, v, bend & 0x7F, (bend >> 7) & 0x7F);
    }
}

void mod_start_note(int32 at, int v, int period)
{
    int tune;

    if(ModV[v].noteon != -1)
    {
	MIDIEVENT(at, ME_NOTEOFF, v, ModV[v].noteon, 0);
    }

    if(ModV[v].start != ModV[v].starttmp)
    {
	int a, b;
	ModV[v].start = ModV[v].starttmp;
	a = (ModV[v].start & 0xff);
	b = ((ModV[v].start >> 8) & 0xff);
	MIDIEVENT(at, ME_PATCH_OFFS,
		  ModV[v].sample + 1, a, b);
    }

    ModV[v].noteon = period2note(period, &tune);
    if(ModV[v].noteon >= 0)
    {
	MIDIEVENT(at, ME_NOTEON, v, ModV[v].noteon, 0x7f);
	ModV[v].period = period;
	ModV[v].period_start = period;
    }
    ModV[v].tuneon = tune;
    mod_pitchbend(at, v, tune);
}

void mod_period_move(int32 at, int v, int newperiod)
{
    int note, tune, diff;

    diff = newperiod - ModV[v].period;
    if(diff == 0 || ModV[v].noteon == -1)
	return;
    ModV[v].period = newperiod;
    note = period2note(ModV[v].period, &tune);
    tune = (note * 256 + tune) -
	(ModV[v].noteon * 256 + ModV[v].tuneon);
    mod_pitchbend(at, v, tune);
}

int period2note(int period, int *finetune)
{
    int note;
    int l, r, m;

    if(period < 56 || period > 1712)
	return -1;

    /* bin search */
    l = 0;
    r = 60;
    while(l < r)
    {
	m = (l + r) / 2;
	if(period_table[m] >= period)
	    l = m + 1;
	else
	    r = m;
    }
    note = l - 1;

    /*
     * 59 >= note >= 0
     * period_table[note] >= period > period_table[note + 1]
     */

    if(period_table[note] == period)
    {
	*finetune = 0;
	return note + MOD_NOTE_OFFSET;
    }

    /* fine tune completion */
    *finetune = (int)(256.0 *
		      (period_table[note] - period) /
		      (period_table[note] - period_table[note + 1]));

    return note + MOD_NOTE_OFFSET;
}
