/* -*- c-file-style: "gnu" -*-
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2001 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
    ALSA 0.[56] support by Katsuhiro Ueno <katsu@blue.sky.or.jp>

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

    alsa_a.c

    Functions to play sound on the ALSA audio driver

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

#if defined(SND_LIB_MINOR)
#define ALSA_LIB  SND_LIB_MINOR
#else
#define ALSA_LIB  3
#endif

#if ALSA_LIB < 4
typedef void  snd_pcm_t;
#endif

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
static int total_bytes;
static int output_counter;
#if ALSA_LIB >= 5
static int bytes_to_go;
#endif

/* export the playback mode */

#define dpm alsa_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_CAN_TRACE|PF_BUFF_FRAGM_OPT,
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
static int frag_size = 0;

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
  if (tmp < 0)
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
  if (tmp < 0)
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
  if (tmp < 0)
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
static int set_playback_info (snd_pcm_t* handle__,
			      int32* encoding__, int32* rate__,
			      const int32 extra_param[5])
{
  int ret_val = 0;
#if ALSA_LIB < 5
  const int32 orig_encoding = *encoding__;
#endif
  const int32 orig_rate = *rate__;
  int tmp;
#if ALSA_LIB >= 5
  snd_pcm_channel_info_t   pinfo;
  snd_pcm_channel_params_t pparams;
  snd_pcm_channel_setup_t  psetup;
#else
  snd_pcm_playback_info_t pinfo;
  snd_pcm_format_t pcm_format;
  struct snd_pcm_playback_params pparams;
  struct snd_pcm_playback_status pstatus;
  memset (&pcm_format, 0, sizeof (pcm_format));
#endif
  memset (&pparams, 0, sizeof (pparams));

#if ALSA_LIB >= 5
  memset (&pinfo, 0, sizeof (pinfo));
  pinfo.channel = SND_PCM_CHANNEL_PLAYBACK;
  tmp = snd_pcm_channel_info (handle__, &pinfo);
#else
  tmp = snd_pcm_playback_info (handle__, &pinfo);
#endif
  if (tmp < 0)
    {
      error_report (tmp);
      return -1;
    }

  /*check sample bit*/
#if ALSA_LIB >= 5
  if (!(pinfo.formats & ~(SND_PCM_FMT_S8 | SND_PCM_FMT_U8)))
    *encoding__ &= ~PE_16BIT; /*force 8bit samples*/
  if (!(pinfo.formats & ~(SND_PCM_FMT_S16 | SND_PCM_FMT_U16)))
    *encoding__ |= PE_16BIT; /*force 16bit samples*/
#else
  if ((pinfo.flags & SND_PCM_PINFO_8BITONLY) != 0)
    *encoding__ &= ~PE_16BIT; /*force 8bit samples*/
  if ((pinfo.flags & SND_PCM_PINFO_16BITONLY) != 0)
    *encoding__ |= PE_16BIT; /*force 16bit samples*/
#endif

  /*check rate*/
  if (pinfo.min_rate > *rate__)
    *rate__ = pinfo.min_rate;
  if (pinfo.max_rate < *rate__)
    *rate__ = pinfo.max_rate;
#if ALSA_LIB >= 5
  pparams.format.rate = *rate__;
#else
  pcm_format.rate = *rate__;
#endif

  /*check channels*/
#if ALSA_LIB >= 5
  if ((*encoding__ & PE_MONO) != 0 && pinfo.min_voices > 1)
    *encoding__ &= ~PE_MONO;
  if ((*encoding__ & PE_MONO) == 0 && pinfo.max_voices < 2)
    *encoding__ |= PE_MONO;

  if ((*encoding__ & PE_MONO) != 0)
    pparams.format.voices = 1; /*mono*/
  else
    pparams.format.voices = 2; /*stereo*/
#else /* ALSA_LIB < 5 */
  if ((*encoding__ & PE_MONO) != 0 && pinfo.min_channels > 1)
    *encoding__ &= ~PE_MONO;
  if ((*encoding__ & PE_MONO) == 0 && pinfo.max_channels < 2)
    *encoding__ |= PE_MONO;

  if ((*encoding__ & PE_MONO) != 0)
    pcm_format.channels = 1; /*mono*/
  else
    pcm_format.channels = 2; /*stereo*/
#endif

  /*check format*/
  if ((*encoding__ & PE_16BIT) != 0)
    { /*16bit*/
      if ((pinfo.formats & SND_PCM_FMT_S16_LE) != 0)
	{
#if ALSA_LIB >= 5
	  pparams.format.format = SND_PCM_SFMT_S16_LE;
#else
	  pcm_format.format = SND_PCM_SFMT_S16_LE;
#endif
	  *encoding__ |= PE_SIGNED;
	}
#if 0
      else if ((pinfo.formats & SND_PCM_FMT_U16_LE) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_U16_LE;
	  *encoding__ &= ~PE_SIGNED;
	}
      else if ((pinfo.formats & SND_PCM_FMT_S16_BE) != 0)
	{
	  pcm_format.format = SND_PCM_SFMT_S16_BE;
	  *encoding__ |= PE_SIGNED;
	}
      else if ((pinfo.formats & SND_PCM_FMT_U16_BE) != 0)
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
    { /*8bit*/
      if ((pinfo.formats & SND_PCM_FMT_U8) != 0)
	{
#if ALSA_LIB >= 5
	  pparams.format.format = SND_PCM_SFMT_U8;
#else
	  pcm_format.format = SND_PCM_SFMT_U8;
#endif
	  *encoding__ &= ~PE_SIGNED;
	}
#if 0
      else if ((pinfo.formats & SND_PCM_FMT_S8) != 0)
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


#if ALSA_LIB < 5
  tmp = snd_pcm_playback_format (handle__, &pcm_format);
  if (tmp < 0)
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
#endif

  /* Set buffer fragment size (in extra_param[1]) */
  if (extra_param[1] != 0)
    tmp = extra_param[1];
  else
    {
      tmp = audio_buffer_size;
      if (!(*encoding__ & PE_MONO))
	tmp <<= 1;
      if (*encoding__ & PE_16BIT)
	tmp <<= 1;
    }

  /* Set buffer fragments (in extra_param[0]) */
#if ALSA_LIB >= 5
#if ALSA_LIB >= 6
  pparams.frag_size = tmp;
  if (extra_param[0] == 0)
    pparams.buffer_size = pinfo.buffer_size;
  else
   {
     int frags = extra_param[0] - (extra_param[0] % pinfo.fragment_align);
     if (frags > pinfo.max_fragment_size)
       frags = pinfo.max_fragment_size;
     if (frags < pinfo.min_fragment_size)
       frags = pinfo.min_fragment_size; 
     pparams.buffer_size = tmp * frags;
   }
  pparams.buf.block.frags_xrun_max = 0;
  pparams.buf.block.frags_min = 1;
#else
  pparams.buf.block.frag_size = tmp;
  pparams.buf.block.frags_max = (extra_param[0] == 0) ? -1 : extra_param[0];
  pparams.buf.block.frags_min = 1;
#endif

  pparams.mode = SND_PCM_MODE_BLOCK;
  pparams.channel = SND_PCM_CHANNEL_PLAYBACK;
  pparams.start_mode = SND_PCM_START_GO;
#if ALSA_LIB >= 6
  pparams.xrun_mode  = SND_PCM_XRUN_FLUSH;
#else
  pparams.stop_mode  = SND_PCM_STOP_STOP;
#endif
  pparams.format.interleave = 1;

  snd_pcm_channel_flush (handle__, SND_PCM_CHANNEL_PLAYBACK);
  tmp = snd_pcm_channel_params (handle__, &pparams);
#else
  pparams.fragment_size  = tmp;
  pparams.fragments_max  = (extra_param[0] == 0) ? -1 : extra_param[0];
  pparams.fragments_room = 1;
  tmp = snd_pcm_playback_params (handle__, &pparams);
#endif /* ALSA_LIB >= 5 */
  if (tmp < 0)
    {
#if ALSA_LIB >= 6
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		"%s doesn't support buffer fragments"
		":request frag_size=%d, buffer_size=%d, min=%d\n",
		dpm.name,
		pparams.frag_size,
		pparams.buffer_size,
		pparams.buf.block.frags_min);
#elif ALSA_LIB == 5
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		"%s doesn't support buffer fragments"
		":request size=%d, max=%d, min=%d\n",
		dpm.name,
		pparams.buf.block.frag_size,
		pparams.buf.block.frags_max,
		pparams.buf.block.frags_min);
#else
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		"%s doesn't support buffer fragments"
		":request size=%d, max=%d, room=%d\n",
		dpm.name,
		pparams.fragment_size,
		pparams.fragments_max,
		pparams.fragments_room);
#endif
      ret_val =1;
    }

#if ALSA_LIB >= 5
  tmp = snd_pcm_channel_prepare (handle__, SND_PCM_CHANNEL_PLAYBACK);
  if (tmp < 0)
    {
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		"unable to prepare channel\n");
      return -1;
    }
#endif

#if ALSA_LIB >= 5
  memset (&psetup, 0, sizeof(psetup));
  psetup.channel = SND_PCM_CHANNEL_PLAYBACK;
  tmp = snd_pcm_channel_setup (handle__, &psetup);
  if (tmp == 0)
    {
      if(psetup.format.rate != orig_rate)
        {
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		    "Output rate adjusted to %d Hz (requested %d Hz)",
		    psetup.format.rate, orig_rate);
	  dpm.rate = psetup.format.rate;
	  ret_val = 1;
	}
#if ALSA_LIB >= 6
      frag_size = psetup.frag_size;
      total_bytes = frag_size * psetup.frags;
#else
      frag_size = psetup.buf.block.frag_size;
      total_bytes = frag_size * psetup.buf.block.frags;
#endif
      bytes_to_go = total_bytes;
    }
#else /* ALSA_LIB < 5 */
  if (snd_pcm_playback_status(handle__, &pstatus) == 0)
    {
      if (pstatus.rate != orig_rate)
	{
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		    "Output rate adjusted to %d Hz (requested %d Hz)",
		    pstatus.rate, orig_rate);
	  dpm.rate = pstatus.rate;
	  ret_val = 1;
	}
      frag_size = pstatus.fragment_size;
      total_bytes = pstatus.count;
    }
#endif
  else
    {
      frag_size = 0;
      total_bytes = -1; /* snd_pcm_playback_status fails */
    }

  return ret_val;
}

static int open_output(void)
{
  int tmp, warnings=0;
  int ret;

  tmp = check_sound_cards (&card, &device, dpm.extra_param);
  if (tmp < 0)
    return -1;

  /* Open the audio device */
  ret = snd_pcm_open (&handle, card, device, SND_PCM_OPEN_PLAYBACK);
  if (ret < 0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		dpm.name, snd_strerror (ret));
      return -1;
    }

  /* They can't mean these */
  dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);
  warnings = set_playback_info (handle, &dpm.encoding, &dpm.rate,
				dpm.extra_param);
  if (warnings < 0)
    {
      close_output ();
      return -1;
    }

#if ALSA_LIB >= 5
  dpm.fd = snd_pcm_file_descriptor (handle, SND_PCM_CHANNEL_PLAYBACK);
#else
  dpm.fd = snd_pcm_file_descriptor (handle);
#endif
  output_counter = 0;
  return warnings;
}

static void close_output(void)
{
  int ret;

  if (handle == NULL)
    return;

  ret = snd_pcm_close (handle);
#if ALSA_LIB == 5 /* Maybe alsa-driver 0.5 has a bug */
  if (ret != -EINVAL)
#endif
    if (ret < 0)
      error_report (ret);
  handle = NULL;

  dpm.fd = -1;
}

static int output_data(char *buf, int32 nbytes)
{
  int n;
#if ALSA_LIB >= 5
  snd_pcm_channel_status_t status;

  n = snd_pcm_write (handle, buf, nbytes);
  if (n <= 0)
    {
      memset (&status, 0, sizeof(status));
      status.channel = SND_PCM_CHANNEL_PLAYBACK;
      if (snd_pcm_channel_status(handle, &status) < 0)
	{
	  ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
		    "%s: could not get channel status", dpm.name);
	  return -1;
	}
#if ALSA_LIB >= 6
      if (status.status == SND_PCM_STATUS_XRUN)
#else
      if (status.status == SND_PCM_STATUS_UNDERRUN)
#endif
	{
#if ALSA_LIB >= 6
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		    "%s: underrun at %d", dpm.name, status.pos_io);
	  output_counter += status.pos_io;
#else
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		    "%s: underrun at %d", dpm.name, status.scount);
	  output_counter += status.scount;
#endif
	  snd_pcm_channel_flush (handle, SND_PCM_CHANNEL_PLAYBACK);
	  snd_pcm_channel_prepare (handle, SND_PCM_CHANNEL_PLAYBACK);
	  bytes_to_go = total_bytes;
	  n = snd_pcm_write (handle, buf, nbytes);
	}
      if (n <= 0)
	{
	  ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
		    "%s: %s", dpm.name,
		    (n < 0) ? snd_strerror(n) : "write error");
	  if (n != -EPIPE)  /* buffer underrun is ignored */
	    return -1;
	}
    }

  if (bytes_to_go > 0) {
    bytes_to_go -= nbytes;
    if (bytes_to_go <= 0) {
      if (snd_pcm_channel_go(handle, SND_PCM_CHANNEL_PLAYBACK) < 0) {
	ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
		  "%s: could not start playing", dpm.name);
	return -1;
      }
    }
  }

#else /* ALSA_LIB < 5 */
  while (nbytes > 0)
    {
      n = snd_pcm_write (handle, buf, nbytes);

      if (n < 0)
        {
	  ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
		    "%s: %s", dpm.name, snd_strerror(n));
	  if (n == -EWOULDBLOCK)
	    continue;
	  return -1;
	}
      buf += n;
      nbytes -= n;
      output_counter += n;
    }
#endif

  return 0;
}

static int acntl(int request, void *arg)
{
  int i;
#if ALSA_LIB >= 5
  snd_pcm_channel_status_t pstatus;
  memset (&pstatus, 0, sizeof (pstatus));
  pstatus.channel = SND_PCM_CHANNEL_PLAYBACK;
#else
  struct snd_pcm_playback_status pstatus;
#endif

  switch (request)
    {
      case PM_REQ_GETFRAGSIZ:
	if (frag_size == 0)
	  return -1;
	*((int *)arg) = frag_size;
	return 0;

      case PM_REQ_GETQSIZ:
	if (total_bytes == -1)
	  return -1;
	*((int *)arg) = total_bytes;
	return 0;

      case PM_REQ_GETFILLABLE:
	if (total_bytes == -1)
	  return -1;
#if ALSA_LIB >= 5
	if (snd_pcm_channel_status(handle, &pstatus) < 0)
	  return -1;
#if ALSA_LIB >= 6
	i = pstatus.bytes_free;
#else
	i = pstatus.free;
#endif
#else /* ALSA_LIB < 5 */
	if (snd_pcm_playback_status(handle, &pstatus) < 0)
	  return -1;
	i = pstatus.count;
#endif
	if(!(dpm.encoding & PE_MONO)) i >>= 1;
	if(dpm.encoding & PE_16BIT) i >>= 1;
	*((int *)arg) = i;
	return 0;

      case PM_REQ_GETFILLED:
	if (total_bytes == -1)
	  return -1;
#if ALSA_LIB >= 5
	if (snd_pcm_channel_status(handle, &pstatus) < 0)
	  return -1;
#if ALSA_LIB >= 6
	i = pstatus.bytes_used;
#else
	i = pstatus.count;
#endif
#else /* ALSA_LIB < 5 */
	if (snd_pcm_playback_status(handle, &pstatus) < 0)
	  return -1;
	i = pstatus.queue;
#endif
	if(!(dpm.encoding & PE_MONO)) i >>= 1;
	if(dpm.encoding & PE_16BIT) i >>= 1;
	*((int *)arg) = i;
	return 0;

      case PM_REQ_GETSAMPLES:
	if (total_bytes == -1)
	  return -1;
#if ALSA_LIB >= 5
	if (snd_pcm_channel_status(handle, &pstatus) < 0)
	  return -1;
#if ALSA_LIB >= 6
	i = output_counter + pstatus.pos_io;
#else
	i = output_counter + pstatus.scount;
#endif
#else
	if (snd_pcm_playback_status(handle, &pstatus) < 0)
	  return -1;
	i = output_counter - pstatus.queue;
#endif
	if (!(dpm.encoding & PE_MONO)) i >>= 1;
	if (dpm.encoding & PE_16BIT) i >>= 1;
	*((int *)arg) = i;
	return 0;

      case PM_REQ_DISCARD:
#if ALSA_LIB >= 5
	if (snd_pcm_playback_drain(handle) < 0)
	  return -1;
	if (snd_pcm_channel_prepare(handle, SND_PCM_CHANNEL_PLAYBACK) < 0)
	  return -1;
	bytes_to_go = total_bytes;
#else
	if (snd_pcm_drain_playback (handle) < 0)
	  return -1; /* error */
#endif
	output_counter = 0;
	return 0;

      case PM_REQ_FLUSH:
#if ALSA_LIB >= 5
	if (snd_pcm_channel_flush(handle, SND_PCM_CHANNEL_PLAYBACK) < 0)
	  return -1;
	if (snd_pcm_channel_prepare(handle, SND_PCM_CHANNEL_PLAYBACK) < 0)
	  return -1;
	bytes_to_go = total_bytes;
#else
	if (snd_pcm_flush_playback(handle) < 0)
	  return -1; /* error */
#endif
	output_counter = 0;
	return 0;
    }
    return -1;
}
