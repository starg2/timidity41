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

    sun_audio.c

    Functions to play sound on a Sun's /dev/audio. 

    THESE ARE UNTESTED -- If you need to make modifications to get
    them to work, please send me the diffs, preferrably with a brief 
    explanation of what was wrong. Thanks!

*/

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h> 

#ifdef SOLARIS
 #include <sys/audioio.h>
#else
 #include <sun/audioio.h>
#endif

#include "config.h"
#include "output.h"
#include "controls.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static void flush_output(void);
static void purge_output(void);

/* export the playback mode */

#define dpm sun_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED,
  -1,
  {0,0,0,0,0}, /* no extra parameters so far */
  "Sun audio device", 'd',
  "/dev/audio",
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

static audio_info_t auinfo;

static int open_output(void)
{
  int fd, tmp, i, warnings=0;
  
  /* Open the audio device */

#ifdef SOLARIS
  fd=open(dpm.name, O_RDWR );
#else
  fd=open(dpm.name, O_RDWR | O_NDELAY);
#endif

  if (fd<0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	   dpm.name, sys_errlist[errno]);
      return -1;
    }


  /* Does any device need byte-swapped data? Turn the bit off here. */
  dpm.encoding &= ~PE_BYTESWAP;


  AUDIO_INITINFO(&auinfo);

  if (ioctl(fd, AUDIO_GETINFO, &auinfo)<0)
    { 
      /* If it doesn't give info, it probably won't take requests
	 either. Assume it's an old device that does 8kHz uLaw only.

	 Disclaimer: I don't know squat about the various Sun audio
	 devices, so if this is not what we should do, I'll gladly
	 accept modifications. */

      ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Cannot inquire %s", dpm.name);

      dpm.encoding = PE_ULAW|PE_MONO;
      dpm.rate = 8000;
      warnings=1;
    }
  else
    {
      ctl->cmsg(CMSG_INFO,VERB_DEBUG, 
		"1. precision=%d  channels=%d  encoding=%d  sample_rate=%d",
		auinfo.play.precision, auinfo.play.channels,
		auinfo.play.encoding, auinfo.play.sample_rate);
      ctl->cmsg(CMSG_INFO,VERB_DEBUG, 
		"1. (dpm.encoding=0x%02x  dpm.rate=%d)",
		dpm.encoding, dpm.rate);


      /* Select 16-bit linear / 8-bit uLaw encoding */

      if (dpm.encoding & PE_ULAW)
	{
	  auinfo.play.precision = 8;
	  auinfo.play.encoding = AUDIO_ENCODING_ULAW;
	}
      else
	{
	  auinfo.play.precision = 16;
	  auinfo.play.encoding = AUDIO_ENCODING_LINEAR;
	}
      if (ioctl(fd, AUDIO_SETINFO, &auinfo)<0)
	{
	  /* Try the other one instead */
	  if (dpm.encoding & PE_ULAW)
	    {
	      auinfo.play.precision = 16;
	      auinfo.play.encoding = AUDIO_ENCODING_LINEAR;
	    }
	  else
	    {
	      auinfo.play.precision = 8;
	      auinfo.play.encoding = AUDIO_ENCODING_ULAW;
	    }
	  if (ioctl(fd, AUDIO_SETINFO, &auinfo)<0)
	    {
	      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		   "%s doesn't support 16-bit linear or 8-bit uLaw samples", 
		   dpm.name);
	      close(fd);
	      return -1;
	    }
	  dpm.encoding ^= PE_ULAW;
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Encoding adjusted to %s",
		    (dpm.encoding & PE_ULAW) ? "uLaw" : "16-bit linear");
	  warnings=1;
	}
      /* Set the other bits right. */
      if (dpm.encoding & PE_ULAW)
	dpm.encoding &= ~(PE_16BIT|PE_SIGNED);
      else
	dpm.encoding |= PE_16BIT|PE_SIGNED;


      /* Select stereo or mono samples */

      auinfo.play.channels = (dpm.encoding & PE_MONO) ? 1 : 2;
      if (ioctl(fd, AUDIO_SETINFO,&auinfo)<0)
	{
	  auinfo.play.channels = (dpm.encoding & PE_MONO) ? 2 : 1;
	  if ((dpm.encoding & PE_MONO) ||
	      (ioctl(fd, AUDIO_SETINFO,&auinfo)<0))
	    {
	      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
		   "%s doesn't support mono or stereo samples", dpm.name);
	      close(fd);
	      return -1;
	    }
	  dpm.encoding ^= PE_MONO;
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Sound adjusted to %sphonic", 
		    (dpm.encoding & PE_MONO) ? "mono" : "stereo");
	  warnings=1;
	}


      /* Select sampling rate */

      auinfo.play.sample_rate=dpm.rate;
      if (ioctl(fd, AUDIO_SETINFO,&auinfo)<0)
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		    "%s doesn't support a %d Hz sample rate",
		    dpm.name, dpm.rate);
	  close(fd);
	  return -1;
	}
      /* This may be pointless -- do the Sun devices give an error if
         the sampling rate can't be produced exactly? */
      if (auinfo.play.sample_rate != dpm.rate)
	{
	  dpm.rate=auinfo.play.sample_rate;
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		    "Output rate adjusted to %d Hz", dpm.rate);
	  warnings=1;
	}
 

      ctl->cmsg(CMSG_INFO,VERB_DEBUG, 
		"2. precision=%d  channels=%d  encoding=%d  sample_rate=%d",
		auinfo.play.precision, auinfo.play.channels,
		auinfo.play.encoding, auinfo.play.sample_rate);
      ctl->cmsg(CMSG_INFO,VERB_DEBUG, 
		"2. (dpm.encoding=0x%02x  dpm.rate=%d)",
		dpm.encoding, dpm.rate);
    }

  dpm.fd=fd;
  
  return warnings;
}

static void output_data(int32 *buf, int32 count)
{
  if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */
  
  if (dpm.encoding & PE_ULAW)
    {
      /* Convert to 8-bit uLaw and write out. */
      s32toulaw(buf, count);
      
      while ((-1==write(dpm.fd, buf, count)) && errno==EINTR)
	;
    }
  else
    {
      /* Convert data to signed 16-bit PCM */
      s32tos16(buf, count);
      
      while ((-1==write(dpm.fd, buf, count * 2)) && errno==EINTR)
	;
    }
}

static void close_output(void)
{
  close(dpm.fd);
}

static void flush_output(void)
{
}

static void purge_output(void)
{
}
