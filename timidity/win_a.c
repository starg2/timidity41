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

/* Max audio blocks waiting to be played */

static LPHWAVEOUT dev;
static int nBlocks;

extern CRITICAL_SECTION critSect;
/* Optional flag */
int win32_wave_allowsync = 1; /* waveOutOpen() fdwOpen : WAVE_ALLOWSYNC */

static void wait (void)
	{
	while (nBlocks)
		Sleep (0);
	}

static int play (void *mem, int len)
	{
	HGLOBAL hg;
	LPWAVEHDR wh;
	MMRESULT res;

	while (nBlocks >= dpm.extra_param[0])
		Sleep (0);

	hg = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof (WAVEHDR));
	if (!hg)
		{
		ctl->cmsg (CMSG_INFO, VERB_NORMAL, "GlobalAlloc failed!");
		return FALSE;
		}
	wh = GlobalLock (hg);
	wh->dwBufferLength = len;
	wh->lpData = mem;

	res = waveOutPrepareHeader (dev, wh, sizeof (WAVEHDR));
	if (res)
		{
		ctl->cmsg (CMSG_INFO, VERB_NORMAL, "waveOutPrepareHeader: %d", res);
		GlobalUnlock (hg);
		GlobalFree (hg);
		return TRUE;
		}
	res = waveOutWrite (dev, wh, sizeof (WAVEHDR));
	if (res)
		{
		ctl->cmsg (CMSG_INFO, VERB_NORMAL, "waveOutWrite: %d", res);
		GlobalUnlock (hg);
		GlobalFree (hg);
		return TRUE;
		}
	EnterCriticalSection (&critSect);
	nBlocks++;
	LeaveCriticalSection (&critSect);
//	cmsg (CMSG_INFO,VERB_NOISY, "Play: %d blocks", nBlocks);
	return FALSE;
	}

#pragma argsused
static void CALLBACK wave_callback (HWAVE hWave, UINT uMsg,
		DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
	{
	WAVEHDR *wh;
	HGLOBAL hg;

	if (uMsg == WOM_DONE)
		{
		EnterCriticalSection (&critSect);
		wh = (WAVEHDR *)dwParam1;
		waveOutUnprepareHeader (dev, wh, sizeof (WAVEHDR));
		hg = GlobalHandle (wh->lpData);
		GlobalUnlock (hg);
		GlobalFree (hg);
		hg = GlobalHandle (wh);
		GlobalUnlock (hg);
		GlobalFree (hg);
		nBlocks--;
		LeaveCriticalSection (&critSect);
//		cmsg (CMSG_INFO, VERB_NOISY, "Callback: %d blocks", nBlocks);
		}
	}

static int open_output (void)
	{
	int i, j, mono, eight_bit, warnings = 0;
	PCMWAVEFORMAT pcm;
	MMRESULT res;

	play_counter = reset_samples = 0;

	/* Check if there is at least one audio device */
	if (!waveOutGetNumDevs ())
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

	if (win32_wave_allowsync)
	  res = waveOutOpen (NULL, 0, (LPWAVEFORMAT)&pcm, NULL, 0, WAVE_FORMAT_QUERY | WAVE_ALLOWSYNC);
	else
	  res = waveOutOpen (NULL, 0, (LPWAVEFORMAT)&pcm, NULL, 0, WAVE_FORMAT_QUERY);
	if (res)
		{
		ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "Format not supported!");
		return -1;
		}
	if (win32_wave_allowsync)
	  res = waveOutOpen (&dev, 0, (LPWAVEFORMAT)&pcm, (DWORD)wave_callback, 0, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
	else
	  res = waveOutOpen (&dev, 0, (LPWAVEFORMAT)&pcm, (DWORD)wave_callback, 0, CALLBACK_FUNCTION);
	if (res)
		{
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't open audio device");
		return -1;
		}
	nBlocks = 0;
	dpm.fd = 0;
	return warnings;
	}

static void add_sample_counter(int32 count)
{
    current_samples(); /* update play_counter */
    play_counter += count;
}

static void output_data (int32 *buf, int32 count)
	{
	int len = count;
	HGLOBAL hg;
	void *b;
	int32 count_arg = count;
	if (!(dpm.encoding & PE_MONO)) /* Stereo sample */
		{
		count *= 2;
		len *= 2;
		}

	if (dpm.encoding & PE_16BIT)
		len *= 2;

	hg = GlobalAlloc (GMEM_MOVEABLE, len);
	if (!hg)
		{
		ctl->cmsg (CMSG_INFO, VERB_NORMAL, "GlobalAlloc failed!");
		return;
		}
	b = GlobalLock (hg);

	if (dpm.encoding & PE_16BIT)
		/* Convert data to signed 16-bit PCM */
		s32tos16 (buf, count);
	else
		/* Convert to 8-bit unsigned. */
		s32tou8 (buf, count);

	CopyMemory(b, buf, len);
	add_sample_counter(count_arg);
	if (play (b, len))
		{
		GlobalUnlock (hg);
		GlobalFree (hg);
		}
	}

static void close_output (void)
	{
	int rc;

	if(dpm.fd == -1)
	    return;
	wait ();
	waveOutClose (dev);
	play_counter = reset_samples = 0;
	dpm.fd = -1;
	}

static int flush_output (void)
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

	wait ();
	play_counter = reset_samples = 0;
	return RC_NONE;
	}

static void purge_output (void)
	{
	if(play_counter == 0 && reset_samples == 0)
	    return; /* Ignore */
	waveOutReset (dev);
	wait ();
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
	    return 0; /* for safety */
	return (int32)es + reset_samples;
	}
