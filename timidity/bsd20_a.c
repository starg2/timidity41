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

    bsd20_a.c
	Written by Yamate Keiichiro <keiich-y@is.aist-nara.ac.jp>
*/

/*
 *  BSD/OS 2.0 audio
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <i386/isa/sblast.h>

#include "timidity.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);
static int32 play_counter, reset_samples;
static double play_start_time;

extern int default_play_event(void *);

/* export the playback mode */

#define dpm bsdi_play_mode

PlayMode dpm = {
    DEFAULT_RATE, PE_SIGNED|PE_MONO, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
    -1,
    {0}, /* default: get all the buffer fragments you can */
    "BSD/OS sblast dsp", 'd',
    "/dev/sb_dsp",
    default_play_event,
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
    int fd, tmp, warnings=0;

    play_counter = reset_samples = 0;

    if ((fd=open(dpm.name, O_RDWR | O_NDELAY, 0)) < 0)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		  dpm.name, strerror(errno));
	return -1;
    }

    /* They can't mean these */
    dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);


    /* Set sample width to whichever the user wants. If it fails, try
       the other one. */

    if (dpm.encoding & PE_16BIT)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: sblast only 8bit",
		  dpm.name);
	close(fd);
	return -1;
    }

    tmp = ((~dpm.encoding) & PE_16BIT) ? PCM_8 : 0;
    ioctl(fd, DSP_IOCTL_COMPRESS, &tmp);
    dpm.encoding &= ~PE_SIGNED;

    /* Try stereo or mono, whichever the user wants. If it fails, try
       the other. */

    tmp=(dpm.encoding & PE_MONO) ? 0 : 1;
    ioctl(fd, DSP_IOCTL_STEREO, &tmp);

  /* Set the sample rate */

    tmp=dpm.rate * ((dpm.encoding & PE_MONO) ? 1 : 2);
    ioctl(fd, DSP_IOCTL_SPEED, &tmp);
    if (tmp != dpm.rate)
    {
	dpm.rate=tmp / ((dpm.encoding & PE_MONO) ? 1 : 2);;
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		  "Output rate adjusted to %d Hz", dpm.rate);
	warnings=1;
    }

    /* Older VoxWare drivers don't have buffer fragment capabilities */

    if (dpm.extra_param[0])
    {
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		  "%s doesn't support buffer fragments", dpm.name);
	warnings=1;
    }

    tmp = 16384;
    ioctl(fd, DSP_IOCTL_BUFSIZE, &tmp);

    dpm.fd = fd;
    return warnings;
}

static void add_sample_counter(int32 count)
{
    current_samples(); /* update offset_samples */
    play_counter += count;
}

static void output_data(int32 *buf, int32 count)
{
    int32 count_arg = count;
    if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */

    /* Convert to 8-bit unsigned and write out. */
    s32tou8(buf, count);
    add_sample_counter(count_arg);
    while ((-1==write(dpm.fd, buf, count)) && errno==EINTR)
	;
}

static void close_output(void)
{
    close(dpm.fd);
    play_counter = reset_samples = 0;
    dpm.fd = -1;
}

static int flush_output(void)
{
    int rc;

    /* extract all trace */
    while(trace_loop())
    {
	rc = check_apply_control();
	if(RC_IS_SKIP_FILE(rc))
	{
	    purge_output();
	    return rc;
	}
    }

    /* wait until play out */
    do
    {
	rc = check_apply_control();
	if(RC_IS_SKIP_FILE(rc))
	{
	    purge_output();
	    return rc;
	}
	current_samples();
    } while(play_counter > 0);

    ioctl(dpm.fd, DSP_IOCTL_FLUSH);
    play_counter = reset_samples = 0;

    return RC_NONE;
}

static void purge_output(void)
{
    ioctl(dpm.fd, DSP_IOCTL_RESET);
    play_counter = reset_samples = 0;
}

static int play_loop(void)
{
    return 0;
}

static int32 current_samples(void)
{
    double realtime, es;

    realtime = get_current_calender_time();
    if(play_counter == 0)
    {
	play_start_time = realtime;
	return reset_samples;
    }
    es = dpm.rate * (realtime - play_start_time);
    if(es >= play_counter)
    {
	/* out of play counter */
	reset_samples += play_counter;
	play_counter = 0;
	play_start_time = realtime;
	return reset_samples;
    }
    if(es < 0)
	return 0; /* for safety */
    return (int32)es + reset_samples;
}
