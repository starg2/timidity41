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
   
   w32_a.c
   
   Functions to play sound on the Windows audio driver (Windows 95/98/NT).
   
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

#if defined(__CYGWIN32__) || defined(__MINGW32__)
/* On cygnus, there is not mmsystem.h for Multimedia API's.
 * mmsystem.h can not distribute becase of Microsoft Lisence
 * Then declare some of them here.
 */

#define WOM_OPEN		0x3BB
#define WOM_CLOSE		0x3BC
#define WOM_DONE		0x3BD
#define WAVE_FORMAT_QUERY	0x0001
#define WAVE_ALLOWSYNC		0x0002
#define WAVE_FORMAT_PCM		1
#define CALLBACK_FUNCTION	0x00030000l
#define WAVERR_BASE		32
#define WAVE_MAPPER		(UINT)-1

DECLARE_HANDLE(HWAVEOUT);
DECLARE_HANDLE(HWAVE);
typedef HWAVEOUT *LPHWAVEOUT;

/* Define WAVEHDR, WAVEFORMAT structure */
typedef struct wavehdr_tag {
    LPSTR       lpData;
    DWORD       dwBufferLength;
    DWORD       dwBytesRecorded;
    DWORD       dwUser;
    DWORD       dwFlags;
    DWORD       dwLoops;
    struct wavehdr_tag *lpNext;
    DWORD       reserved;
} WAVEHDR;

typedef struct {
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
    WORD    wBitsPerSample;
    WORD    cbSize;
} WAVEFORMAT;

typedef struct waveoutcaps_tag {
    WORD    wMid;
    WORD    wPid;
    UINT    vDriverVersion;
#define MAXPNAMELEN      32
    char    szPname[MAXPNAMELEN];
    DWORD   dwFormats;
    WORD    wChannels;
    DWORD   dwSupport;
} WAVEOUTCAPS;

typedef WAVEHDR *LPWAVEHDR;
typedef WAVEFORMAT *LPWAVEFORMAT;
typedef WAVEOUTCAPS *LPWAVEOUTCAPS;
typedef UINT MMRESULT;

MMRESULT WINAPI waveOutOpen(LPHWAVEOUT, UINT,
			    LPWAVEFORMAT, DWORD, DWORD, DWORD);
MMRESULT WINAPI waveOutClose(HWAVEOUT);
MMRESULT WINAPI waveOutPrepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT WINAPI waveOutUnprepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT WINAPI waveOutWrite(HWAVEOUT, LPWAVEHDR, UINT);
UINT WINAPI waveOutGetNumDevs(void);

MMRESULT WINAPI waveOutReset(HWAVEOUT);
MMRESULT WINAPI waveOutGetDevCaps(UINT, LPWAVEOUTCAPS, UINT);
MMRESULT WINAPI waveOutGetDevCapsA(UINT, LPWAVEOUTCAPS, UINT);
#define waveOutGetDevCaps waveOutGetDevCapsA
MMRESULT WINAPI waveOutGetID(HWAVEOUT, UINT*);

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
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);

#define DATA_BLOCK_SIZE (4*AUDIO_BUFFER_SIZE)
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
static volatile int nBlocks;
static const char *mmerror_code_string(MMRESULT res);
static int msec_per_block;

/* export the playback mode */

#define dpm w32_play_mode

PlayMode dpm = {
    33075,
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_CAN_TRACE|PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "Windows audio driver", 'd',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

extern CRITICAL_SECTION critSect;
/* Optional flag */
static int w32_wave_allowsync = 1; /* waveOutOpen() fdwOpen : WAVE_ALLOWSYNC */
//static int w32_wave_allowsync = 0;

static void wait_playing(int all)
{
    if(all)
    {
	while(nBlocks)
	    Sleep(msec_per_block);
	reset_data_block();
    }
    else
    {
#ifndef IA_W32GUI
	if(ctl->trace_playing)
	    Sleep(0);
	else
#endif /* IA_W32GUI */
	    Sleep(msec_per_block);
    }
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
    WAVEFORMAT wf;
    WAVEOUTCAPS caps;
    MMRESULT res;
    UINT devid;

    if(dpm.extra_param[0] < 8)
    {
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d",
		  dpm.extra_param[0]);
	dpm.extra_param[0] = 8;
    }

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

    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = mono ? 1 : 2;
    wf.nSamplesPerSec = i = dpm.rate;
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
    wf.nAvgBytesPerSec = i;
    wf.nBlockAlign = j;
    wf.wBitsPerSample = eight_bit ? 8 : 16;
    wf.cbSize=sizeof(WAVEFORMAT);

    dev = 0;
    if (w32_wave_allowsync)
	res = waveOutOpen (&dev, WAVE_MAPPER, (LPWAVEFORMAT)&wf,
			   (DWORD)wave_callback, 0,
			   CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
    else
	res = waveOutOpen (&dev, WAVE_MAPPER, (LPWAVEFORMAT)&wf,
			   (DWORD)wave_callback, 0, CALLBACK_FUNCTION);
    if (res)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Can't open audio device: "
		  "encoding=<%s>, rate=<%d>, ch=<%d>: %s",
		  output_encoding_string(dpm.encoding),
		  dpm.rate,
		  wf.nChannels,
		  mmerror_code_string(res));
	return -1;
    }

    devid = 0;
    memset(&caps, 0, sizeof(WAVEOUTCAPS));
    waveOutGetID(dev, &devid);
    res = waveOutGetDevCaps(devid, &caps, sizeof(WAVEOUTCAPS));
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Play Device ID: %d", devid);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Manufacture ID: %d");
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Product ID: %d", caps.wPid);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Version of the driver: %d", caps.vDriverVersion);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Product name: %s", caps.szPname);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Formats supported: 0x%x", caps.dwFormats);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "number of sources supported: %d", caps.wChannels);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "functionality supported by driver: 0x%x", caps.dwSupport);

    /* Prepere audio queue buffer */
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

    /* calc. msec/block */
    msec_per_block = AUDIO_BUFFER_SIZE;
    if(!(dpm.encoding & PE_MONO))
	msec_per_block *= 2;
    if(dpm.encoding & PE_16BIT)
	msec_per_block *= 2;
    msec_per_block = msec_per_block * 1000 / dpm.rate;

    return warnings;
}

static int output_data(char *buf, int32 nbytes)
{
    int32 len = nbytes, n;
    HGLOBAL data_hg;
    HGLOBAL head_hg;
    void *b;
    struct data_block_t *block;
    int32 total;
    char *bp;
    MMRESULT res;
    LPWAVEHDR wh;

    total = 0;
    bp = (char *)buf;
    while(len > 0)
    {
	if((block = new_data_block()) == NULL)
	{
	    wait_playing(0);
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
	    return -1;
	}
	res = waveOutWrite(dev, wh, sizeof(WAVEHDR));
	if(res)
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "waveOutWrite(): %s",
		      mmerror_code_string(res));
	    return -1;
	}

	len -= n;
	bp += n;
    }
    return 0;
}

static void close_output(void)
{
    int i;

    if(dpm.fd == -1)
	return;
    wait_playing(1);
    waveOutClose(dev);

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

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_GETQSIZ:
	*(int *)arg = DATA_BLOCK_NUM * AUDIO_BUFFER_SIZE;
	if(!(dpm.encoding & PE_MONO))
	    *(int *)arg *= 2;
	if(dpm.encoding & PE_16BIT)
	    *(int *)arg *= 2;
	return 0;
      case PM_REQ_DISCARD:
	waveOutReset(dev);
	wait_playing(1);
	return 0;
      case PM_REQ_FLUSH:
	wait_playing(1);
	return 0;
    }
    return -1;
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
