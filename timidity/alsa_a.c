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

    linux_audio.c

    Functions to play sound on the VoxWare audio driver (Linux or FreeBSD)

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

/*ALSA header file*/
#include <sys/asoundlib.h>

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);
static int total_bytes, output_counter;

/* export the playback mode */

#define dpm alsa_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_CAN_TRACE,
  -1,
  {0}, /* default: get all the buffer fragments you can */
  "ALSA pcm device", 's',
  "/dev/snd/pcm00",
  open_output,
  close_output,
  output_data,
  acntl
};

/*************************************************************************/
/* We currently only honor the PE_MONO bit, the sample rate, and the
   number of buffer fragments. We try 16-bit signed data first, and
   then 8-bit unsigned if it fails. If you have a sound device that
   can't handle either, let me know. */


/*ALSA PCM handler*/
static snd_pcm_t* handle = NULL;
static int card = 0;
static int device = 0;


static void error_report (int snd_error)
{
  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	    dpm.name, snd_strerror (snd_error));
}

/*return value == 0 sucess
               == -1 fails
 */
static int check_sound_cards (int* card__, int* device__,
			      const int32 extra_param[5])
{
  /*Search sound cards*/
  struct snd_ctl_hw_info ctl_hw_info;
  snd_pcm_info_t pcm_info;
  snd_ctl_t* ctl_handle;
  const char* env_sound_card = getenv ("TIMIDITY_SOUND_CARD");
  const char* env_pcm_device = getenv ("TIMIDITY_PCM_DEVICE");
  int tmp;

  /*specify card*/
  *card__ = 0;
  if (env_sound_card != NULL)
    *card__ = atoi (env_sound_card);
  /*specify device*/
  *device__ = 0;
  if (env_pcm_device != NULL)
    *device__ = atoi (env_pcm_device);
  
  tmp = snd_cards ();
  if (tmp == 0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "No sound card found.");
      return -1;
    }
  if (tmp < 0)
    {
      error_report (tmp);
      return -1;
    }
  
  if (*card__ < 0 || *card__ >= tmp)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "There is %d sound cards."
		" %d is invalid sound card. assuming 0.",
		tmp, *card__);
      *card__ = 0;
    }

  tmp = snd_ctl_open (&ctl_handle, *card__);
  if (tmp != 0)
    {
      error_report (tmp);
      return -1;
    }

  /*check whether sound card has pcm device(s)*/
  tmp = snd_ctl_hw_info (ctl_handle, & ctl_hw_info);
  if (ctl_hw_info.pcmdevs == 0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		"%d-th sound card(%s) has no pcm device",
		ctl_hw_info.longname, *card__);
      snd_ctl_close (ctl_handle);
      return -1;
    }
  
  if (*device__ < 0 || *device__ >= ctl_hw_info.pcmdevs)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		"%d-th sound cards(%s) has %d pcm device(s)."
		" %d is invalid pcm device. assuming 0.",
		*card__, ctl_hw_info.longname, ctl_hw_info.pcmdevs, *device__);
      *device__ = 0;
      
      if (ctl_hw_info.pcmdevs == 0)
	{/*sound card has no pcm devices*/
	  snd_ctl_close (ctl_handle);
	  return -1;
	}
    }

  /*check whether pcm device is able to playback*/
  tmp = snd_ctl_pcm_info(ctl_handle, *device__, &pcm_info);
  if (tmp != 0)
    {
      error_report (tmp);
      snd_ctl_close (ctl_handle);
      return -1;
    }
  
  if ((pcm_info.flags & SND_PCM_INFO_PLAYBACK) == 0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		"%d-th sound cards(%s), device=%d, "
		"type=%d, flags=%d, id=%s, name=%s,"
		" does not support playback",
		*card__, ctl_hw_info.longname, ctl_hw_info.pcmdevs,
		pcm_info.type, pcm_info.flags, pcm_info.id, pcm_info.name);
      snd_ctl_close (ctl_handle);
      return -1;
    }
  
  tmp = snd_ctl_close (ctl_handle);
  if (tmp != 0)
    {
      error_report (tmp);
      return -1;
    }

  return 0;
}

/*return value == 0 sucess
               == 1 warning
               == -1 fails
 */
static int set_playback_info (void* handle__,
			      int32* encoding__, int32* rate__,
			      const int32 extra_param[5])
{
  int ret_val = 0;
  const int32 orig_encoding = *encoding__;
  const int32 orig_rate = *rate__;
  snd_pcm_playback_info_t playback_info;
  snd_pcm_format_t pcm_format;
  struct snd_pcm_playback_params playback_params;
  struct snd_pcm_playback_status playback_status;
  int tmp;
  memset (&pcm_format, 0, sizeof (pcm_format));
  memset (&playback_params, 0, sizeof (playback_params));
  
  tmp = snd_pcm_playback_info (handle__, &playback_info);
  if (tmp != 0)
    {
      error_report (tmp);
      return -1;
    }

  /*check sample bit*/
  if ((playback_info.flags & SND_PCM_PINFO_8BITONLY) != 0)
    *encoding__ &= ~PE_16BIT;/*force 8bit samles*/
  if ((playback_info.flags & SND_PCM_PINFO_16BITONLY) != 0)
    *encoding__ |= PE_16BIT;/*force 16bit samples*/
  
  /*check rate*/
  if (playback_info.min_rate > *rate__)
    *rate__ = playback_info.min_rate;
  if (playback_info.max_rate < *rate__)
    *rate__ = playback_info.max_rate;
  pcm_format.rate = *rate__;

  /*check channels*/
  if ((*encoding__ & PE_MONO) != 0 && playback_info.min_channels > 1)
    *encoding__ &= ~PE_MONO;
  if ((*encoding__ & PE_MONO) == 0 && playback_info.max_channels < 2)
    *encoding__ |= PE_MONO;
  
  if ((*encoding__ & PE_MONO) != 0)
    pcm_format.channels = 1;/*mono*/
  else
    pcm_format.channels = 2;/*stereo*/

  /*check format*/
  if ((*encoding__ & PE_16BIT) != 0)
    {/*16bit*/
      if ((playback_info.formats & SND_PCM_FMT_S16_LE) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_S16_LE;
	  *encoding__ |= PE_SIGNED;
	}
#if 0
      else if ((playback_info.formats & SND_PCM_FMT_U16_LE) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_U16_LE;
	  *encoding__ &= ~PE_SIGNED;
	}
      else if ((playback_info.formats & SND_PCM_FMT_S16_BE) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_S16_BE;
	  *encoding__ |= PE_SIGNED;
	}
      else if ((playback_info.formats & SND_PCM_FMT_U16_BE) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_U16_LE;
	  *encoding__ &= ~PE_SIGNED;
	}
#endif
      else
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		    "%s doesn't support 16 bit sample width",
		    dpm.name);
	  return -1;
	}
    }
  else
    {/*8bit*/
      if ((playback_info.formats & SND_PCM_FMT_U8) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_U8;
	  *encoding__ &= ~PE_SIGNED;
	}
#if 0
      else if ((playback_info.formats & SND_PCM_FMT_S8) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_U16_LE;
	  *encoding__ |= PE_SIGNED;
	}
#endif
      else
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		    "%s doesn't support 8 bit sample width",
		    dpm.name);
	  return -1;
	}
    }

  
  tmp = snd_pcm_playback_format (handle__, &pcm_format);
  if (tmp != 0)
    {
      error_report (tmp);
      return -1;
    }
  /*check result of snd_pcm_playback_format*/
  if ((*encoding__ & PE_16BIT) != (orig_encoding & PE_16BIT ))
    {
      ctl->cmsg (CMSG_WARNING, VERB_VERBOSE,
		 "Sample width adjusted to %d bits",
		 ((*encoding__ & PE_16BIT) != 0)? 16:8);
      ret_val = 1;
    }
  if (((pcm_format.channels == 1)? PE_MONO:0) != (orig_encoding & PE_MONO))
    {
      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Sound adjusted to %sphonic",
		((*encoding__ & PE_MONO) != 0)? "mono" : "stereo");
      ret_val = 1;
    }
  
  /* Set buffer fragments (in extra_param[0]) */
  tmp = AUDIO_BUFFER_BITS;
  if (!(*encoding__ & PE_MONO))
    tmp++;
  if (*encoding__ & PE_16BIT)
    tmp++;
#if 0
  tmp++;
  playback_params.fragment_size = (1 << tmp);
  if (extra_param[0] == 0)
    playback_params.fragments_max = 7;/*default value. What's value is apporpriate?*/
  else
    playback_params.fragments_max = extra_param[0];
#else
  playback_params.fragment_size = (1 << tmp);

  if (extra_param[0] == 0)
    playback_params.fragments_max = 15;/*default value. What's value is apporpriate?*/
  else
    playback_params.fragments_max = extra_param[0];

#endif
  playback_params.fragments_room = 1;
  tmp = snd_pcm_playback_params (handle__, &playback_params);
  if (tmp != 0)
    {
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		"%s doesn't support buffer fragments"
		":request size=%d, max=%d, room=%d\n",
		dpm.name,
		playback_params.fragment_size,
		playback_params.fragments_max,
		playback_params.fragments_room);
      ret_val =1;
    }

  if(snd_pcm_playback_status(handle__, &playback_status) == 0)
    {
      if (playback_status.rate != orig_rate)
	{
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		    "Output rate adjusted to %d Hz (requested %d Hz)",
		    playback_status.rate, orig_rate);
	  dpm.rate = playback_status.rate;
	  ret_val = 1;
	}
      total_bytes = playback_status.count;
    }
  else
    total_bytes = -1; /* snd_pcm_playback_status fails */

  return ret_val;
}

static int open_output(void)
{
  int tmp, warnings=0;
  int ret;
  
  tmp = check_sound_cards (&card, &device, dpm.extra_param);
  if (tmp != 0)
    return -1;
  
  /* Open the audio device */
  ret = snd_pcm_open (&handle, card, device, SND_PCM_OPEN_PLAYBACK);
  if (ret != 0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		dpm.name, snd_strerror (ret));
      return -1;
    }

  /* They can't mean these */
  dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);
  warnings = set_playback_info (handle, &dpm.encoding, &dpm.rate,
				dpm.extra_param);
  if (warnings == -1)
    {
      close_output ();
      return -1;
    }

  dpm.fd = snd_pcm_file_descriptor (handle);
  output_counter = 0;
  return warnings;
}

static void close_output(void)
{
  int ret;
  
  if (handle == NULL)
    return;
  
  ret = snd_pcm_close (handle);
  if (ret != 0)
    error_report (ret);
  handle = NULL;
  
  dpm.fd = -1;
}

static int output_data(char *buf, int32 nbytes)
{
    int n;

    while(nbytes > 0)
    {
	n = snd_pcm_write(handle, buf, nbytes);
	if(n == -1)
	{
	    ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
		      "%s: %s", dpm.name, strerror(errno));
	    if(errno == EWOULDBLOCK)
		continue;
	    return -1;
	}
	buf += n;
	nbytes -= n;
	output_counter += n;
    }

    return 0;
}

static int acntl(int request, void *arg)
{
    struct snd_pcm_playback_status playback_status;
    int i;

    switch(request)
    {
      case PM_REQ_GETQSIZ:
	if(total_bytes == -1)
	  return -1;
	*((int *)arg) = total_bytes;
	return 0;

      case PM_REQ_GETFILLABLE:
	if(total_bytes == -1)
	  return -1;
	if(snd_pcm_playback_status(handle, &playback_status) != 0)
	  return -1;
	*((int *)arg) = playback_status.count;
	return 0;

      case PM_REQ_GETFILLED:
	if(total_bytes == -1)
	  return -1;
	if(snd_pcm_playback_status(handle, &playback_status) != 0)
	  return -1;
	*((int *)arg) = playback_status.queue;
	return 0;

      case PM_REQ_GETSAMPLES:
	if(total_bytes == -1)
	  return -1;
	if(snd_pcm_playback_status(handle, &playback_status) != 0)
	  return -1;
	i = output_counter - playback_status.queue;
	if(!(dpm.encoding & PE_MONO)) i >>= 1;
	if(dpm.encoding & PE_16BIT) i >>= 1;
	*((int *)arg) = i;
	return 0;

      case PM_REQ_DISCARD:
	if(snd_pcm_drain_playback (handle) != 0)
	    return -1; /* error */
	output_counter = 0;
	return 0;

      case PM_REQ_FLUSH:
	if(snd_pcm_flush_playback(handle) != 0)
	  return -1; /* error */
	output_counter = 0;
	return 0;
    }
    return -1;
}
