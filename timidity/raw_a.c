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

    raw_audio.c

    Functions to output raw sound data to a file or stdout.

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

/* export the playback mode */

#define dpm raw_play_mode

PlayMode dpm = {
    DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM,
    -1,
    {0,0,0,0,0},
    "Raw waveform data", 'r',
    "output.raw",
    open_output,
    close_output,
    output_data,
    acntl
};

/*************************************************************************/

static int open_output(void)
{
    dpm.encoding = validate_encoding(dpm.encoding, 0, 0);

    if(dpm.name && dpm.name[0] == '-' && dpm.name[1] == '\0')
	dpm.fd = 1; /* data to stdout */
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
    }
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
    return n;
}

static void close_output(void)
{
    if(dpm.fd != 1 && dpm.fd != -1) /* We don't close stdout */
	close(dpm.fd);
    dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_DISCARD:
	return 0;
    }
    return -1;
}
