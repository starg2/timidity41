/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    aq.c - Audio queue.
	      Written by Masanao Izumo <mo@goice.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "aq.h"
#include "timer.h"
#include "controls.h"
#include "miditrace.h"
#include "instrum.h"
#include "playmidi.h"


#define TEST_SPARE_RATE 0.9
#define MAX_BUCKET_TIME 0.5
///r
#define MAX_FILLED_TIME 3000.0 // def 2.0


static int32 device_qsize;
///r
static int32 Bps = 1;	/* Bytes per sample frame */
static int32 bucket_size;
static int32 nbuckets = 0;
static double bucket_time;
int32 aq_fill_buffer_flag = 0;
static int32 aq_start_count;
static int32 aq_add_count;

static int32 play_counter, play_offset_counter;
static double play_start_time;

typedef struct _AudioBucket
{
    uint8 *data;
    int32 len;
    struct _AudioBucket *next;
} AudioBucket;

static AudioBucket *base_buckets = NULL;
static AudioBucket *allocated_bucket_list = NULL;
static AudioBucket *head = NULL;
static AudioBucket *tail = NULL;

static void alloc_soft_queue(void);
static void set_bucket_size(int32 size);
static int32 add_play_bucket(const uint8 *buf, int32 n);
static void reuse_audio_bucket(AudioBucket *bucket);
static AudioBucket *next_allocated_bucket(void);
static void flush_buckets(void);
static int32 aq_fill_one(void);
static void aq_wait_ticks(void);
static int32 estimate_queue_size(void);

/* effect.c */
extern void init_effect(void);
extern void do_effect(DATA_T *buf, int32 count);


///r
int aq_calc_fragsize(void)
{
    int bps, bs;
    double dq, bt;

	bps = get_encoding_sample_size(play_mode->encoding);
    bs = audio_buffer_size * bps;
    dq = play_mode->rate * MAX_FILLED_TIME * bps;
    while(bs * 2 > dq)
	bs /= 2;

    bt = (double)bs / bps * div_playmode_rate;
    while(bt > MAX_BUCKET_TIME)
    {
	bs /= 2;
	bt = (double)bs / bps * div_playmode_rate;
    }

    return bs;
}
///r
void aq_setup(void)
{
    int32 frag_size;

    /* Initialize Bps, bucket_size, device_qsize, and bucket_time */

    Bps = get_encoding_sample_size(play_mode->encoding);
    general_output_convert_setup();

    if (play_mode->acntl(PM_REQ_GETFRAGSIZ, &frag_size) == -1)
	frag_size = audio_buffer_size * Bps;
    set_bucket_size(frag_size);
    bucket_time = (double)bucket_size / Bps * div_playmode_rate;

    if (IS_STREAM_TRACE)
    {
	if (play_mode->acntl(PM_REQ_GETQSIZ, &device_qsize) == -1)
	    device_qsize = estimate_queue_size();
	if (bucket_size * 2 > device_qsize) {
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		    "Warning: Audio buffer is too small.");
	  device_qsize = 0;
	} else {
	  device_qsize -= device_qsize % Bps; /* Round Bps */
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		    "Audio device queue size: %d bytes", device_qsize);
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		    "Write bucket size: %d bytes (%d msec)",
		    bucket_size, (int)(bucket_time * 1000 + 0.5));
	}
    }
    else
    {
	device_qsize = 0;
	free_soft_queue();
	nbuckets = 0;
    }

    init_effect();
    aq_add_count = 0;
}

void aq_set_soft_queue(double soft_buff_time, double fill_start_time)
{
    static double last_soft_buff_time, last_fill_start_time;
    int nb;

    /* for re-initialize */
    if(soft_buff_time < 0)
	soft_buff_time = last_soft_buff_time;
    if(fill_start_time < 0)
	fill_start_time = last_fill_start_time;

    nb = (int)(soft_buff_time / bucket_time);
    if(nb == 0)
		aq_start_count = 0;
    else
		aq_start_count = (int32)(fill_start_time * play_mode->rate);
    aq_fill_buffer_flag = (aq_start_count > 0);

    if(nbuckets != nb)
    {
		nbuckets = nb;
		alloc_soft_queue();
    }

    last_soft_buff_time = soft_buff_time;
    last_fill_start_time = fill_start_time;
}

/* Estimates the size of audio device queue.
 * About sun audio, there are long-waiting if buffer of device audio is full.
 * So it is impossible to completely estimate the size.
 */
static int32 estimate_queue_size(void)
{
    uint8 *nullsound;
    double tb, init_time, chunktime;
    int32 qbytes, max_qbytes;
    int32 ntries;
	
#ifdef ALIGN_SIZE
    nullsound = (uint8*) aligned_malloc(bucket_size, ALIGN_SIZE);
#else
    nullsound = (uint8*) safe_malloc(bucket_size);
#endif
    memset(nullsound, 0, bucket_size);
    if (play_mode->encoding & (PE_ULAW | PE_ALAW))
	general_output_convert((DATA_T*)nullsound, bucket_size / Bps);
    tb = play_mode->rate * Bps * TEST_SPARE_RATE;
    ntries = 1;
    max_qbytes = play_mode->rate * MAX_FILLED_TIME * Bps;

  retry:
    chunktime = (double)bucket_size / Bps * div_playmode_rate;
    qbytes = 0;

    init_time = get_current_calender_time();	/* Start */
    for(;;)
    {
		double start, diff;

		start = get_current_calender_time();
		if(start - init_time > 1.0) /* ?? */
		{
			ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
				  "Warning: Audio test is terminated");
			break;
		}
		play_mode->output_data(nullsound, bucket_size);
		diff = get_current_calender_time() - start;
///r
		if(diff > chunktime * DIV_2 || chunktime < diff)
	//		if(qbytes > 1024*512) // ??
			break;
		qbytes += (int32)((chunktime - diff) * tb);

		if(qbytes > max_qbytes)
		{
			qbytes = max_qbytes;
			break;
		}
    }
    play_mode->acntl(PM_REQ_DISCARD, NULL);

    if(bucket_size * 2 > qbytes)
    {
		if(ntries == 4)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NOISY,
				  "Can't estimate audio queue length");
			set_bucket_size(audio_buffer_size * Bps);
			free(nullsound);
			return 2 * audio_buffer_size * Bps;
		}

		ctl->cmsg(CMSG_WARNING, VERB_DEBUG,
			  "Retry to estimate audio queue length (%d times)",
			  ntries);
		set_bucket_size(divi_2(bucket_size));
		ntries++;
		goto retry;
    }
	
#ifdef ALIGN_SIZE
    aligned_free(nullsound);
#else
    free(nullsound);
#endif

    return qbytes;
}

/* Send audio data to play_mode->output_data() */
static int aq_output_data(uint8 *buff, int nbytes)
{
    int i;

    play_counter += nbytes / Bps;

    while(nbytes > 0)
    {
	i = nbytes;
	if(i > bucket_size)
	    i = bucket_size;
	if(play_mode->output_data(buff, i) == -1)
	    return -1;
	nbytes -= i;
	buff += i;
    }

    return 0;
}

extern void do_effect2(uint8 *buf, int32 count); // effect.c

int aq_add(DATA_T *samples, int32 count)
{
    int32 nbytes, i;
    uint8 *buff;

	if(!(play_mode->flag & PF_PCM_STREAM))
	return 0;

    if(!count)
    {
		if(!aq_fill_buffer_flag)
			return aq_fill_nonblocking();
		return 0;
    }
	
	aq_add_count += count;
    nbytes = general_output_convert(samples, count);
    buff = (uint8 *)samples;
    do_effect2(buff, count);

    if(device_qsize == 0 || nbuckets == 0)
      return play_mode->output_data(buff, nbytes);

    aq_fill_buffer_flag = (aq_add_count <= aq_start_count);

    if(!aq_fill_buffer_flag && aq_fill_nonblocking() == -1)
	    return -1;

    if(!ctl->trace_playing)
    {
		while((i = add_play_bucket(buff, nbytes)) < nbytes)
		{
			buff += i;
			nbytes -= i;
			if(head && head->len == bucket_size)
			{
				if(aq_fill_one() == -1)
					return -1;
			}
			aq_fill_buffer_flag = 0;
		}
		return 0;
    }

    trace_loop();
    while((i = add_play_bucket(buff, nbytes)) < nbytes)
    {
	/* Software buffer is full.
	 * Write one bucket to audio device.
	 */
		buff += i;
		nbytes -= i;
		aq_wait_ticks();
		trace_loop();
		if(aq_fill_nonblocking() == -1)
			return -1;
		aq_fill_buffer_flag = 0;
    }
    return 0;
}

static void set_bucket_size(int32 size)
{
//    if (size == bucket_size)
//	return;
    bucket_size = size;
//    if (nbuckets != 0)
	alloc_soft_queue();
}

/* alloc_soft_queue() (re-)initializes audio buckets. */
static void alloc_soft_queue(void)
{
    int i;
    uint8 *base;
	int32 nb = nbuckets > 1 ? nbuckets : 1;

    free_soft_queue();
///r def safe_malloc(
    base_buckets = (AudioBucket*) safe_large_malloc(nb * sizeof(AudioBucket));
    base = (uint8*) safe_large_malloc(nb * bucket_size);
    base_buckets[0].len = 0;
    base_buckets[0].next = 0;
    for (i = 0; i < nb; i++)
	base_buckets[i].data = base + i * bucket_size;
    flush_buckets();
}

void free_soft_queue(void)
{
    if(base_buckets)
    {
	free(base_buckets[0].data);
	free(base_buckets);
	base_buckets = NULL;
    }
}

/* aq_fill_one() transfers one audio bucket to device. */
static int aq_fill_one(void)
{
    AudioBucket *tmp;
	
    if (!head)
	return 0;
    if(aq_output_data(head->data, bucket_size) == -1)
	return -1;
    tmp = head;
    head = head->next;
    reuse_audio_bucket(tmp);
    return 0;
}

/* aq_fill_nonblocking() transfers some audio buckets to device.
 * This function is non-blocking.  But it is possible to block because
 * of miss-estimated aq_fillable() calculation.
 */
int aq_fill_nonblocking(void)
{
    int32 i, nfills;
    AudioBucket *tmp;

    if(!head || head->len != bucket_size || !IS_STREAM_TRACE)
		return 0;

    nfills = (aq_fillable() * Bps) / bucket_size;
    for(i = 0; i < nfills; i++)
    {
		if(!head || head->len != bucket_size)
			break;
		if(aq_output_data(head->data, bucket_size) == -1)
			return RC_ERROR;
		tmp = head;
		head = head->next;
		reuse_audio_bucket(tmp);
    }
    return 0;
}

int32 aq_samples(void)
{
    double realtime, es;
    int s;

    if(play_mode->acntl(PM_REQ_GETSAMPLES, &s) != -1)
    {
	/* Reset counter & timer */
		if(play_counter)
		{
			play_start_time = get_current_calender_time();
			play_offset_counter = s;
			play_counter = 0;
		}
		return s;
    }

    if(!IS_STREAM_TRACE)
	return -1;

    realtime = get_current_calender_time();
    if(play_counter == 0)
    {
		play_start_time = realtime;
		return play_offset_counter;
    }
    es = play_mode->rate * (realtime - play_start_time);

    if(es >= play_counter)
    {
	/* Ouch!
	 * Audio device queue may be empty!
	 * Reset counters.
	 */
		play_offset_counter += play_counter;
		play_counter = 0;
		play_start_time = realtime;
		return play_offset_counter;
    }

    return (int32)es + play_offset_counter;
}

int32 aq_filled(void)
{
    double realtime, es;
    int filled;

    if(!IS_STREAM_TRACE)
	return 0;

    if(play_mode->acntl(PM_REQ_GETFILLED, &filled) != -1)
      return filled;

    realtime = get_current_calender_time();
    if(play_counter == 0)
    {
		play_start_time = realtime;
		return 0;
    }
    es = play_mode->rate * (realtime - play_start_time);
    if(es >= play_counter)
    {
		/* out of play counter */
		play_offset_counter += play_counter;
		play_counter = 0;
		play_start_time = realtime;
		return 0;
    }
    return play_counter - (int32)es;
}

int32 aq_soft_filled(void)
{
    int32 bytes;
    AudioBucket *cur;

    bytes = 0;
    for(cur = head; cur != NULL; cur = cur->next)
		bytes += cur->len;
    return bytes / Bps;
}

int32 aq_fillable(void)
{
    int fillable;
    if(!IS_STREAM_TRACE)
		return 0;
    if(play_mode->acntl(PM_REQ_GETFILLABLE, &fillable) != -1)
		return fillable;
    return device_qsize / Bps - aq_filled();
}

double aq_filled_ratio(void)
{
    double ratio;

    if(!IS_STREAM_TRACE)
		return 1.0;
    ratio = (double)aq_filled() * Bps / device_qsize;
    if(ratio > 1.0)
		return 1.0; /* for safety */
    return ratio;
}
///r
int aq_get_dev_queuesize(void)
{
    if(!IS_STREAM_TRACE)
		return 0;
    return device_qsize / Bps;
}

int aq_soft_flush(void)
{
    int rc;

    while(head)
    {
		if(head->len < bucket_size)
		{
			/* Add silence code */
			memset (head->data + head->len, 0, bucket_size - head->len);
			head->len = bucket_size;
		}
		if(aq_fill_one() == -1)
			return RC_ERROR;
		trace_loop();
		rc = check_apply_control();
		if(RC_IS_SKIP_FILE(rc))
		{
			play_mode->acntl(PM_REQ_DISCARD, NULL);
			flush_buckets();
			return rc;
		}
    }
    play_mode->acntl(PM_REQ_OUTPUT_FINISH, NULL);
    return RC_NONE;
}

int aq_flush(int discard)
{
    int rc;
    int more_trace;

    /* to avoid infinite loop */
    double t, timeout_expect;

    aq_add_count = 0;
    init_effect();

    if(discard)
    {
		trace_flush();
		if(play_mode->acntl(PM_REQ_DISCARD, NULL) != -1)
		{
			flush_buckets();
			return RC_NONE;
		}
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "ERROR: Can't discard audio buffer");
    }

    if(!IS_STREAM_TRACE)
    {
		play_mode->acntl(PM_REQ_FLUSH, NULL);
		play_counter = play_offset_counter = 0;
		return RC_NONE;
    }
	
    rc = aq_soft_flush();

    if(RC_IS_SKIP_FILE(rc)){
	return rc;
	}

    more_trace = 1;
    t = get_current_calender_time();
    timeout_expect = t + (double)aq_filled() * div_playmode_rate;

    while(more_trace || aq_filled() > 0)
    {
		rc = check_apply_control();
		if(RC_IS_SKIP_FILE(rc))
		{
			play_mode->acntl(PM_REQ_DISCARD, NULL);
			flush_buckets();
			return rc;
		}
		more_trace = trace_loop();

		t = get_current_calender_time();
		if(t >= timeout_expect - 0.1)
		  break;
///r
		if(!more_trace){
			const uint32 max_wait = 200 * 1000;
			uint32 wait = (timeout_expect - t) * 100000;
//			usleep((uint32)((double)aq_filled() * div_playmode_rate * 1000000.0)); // while aq_filled() > 0
//			usleep((unsigned long)(200000)); // ÇƒÇ´Ç∆Å[Ç»å≈íËíl 20ms
			usleep(wait < max_wait ? wait : max_wait);
		}else
		  aq_wait_ticks();
    }
    trace_flush();
    play_mode->acntl(PM_REQ_FLUSH, NULL);
    flush_buckets();
    return RC_NONE;
}

/* Wait a moment */
static void aq_wait_ticks(void)
{
    int32 trace_wait, wait_samples;
///r
#ifdef USE_TRACE_TIMER
	return;
#endif
	trace_wait = trace_wait_samples();
    if(device_qsize == 0 || trace_wait == 0)
		return; /* No wait */

    wait_samples = (device_qsize / Bps) * 0.2; // def 20%
    if(trace_wait != -1 && trace_wait < wait_samples) /* There are more trace events */
      	wait_samples = trace_wait;
    usleep((unsigned int)((double)wait_samples * div_playmode_rate * 1000000.0));
}

/* add_play_bucket() attempts to add buf to audio bucket.
 * It returns actually added bytes.
 */
static int32 add_play_bucket(const uint8 *buf, int n)
{
    int total;

    if(n == 0)
	return 0;

    if(!nbuckets) {
      play_mode->output_data((uint8 *)buf, n);
	  aq_output_data((uint8*)buf, n);
      return n;
    }

    if(head == NULL)
		head = tail = next_allocated_bucket();

    total = 0;
    while(n > 0)
    {
		int i;

		if(tail->len == bucket_size)
		{
			AudioBucket *b;
			if((b = next_allocated_bucket()) == NULL)
			break;
			if(head == NULL)
				head = tail = b;
			else
				tail = tail->next = b;
		}

		i = bucket_size - tail->len;
		if(i > n)
			i = n;
		memcpy(tail->data + tail->len, buf + total, i);
		total += i;
		n     -= i;
		tail->len += i;
    }

    return total;
}

/* Flush and clear audio bucket */
static void flush_buckets(void)
{
    int i;

    allocated_bucket_list = NULL;
    for(i = 0; i < nbuckets; i++)
	reuse_audio_bucket(&base_buckets[i]);
    head = tail = NULL;
    aq_fill_buffer_flag = (aq_start_count > 0);
    play_counter = play_offset_counter = 0;
}

/* next_allocated_bucket() gets free bucket.  If all buckets is used, it
 * returns NULL.
 */
static AudioBucket *next_allocated_bucket(void)
{
    AudioBucket *b;
	
    if (!allocated_bucket_list)
	return NULL;
    b = allocated_bucket_list;
    allocated_bucket_list = allocated_bucket_list->next;
    b->len = 0;
    b->next = NULL;
    return b;
}

/* Reuse specified bucket */
static void reuse_audio_bucket(AudioBucket *bucket)
{
    bucket->len = 0;
    bucket->next = allocated_bucket_list;
    allocated_bucket_list = bucket;
}

///r
// kobarin
void free_audio_bucket(void)
{
    free_soft_queue();
    nbuckets = 0;
}
