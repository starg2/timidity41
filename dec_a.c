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

    dec_audio.c

    Functions to play sound on DEC OSF/1 with MME support

    Ported by Chi Ming HUNG <cmhung@insti.physics.sunysb.edu>

    Note: This was tested on a DEC alpha 250 4/266.  At the default sampling
    rate of 32kHz an occasional break in playing could be detected under
    CPU load of around 3.0  Try using lower sampling rate or using some of
    the hacks in config.h if you have a slow and/or busy machine.

    Oh timidity works great as a MIDI-"viewer" for WWW browsers.  Just add

      audio/midi; timidity1 %s >/dev/null 2>&1

    to your .mailcap file and you can click and play MIDI files on the net!
    Try e.g. the Classical MIDI Archive:

      http://www.prs.net/midi.html

    Thanks Tuukka for a great program!!
*/

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h> 

#include <mme/mme_api.h>

#include "config.h"
#include "output.h"
#include "controls.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static void flush_output(void);
static void purge_output(void);

/* export the playback mode */

#define dpm dec_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED,
  -1,
  {0,0,0,0,0}, /* no extra parameters so far */
  "DEC audio device", 'd',
  "",
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output  
};

/* Global variables */

static HWAVEOUT		mms_device_handle = 0;
static UINT		mms_device_id = 0;
static LPWAVEHDR	mms_lpWaveHeader;


/********** MMS_Audio_Init **********************
  This just does some generic initialization.  The actual audio
  output device open is done in the Audio_On routine so that we
  can get a device which matches the requested sample rate & format
 *****/

void MMS_Audio_Init()
{
  MMRESULT	status;
  LPWAVEOUTCAPS	lpCaps;


  if( waveOutGetNumDevs() < 1 ) 
  {
    fprintf(stderr,"Audio disabled - No Multimedia Services compatible audio devices available\n");
    return;
  }

  /* Figure out device capabilities  - Just use device 0 for now */

  if((lpCaps = (LPWAVEOUTCAPS)mmeAllocMem(sizeof(WAVEOUTCAPS))) == NULL ) {
      fprintf(stderr,"Failed to allocate WAVEOUTCAPS struct\n");
      return;
  }
  status = waveOutGetDevCaps( 0, lpCaps, sizeof(WAVEOUTCAPS));
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutGetDevCaps failed - status = %d\n", status);
  }

  mmeFreeMem(lpCaps);

}


/*********  MMS_wave_callback  ****************
 * This is only used so that we know the audio device has finished
 * playing one block of data and is ready to accept the next
 **********/

static void MMS_wave_callback(hWaveOut,wMsg,dwInstance,lParam1,lParam2)
HANDLE hWaveOut;
UINT wMsg;
DWORD dwInstance;
LPARAM lParam1;
LPARAM lParam2;
{

   switch(wMsg)
     {
     case WOM_OPEN:
     case WOM_CLOSE: {
       break;
     }
     case WOM_DONE: {
       break;
     }
     default: {
       fprintf(stderr,
	       "Unknown MMS waveOut callback messages receieved.\n");
       break;
     }
     }
}



/********** MMS_Audio_On **********************
 * Turn On Audio Stream.
 *
 *****/

void MMS_Audio_On()
{
  MMRESULT status;
  LPPCMWAVEFORMAT lpWaveFormat;

  /* Setting up the parameters for audio device */
  if((lpWaveFormat = (LPPCMWAVEFORMAT)
      mmeAllocMem(sizeof(PCMWAVEFORMAT))) == NULL ) 
    {
      fprintf(stderr,"Failed to allocate PCMWAVEFORMAT struct\n");
      return;
    }
    lpWaveFormat->wf.nSamplesPerSec = dpm.rate;
    lpWaveFormat->wf.nChannels = 2;
    lpWaveFormat->wBitsPerSample = 16;
    lpWaveFormat->wf.wFormatTag = WAVE_FORMAT_PCM; 
    /*    lpWaveFormat->wf.nBlockAlign = lpWaveFormat->wf.nChannels *
     * ((lpWaveFormat->wBitsPerSample+7)/8) ;   
     *lpWaveFormat->wf.nAvgBytesPerSec = lpWaveFormat->wf.nBlockAlign *
     * lpWaveFormat->wf.nSamplesPerSec ; */
    
    /* Open the audio device in the appropriate rate/format */
    mms_device_handle = 0;
    status = waveOutOpen( &mms_device_handle,
			  WAVE_MAPPER,
			  (LPWAVEFORMAT)lpWaveFormat,
			  MMS_wave_callback,
			  NULL,
			  WAVE_ALLOWSYNC | CALLBACK_FUNCTION  );

    if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutOpen failed - status = %d\n", status);
    }

    fprintf(stderr,"Opened waveOut device #%d \n",
	    mms_device_id);
    fprintf(stderr,
	    "Format=%d, Rate=%d, channels=%d, bps=%d \n",
	    lpWaveFormat->wf.wFormatTag,
	    lpWaveFormat->wf.nSamplesPerSec,
	    lpWaveFormat->wf.nChannels,
	    lpWaveFormat->wBitsPerSample );
    
    mmeFreeMem(lpWaveFormat);

    /* Allocate wave header for use in write */
    if((mms_lpWaveHeader = (LPWAVEHDR)
	mmeAllocMem(sizeof(WAVEHDR))) == NULL ) 
      {
	fprintf(stderr,"Failed to allocate WAVEHDR struct\n");
	return;
      }

    /* Allocate shared audio buffer for communicating with audio device */
    if ( (mms_lpWaveHeader->lpData = (LPSTR)
	  mmeAllocMem(1024*8 )) == NULL)
      {
	fprintf(stderr,"Failed to allocate shared audio buffer\n");
	return;
      }

}



/* Open the audio device */

static int open_output(void)
{
  int warnings=0;

  MMS_Audio_Init();
  MMS_Audio_On();
  
  return warnings;
}



/* Output of audio data from timidity */

static void output_data(int32 *buf, int32 count)
{

  MMRESULT	status;

  if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */

  /* Convert data to signed 16-bit PCM */
  s32tos16l(buf, count);

  /* write output data to audio device */
  mms_lpWaveHeader->dwBufferLength = 2*count;
  memcpy( mms_lpWaveHeader->lpData, buf, 2*count);
  status = waveOutWrite(mms_device_handle, mms_lpWaveHeader,
			sizeof(WAVEHDR));
  if( status != MMSYSERR_NOERROR ) 
    {
      fprintf(stderr,"waveOutWrite failed - status = %d\n",status);
    }

  /* Wait for callback from audio device before continuing */
  mmeWaitForCallbacks();
  mmeProcessCallbacks();

}


/* close output device */
static void close_output(void)
{
  MMRESULT status;

  status = waveOutReset(mms_device_handle);
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutReset failed - status = %d\n", status);
  }
  status = waveOutClose(mms_device_handle);
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutClose failed - status = %d\n", status);
  }

  mmeFreeMem(mms_lpWaveHeader);

}


/* not sure what to do here */
static void flush_output(void) {}

static void purge_output(void)
{
  MMRESULT status;
 
  status = waveOutReset(mms_device_handle);
  if( status != MMSYSERR_NOERROR ) {
    fprintf(stderr,"waveOutReset failed - status = %d\n", status);
 }
} 
