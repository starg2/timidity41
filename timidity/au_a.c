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

#ifdef __W32__
#include <stdlib.h>
#include <io.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

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

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 bytes);
static int acntl(int request, void *arg);
static int write_u32(uint32 value);

/* export the playback mode */

#define dpm au_play_mode

PlayMode dpm = {
    8000, PE_MONO|PE_ULAW, PF_PCM_STREAM,
    -1,
    {0,0,0,0,0},
    "Sun audio file", 'u',
    "output.au",
    open_output,
    close_output,
    output_data,
    acntl
};

/*************************************************************************/

#define UPDATE_HEADER_STEP (128*1024) /* 128KB */
static uint32 bytes_output, next_bytes;
static int already_warning_lseek;

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
    char *comment = "";
    int include_enc, exclude_enc;

    include_enc = exclude_enc = 0;
    if(dpm.encoding & PE_16BIT)
    {
#ifdef LITTLE_ENDIAN
	include_enc = PE_BYTESWAP;
#else
	exclude_enc = PE_BYTESWAP;
#endif /* LITTLE_ENDIAN */
	include_enc |= PE_SIGNED;
    }
    else if(!(dpm.encoding & (PE_ULAW|PE_ALAW)))
    {
	/* is 8 bit au unsigned ? */
	exclude_enc = PE_SIGNED;
    }

    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);
    bytes_output = 0;

    if(dpm.name && dpm.name[0] == '-' && dpm.name[1] == '\0')
    {
	dpm.fd = 1; /* data to stdout */
	comment = "(stdout)";
    }
    else
    {
	/* Open the audio file */
	dpm.fd = open(dpm.name, FILE_OUTPUT_MODE);
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

    next_bytes = bytes_output + UPDATE_HEADER_STEP;
    already_warning_lseek = 0;

    return 0;
}


static int update_header(void)
{
    off_t save_point;

    if(already_warning_lseek)
	return 0;

    save_point = lseek(dpm.fd, 0, SEEK_CUR);
    if(save_point == -1 || lseek(dpm.fd, 8, SEEK_SET) == -1)
    {
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		  "Warning: %s: %s: Can't make valid header",
		  dpm.name, strerror(errno));
	already_warning_lseek = 1;
	return 0;
    }

    if(write_u32(bytes_output) == -1) return -1;
    lseek(dpm.fd, save_point, SEEK_SET);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "%s: Update au header (size=%d)", dpm.name, bytes_output);
    return 0;
}


static int output_data(char *buf, int32 bytes)
{
    int n;

    while(((n = write(dpm.fd, buf, bytes)) == -1) && errno == EINTR)
	;
    if(n == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		  dpm.name, strerror(errno));
	return -1;
    }

    bytes_output += bytes;

#if UPDATE_HEADER_STEP > 0
    if(bytes_output >= next_bytes)
    {
	if(update_header() == -1) return -1;
	next_bytes = bytes_output + UPDATE_HEADER_STEP;
    }
#endif /* UPDATE_HEADER_STEP */
    return n;
}

static void close_output(void)
{
    if(dpm.fd != 1) /* We don't close stdout */
    {
	if(dpm.fd != 1 && /* We don't close stdout */
	   dpm.fd != -1)
	{
	    if(update_header() == -1)
		return;
	    close(dpm.fd);
	    dpm.fd = -1;
	}
    }
}

static int acntl(int request, void *arg)
{
    return -1;
}
