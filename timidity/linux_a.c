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

#ifdef linux
#include <sys/ioctl.h> /* new with 1.2.0? Didn't need this under 1.1.64 */
#include <linux/soundcard.h>
#endif

#ifdef __FreeBSD__
#include <machine/soundcard.h>
#include <sys/filio.h>
#endif

#ifdef __bsdi__
#include <sys/soundcard.h>
#endif

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

#define dpm linux_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
  -1,
  {0}, /* default: get all the buffer fragments you can */
  "Linux dsp device", 'd',
  "/dev/dsp",
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

static void fd_set_nonblocking(int fd, int noblock)
{
    int arg;
    if(noblock)
	arg = 1;
    else
	arg = 0;

    if(ioctl(fd, FIONBIO, &arg) < 0)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ioctl:FIONBIO %s",
		  strerror(errno));
	sleep(3);
    }
}

static int open_output(void)
{
  int fd, tmp, i, warnings=0;

  play_counter = reset_samples = 0;

  /* Open the audio device */
  fd=open(dpm.name, O_RDWR | O_NDELAY);
  if (fd<0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
	   dpm.name, strerror(errno));
      return -1;
    }

  /*
   * Modified: Fri Nov 20 1998
   *
   * Reported from http://www.ife.ee.ethz.ch/~sailer/linux/pciaudio.html
   *   by Thomas Sailer <sailer@ife.ee.ethz.ch>
   * OSS/Free sets nonblocking mode with an ioctl, unlike the rest of the
   * kernel, which uses the O_NONBLOCK flag. I want to regularize that API,
   * and this trips on Timidity.
   */
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NDELAY);

  /* They can't mean these */
  dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);


  /* Set sample width to whichever the user wants. If it fails, try
     the other one. */

  i=tmp=(dpm.encoding & PE_16BIT) ? 16 : 8;
  if (ioctl(fd, SNDCTL_DSP_SAMPLESIZE, &tmp)<0 || tmp!=i)
    {
      /* Try the other one */
      i=tmp=(dpm.encoding & PE_16BIT) ? 8 : 16;
      if (ioctl(fd, SNDCTL_DSP_SAMPLESIZE, &tmp)<0 || tmp!=i)
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		    "%s doesn't support 16- or 8-bit sample width",
		    dpm.name);
	  close(fd);
	  return -1;
	}
      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		"Sample width adjusted to %d bits", tmp);
      dpm.encoding ^= PE_16BIT;
      warnings=1;
    }
  if (dpm.encoding & PE_16BIT)
    dpm.encoding |= PE_SIGNED;
  else
    dpm.encoding &= ~PE_SIGNED;


  /* Try stereo or mono, whichever the user wants. If it fails, try
     the other. */

  i=tmp=(dpm.encoding & PE_MONO) ? 0 : 1;
  if ((ioctl(fd, SNDCTL_DSP_STEREO, &tmp)<0) || tmp!=i)
    {
      i=tmp=(dpm.encoding & PE_MONO) ? 1 : 0;

      if ((ioctl(fd, SNDCTL_DSP_STEREO, &tmp)<0) || tmp!=i)
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	       "%s doesn't support mono or stereo samples",
	       dpm.name);
	  close(fd);
	  return -1;
	}
      if (tmp==0) dpm.encoding |= PE_MONO;
      else dpm.encoding &= ~PE_MONO;
      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Sound adjusted to %sphonic",
	   (tmp==0) ? "mono" : "stereo");
      warnings=1;
    }


  /* Set the sample rate */

  tmp=dpm.rate;
  if (ioctl(fd, SNDCTL_DSP_SPEED, &tmp)<0)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	   "%s doesn't support a %d Hz sample rate",
	   dpm.name, dpm.rate);
      close(fd);
      return -1;
    }
  if (tmp != dpm.rate)
    {
      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		"Output rate adjusted to %d Hz (requested %d Hz)", tmp, dpm.rate);
      dpm.rate=tmp;
      warnings=1;
    }

  /* Older VoxWare drivers don't have buffer fragment capabilities */
#ifdef SNDCTL_DSP_SETFRAGMENT
  /* Set buffer fragments (in extra_param[0]) */

  tmp=AUDIO_BUFFER_BITS;
  if (!(dpm.encoding & PE_MONO)) tmp++;
  if (dpm.encoding & PE_16BIT) tmp++;
  tmp |= (dpm.extra_param[0]<<16);
  i=tmp;
  if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &tmp)<0)
    {
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
	   "%s doesn't support %d-byte buffer fragments", dpm.name, (1<<i));
      /* It should still work in some fashion. We should use a
	 secondary buffer anyway -- 64k isn't enough. */
      warnings=1;
    }
#else
  if (dpm.extra_param[0])
    {
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		"%s doesn't support buffer fragments", dpm.name);
      warnings=1;
    }
#endif

  dpm.fd=fd;
  noblocking_flag = ctl->trace_playing;
  fd_set_nonblocking(fd, noblocking_flag);
  initialize_audio_bucket();

  return warnings;
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

static void close_output(void)
{
  if(dpm.fd == -1)
    return;
  close(dpm.fd);
  flush_buckets();
  play_counter = reset_samples = 0;
  dpm.fd=-1;
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

    ioctl(dpm.fd, SNDCTL_DSP_SYNC);
    play_counter = reset_samples = 0;

    return RC_NONE;
}

static void purge_output(void)
{
    ioctl(dpm.fd, SNDCTL_DSP_RESET);
    flush_buckets();
    play_counter = reset_samples = 0;
}

static void flush_buckets(void)
{
    reuse_audio_bucket_list(head);
    BUCKET_LIST_INIT(head, tail);
    audio_bucket_size = 0;
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
	fd_set_nonblocking(fd, noblocking_flag);
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

	    n = write(fd, head->data + head->pos, head->len - head->pos);
	    if(n == -1)
	    {
		if(errno == EWOULDBLOCK)
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
