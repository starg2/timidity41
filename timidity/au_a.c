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

    au_a.c

    Functions to output Sun audio file (*.au).
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#ifdef __WIN32__
#include <stdlib.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>

#ifdef __FreeBSD__
#include <stdio.h>
#endif

#include "timidity.h"
#include "output.h"
#include "controls.h"

#ifdef __WIN32__
#define OPEN_MODE O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define OPEN_MODE O_WRONLY | O_CREAT | O_TRUNC
#endif

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);
static int write_u32(uint32 value);

extern int default_play_event(void *);

/* export the playback mode */

#define dpm au_play_mode

PlayMode dpm = {
    8000, PE_MONO|PE_SIGNED|PE_ULAW, PF_NEED_INSTRUMENTS,
    -1,
    {0,0,0,0,0},
    "Sun audio file", 'u',
    "output.au",
    default_play_event,
    open_output,
    close_output,
    output_data,
    flush_output,
    purge_output,
    current_samples,
    play_loop
};

/*************************************************************************/

/* Count the number of bytes output so the header can be fixed when
   closing the file */
static uint32 bytes_output;

static int write_u32(uint32 value)
{
    int n;
    value = BE_LONG(value);
    if((n = write(dpm.fd, &value, 4)) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		  dpm.name, strerror(errno));
	close(dpm.fd);
	dpm.fd = -1;
	return -1;
    }
    return n;
}

static int write_str(char *s)
{
    int n;
    if((n = write(dpm.fd, s, strlen(s))) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		  dpm.name, strerror(errno));
	close(dpm.fd);
	dpm.fd = -1;
	return -1;
    }
    return n;
}

/* Sun Audio File Encoding Tags */
#define AUDIO_FILE_ENCODING_MULAW_8     1      /* 8-bit ISDN u-law */
#define AUDIO_FILE_ENCODING_LINEAR_8    2      /* 8-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_16   3      /* 16-bit linear PCM */
#define AUDIO_FILE_ENCODING_ALAW_8      27     /* 8-bit ISDN A-law */

static int open_output(void)
{
    char *comment;

    dpm.encoding &= ~PE_BYTESWAP;
    dpm.encoding |= PE_SIGNED;

    if(dpm.encoding & (PE_ULAW|PE_ALAW))
	dpm.encoding &= ~PE_16BIT;

    bytes_output = 0;

    if(dpm.name && dpm.name[0] == '-' && dpm.name[1] == '\0')
    {
	dpm.fd = 1; /* data to stdout */
	comment = "(stdout)";
    }
    else
    {
	/* Open the audio file */
	dpm.fd = open(dpm.name, OPEN_MODE, 0644);
	if(dpm.fd < 0)
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		      dpm.name, strerror(errno));
	    return -1;
	}
	comment = dpm.name;
    }

    /* magic */
    if(write_str(".snd") == -1) return -1;

    /* header size */
    if(write_u32((uint32)(24 + strlen(comment))) == -1) return -1;

    /* sample data size */
    if(write_u32((uint32)0xffffffff) == -1) return -1;

    /* audio file encoding */
    if(dpm.encoding & PE_ULAW)
    {
	if(write_u32((uint32)AUDIO_FILE_ENCODING_MULAW_8) == -1) return -1;
    }
    else if(dpm.encoding & PE_ALAW)
    {
	if(write_u32((uint32)AUDIO_FILE_ENCODING_ALAW_8) == -1) return -1;
    }
    else if(dpm.encoding & PE_16BIT)
    {
	if(write_u32((uint32)AUDIO_FILE_ENCODING_LINEAR_16) == -1) return -1;
    }
    else
    {
	if(write_u32((uint32)AUDIO_FILE_ENCODING_LINEAR_8) == -1) return -1;
    }

    /* sample rate */
    if(write_u32((uint32)dpm.rate) == -1) return -1;

    /* number of channels */
    if(dpm.encoding & PE_MONO)
    {
	if(write_u32((uint32)1) == -1) return -1;
    }
    else
    {
	if(write_u32((uint32)2) == -1) return -1;
    }

    /* comment */
    if(write_str(comment) == -1) return -1;

    return 0;
}

static void output_data(int32 *buf, int32 count)
{
    if(!(dpm.encoding & PE_MONO))
	count*=2; /* Stereo samples */

    if(dpm.encoding & PE_16BIT)
    {
	s32tos16b(buf, count); /* Big-endian data */
	while((-1==write(dpm.fd, buf, count * 2)) && errno==EINTR)
	    ;
	bytes_output += count * 2;
    }
    else
    {
	if(dpm.encoding & PE_ULAW)
	    s32toulaw(buf, count);
	else if(dpm.encoding & PE_ALAW)
	    s32toalaw(buf, count);
	else
	    s32tos8(buf, count);

	while((-1==write(dpm.fd, buf, count)) && errno==EINTR)
	    ;
	bytes_output += count;
    }
}

static void close_output(void)
{
    if(dpm.fd != 1) /* We don't close stdout */
    {
	/* It's not stdout, so it's probably a file, and we can try
	   fixing the block lengths in the header before closing. */
	if(lseek(dpm.fd, 8, SEEK_SET) >= 0)
	    write_u32(bytes_output);
	close(dpm.fd);
    }
    dpm.fd = -1;
}

/* Dummies */
static int flush_output(void)	{ return RC_NONE; }
static void purge_output(void)	{ }
static int play_loop(void)	{ return 0; }
static int32 current_samples(void) { return -1; }
