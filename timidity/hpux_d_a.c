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

    Functions to play sound on a HP's audio device

    Copyright 1997 Lawrence T. Hoff

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h> 

#include <sys/audio.h>

#include "timidity.h"
#include "config.h"
#include "output.h"
#include "controls.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);

extern int default_play_event(void *);

/* export the playback mode */

#define dpm hpux_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
  -1,
  {0,0,0,0,0}, /* no extra parameters so far */
  "HP audio device", 'd',
  "/dev/audio",
  default_play_event,
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output  
};

/*************************************************************************/
/*
   Encoding will be 16-bit linear signed, unless PE_ULAW is set, in
   which case it'll be 8-bit uLaw. I don't think it's worthwhile to
   implement any 8-bit linear modes as the sound quality is
   unrewarding. PE_MONO is honored.  */

static int open_output(void)
{

if (dpm.encoding & PE_ULAW)
  dpm.encoding &= ~PE_16BIT;

if (!(dpm.encoding & PE_16BIT))
  dpm.encoding &= ~PE_BYTESWAP;

dpm.fd = open(dpm.name, O_WRONLY, 0);
if(dpm.fd == -1)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	   dpm.name, strerror(errno));
      return -1;
    }

(void) ioctl(dpm.fd, AUDIO_SET_SAMPLE_RATE, dpm.rate);

(void) ioctl(dpm.fd, AUDIO_SET_DATA_FORMAT, (dpm.encoding & PE_16BIT)?
	AUDIO_FORMAT_LINEAR16BIT: AUDIO_FORMAT_ULAW);

(void) ioctl(dpm.fd, AUDIO_SET_CHANNELS, (dpm.encoding & PE_MONO)?1:2);

/* set some reasonable buffer size */
(void) ioctl(dpm.fd, AUDIO_SET_TXBUFSIZE, 8*4096);

/* output to all devices */
(void) ioctl(dpm.fd, AUDIO_SET_OUTPUT, 
	AUDIO_OUT_SPEAKER | AUDIO_OUT_HEADPHONE | AUDIO_OUT_LINE);

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

      /* write the data out */
      write(dpm.fd, buf, count * 2);

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
      
      /* write the data out */
      write(dpm.fd, buf, count);
    }
}

static void close_output(void)
{
  /* free resources */
  close(dpm.fd);
}

static int flush_output(void)
{
(void) ioctl(dpm.fd, AUDIO_DRAIN, RESET_TX_BUF);
return RC_NONE;
}

static void purge_output(void)
{
(void) ioctl(dpm.fd, AUDIO_RESET, RESET_TX_BUF);
}
