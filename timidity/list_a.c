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

    list_a.c - list MIDI programs
*/ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifndef __WIN32__
#include <unistd.h>
#endif
#include <stdlib.h>

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

static int play_event(void *);
static int open_output(void);
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);

static int32 play_counter;
static int32 tonebank_start_time[128][128];
static int32 drumset_start_time[128][128];
static int tonebank_counter[128][128];
static int drumset_counter[128][128];
static Channel channel[MAX_CHANNELS];

#define dmp list_play_mode

PlayMode dmp =
{
    DEFAULT_RATE, 0, 0,
    -1,
    {0,0,0,0,0},
    "List MIDI event", 'l',
    "-",
    play_event,
    open_output,
    close_output,
    output_data,
    flush_output,
    purge_output,
    current_samples,
    play_loop
};

static int open_output(void)
{
    int i, j;

    play_counter = 0;
    for(i = 0; i < 128; i++)
	for(j = 0; j < 128; j++)
	{
	    tonebank_start_time[i][j] = -1;
	    drumset_start_time[i][j] = -1;
	}
    memset(tonebank_counter, 0, sizeof(tonebank_counter));
    memset(drumset_counter, 0, sizeof(drumset_counter));
    return 0;
}

static void output_data(int32 *buf, int32 count)
{
    play_counter += count;
}

static int flush_output(void)
{
    return RC_NONE;
}

static void purge_output(void)
{
}

static int32 current_samples(void)
{
    return play_counter;
}

static int play_loop(void)
{
    return 1;
}

static char *time_str(int t)
{
    static char buff[32];

    t = (int)((double)t / (double)play_mode->rate + 0.5);
    sprintf(buff, "%d:%02d", t / 60, t % 60);
    return buff;
}

static void close_output(void)
{
    int i, j;

    for(i = 0; i < 128; i++)
	for(j = 0; j < 128; j++)
	    if(tonebank_start_time[i][j] != -1)
	    {
		ctl->cmsg(CMSG_TEXT, VERB_VERBOSE,
		    "Tonebank %d %d (start at %s, %d times note on)",
		    i, j,
		    time_str(tonebank_start_time[i][j]),
		    tonebank_counter[i][j]);
	    }
    for(i = 0; i < 128; i++)
	for(j = 0; j < 128; j++)
	    if(drumset_start_time[i][j] != -1)
	    {
		ctl->cmsg(CMSG_TEXT, VERB_VERBOSE,
		    "Drumset %d %d (start at %s, %d times note on)",
		    i, j,
		    time_str(drumset_start_time[i][j]),
		    drumset_counter[i][j]);
	    }
}

static int play_event(void *p)
{
    MidiEvent *ev = (MidiEvent *)p;
    int ch;

    ch = ev->channel;
    switch(ev->type)
    {
      case ME_NOTEON:
	if(ev->b)
	{
	    /* Note on */
	    int bank;
	    int inst;

	    bank = channel[ch].bank;
	    if(ISDRUMCHANNEL(ch))
	    {
		inst = ev->a;
		if(drumset_start_time[bank][inst] == -1)
		{
		    drumset_start_time[bank][inst] = ev->time;
		}
		drumset_counter[bank][inst]++;
	    }
	    else
	    {
		inst = channel[ch].program;
		if(tonebank_start_time[bank][inst] == -1)
		    tonebank_start_time[bank][inst] = ev->time;
		tonebank_counter[bank][inst]++;
	    }
	}
	break;
      case ME_PROGRAM:
	if(ISDRUMCHANNEL(ch))
	    channel[ch].bank = ev->a;
	else
	{
	    if(current_file_info && current_file_info->mid == 0x43) /* XG */
		channel[ch].bank = channel[ch].bank_lsb;
	    else
		channel[ch].bank = channel[ch].bank_msb;
	    channel[ch].program = ev->a;
	}
	break;
      case ME_TONE_BANK_LSB:
	channel[ch].bank_lsb = ev->a;
	break;
      case ME_TONE_BANK_MSB:
	channel[ch].bank_msb = ev->a;
	break;
      case ME_RESET:
	memset(channel, 0, sizeof(channel));
	break;
    }
    return RC_NONE;
}
