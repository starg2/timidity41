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

/* Define if you want to use soft audio buffering (AUDIO_FILLING_SEC sec.) */
/* #define AUDIO_FILLING_MILSEC 3000 */

/* Defined if you want to use initial audio buffering */
/* #define INITIAL_FILLING */

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);
static int32 play_counter, reset_samples;
static double play_start_time;
static int noblocking_flag;

extern int default_play_event(void *);

/* export the playback mode */

#define dpm alsa_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
  -1,
  {0}, /* default: get all the buffer fragments you can */
  "ALSA pcm device", 's',
  "/dev/snd/pcm00",
  default_play_event,
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output,
  current_samples,
  play_loop
};


/* Must be multiple of 4 */
#define BUCKETSIZE (((16 * 1024) - 2 * sizeof(int) - sizeof(void *)) & ~3)

typedef struct _AudioBucket
{
    char data[BUCKETSIZE];
    int pos, len;
    struct _AudioBucket *next;
} AudioBucket;

static AudioBucket *head = NULL;
static AudioBucket *tail = NULL;
static int audio_bucket_size = 0;
static void initialize_audio_bucket(void);
static void reuse_audio_bucket(AudioBucket *bucket);
static void reuse_audio_bucket_list(AudioBucket *bucket);
static AudioBucket *new_allocated_bucket(void);
static void flush_buckets(void);
#define BUCKET_LIST_INIT(head, tail) head = tail = NULL

#define ADD_BUCKET(head, tail, bucket)	\
    if(head == NULL)			\
	head = tail = bucket;		\
    else				\
	tail = tail->next = bucket

#define ADD_BUCKET_LIST(head, tail, bucket)	\
    if(head == NULL)				\
	head = tail = bucket;			\
    else if(bucket)				\
	tail = tail->next = bucket;		\
    if(tail) while(tail->next) tail = tail->next


#ifdef INITIAL_FILLING
static int filling_flag;
#endif /* INITIAL_FILLING */
static int32 max_audio_buffersize;


/*************************************************************************/
/* We currently only honor the PE_MONO bit, the sample rate, and the
   number of buffer fragments. We try 16-bit signed data first, and
   then 8-bit unsigned if it fails. If you have a sound device that
   can't handle either, let me know. */


/*ALSA PCM handler*/
static void* handle = NULL;
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
  void* ctl_handle;
  int tmp = snd_cards ();

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
  
  if (*card__ >= tmp)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "There is %d sound cards."
		" %d is invalid sound card. assuming 0.",
		tmp, *card__);
      *card__ = 0;
    }

  /*XXX specify card*/
  tmp = snd_ctl_open (&ctl_handle, card);
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
  
  /*XXX specify device*/
  if (*device__ >= ctl_hw_info.pcmdevs)
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
		    "%s doesn't support mono or stereo samples",
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
		    "%s doesn't support mono or stereo samples",
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
  if (pcm_format.rate != orig_rate)
    {
      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
                "Output rate adjusted to %d Hz (requested %d Hz)",
		pcm_format.rate, orig_rate);
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

  return ret_val;
}

static void set_block_mode (void* handle__, int enable)
{
  int ret = snd_pcm_block_mode (handle__, enable);
  if (ret != 0)
    {
      error_report (ret);
      sleep(3);
    }
}

static int open_output(void)
{
  int tmp, warnings=0;
  int ret;
  
  play_counter = reset_samples = 0;

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

  noblocking_flag = ctl->trace_playing;
  set_block_mode (handle, noblocking_flag);
  initialize_audio_bucket();
  
  dpm.fd = snd_pcm_file_descriptor (handle);
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
  
  flush_buckets();
  play_counter = reset_samples = 0;
  dpm.fd = -1;
}

static void push_play_bucket(const char *buf, int n)
{
    if(buf == NULL || n == 0)
	return;

    if(head == NULL)
	head = tail = new_allocated_bucket();

    audio_bucket_size += n;
    while(n)
    {
	int i;

	if(tail->len == BUCKETSIZE)
	{
	    AudioBucket *b;
	    b = new_allocated_bucket();
	    ADD_BUCKET(head, tail, b);
	}

	i = BUCKETSIZE - tail->len;
	if(i > n)
	    i = n;
	memcpy(tail->data + tail->len, buf, i);

	buf += i;
	n   -= i;
	tail->len += i;
    }
}

static void add_sample_counter(int32 count)
{
    current_samples(); /* update offset_samples */
    play_counter += count;
}

static void output_data(int32 *buf, int32 count)
{
  if(count == 0)
      return;
  if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */

  if (dpm.encoding & PE_16BIT)
    {
      /* Convert data to signed 16-bit PCM */
      s32tos16(buf, count);
      count *= 2;
    }
  else
    {
      /* Convert to 8-bit unsigned and write out. */
      s32tou8(buf, count);
    }

#ifdef INITIAL_FILLING
    current_samples();
    if(audio_bucket_size < max_audio_buffersize && play_counter == 0)
    {
	filling_flag = 1;
	push_play_bucket((char *)buf, count);
    }
    else
    {
	filling_flag = 0;
#else
    if(audio_bucket_size == 0)
	push_play_bucket((char *)buf, count);
    else
    {
#endif /* INITIAL_FILLING */
	if(audio_bucket_size > max_audio_buffersize)
	{
	    do
	    {
		play_loop();
		trace_loop();
	    } while(audio_bucket_size > max_audio_buffersize);
	}
	push_play_bucket((char *)buf, count);
    }
}


static int flush_output(void)
{
    int rc;
#ifdef INITIAL_FILLING
    filling_flag = 0;
#endif /* INITIAL_FILLING */

    if(audio_bucket_size == 0 && play_counter == 0 && reset_samples == 0)
	return RC_NONE;

    /* extract all trace */
    if(ctl->trace_playing)
    {
	while(trace_loop() && play_loop()) /* Must call both trace/play loop */
	{
	    rc = check_apply_control();
	    if(RC_IS_SKIP_FILE(rc))
	    {
		purge_output();
		return rc;
	    }
	}
	while(trace_loop() || play_loop()) /* Call trace or play loop */
	{
	    rc = check_apply_control();
	    if(RC_IS_SKIP_FILE(rc))
	    {
		purge_output();
		return rc;
	    }
	}
    }
    else
    {
	trace_flush();
	while(play_loop())
	{
	    usleep(100000);
	    rc = check_apply_control();
	    if(RC_IS_SKIP_FILE(rc))
	    {
		purge_output();
		return rc;
	    }
	}
    }

    /* wait until play out */
    do
    {
	usleep(100000);
	rc = check_apply_control();
	if(RC_IS_SKIP_FILE(rc))
	{
	    purge_output();
	    return rc;
	}
	current_samples();
    } while(play_counter > 0);

    /*ioctl(dpm.fd, SNDCTL_DSP_SYNC);*/
    if (snd_pcm_flush_playback (handle) != 0)
      {
	/*error!*/
      }
    play_counter = reset_samples = 0;

    return RC_NONE;
}

static void purge_output(void)
{
    /*ioctl(dpm.fd, SNDCTL_DSP_RESET);*/
    int ret = snd_pcm_drain_playback (handle);
    if (ret != 0)
      {
	/* error !*/
      }
    flush_buckets();
    play_counter = reset_samples = 0;
}

static void flush_buckets(void)
{
    reuse_audio_bucket_list(head);
    BUCKET_LIST_INIT(head, tail);
    audio_bucket_size = 0;
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

static int play_loop(void)
{
    AudioBucket *tmp;
    int bpf; /* Bytes per sample frame */
    int fd;

    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	      "Audio Soft Buffer: %d", audio_bucket_size);

#ifdef INITIAL_FILLING
    if(filling_flag)
	return 0;
#endif /* INITIAL_FILLING */

    fd = dpm.fd;

    /* ctl->trace_playing can be changed in `N' interface */
    if(noblocking_flag != ctl->trace_playing)
    {
	noblocking_flag = ctl->trace_playing;
	set_block_mode (handle, noblocking_flag);
    }

    bpf = 1;
    if(!(dpm.encoding & PE_MONO))
	bpf = 2;
    if(dpm.encoding & PE_16BIT)
	bpf *= 2;

    while(head)
    {
	if(head->pos < head->len)
	{
	    int n;

	    /*n = write(fd, head->data + head->pos, head->len - head->pos);*/
	    n = snd_pcm_write (handle,
			       head->data + head->pos, head->len - head->pos);
	    /*if(n == -1)*/
	    if(n < 0)
	    {
                 /*if(errno == EWOULDBLOCK)*/
	        if(n == EWOULDBLOCK)
		    return 1;
		return 0;
	    }
	    audio_bucket_size -= n;
	    head->pos += n;
	    add_sample_counter(n / bpf);
	}
	if(head->pos != head->len)
	    return 1;
	tmp = head;
	head = head->next;
	reuse_audio_bucket(tmp);
    }
    return 0;
}

#ifdef AUDIO_FILLING_MILSEC
#define ALLOCATED_N_BUCKET 32
#else
#define ALLOCATED_N_BUCKET 4
#endif /* AUDIO_FILLING_MILSEC */

static AudioBucket *allocated_bucket_list = NULL;

static void initialize_audio_bucket(void)
{
    int i;
    AudioBucket *b;

    b = (AudioBucket *)safe_malloc(ALLOCATED_N_BUCKET * sizeof(AudioBucket));
    for(i = 0; i < ALLOCATED_N_BUCKET; i++)
	reuse_audio_bucket(b + i);
}

static AudioBucket *new_allocated_bucket(void)
{
    AudioBucket *b;

    if(allocated_bucket_list == NULL)
	b = (AudioBucket *)safe_malloc(sizeof(AudioBucket));
    else
    {
	b = allocated_bucket_list;
	allocated_bucket_list = allocated_bucket_list->next;
    }
    memset(b, 0, sizeof(AudioBucket));
    return b;
}

static void reuse_audio_bucket(AudioBucket *bucket)
{
    if(bucket == NULL)
	return;
    bucket->next = allocated_bucket_list;
    allocated_bucket_list = bucket;
}

static void reuse_audio_bucket_list(AudioBucket *bucket)
{
    while(bucket)
    {
	AudioBucket *tmp;

	tmp = bucket;
	bucket = bucket->next;
	reuse_audio_bucket(tmp);
    }
}
