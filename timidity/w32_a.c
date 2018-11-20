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

    w32_a.c

    Functions to play sound on the Windows audio driver (Windows 95/98/NT).

    Modified by Masanao Izumo <mo@goice.co.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */


#include "timidity.h"

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

///r
#include "w32_a.h"
#include "common.h"
#include "miditrace.h"

int opt_wmme_device_id = -1;
int opt_wave_format_ext = 1;
int opt_wmme_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;
int opt_wmme_buffer_num = 64;

/*****************************************************************************************************************************/

static int  open_output     (void); /* 0=success, 1=warning, -1=fatal error */
static void close_output    (void);
static int  output_data     (const uint8 * Data, size_t Size);
static int  acntl           (int request, void * arg);
static int  detect          (void);

/*****************************************************************************************************************************/

#define dpm w32_play_mode

PlayMode dpm =
{
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM|PF_CAN_TRACE|PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "Windows audio driver", 'd',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl,
	detect
};

/*****************************************************************************************************************************/

///r
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */ /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;

typedef WAVEFORMATEXTENSIBLE *PWAVEFORMATEXTENSIBLE, *LPWAVEFORMATEXTENSIBLE;

#define  WAVE_FORMAT_UNKNOWN      0x0000
#define  WAVE_FORMAT_PCM          0x0001
#define  WAVE_FORMAT_ADPCM        0x0002
#define  WAVE_FORMAT_IEEE_FLOAT   0x0003
#define  WAVE_FORMAT_ALAW         0x0006
#define  WAVE_FORMAT_MULAW        0x0007
#define  WAVE_FORMAT_EXTENSIBLE   0xFFFE

#define SPEAKER_FRONT_LEFT        0x1
#define SPEAKER_FRONT_RIGHT       0x2
#define SPEAKER_FRONT_CENTER      0x4
#define SPEAKER_MONO	          (SPEAKER_FRONT_CENTER)
#define SPEAKER_STEREO	          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)

/*****************************************************************************************************************************/

#define DIM(a) sizeof(a) / sizeof(a[0])

static const char * mmsyserr_code_string[] =
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

static const char * waverr_code_sring[] =
{
    "unsupported wave format",
    "still something playing",
    "header not prepared",
    "device is synchronous",
};

static const char * MMErrorMessage(MMRESULT ErrorCode)
{
    static char s[32];

    if (ErrorCode >= WAVERR_BASE)
    {
        ErrorCode -= WAVERR_BASE;

        if (ErrorCode > DIM(waverr_code_sring))
        {
            wsprintfA(s, "Unknown wave error %d", ErrorCode);
            return s;
        }
        else
            return waverr_code_sring[ErrorCode];
    }
    else
    if (ErrorCode > DIM(mmsyserr_code_string))
    {
        wsprintfA(s, "Unknown multimedia error %d", ErrorCode);
        return s;
    }
    else
        return mmsyserr_code_string[ErrorCode];
}

#undef DIM

#ifdef OUTPUT_DEBUG_STR
static void DebugPrint(const char *format, ...);
#define DEBUGPRINT DebugPrint
#else
#define DEBUGPRINT (void)
#endif

/*****************************************************************************************************************************/

struct MMBuffer
{
    int                 Number;
    int                 Prepared;   // Non-zero if this buffer has been prepared.
    HGLOBAL             hData;
    void *              Data;
    HGLOBAL             hHead;
    WAVEHDR *           Head;
    struct MMBuffer *   Next;
};

static struct MMBuffer *            Buffers;
static volatile struct MMBuffer *   FreeBuffers;
static volatile int                 NumBuffersInUse;
static HWAVEOUT                     hDevice;
static int                          BufferDelay; // in milliseconds
static int                          BufferBlockDelay; // in milliseconds
static DWORD                        CurrentTime = 0;
static volatile BOOL                DriverClosing = FALSE;
static volatile BOOL                OutputWorking = FALSE;
static const int                    AllowSynchronousWaveforms = 1;
static int data_block_size;
static int data_block_num = 64;
static int data_block_trunc_size = sizeof(int16) * 2 * (1L << DEFAULT_AUDIO_BUFFER_BITS);

/*****************************************************************************************************************************/

static void BufferPoolReset(void)
{
    int i;

    DEBUGPRINT("Resetting buffer pool...\n");
    Buffers[0].Number   = 0;
    Buffers[0].Prepared = 0;
    Buffers[0].Next     = &Buffers[1];
    for (i = 1; i < data_block_num - 1; i++)
    {
        Buffers[i].Number   = i;
        Buffers[i].Prepared = 0;
        Buffers[i].Next     = &Buffers[i + 1];
    }
    Buffers[i].Number   = i;
    Buffers[i].Prepared = 0;
    Buffers[i].Next     = NULL;
    FreeBuffers     = &Buffers[0];
    NumBuffersInUse = 0;
    DEBUGPRINT("Buffer pool reset.\n", NumBuffersInUse);
}

static struct MMBuffer * GetBuffer()
{
    struct MMBuffer *   b;

    DEBUGPRINT("%2d: Getting buffer...\n", NumBuffersInUse);
    EnterCriticalSection(&critSect);
    if (FreeBuffers)
    {
    	b           = (struct MMBuffer *)FreeBuffers;
        FreeBuffers = FreeBuffers->Next;
        NumBuffersInUse++;
    /** If this buffer is still prepared we can safely unprepare it because we got it from the free buffer list. **/
        if (b->Prepared)
        {
            waveOutUnprepareHeader(hDevice, (LPWAVEHDR) b->Head, sizeof(WAVEHDR));
            b->Prepared = 0;
        }
        b->Next     = NULL;
    }
    else
        b = NULL;
    LeaveCriticalSection(&critSect);
    DEBUGPRINT("%2d: Got buffer.\n", NumBuffersInUse);
    return b;
}

static void PutBuffer(struct MMBuffer * b)
{
    DEBUGPRINT("%2d: Putting buffer...\n", NumBuffersInUse);
    b->Next     = (struct MMBuffer *)FreeBuffers;
    FreeBuffers = b;
    NumBuffersInUse--;
    DEBUGPRINT("%2d: Buffer put.\n", NumBuffersInUse);
}

static void CALLBACK OnPlaybackEvent(HWAVE hWave, UINT Msg, DWORD_PTR UserData, DWORD_PTR Param1, DWORD_PTR Param2)
{
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Msg: 0x%08X, Num. buffers in use: %d", Msg, NumBuffersInUse);
    switch (Msg)
    {
    case WOM_OPEN:
        DEBUGPRINT("%2d: Device opened.\n", NumBuffersInUse);
        break;
    case WOM_CLOSE:
        DEBUGPRINT("%2d: Device closed.\n", NumBuffersInUse);
        break;
    case WOM_DONE:
        EnterCriticalSection(&critSect);
        PutBuffer(&Buffers[((WAVEHDR *)Param1)->dwUser]);
        LeaveCriticalSection(&critSect);
        break;
    default:
        DEBUGPRINT("%2d: Unknown play back event 0x%08X.\n", NumBuffersInUse, Msg);
        break;        
    }
}

static void print_device_list(void)
{
	UINT num;
	int i, list_num;
	WAVEOUTCAPS woc;
	DEVICELIST device[DEVLIST_MAX];

	num = waveOutGetNumDevs();
	list_num=0;
	for(i = 0 ; i < num  && i < DEVLIST_MAX ; i++){
		if (MMSYSERR_NOERROR == waveOutGetDevCaps((UINT)i, &woc, sizeof(woc)) ){
			device[list_num].deviceID=i;
			char *s = tchar_to_char(woc.szPname);
			strncpy(device[list_num].name, WMME_DEVICE_NAME_MAX_LENGTH, s);
			device[list_num].name[WMME_DEVICE_NAME_MAX_LENGTH - 1] = '\0';
			safe_free(s);
			list_num++;
		}
	}
	for(i=0;i<list_num;i++){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%2d %s", device[i].deviceID, device[i].name);
	}
}

/*****************************************************************************************************************************/

static int open_output(void)
{
    int             i;
	WAVEFORMATEXTENSIBLE wfe = {0};
	WAVEFORMATEX    *pwf = NULL;
    WAVEOUTCAPS     woc;
    MMRESULT        Result;
    UINT            uDeviceID, DeviceID;
	int ret;
	int tmp;
	int format_ext;
	GUID guid_WAVE_FORMAT = {WAVE_FORMAT_UNKNOWN,0x0000,0x0010,{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/** Check if there is at least one audio device. **/
    if (waveOutGetNumDevs() == 0)
    {
        ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "No audio devices present!");
        return -1;
    }
	opt_wmme_device_id = opt_output_device_id != -3 ? opt_output_device_id : opt_wmme_device_id; 
	if (opt_wmme_device_id == -2){
		print_device_list();
        return -1;
	}
/** They can't mean these. **/
	dpm.encoding &= ~(PE_BYTESWAP);
	if (dpm.encoding & (PE_16BIT | PE_24BIT | PE_32BIT | PE_F32BIT | PE_64BIT | PE_F64BIT)){
        dpm.encoding |= PE_SIGNED;
	}else // 8bit PE_ULAW PE_ALAW only unsigned
        dpm.encoding &= ~PE_SIGNED;
	format_ext = (dpm.encoding & (PE_ULAW | PE_ALAW)) ? 0 : opt_wave_format_ext;
	if(format_ext){
		pwf = &wfe.Format;
		memcpy(&wfe.SubFormat, &guid_WAVE_FORMAT, sizeof(GUID));
		pwf->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		pwf->cbSize       = (WORD)22;
	}else{
		pwf = (WAVEFORMATEX *)&wfe;
		pwf->cbSize       = (WORD)0;
	}
	if(dpm.encoding & PE_16BIT){
		if(format_ext){			
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 16;
			pwf->wBitsPerSample = (WORD) 16;
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 16;
		}
	}else if(dpm.encoding & PE_24BIT){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 24;
			pwf->wBitsPerSample = (WORD) 24;
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 24;
		}
	}else if(dpm.encoding & PE_32BIT){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 32;
			pwf->wBitsPerSample = (WORD) 32;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 32;
		}
	}else if(dpm.encoding & PE_F32BIT){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
			wfe.Samples.wValidBitsPerSample = 32;
			pwf->wBitsPerSample = (WORD) 32;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			pwf->wBitsPerSample = (WORD) 32;
		}
	}else if(dpm.encoding & PE_64BIT){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 64;
			pwf->wBitsPerSample = (WORD) 64;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 64;
		}
	}else if(dpm.encoding & PE_F64BIT){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
			wfe.Samples.wValidBitsPerSample = 64;
			pwf->wBitsPerSample = (WORD) 64;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			pwf->wBitsPerSample = (WORD) 64;
		}		
	}else if(dpm.encoding & PE_ALAW){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_ALAW;
			wfe.Samples.wValidBitsPerSample = 8;
			pwf->wBitsPerSample = (WORD) 8;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_ALAW;
			pwf->wBitsPerSample = (WORD) 8;
		}
	}else if(dpm.encoding & PE_ULAW){
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_MULAW;
			wfe.Samples.wValidBitsPerSample = 8;
			pwf->wBitsPerSample = (WORD) 8;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_MULAW;
			pwf->wBitsPerSample = (WORD) 8;
		}
	}else{ // 8bit
		if(format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 8;
			pwf->wBitsPerSample = (WORD) 8;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 8;
		}
	}
	pwf->nChannels       = (WORD)dpm.encoding & PE_MONO ? 1 : 2;
	pwf->nSamplesPerSec  = (DWORD)dpm.rate;
	pwf->nBlockAlign     = (WORD)(pwf->nChannels * pwf->wBitsPerSample / 8);
	pwf->nAvgBytesPerSec = (DWORD)pwf->nSamplesPerSec * pwf->nBlockAlign;
	wfe.dwChannelMask = pwf->nChannels == 1 ? SPEAKER_MONO : SPEAKER_STEREO;
    if (dpm.extra_param[0] < 2)
    {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,X", dpm.extra_param[0]);
        dpm.extra_param[0] = 2;
    }
    if (audio_buffer_bits < 7)
    {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: X,%d", audio_buffer_bits);
        audio_buffer_bits = 7;
    }
/** check buffer size **/
	tmp = dpm.extra_param[0] * audio_buffer_size;
	if(tmp < 4096)
    {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,%d", dpm.extra_param[0], audio_buffer_bits);
		dpm.extra_param[0] = DEFAULT_AUDIO_BUFFER_NUM;
		audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;
    }
	data_block_num = dpm.extra_param[0];
	data_block_size = audio_buffer_size;
	data_block_trunc_size = data_block_size * pwf->nBlockAlign;
/** Open the device. **/
    DEBUGPRINT("Opening device...\n");
	hDevice = 0;
    DriverClosing = FALSE;
	uDeviceID = opt_wmme_device_id == -1 ? WAVE_MAPPER : (UINT)opt_wmme_device_id;   
	if (AllowSynchronousWaveforms)
		Result = waveOutOpen(&hDevice, uDeviceID, (WAVEFORMATEX *) &wfe, (DWORD_PTR) OnPlaybackEvent, 0, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
	else
		Result = waveOutOpen(&hDevice, uDeviceID, (WAVEFORMATEX *) &wfe, (DWORD_PTR) OnPlaybackEvent, 0, CALLBACK_FUNCTION);
    if (Result)
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't open audio device: encoding=<%s>, rate=<%d>, ch=<%d>: %s", output_encoding_string(dpm.encoding), dpm.rate, pwf->nChannels, MMErrorMessage(Result));
        return -1;
    }
    else
	{
        DEBUGPRINT("Device opened.\n");
	}
/** Get the device ID. **/
    DeviceID = 0;
    waveOutGetID(hDevice, &DeviceID);
/** Get the device capabilities. **/
    memset(&woc, 0, sizeof(WAVEOUTCAPS));
    Result = waveOutGetDevCaps(DeviceID, &woc, sizeof(WAVEOUTCAPS));
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Device ID: %d",              DeviceID);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Manufacture ID: %d",         woc.wMid);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Product ID: %d",             woc.wPid);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Driver version: %d",         woc.vDriverVersion);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Product name: %s",           woc.szPname);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Formats supported: 0x%08X",  woc.dwFormats);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Max. channels: %d",          woc.wChannels);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Supported features: 0x%08X", woc.dwSupport);
/** Calculate the buffer delay. **/
    BufferDelay = data_block_size * data_block_num * 1000 / dpm.rate;
	BufferBlockDelay = data_block_size * 1000 / dpm.rate / 4;
/** Create the buffer pool. **/
    Buffers = (struct MMBuffer *) safe_malloc(data_block_num * sizeof(struct MMBuffer));	
    for (i = 0; i < data_block_num; i++)
    {
        struct MMBuffer *   b;

        b = &Buffers[i];
        b->hData = GlobalAlloc(GMEM_ZEROINIT, data_block_trunc_size);
        b->Data  = GlobalLock (b->hData);
        b->hHead = GlobalAlloc(GMEM_ZEROINIT, sizeof(WAVEHDR));
        b->Head  = GlobalLock (b->hHead);
    }
    BufferPoolReset();
/** Set the file descriptor. **/
    dpm.fd = 0;
    return 0;
}

static void close_output(void)
{
	int i, flg;

    if (dpm.fd == -1)
		return;	
/**  WaitForAllBuffers **/
    DEBUGPRINT("%2d: Waiting for all buffers to be dequeued...\n", NumBuffersInUse);
	for (;;) {
		EnterCriticalSection(&critSect);
		flg = (NumBuffersInUse || OutputWorking) ? 1 : 0;
		LeaveCriticalSection(&critSect);
		if(!flg) break;
		Sleep(BufferDelay);
	}
    DEBUGPRINT("%2d: All buffers dequeued.\n", NumBuffersInUse);
    BufferPoolReset();
    DEBUGPRINT("Closing device...\n");
    waveOutReset(hDevice);
    waveOutClose(hDevice);
    DEBUGPRINT("Device closed.\n");
/** Free all buffers. **/
    for (i = 0; i < data_block_num; i++)
    {
        struct MMBuffer *   block;

        block = &Buffers[i];
        GlobalUnlock(block->hHead);
        GlobalFree  (block->hHead);
        GlobalUnlock(block->hData);
        GlobalFree  (block->hData);
    }
    safe_free(Buffers);
    Buffers = NULL;
/** Reset the file descriptor. **/
    dpm.fd = -1;
}

static int output_data(const uint8 *Data, size_t Size)
{
    const uint8 *  d;
    int32   s;
    int32   c;
    const int32  max_continue = 1164;
	
    if (!hDevice)
		return -1;	
    OutputWorking = TRUE;
    d = Data;
    s = Size;
	c = max_continue;
    while (!DriverClosing && dpm.fd != -1 && s > 0 && c > 0)
    {
        int32               n;
        struct MMBuffer *   b;
        MMRESULT            Result;
        LPWAVEHDR           wh;

        if ((b = GetBuffer()) == NULL)
        {
			DEBUGPRINT("%2d: WaitForBuffer %dms...\n", NumBuffersInUse, BufferBlockDelay);
			Sleep(BufferBlockDelay); /**  WaitForBuffer **/
			--c;
            continue;
        }
		if (DriverClosing || dpm.fd == -1) {
			break;
		}
		n = (s <= data_block_trunc_size) ? s : data_block_trunc_size;
        CopyMemory(b->Data, d, n);
        wh = b->Head;
        wh->dwBufferLength = n;
        wh->lpData         = b->Data;
        wh->dwUser         = b->Number;
    /** Prepare the buffer. **/
        DEBUGPRINT("%2d: Preparing buffer %d...\n", NumBuffersInUse, wh->dwUser);
        Result = waveOutPrepareHeader(hDevice, wh, sizeof(WAVEHDR));
        if (Result)
        {
            DEBUGPRINT("%2d: Buffer preparation failed.\n", NumBuffersInUse);
            ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "waveOutPrepareHeader(): %s", MMErrorMessage(Result));
            return -1;
        }
        else
		{
            DEBUGPRINT("%2d: Buffer %d prepared.\n", NumBuffersInUse, wh->dwUser);
		}
        b->Prepared = 1;
    /** Queue the buffer. **/
        DEBUGPRINT("%2d: Queueing buffer %d...\n", NumBuffersInUse, wh->dwUser);
        Result = waveOutWrite(hDevice, wh, sizeof(WAVEHDR));
        if (Result)
        {
            DEBUGPRINT("%2d: Buffer queueing failed.\n", NumBuffersInUse);
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "waveOutWrite(): %s", MMErrorMessage(Result));
            return -1;
        }
        else
		{
            DEBUGPRINT("%2d: Buffer %d queued.\n", NumBuffersInUse, wh->dwUser);
		}
        d += n;
        s -= n;
		c = max_continue;
    }	
    OutputWorking = FALSE;
    return 0;
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
    case PM_REQ_GETQSIZ:
        *(int *)arg = (data_block_num-1) * data_block_trunc_size;
        return 0;
/*
    case PM_REQ_GETFILLABLE:
		*(int *)arg = (data_block_num - NumBuffersInUse - 1) * data_block_size;
		return 0;
*/
    case PM_REQ_GETFILLED:
		*(int *)arg = NumBuffersInUse * data_block_trunc_size;
    return 0;

    case PM_REQ_DISCARD:
		DEBUGPRINT("Resetting audio device.\n");
		waveOutReset(hDevice);
		close_output();
		open_output();
		DEBUGPRINT("Audio device reset.\n");
		return 0;

    case PM_REQ_FLUSH:
		close_output();
		open_output();
        return 0;
 
    case PM_REQ_PLAY_START: /* Called just before playing */
    case PM_REQ_PLAY_END: /* Called just after playing */
    	return 0;
    }
    return -1;
}

static int detect(void)
{
	if (waveOutGetNumDevs() == 0)
		return 0;	/* not found */
	return 1;	/* found */
}

/*****************************************************************************************************************************/

int wmme_device_list(DEVICELIST *device)
{
	int i, num;
	WAVEOUTCAPS woc;

	num = waveOutGetNumDevs();
	for(i = -1 ; i < num  && i < (DEVLIST_MAX - 2) ; i++){ // -1, 0
		waveOutGetDevCaps((UINT)i, &woc, sizeof(woc));
		device[i+1].deviceID=i;
		char *s = tchar_to_char(woc.szPname);
		strncpy(device[i+1].name, s, WMME_DEVICE_NAME_MAX_LENGTH - 1);
		device[i + 1].name[WMME_DEVICE_NAME_MAX_LENGTH - 1] = '\0';
		safe_free(s);
	}
	return num;
}