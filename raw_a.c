/* 

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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

#ifdef __WIN32__
#include <stdlib.h>
#include <io.h>
#include <string.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>

#ifdef __FreeBSD__
#include <stdio.h>
#endif

#include "config.h"
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
static void flush_output(void);
static void purge_output(void);

/* export the playback mode */


#define dpm raw_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED,
  -1,
  {0,0,0,0,0},
  "raw waveform data", 'r',
  "output.raw",
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output  
};

/*************************************************************************/

static int open_output(void)
{
  if (dpm.encoding & PE_ULAW)
    dpm.encoding &= ~PE_16BIT;

  if (!(dpm.encoding & PE_16BIT))
    dpm.encoding &= ~PE_BYTESWAP;

  if (dpm.name && dpm.name[0]=='-' && dpm.name[1]=='\0')
    dpm.fd=1; /* data to stdout */
  else
    {
      /* Open the audio file */
		dpm.fd=open(dpm.name, OPEN_MODE, 0644);
      if (dpm.fd<0)
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		    dpm.name, sys_errlist[errno]);
	  return -1;
	}
    }
  return 0;
}

static void output_data(int32 *buf, int32 count)
{
  if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */
  
  if (dpm.encoding & PE_16BIT)
    {
      if (dpm.encoding & PE_BYTESWAP)
	{
	  if (dpm.encoding & PE_SIGNED)
	    s32tos16x(buf, count);
	  else
	    s32tou16x(buf, count);
	}
      else
	{
	  if (dpm.encoding & PE_SIGNED)
	    s32tos16(buf, count);
	  else 
	    s32tou16(buf, count);
	}
      
      while ((-1==write(dpm.fd, buf, count * 2)) && errno==EINTR)
	;
    }
  else
    {
      if (dpm.encoding & PE_ULAW)
	{
	  s32toulaw(buf, count);
	}
      else
	{
	  if (dpm.encoding & PE_SIGNED)
	    s32tos8(buf, count);
	  else 
	    s32tou8(buf, count);
	}
      
      while ((-1==write(dpm.fd, buf, count)) && errno==EINTR)
	;
    }
}

static void close_output(void)
{
  if (dpm.fd != 1) /* We don't close stdout */
    close(dpm.fd);
}

static void flush_output(void) { }
static void purge_output(void) { }
