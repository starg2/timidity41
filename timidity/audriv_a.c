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

    audriv_audio.c

    Functions to play sound on audriv_*.
    Written by Masanao Izumo <mo@goice.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "aenc.h"
#include "audriv.h"
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
static int32 output_counter = 0;
static int noblocking_flag;

extern int default_play_event(void *);

#ifdef SOLARIS
/* shut gcc warning up */
int usleep(unsigned int useconds);
#endif

/* export the playback mode */

#define dpm audriv_play_mode
PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
  -1,
  {0,0,0,0,0}, /* no extra parameters so far */

#if defined(AU_DEC)
  "DEC audio device",
#elif defined(sgi)
  "SGI audio device",
#elif defined(sun)
  "Sun audio device",
#elif defined(__NetBSD__)
  "NetBSD audio device",
#elif defined(AU_NONE)
  "Psedo audio device (No audio)",
#else
  "Audriv audio device",
#endif

  'd', "",
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

/********** Audio_Init **********************
  This just does some generic initialization.  The actual audio
  output device open is done in the Audio_On routine so that we
  can get a device which matches the requested sample rate & format
 *****/

static int Audio_Init(void)
{
    static int init_flag = False;

    if(init_flag)
	return 0;
    if(!audriv_setup_audio())
    {
	ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "%s", audriv_errmsg);
	return -1;
    }
    noblocking_flag = ctl->trace_playing;
    audriv_set_noblock_write(noblocking_flag);
    initialize_audio_bucket();
    init_flag = True;
    return 0;
}

static int audio_init_open(void)
{
    int ch, enc;

    if(audriv_set_play_sample_rate(dpm.rate) == False)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Sample rate %d is not supported", dpm.rate);
	return -1;
    }

    if(dpm.encoding & PE_MONO)/* Mono samples */
	ch = 1;
    else
	ch = 2;

    if(audriv_set_play_channels(ch) == False)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Channel %d is not supported", ch);
	return -1;
    }

    if(dpm.encoding & PE_ULAW)
    {
	enc = AENC_G711_ULAW;
	dpm.encoding &= ~PE_16BIT;
    }
    else if(dpm.encoding & PE_ALAW)
    {
	enc = AENC_G711_ALAW;
	dpm.encoding &= ~PE_16BIT;
    }
    else if(dpm.encoding & PE_16BIT)
    {
#if defined(LITTLE_ENDIAN)
	if(dpm.encoding & PE_BYTESWAP)
	    if(dpm.encoding & PE_SIGNED)
		enc = AENC_SIGWORDB;
	    else
		enc = AENC_UNSIGWORDL;
	else
	    if(dpm.encoding & PE_SIGNED)
		enc = AENC_SIGWORDL;
	    else
		enc = AENC_UNSIGWORDL;
#else /* BIG_ENDIAN */
	if(dpm.encoding & PE_BYTESWAP)
	    if(dpm.encoding & PE_SIGNED)
		enc = AENC_SIGWORDL;
	    else
		enc = AENC_UNSIGWORDB;
	else
	    if(dpm.encoding & PE_SIGNED)
		enc = AENC_SIGWORDB;
	    else
		enc = AENC_UNSIGWORDB;
#endif
    }
    else
    {
	/* 8 bit */
	if(dpm.encoding & PE_SIGNED)
	    enc = AENC_SIGBYTE;
	else
	    enc = AENC_UNSIGBYTE;
    }

    if(audriv_set_play_encoding(enc) == False)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Output format is not supported: %s", AENC_NAME(enc));
	return -1;
    }

    return 0;
}

/********** Audio_On **********************
 * Turn On Audio Stream.
 *
 *****/
int Audio_On(void)
{
    static int init_flag = 0;

    if(!init_flag)
    {
	int i;

	i = audio_init_open();
	if(i)
	    return i;
	init_flag = 1;
    }

    if(!audriv_play_open())
    {
	ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "%s", audriv_errmsg);
	ctl->close();
	exit(1);
    }
    flush_buckets();

#ifdef INITIAL_FILLING
    filling_flag = 1;
#endif /* INITIAL_FILLING */

#ifdef AUDIO_FILLING_MILSEC
    if(dpm.encoding & PE_MONO)
	/* Mono */
	max_audio_buffersize = (int32)(AUDIO_FILLING_MILSEC * (2.0/1000.0)
				       * dpm.rate);
    else
	/* Stereo */
	max_audio_buffersize = (int32)(AUDIO_FILLING_MILSEC * (4.0/1000.0)
				       * dpm.rate);
#else
    max_audio_buffersize = 0;
#endif /* AUDIO_FILLING_MILSEC */
    return 0;
}

/* Open the audio device */
static int open_output(void)
{
    int i;
    if((i = Audio_Init()) != 0)
	return i;
    if((i = Audio_On()) != 0)
	return i;
    dpm.fd = 0;
    return 0;
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


/* Output of audio data from timidity */
static void output_data(int32 *buf, int32 count)
{
    if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */

    if(dpm.encoding & PE_16BIT)
    {
	if(dpm.encoding & PE_BYTESWAP)
	{
	    if(dpm.encoding & PE_SIGNED)
		s32tos16x(buf, count);
	    else
		s32tou16x(buf, count);
	}
	else
	{
	    if(dpm.encoding & PE_SIGNED)
		s32tos16(buf, count);
	    else
		s32tou16(buf, count);
	}
	count *= 2; /* in bytes */
    }
    else
    {
	if(dpm.encoding & PE_ULAW)
	{
	    s32toulaw(buf, count);
	}
	else if(dpm.encoding & PE_ALAW)
	{
	    s32toalaw(buf, count);
	}
	else
	{
	    if(dpm.encoding & PE_SIGNED)
		s32tos8(buf, count);
	    else
		s32tou8(buf, count);
	}
    }

#ifdef INITIAL_FILLING
    if(audio_bucket_size < max_audio_buffersize && audriv_get_filled() == 0)
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

/* close output device */
static void close_output(void)
{
    if(dpm.fd == -1)
	return;
    while(trace_loop() && audriv_play_active())
	;
    trace_flush();
    flush_buckets();
    audriv_play_close();
    output_counter = 0;
    dpm.fd = -1;
}

static int flush_output(void)
{
    int rc;
#ifdef INITIAL_FILLING
    filling_flag = 0;
#endif /* INITIAL_FILLING */

    if(audio_bucket_size == 0 && output_counter == 0)
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
#ifdef HAVE_USLEEP
	    usleep(100000);
#endif /* HAVE_USLEEP */
	    rc = check_apply_control();
	    if(RC_IS_SKIP_FILE(rc))
	    {
		purge_output();
		return rc;
	    }
	}
    }

    while(audriv_play_active())	/* Loop until play out  */
    {
#ifdef HAVE_USLEEP
	usleep(100000);
#else
	audriv_wait_play();
#endif /* HAVE_USLEEP */
	rc = check_apply_control();
	if(RC_IS_SKIP_FILE(rc))
	{
	    purge_output();
	    return rc;
	}
    }
    purge_output();

    return RC_NONE;
}

static void purge_output(void)
{
    if(output_counter)
    {
	audriv_play_stop(); /* Reset audriv's sample counter */
	Audio_On();
	output_counter = 0;
    }
}

static int32 current_samples(void)
{
#ifdef INITIAL_FILLING
    if(filling_flag)
	return 0;
#endif /* INITIAL_FILLING */
    return audriv_play_samples();
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

    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	      "Audio Soft Buffer: %d", audio_bucket_size);

#ifdef INITIAL_FILLING
    if(filling_flag)
	return 0;
#endif /* INITIAL_FILLING */

    /* ctl->trace_playing can be changed in `N' interface */
    if(noblocking_flag != ctl->trace_playing)
    {
	noblocking_flag = ctl->trace_playing;
	audriv_set_noblock_write(noblocking_flag);
    }

    while(head)
    {
	if(head->pos < head->len)
	{
	    int n;

	    n = audriv_write(head->data + head->pos,
			     head->len - head->pos);
	    if(n == -1) /* error */
		return 0;
	    audio_bucket_size -= n;
	    head->pos += n;
	    output_counter += n;
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
