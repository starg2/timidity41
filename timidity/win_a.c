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
   
   win_audio.c
   
   Functions to play sound on the Win32 audio driver (Win 95 or Win NT).
   
   Modified by Masanao Izumo <mo@goice.co.jp>
   
   */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <windows.h>

#ifdef __CYGWIN32__
#define HAVE_WAVEFORMAT_CBSIZE
/* On cygnus, there is no header file for Multimedia API's. */
/* Then declare some of them here. */

#define NEAR
#define FAR
#define WOM_OPEN		0x3BB
#define WOM_CLOSE		0x3BC
#define WOM_DONE		0x3BD
#define WAVE_FORMAT_QUERY	0x0001
#define WAVE_ALLOWSYNC		0x0002
#define WAVE_FORMAT_PCM		1
#define CALLBACK_FUNCTION	0x00030000l
#define WAVERR_BASE		32

DECLARE_HANDLE(HWAVEOUT);
DECLARE_HANDLE(HWAVE);
typedef HWAVEOUT *LPHWAVEOUT;

/* Define WAVEHDR, WAVEFORMAT, PCMWAVEFORMAT structure */
typedef struct wavehdr_tag {
    LPSTR       lpData;
    DWORD       dwBufferLength;
    DWORD       dwBytesRecorded;
    DWORD       dwUser;
    DWORD       dwFlags;
    DWORD       dwLoops;
    struct wavehdr_tag FAR *lpNext;
    DWORD       reserved;
} WAVEHDR;

typedef struct {
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
#ifdef HAVE_WAVEFORMAT_CBSIZE
    WORD    cbSize;
#endif
} WAVEFORMAT;

typedef struct {
    WAVEFORMAT  wf;
    WORD        wBitsPerSample;
} PCMWAVEFORMAT;

typedef WAVEHDR *LPWAVEHDR;
typedef WAVEFORMAT *LPWAVEFORMAT;
typedef UINT MMRESULT;

MMRESULT WINAPI waveOutOpen(LPHWAVEOUT, UINT,
			    LPWAVEFORMAT, DWORD, DWORD, DWORD);
MMRESULT WINAPI waveOutClose(HWAVEOUT);
MMRESULT WINAPI waveOutPrepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT WINAPI waveOutUnprepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT WINAPI waveOutWrite(HWAVEOUT, LPWAVEHDR, UINT);
UINT WINAPI waveOutGetNumDevs(void);
MMRESULT WINAPI waveOutReset(HWAVEOUT);

#endif /* __CYGWIN32__ */


#include "timidity.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "mblock.h"
#include "miditrace.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);
static int32 play_counter, reset_samples;
static double play_start_time;

extern int default_play_event(void *);

/* #define DATA_BLOCK_SIZE (1024*2)  for small shared memory implementation */
#define DATA_BLOCK_SIZE (1024*16)
#define DATA_BLOCK_NUM  (dpm.extra_param[0])
#define DATA_MIN_NBLOCKS (DATA_BLOCK_NUM-1)

struct data_block_t
{
    HGLOBAL data_hg;
    HGLOBAL head_hg;
    void *data;
    WAVEHDR *head;
    int blockno;
    struct data_block_t *next;
};

static struct data_block_t *all_data_block;
static struct data_block_t *free_data_block;
static void reuse_data_block(struct data_block_t *);
static void reset_data_block(void);
static struct data_block_t *new_data_block();

static HWAVEOUT dev;
static int nBlocks;
static const char *mmerror_code_string(MMRESULT res);

/* export the playback mode */

#define dpm win32_play_mode

PlayMode dpm = {
    DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
    -1,
    {16},
    "Win32 audio driver", 'd',
    NULL,
    default_play_event,
    open_output,
    close_output,
    output_data,
    flush_output,
    purge_output,
    current_samples,
    play_loop
};

extern CRITICAL_SECTION critSect;
/* Optional flag */
static int win32_wave_allowsync = 1; /* waveOutOpen() fdwOpen : WAVE_ALLOWSYNC */
//static int win32_wave_allowsync = 0;

static void wait(void)
{
    while(nBlocks)
	Sleep(0);
}

static void CALLBACK wave_callback(HWAVE hWave, UINT uMsg,
				   DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "MMCallback: Msg=0x%x, nBlocks=%d", uMsg, nBlocks);

    if(uMsg == WOM_DONE)
    {
	WAVEHDR *wh;

	EnterCriticalSection (&critSect);
	wh = (WAVEHDR *)dwParam1;
	waveOutUnprepareHeader(dev, wh, sizeof(WAVEHDR));
	reuse_data_block(&all_data_block[wh->dwUser]);
	LeaveCriticalSection (&critSect);
    }
}

static int open_output(void)
{
    int i, j, mono, eight_bit, warnings = 0;
    PCMWAVEFORMAT pcm;
    MMRESULT res;

    play_counter = reset_samples = 0;

    /* Check if there is at least one audio device */
    if (!(i=waveOutGetNumDevs ()))
    {
	ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "No audio devices present!");
	return -1;
    }

    /* They can't mean these */
    dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);

    if (dpm.encoding & PE_16BIT)
	dpm.encoding |= PE_SIGNED;
    else
	dpm.encoding &= ~PE_SIGNED;

    mono = (dpm.encoding & PE_MONO);
    eight_bit = !(dpm.encoding & PE_16BIT);

    memset(&pcm, 0, sizeof(pcm));
    pcm.wf.wFormatTag = WAVE_FORMAT_PCM;
    pcm.wf.nChannels = mono ? 1 : 2;
    pcm.wf.nSamplesPerSec = i = dpm.rate;
    j = 1;
    if (!mono)
    {
	i *= 2;
	j *= 2;
    }
    if (!eight_bit)
    {
	i *= 2;
	j *= 2;
    }
    pcm.wf.nAvgBytesPerSec = i;
    pcm.wf.nBlockAlign = j;
    pcm.wBitsPerSample = eight_bit ? 8 : 16;
#ifdef HAVE_WAVEFORMAT_CBSIZE
    pcm.wf.cbSize=sizeof(WAVEFORMAT);
#endif

    dev = 0;
    if (win32_wave_allowsync)
	res = waveOutOpen (&dev, 0, (LPWAVEFORMAT)&pcm, (DWORD)wave_callback, 0, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
    else
	res = waveOutOpen (&dev, 0, (LPWAVEFORMAT)&pcm, (DWORD)wave_callback, 0, CALLBACK_FUNCTION);
    if (res)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Can't open audio device: encoding=<%s>, rate=<%d>: %s",
		  output_encoding_string(dpm.encoding),
		  dpm.rate,
		  mmerror_code_string(res));
	return -1;
    }

    all_data_block = (struct data_block_t *)
	safe_malloc(DATA_BLOCK_NUM * sizeof(struct data_block_t));
    for(i = 0; i < DATA_BLOCK_NUM; i++)
    {
	struct data_block_t *block;
	block = &all_data_block[i];
	block->data_hg = GlobalAlloc(GMEM_ZEROINIT, DATA_BLOCK_SIZE);
	block->data = GlobalLock(block->data_hg);
	block->head_hg = GlobalAlloc(GMEM_ZEROINIT, sizeof(WAVEHDR));
	block->head = GlobalLock(block->head_hg);
    }
    reset_data_block();
    dpm.fd = 0;
    return warnings;
}

static void add_sample_counter(int32 count)
{
    current_samples();		/* update play_counter */
    play_counter += count;
}

static void output_data (int32 *buf, int32 count)
{
    int32 len = count, n;
    HGLOBAL data_hg;
    HGLOBAL head_hg;
    void *b;
    int32 count_arg = count;
    struct data_block_t *block;
    int32 total;
    char *bp;
    MMRESULT res;
    LPWAVEHDR wh;

    if(!(dpm.encoding & PE_MONO)) /* Stereo sample */
    {
	count *= 2;
	len *= 2;
    }

    if(dpm.encoding & PE_16BIT)
	len *= 2;

    if (dpm.encoding & PE_16BIT)
	/* Convert data to signed 16-bit PCM */
	s32tos16 (buf, count);
    else
	/* Convert to 8-bit unsigned. */
	s32tou8 (buf, count);

    total = 0;
    bp = (char *)buf;
    add_sample_counter(count_arg);
    while(len > 0)
    {
	if((block = new_data_block()) == NULL)
	{
	    Sleep(0);
	    continue;
	}
	if(len <= DATA_BLOCK_SIZE)
	    n = len;
	else
	    n = DATA_BLOCK_SIZE;
	data_hg = block->data_hg;
	head_hg = block->head_hg;

	b = block->data;
	CopyMemory(b, bp, n);

	wh = block->head;
	wh->dwBufferLength = n;
	wh->lpData = b;
	wh->dwUser = block->blockno;

	res = waveOutPrepareHeader(dev, wh, sizeof(WAVEHDR));
	if(res)
	{
	    ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "waveOutPrepareHeader(): %s",
		       mmerror_code_string(res));
	    safe_exit(1);
	}
	res = waveOutWrite(dev, wh, sizeof(WAVEHDR));
	if(res)
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "waveOutWrite(): %s",
		      mmerror_code_string(res));
	    safe_exit(1);
	}

	len -= n;
	bp += n;
    }
}

static void close_output(void)
{
    int i;

    if(dpm.fd == -1)
	return;
    wait();
    waveOutClose(dev);
    play_counter = reset_samples = 0;

    for(i = 0; i < DATA_BLOCK_NUM; i++)
    {
	struct data_block_t *block;
	block = &all_data_block[i];
	GlobalUnlock(block->head_hg);
	GlobalFree(block->head_hg);
	GlobalUnlock(block->data_hg);
	GlobalFree(block->data_hg);
    }
    free(all_data_block);

    dpm.fd = -1;
}

static int flush_output(void)
{
    int rc;

    if(play_counter == 0 && reset_samples == 0)
	return RC_NONE;

    /* extract all trace */
    while(trace_loop())
    {
	rc = check_apply_control();
	if(RC_IS_SKIP_FILE(rc))
	{
	    purge_output();
	    return rc;
	}
	Sleep(0);
    }

    /* wait until play out */
    do
    {
	rc = check_apply_control();
	if(RC_IS_SKIP_FILE(rc))
	{
	    purge_output();
	    return rc;
	}
	current_samples();
	Sleep(0);
    } while(play_counter > 0);

    wait();
    reset_data_block();
    play_counter = reset_samples = 0;
    return RC_NONE;
}

static void purge_output(void)
{
    if(play_counter == 0 && reset_samples == 0)
	return;			/* Ignore */
    waveOutReset(dev);
    wait();
    reset_data_block();
    play_counter = reset_samples = 0;
}


static int play_loop(void)
{
    return 0;
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
	return 0;		/* for safety */
    return (int32)es + reset_samples;
}


#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])
static const char *mmsyserr_code_string[] =
{
    "no error",
    "unspecified error",
    "device ID out of range",
    "driver failed enable",
    "device already allocated",
    "device handle is invalid",
    "no device driver present",
    "memory allocation error",
    "function isn't supported",
    "error value out of range",
    "invalid flag passed",
    "invalid parameter passed",
    "handle being used",
};

static const char *waverr_code_sring[] =
{
    "unsupported wave format",
    "still something playing",
    "header not prepared",
    "device is synchronous",
};

static const char *mmerror_code_string(MMRESULT err_code)
{
    static char s[32];

    if(err_code >= WAVERR_BASE)
    {
	err_code -= WAVERR_BASE;
	if(err_code > ARRAY_SIZE(waverr_code_sring))
	{
	    sprintf(s, "WAVERR %d", err_code);
	    return s;
	}
	return waverr_code_sring[err_code];
    }
    if(err_code > ARRAY_SIZE(mmsyserr_code_string))
    {
	sprintf(s, "MMSYSERR %d", err_code);
	return s;
    }
    return mmsyserr_code_string[err_code];
}


static struct data_block_t *new_data_block()
{
    struct data_block_t *p;

    p = NULL;
    EnterCriticalSection (&critSect);
    if(free_data_block != NULL)
    {
	p = free_data_block;
	free_data_block = free_data_block->next;
	nBlocks++;
	p->next = NULL;
    }
    LeaveCriticalSection (&critSect);

    return p;
}

static void reuse_data_block(struct data_block_t *block)
{
    block->next = free_data_block;
    free_data_block = block;
    nBlocks--;
}

static void reset_data_block(void)
{
    int i;

    all_data_block[0].blockno = 0;
    all_data_block[0].next = &all_data_block[1];
    for(i = 1; i < DATA_BLOCK_NUM - 1; i++)
    {
	all_data_block[i].blockno = i;
	all_data_block[i].next = &all_data_block[i + 1];
    }
    all_data_block[i].blockno = i;
    all_data_block[i].next = NULL;
    free_data_block = &all_data_block[0];
    nBlocks = 0;
}
