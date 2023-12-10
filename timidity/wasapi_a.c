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

    wasapi_a.c
	
	Functions to play sound using WASAPI (Windows Vista).

Based on <https://github.com/Microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/multimedia/audio/RenderExclusiveEventDriven>,
which is distributed under the MIT License:

----
The MIT License (MIT)

Copyright (c) Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Portions of this repo are provided under the SIL Open Font License.
See the LICENSE file in individual samples for additional details
----

Written by Starg.

*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */


#ifdef AU_WASAPI


#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <process.h>
#include <tchar.h>

#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "output.h"
#include "wasapi_a.h"

int opt_wasapi_device_id = -1; // default render
int opt_wasapi_latency = 10; //  ms
int opt_wasapi_format_ext = 1;
int opt_wasapi_exclusive = 0; // shared
int opt_wasapi_polling = 0; // 0:event 1:polling
int opt_wasapi_priority = 0; // 0:Auto, 1:Audio, 2:Capture, 3:Distribution, 4:Games, 5:Playback, 6:ProAudio, 7:WindowManager
int opt_wasapi_stream_category = 0;
int opt_wasapi_stream_option = 0;

/*****************************************************************************************************************************/

static int  open_output     (void); /* 0=success, 1=warning, -1=fatal error */
static void close_output    (void);
static int  output_data     (const uint8 *buf, size_t nbytes);
static int  acntl           (int request, void *arg);
static int  detect          (void);

/*****************************************************************************************************************************/

#define dpm wasapi_play_mode

PlayMode dpm = {
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "WASAPI", 'x',
    NULL,
    &open_output,
    &close_output,
    &output_data,
    &acntl,
    &detect
};

/*****************************************************************************************************************************/

#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <winbase.h>
#include <objbase.h>
//#include <Avrt.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#undef INITGUID

const CLSID tim_CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const IID tim_IID_IMMDeviceEnumerator    = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const IID tim_IID_IAudioClient           = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
const IID tim_IID_IAudioRenderClient     = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};
const IID tim_IID_IAudioClient2          = {0x726778CD, 0xF60A, 0x4EDA, {0x82, 0xDE, 0xE4, 0x76, 0x10, 0xCD, 0x78, 0xAA}};

// Some compilers do not have the latest version of AudioClientProperties
typedef struct {
	UINT32 cbSize;
	BOOL bIsOffload;
	INT /* AUDIO_STREAM_CATEGORY */ eCategory;
	INT /* AUDCLNT_STREAMOPTIONS */ Options;
} timAudioClientProperties;

#define SPEAKER_FRONT_LEFT        0x1
#define SPEAKER_FRONT_RIGHT       0x2
#define SPEAKER_FRONT_CENTER      0x4
#define SPEAKER_MONO	          (SPEAKER_FRONT_CENTER)
#define SPEAKER_STEREO	          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)

static HRESULT check_hresult(HRESULT hr, const char *operation)
{
	if (FAILED(hr)) {
		switch (hr) {
		
#define HANDLE_HRESULT(hresult) \
	case hresult: \
		ctl->cmsg(CMSG_ERROR, VERB_VERBOSE, "WASAPI: %s failed with HRESULT = %s", operation, #hresult); \
		break;

			HANDLE_HRESULT(E_UNEXPECTED);
			HANDLE_HRESULT(E_NOTIMPL);
			HANDLE_HRESULT(E_OUTOFMEMORY);
			HANDLE_HRESULT(E_INVALIDARG);
			HANDLE_HRESULT(E_NOINTERFACE);
			HANDLE_HRESULT(E_POINTER);
			HANDLE_HRESULT(E_HANDLE);
			HANDLE_HRESULT(E_ABORT);
			HANDLE_HRESULT(E_FAIL);
			HANDLE_HRESULT(E_ACCESSDENIED);

			HANDLE_HRESULT(CO_E_NOTINITIALIZED);

			HANDLE_HRESULT(AUDCLNT_E_NOT_INITIALIZED);
			HANDLE_HRESULT(AUDCLNT_E_ALREADY_INITIALIZED);
			HANDLE_HRESULT(AUDCLNT_E_WRONG_ENDPOINT_TYPE);
			HANDLE_HRESULT(AUDCLNT_E_DEVICE_INVALIDATED);
			HANDLE_HRESULT(AUDCLNT_E_NOT_STOPPED);
			HANDLE_HRESULT(AUDCLNT_E_BUFFER_TOO_LARGE);
			HANDLE_HRESULT(AUDCLNT_E_OUT_OF_ORDER);
			HANDLE_HRESULT(AUDCLNT_E_UNSUPPORTED_FORMAT);
			HANDLE_HRESULT(AUDCLNT_E_INVALID_SIZE);
			HANDLE_HRESULT(AUDCLNT_E_DEVICE_IN_USE);
			HANDLE_HRESULT(AUDCLNT_E_BUFFER_OPERATION_PENDING);
			HANDLE_HRESULT(AUDCLNT_E_THREAD_NOT_REGISTERED);
			HANDLE_HRESULT(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED);
			HANDLE_HRESULT(AUDCLNT_E_ENDPOINT_CREATE_FAILED);
			HANDLE_HRESULT(AUDCLNT_E_SERVICE_NOT_RUNNING);
			HANDLE_HRESULT(AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED);
			HANDLE_HRESULT(AUDCLNT_E_EXCLUSIVE_MODE_ONLY);
			HANDLE_HRESULT(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL);
			HANDLE_HRESULT(AUDCLNT_E_EVENTHANDLE_NOT_SET);
			HANDLE_HRESULT(AUDCLNT_E_INCORRECT_BUFFER_SIZE);
			HANDLE_HRESULT(AUDCLNT_E_BUFFER_SIZE_ERROR);
			HANDLE_HRESULT(AUDCLNT_E_CPUUSAGE_EXCEEDED);
			HANDLE_HRESULT(AUDCLNT_E_BUFFER_ERROR);
			HANDLE_HRESULT(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED);
			HANDLE_HRESULT(AUDCLNT_E_INVALID_DEVICE_PERIOD);
			HANDLE_HRESULT(AUDCLNT_E_INVALID_STREAM_FLAG);
			HANDLE_HRESULT(AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE);
			HANDLE_HRESULT(AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES);
			HANDLE_HRESULT(AUDCLNT_E_OFFLOAD_MODE_ONLY);
			HANDLE_HRESULT(AUDCLNT_E_NONOFFLOAD_MODE_ONLY);
			HANDLE_HRESULT(AUDCLNT_E_RESOURCES_INVALIDATED);
			HANDLE_HRESULT(AUDCLNT_E_RAW_MODE_UNSUPPORTED);
			HANDLE_HRESULT(AUDCLNT_E_ENGINE_PERIODICITY_LOCKED);
			HANDLE_HRESULT(AUDCLNT_E_ENGINE_FORMAT_LOCKED);
			HANDLE_HRESULT(AUDCLNT_E_HEADTRACKING_ENABLED);
			HANDLE_HRESULT(AUDCLNT_E_HEADTRACKING_UNSUPPORTED);
			HANDLE_HRESULT(AUDCLNT_E_EFFECT_NOT_AVAILABLE);
			HANDLE_HRESULT(AUDCLNT_E_EFFECT_STATE_READ_ONLY);

#undef HANDLE_HRESULT

		default:
			ctl->cmsg(CMSG_ERROR, VERB_VERBOSE, "WASAPI: %s failed with HRESULT = 0x%X", operation, hr);
			break;
		}
	}

	return hr;
}

static BOOL check_hresult_failed(HRESULT hr, const char *operation)
{
	return FAILED(check_hresult(hr, operation));
}

/*****************************************************************************************************************************/

typedef struct WABufferBlock_t {
	struct WABufferBlock_t *pNext;
	size_t CurrentSize;
	size_t Capacity;
	uint8 Data[1];
} WABufferBlock;

typedef struct WABuffer_t {
	WABufferBlock *pHead;
	WABufferBlock *pTail;
	WABufferBlock *pFree;
	uint32 FilledByte;
} WABuffer;

static WABuffer Buffers = {NULL, NULL, NULL, 0};

static int get_filled_byte(void)
{
	return Buffers.FilledByte;
}

static int is_buffer_empty(void)
{
	return !Buffers.pHead;
}

static size_t calc_output_bytes(size_t max_bytes)
{
#if 1 // use FilledByte
	return Buffers.FilledByte > max_bytes ? max_bytes : Buffers.FilledByte;
#else
	size_t bytes = 0;
	WABufferBlock *block = Buffers.pHead;

	while(bytes < max_bytes && block){
		bytes += block->CurrentSize;
		block = block->pNext;
	}
	return bytes > max_bytes ? max_bytes : bytes;
#endif
}

static void clear_buffer(void)
{
	if(Buffers.pTail){
		Buffers.pTail->pNext = Buffers.pFree;
		Buffers.pFree = Buffers.pHead;
		Buffers.pHead = NULL;
		Buffers.pTail = NULL;
	}
	Buffers.FilledByte = 0;
}

static void free_buffer(void)
{
	WABufferBlock *block = NULL;

	clear_buffer();
	block = Buffers.pFree;
	while(block){
		WABufferBlock* pNext = block->pNext;
		free(block);
		block = pNext;
	}
	Buffers.pFree = NULL;	
}

/* if *pbuf == NULL, appends `size` count of zeros */
static void input_buffer_partial(WABufferBlock *block, const uint8 **pbuf, size_t *bytes)
{
	size_t pushLength = block->Capacity - block->CurrentSize;

	if(pushLength > *bytes)
		pushLength = *bytes;
	if(*pbuf){
		memcpy(block->Data + block->CurrentSize, *pbuf, pushLength);
		*pbuf += pushLength;
	}else{
		memset(block->Data + block->CurrentSize, 0, pushLength);
	}
	*bytes -= pushLength;
	block->CurrentSize += pushLength;
}

/* if buf == NULL, appends `size` count of zeros */
static int input_buffer(const uint8 *buf, size_t bytes)
{
	size_t pbytes = bytes;
	int flg = 0;

	while (bytes > 0){
		if(Buffers.pTail && Buffers.pTail->CurrentSize < Buffers.pTail->Capacity){
			input_buffer_partial(Buffers.pTail, &buf, &bytes);
		}else if(Buffers.pFree){
			WABufferBlock *block = Buffers.pFree;

			Buffers.pFree = Buffers.pFree->pNext;
			block->CurrentSize = 0;
			block->pNext = NULL;
			if(Buffers.pTail){
				Buffers.pTail->pNext = block;
				Buffers.pTail = block;
			}else{
				Buffers.pHead = block;
				Buffers.pTail = block;
			}
			input_buffer_partial(block, &buf, &bytes);
		}else{
			size_t capacity = bytes * 4;
			WABufferBlock *new_block = (WABufferBlock *)safe_malloc(sizeof(WABufferBlock) + capacity);
			
			if(!new_block){
				flg = 1; // error
				break;
			}
			if(Buffers.pTail)
				Buffers.pTail->pNext = new_block;
			else
				Buffers.pHead = new_block;
			Buffers.pTail = new_block;
			Buffers.pTail->pNext = NULL;
			Buffers.pTail->Capacity = capacity;
			Buffers.pTail->CurrentSize = 0;
			input_buffer_partial(Buffers.pTail, &buf, &bytes);
		}
	}
	Buffers.FilledByte += pbytes - bytes;
	return flg ? FALSE : TRUE;
}

static void output_buffer(uint8 *buff, size_t bytes)
{
	size_t pbytes = bytes, tmp_bytes, out_bytes = 0;
	WABufferBlock *block = Buffers.pHead;

	while(bytes > 0 && block){
		tmp_bytes = block->CurrentSize;
		if(tmp_bytes > bytes)
			tmp_bytes = bytes;
		memcpy(buff, block->Data, tmp_bytes);
		buff += tmp_bytes;
		bytes -= tmp_bytes;
		out_bytes += tmp_bytes;
		block = block->pNext;
	}
	while (out_bytes > 0 && Buffers.pHead){
		tmp_bytes = Buffers.pHead->CurrentSize;
		if(Buffers.pHead->CurrentSize > out_bytes){
			tmp_bytes = out_bytes;
			memmove(Buffers.pHead->Data, Buffers.pHead->Data + out_bytes, Buffers.pHead->CurrentSize - out_bytes);
			Buffers.pHead->CurrentSize -= out_bytes;
		}else{
			WABufferBlock *pNext = Buffers.pHead->pNext;
			Buffers.pHead->pNext = Buffers.pFree;
			Buffers.pFree = Buffers.pHead;
			Buffers.pHead = pNext;
			if(!pNext)
				Buffers.pTail = NULL;
		}
		out_bytes -= tmp_bytes;
	}
	Buffers.FilledByte -= pbytes - bytes - out_bytes;
}

/*****************************************************************************************************************************/

typedef HANDLE (WINAPI *fAvSetMmThreadCharacteristics)(LPCSTR,LPDWORD);
typedef BOOL   (WINAPI *fAvRevertMmThreadCharacteristics)(HANDLE);

static HINSTANCE hDll = NULL;
static fAvSetMmThreadCharacteristics pAvSetMmThreadCharacteristics = NULL;
static fAvRevertMmThreadCharacteristics pAvRevertMmThreadCharacteristics = NULL;

static int load_avrt(void)
{
    hDll = LoadLibrary("avrt.dll");
    if(hDll == NULL)
        return FALSE;	
#ifdef UNICODE
	pAvSetMmThreadCharacteristics = (fAvSetMmThreadCharacteristics)GetProcAddress(hDll, "AvSetMmThreadCharacteristicsW");
#else
	pAvSetMmThreadCharacteristics = (fAvSetMmThreadCharacteristics)GetProcAddress(hDll, "AvSetMmThreadCharacteristicsA");
#endif
	pAvRevertMmThreadCharacteristics = (fAvRevertMmThreadCharacteristics)GetProcAddress(hDll, "AvRevertMmThreadCharacteristics");
	return (int)(pAvSetMmThreadCharacteristics && pAvRevertMmThreadCharacteristics);
}

static void free_avrt(void)
{
    if(hDll)
        FreeLibrary(hDll);
	hDll = NULL;	
	pAvSetMmThreadCharacteristics = NULL;
	pAvRevertMmThreadCharacteristics = NULL;
}

static const char *RTThreadPriorityName[] =
{
	NULL,
	"Audio",
	"Capture",
	"Distribution",
	"Games",
	"Playback",
	"Pro Audio",
	"Window Manager"
};

/*****************************************************************************************************************************/

static int WinVer = -1;

static int get_winver(void)
{
	DWORD winver, major, minor;
	int ver = 0;
	
	if(WinVer != -1)
		return WinVer;
	winver = GetVersion();
	if(winver & 0x80000000){ // Win9x
		WinVer = 0;
		return 0;
	}		
	major = (DWORD)(LOBYTE(LOWORD(winver)));
	minor = (DWORD)(HIBYTE(LOWORD(winver)));
	switch (major){
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		ver = 0;
		break;
	case 6:
		switch (minor){
		case 0: ver = 1; break; // vista
		case 1: ver = 2; break; // 7
		case 2: ver = 3; break; // 8
		case 3: ver = 4; break; // 8.1
		default: ver = 5; break; // 8.2?
		}
		break;
	case 10:
		switch (minor){
		case 0: ver = 6; break; // 10
		default: ver = 7; break; // 10.1?
		}
		break;
	default:
		ver = 8; // 11?
		break;
	}
	WinVer = ver;
	return ver;
}

/*****************************************************************************************************************************/

static HANDLE hEventTcv = NULL;
static HANDLE hRenderThread = NULL;
static IMMDevice* pMMDevice = NULL;
static IAudioClient* pAudioClient = NULL;
static IAudioRenderClient* pAudioRenderClient = NULL;
static UINT32 FrameBytes = 0;
static UINT32 BufferFrames = 0;
static int ThreadPriorityNum = 0;
static int IsExclusive = 0;
static int IsPolling = 0;
static int IsStarted = 0;
static int IsOpened = 0;
static int IsThreadStart = 0;
static int IsThreadExit = 0;
static int IsCoInit = 0;
static int CoInitThreadId = 0;
static int CvtMode = 0; // 0:no convert 1:convert 24bit(3byte->4byte)
static uint32 QueueSize = sizeof(int16) * 2 * (1L << DEFAULT_AUDIO_BUFFER_BITS) * 2;
static uint32 WaitTime = 1;
static uint32 ThreadWaitTime = 1;

static int write_buffer_event(void)
{	
	UINT32 padding = 0;
	size_t out_bytes, out_frames;
	BYTE *buf = NULL;

	if(!IsExclusive)
		if(check_hresult_failed(IAudioClient_GetCurrentPadding(pAudioClient, &padding), "IAudioClient::GetCurrentPadding()"))
			return FALSE;
	out_bytes = calc_output_bytes((BufferFrames - padding) * FrameBytes);
	out_frames = out_bytes / FrameBytes;
	if(IsExclusive && out_frames < BufferFrames){
		if(!input_buffer(NULL, BufferFrames * FrameBytes - out_bytes))
			return FALSE;
		out_bytes = BufferFrames * FrameBytes;
		out_frames = out_bytes / FrameBytes;
	}else if(out_frames == 0){
		if(!input_buffer(NULL, FrameBytes - out_bytes))
			return FALSE;		
		out_bytes = FrameBytes;
		out_frames = 1;
	}
	if(check_hresult_failed(IAudioRenderClient_GetBuffer(pAudioRenderClient, out_frames, &buf), "IAudioRenderClient::GetBuffer()"))
		return FALSE;
	output_buffer((uint8 *)buf, out_bytes);
	IAudioRenderClient_ReleaseBuffer(pAudioRenderClient, out_frames, 0);
	return TRUE;
}

static int write_buffer_polling(void)
{	
	UINT32 padding = 0;
	size_t out_bytes, out_frames;
	BYTE *buf = NULL;

	if(check_hresult_failed(IAudioClient_GetCurrentPadding(pAudioClient, &padding), "IAudioClient::GetCurrentPadding()"))
		return FALSE;
	out_bytes = calc_output_bytes((BufferFrames - padding) * FrameBytes);
	out_frames = out_bytes / FrameBytes;
	if(check_hresult_failed(IAudioRenderClient_GetBuffer(pAudioRenderClient, out_frames, &buf), "IAudioRenderClient::GetBuffer()"))
		return FALSE;
	output_buffer((uint8 *)buf, out_bytes);
	IAudioRenderClient_ReleaseBuffer(pAudioRenderClient, out_frames, 0);
	return TRUE;
}

static unsigned int WINAPI render_thread(void *arglist)
{
	int ret = 1;
	HANDLE hMmCss = NULL;
	DWORD mmCssTaskIndex = 0;	
		
	if(check_hresult_failed(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), "CoInitializeEx()"))
		return 1;
	hMmCss = (pAvSetMmThreadCharacteristics)(RTThreadPriorityName[ThreadPriorityNum], &mmCssTaskIndex);
	if(!hMmCss)
		goto thread_exit;
	IsThreadStart = 1;
	WaitForSingleObject(hEventTcv, INFINITE); // wait initialize open_output
	ResetEvent(hEventTcv);
	if(!IsPolling){
		for(;;){ // event
			WaitForSingleObject(hEventTcv, INFINITE);
			ResetEvent(hEventTcv);
			if(IsThreadExit) break;		
			EnterCriticalSection(&critSect);
			write_buffer_event();
			LeaveCriticalSection(&critSect);
		}
	}else{
		for(;;){ // polling
			WaitForSingleObject(hEventTcv, ThreadWaitTime);
			ResetEvent(hEventTcv);
			if(IsThreadExit) break;
			EnterCriticalSection(&critSect);
			write_buffer_polling();
			LeaveCriticalSection(&critSect);
		}
	}
	ret = 0;
thread_exit:
	if(hMmCss)
		(pAvRevertMmThreadCharacteristics)(hMmCss);
	CoUninitialize();
	IsThreadStart = 0;
	return ret;
}

/*****************************************************************************************************************************/

/* returns TRUE on success */
static int get_default_device(IMMDevice** ppMMDevice)
{
	IMMDeviceEnumerator *pEnumerator = NULL;

	if(check_hresult_failed(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void**)&pEnumerator), "CoCreateInstance(CLSID_MMDeviceEnumerator, ...)"))
		goto error;
	if(check_hresult_failed(IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, ppMMDevice), "IMMDeviceEnumerator::GetDefaultAudioEndpoint()"))
		goto error;
	if(pEnumerator)
		IMMDeviceEnumerator_Release(pEnumerator);
	return TRUE;
error:
	if(pEnumerator)
		IMMDeviceEnumerator_Release(pEnumerator);
	return FALSE;
}

static int get_device(IMMDevice **ppMMDevice, int devnum)
{
	int i;
	UINT num;
	HRESULT hr;
	IMMDeviceEnumerator *pde = NULL;
	IMMDeviceCollection *pdc = NULL;
	IMMDevice *pdev = NULL;
	WCHAR *pszDeviceId = NULL;

	if(devnum > WASAPI_DEVLIST_MAX - 2)
		devnum = -1;
	if(devnum < 0)
		return get_default_device(ppMMDevice);		
	if(check_hresult_failed(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void **)&pde), "CoCreateInstance(CLSID_MMDeviceEnumerator, ...)"))
		goto error;
	if(check_hresult_failed(IMMDeviceEnumerator_EnumAudioEndpoints(pde, eRender, DEVICE_STATE_ACTIVE, &pdc), "IMMDeviceEnumerator::EnumAudioEndpoints()"))
		goto error;	
	if(check_hresult_failed(IMMDeviceCollection_GetCount(pdc, &num), "IMMDeviceCollection::GetCount()"))
		goto error;
	if(num <= 0)
		goto error;
	if(num > WASAPI_DEVLIST_MAX - 2)
		num = WASAPI_DEVLIST_MAX - 2;
	if(devnum > num)
		goto error;
	if(check_hresult_failed(IMMDeviceCollection_Item(pdc, devnum, &pdev), "IMMDeviceCollection::Item()"))
		goto error;  
	if(check_hresult_failed(IMMDevice_GetId(pdev, &pszDeviceId), "IMMDevice::GetId()"))
		goto error;
	if(check_hresult_failed(IMMDeviceEnumerator_GetDevice(pde, pszDeviceId, ppMMDevice), "IMMDeviceEnumerator::GetDevice()"))
		goto error; 		
	if(pszDeviceId)
		CoTaskMemFree(pszDeviceId);
	if(pdev)
		IMMDevice_Release(pdev);
	if(pdc)
		IMMDeviceCollection_Release(pdc);
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return TRUE;

error:	
	if(pszDeviceId)
		CoTaskMemFree(pszDeviceId);
	if(pdev)
		IMMDevice_Release(pdev);
	if(pdc)
		IMMDeviceCollection_Release(pdc);
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return FALSE;
}

static void print_device_list(void)
{
	int i;
	UINT num;
	HRESULT hr;
	IMMDeviceEnumerator *pde = NULL;
	IMMDeviceCollection *pdc = NULL;
	IPropertyStore *pps = NULL;
	IAudioClient *tmpClient = NULL;	
	REFERENCE_TIME LatencyMax, LatencyMin;
	IMMDevice *defdev = NULL;	
	WASAPI_DEVICELIST *device = NULL;

	if(!get_winver())
		goto error0;
	if(detect() == FALSE)
		goto error0;	
	if(!get_default_device(&defdev))
		goto error0;	
	device = (WASAPI_DEVICELIST *)safe_malloc(sizeof(WASAPI_DEVICELIST) * WASAPI_DEVLIST_MAX);
	if(!device)
		goto error1;
	memset(device, 0, sizeof(WASAPI_DEVICELIST) * WASAPI_DEVLIST_MAX);
	device[0].deviceID = -1;
	device[0].LatencyMax = 10;
	device[0].LatencyMin = 3;
	strcpy(device[0].name, "Default Render Device");	
	if(check_hresult_failed(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void **)&pde), "CoCreateInstance(CLSID_MMDeviceEnumerator, ...)"))
		goto error1;
	if(check_hresult_failed(IMMDeviceEnumerator_EnumAudioEndpoints(pde, eRender, DEVICE_STATE_ACTIVE, &pdc), "IMMDeviceEnumerator::EnumAudioEndpoints()"))
		goto error1;	
	LatencyMax = 100000;
	LatencyMin = 30000;
	if(SUCCEEDED(IMMDevice_Activate(defdev, &tim_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&tmpClient))){
		if (FAILED(IAudioClient_GetDevicePeriod(tmpClient, &LatencyMax, &LatencyMin))) {
			LatencyMax = 100000;
			LatencyMin = 30000;
		}
	}
	LatencyMax /= 10000; // hns to ms
	LatencyMin /= 10000; // hns to ms
	if(LatencyMax > 1000)
		LatencyMax = 1000;
	if(LatencyMin < 0)
		LatencyMin = 1;
	device[0].LatencyMax = LatencyMax;
	device[0].LatencyMin = LatencyMin;
	if(tmpClient){
		IAudioClient_Release(tmpClient);
		tmpClient = NULL;
	}	
	if(defdev){
		IMMDevice_Release(defdev);
		defdev = NULL;
	}
	if(check_hresult_failed(IMMDeviceCollection_GetCount(pdc, &num), "IMMDeviceCollection::GetCount()"))
		goto error1;
	if(num <= 0)
		goto error1;
	if(num > WASAPI_DEVLIST_MAX - 2)
		num = WASAPI_DEVLIST_MAX - 2;
	for(i = 0; i < num; i++){ // -1, 0
		IMMDevice *dev = NULL;
		PROPVARIANT value;
		IAudioClient *tmpClient = NULL;

		if(check_hresult_failed(IMMDeviceCollection_Item(pdc, i, &dev), "IMMDeviceCollection::Item()"))
			goto error1;	
		device[i+1].deviceID = i;
		if(check_hresult_failed(IMMDevice_OpenPropertyStore(dev, STGM_READ, &pps), "IMMDevice::OpenPropertyStore()"))
			goto error1;
		PropVariantInit(&value);
		if(check_hresult_failed(IPropertyStore_GetValue(pps, &PKEY_Device_FriendlyName, &value), "IPropertyStore::GetValue()")) {
			PropVariantClear(&value);
		}else{
			if(value.pwszVal)
#ifdef UNICODE
				WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, (int)wcslen(value.pwszVal), device[i+1].name, WASAPI_DEVLIST_LEN - 1, 0, 0);
#else
				WideCharToMultiByte(CP_ACP, 0, value.pwszVal, (int)wcslen(value.pwszVal), device[i+1].name, WASAPI_DEVLIST_LEN - 1, 0, 0);
#endif
			else
				_snprintf(device[i+1].name, WASAPI_DEVLIST_LEN - 1, "Device Error %d", i);
		}
		PropVariantClear(&value);
		LatencyMax = 100000;
		LatencyMin = 30000;
		if(SUCCEEDED(IMMDevice_Activate(dev, &tim_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&tmpClient))){
			if (FAILED(IAudioClient_GetDevicePeriod(tmpClient, &LatencyMax, &LatencyMin))) {
				LatencyMax = 100000;
				LatencyMin = 30000;
			}
		}
		LatencyMax /= 10000; // hns to ms
		LatencyMin /= 10000; // hns to ms
		if(LatencyMax > 1000)
			LatencyMax = 1000;
		if(LatencyMin < 0)
			LatencyMin = 1;
		device[i+1].LatencyMax = LatencyMax;
		device[i+1].LatencyMin = LatencyMin;
		if(tmpClient){
			IAudioClient_Release(tmpClient);
			tmpClient = NULL;
		}		
		if(dev){
			IMMDevice_Release(dev);
			dev = NULL;
		}
		if(pps){
			IPropertyStore_Release(pps);
			pps = NULL;
		}
	}
	if(pdc)
		IMMDeviceCollection_Release(pdc);
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	for(i = 0; i < num; i++){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%2d %s", device[i].deviceID, device[i].name);
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, " Latency min:%d max:%d", device[i].LatencyMin, device[i].LatencyMax);
	}
	if(device)
		safe_free(device);
	return;
error1:
	if(tmpClient)
		IAudioClient_Release(tmpClient);
	if(pdc){
		IMMDeviceCollection_Release(pdc);
	}
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	if(device)
		safe_free(device);
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "WASAPI print_device_list() error.");
	return;
error0:
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "WASAPI output device not found");
	return;
}

/*****************************************************************************************************************************/
/* interface function */

void close_output(void)
{
	if(IsStarted){
		if(pAudioClient){
			IAudioClient_Stop(pAudioClient);
		}
		IsStarted = FALSE;
	}
	IsThreadExit = 1;
	if(hRenderThread){
		if(hEventTcv){
			SetEvent(hEventTcv);
			WaitForSingleObject(hRenderThread, INFINITE);
		}
		CloseHandle(hRenderThread);
		hRenderThread = NULL;
	}
	IsThreadExit = 0;
	free_buffer();
	if(pAudioRenderClient){
		IAudioRenderClient_Release(pAudioRenderClient);
		pAudioRenderClient = NULL;
	}
	if(pAudioClient){
		IAudioClient_Release(pAudioClient);
		pAudioClient = NULL;
	}
	if(pMMDevice){
		IMMDevice_Release(pMMDevice);
		pMMDevice = NULL;
	}
	if(hEventTcv){
		CloseHandle(hEventTcv);
		hEventTcv = NULL;
	}
	if(IsCoInit && CoInitThreadId == GetCurrentThreadId()){
		CoUninitialize();
		IsCoInit = 0;
	}
	BufferFrames = 0;
	free_avrt();
#ifdef USE_TEMP_ENCODE
	reset_temporary_encoding();
#endif
	IsOpened = 0;
}

/* 0=success, 1=warning, -1=fatal error */
int open_output(void)
{
	HRESULT hr;
	int include_enc, exclude_enc;
	WAVEFORMATEXTENSIBLE wfe = {0};
	WAVEFORMATEX *pwf = NULL;
	AUDCLNT_SHAREMODE ShareMode;
	uint32 StreamFlags;
	REFERENCE_TIME Periodicity, LatencyMax, LatencyMin, BufferDuration;
	GUID guid_WAVE_FORMAT = {WAVE_FORMAT_UNKNOWN,0x0000,0x0010,{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
	BYTE *buf;
	int device_id;

	close_output();	
		
	if(!get_winver()){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WASAPI ERROR! WASAPI require Windows Vista and later.");
		return -1;
	}	
	device_id = opt_output_device_id == -3 ? opt_wasapi_device_id : opt_output_device_id;
	if(device_id == -2){
		print_device_list();
        return -1;
	}
	if(!load_avrt()){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WASAPI ERROR! AVRT.DLL function failed.");
		goto error;
	}
	IsExclusive = opt_wasapi_exclusive;
	IsPolling = opt_wasapi_polling;
	hEventTcv = CreateEvent(NULL,FALSE,FALSE,NULL); // reset manual
	if(!hEventTcv)
		goto error;	
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if(FAILED(hr) && hr != RPC_E_CHANGED_MODE){
		check_hresult(hr, "CoInitializeEx()");
		goto error;	
	}
    if(hr != RPC_E_CHANGED_MODE){
		IsCoInit = 1;
		CoInitThreadId = GetCurrentThreadId();
    }
	if(!get_device(&pMMDevice, device_id))
		goto error;
	if(check_hresult_failed(IMMDevice_Activate(pMMDevice, &tim_IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&pAudioClient), "IMMDevice::Activate()"))
		goto error;	
	include_enc = PE_SIGNED;
	exclude_enc = PE_BYTESWAP | PE_ULAW | PE_ALAW;	
	if(!(dpm.encoding & (PE_F64BIT | PE_64BIT | PE_F32BIT | PE_32BIT | PE_24BIT | PE_16BIT))) // 8bit
		include_enc |= PE_16BIT;
	dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);	

	if(opt_wasapi_format_ext){
		pwf = &wfe.Format;
		memcpy(&wfe.SubFormat, &guid_WAVE_FORMAT, sizeof(GUID));
		pwf->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		pwf->cbSize = (WORD)22;
	}else{
		pwf = (WAVEFORMATEX *)&wfe;
		pwf->cbSize = (WORD)0;
	}
	CvtMode = 0;
	if(dpm.encoding & PE_16BIT){
		if(opt_wasapi_format_ext){			
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 16;
			pwf->wBitsPerSample = (WORD) 16;
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 16;
		}
	}else if(dpm.encoding & PE_24BIT){
		if(opt_wasapi_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 24;
			if(IsExclusive){
				pwf->wBitsPerSample = (WORD) 32;
				CvtMode = 2;
			}else{
				pwf->wBitsPerSample = (WORD) 24;
			}
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 24;
			if(IsExclusive)
				CvtMode = 2;
		}
	}else if(dpm.encoding & PE_32BIT){
		if(opt_wasapi_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 32;
			pwf->wBitsPerSample = (WORD) 32;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 32;
		}
	}else if(dpm.encoding & PE_F32BIT){
		if(opt_wasapi_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
			wfe.Samples.wValidBitsPerSample = 32;
			pwf->wBitsPerSample = (WORD) 32;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			pwf->wBitsPerSample = (WORD) 32;
		}
	}else if(dpm.encoding & PE_64BIT){
		if(opt_wasapi_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 64;
			pwf->wBitsPerSample = (WORD) 64;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 64;
		}
	}else if(dpm.encoding & PE_F64BIT){
		if(opt_wasapi_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
			wfe.Samples.wValidBitsPerSample = 64;
			pwf->wBitsPerSample = (WORD) 64;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			pwf->wBitsPerSample = (WORD) 64;
		}
	}else{ // 8bit // error
		if(opt_wasapi_format_ext){
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
	wfe.dwChannelMask = pwf->nChannels==1 ? SPEAKER_MONO : SPEAKER_STEREO;
	FrameBytes = pwf->nBlockAlign;	

#ifdef USE_TEMP_ENCODE
	if(CvtMode == 2){			
		int tmp_enc = dpm.encoding;
		tmp_enc &= ~PE_24BIT;
		tmp_enc |= PE_32BIT;
		set_temporary_encoding(tmp_enc);
	}else if(CvtMode == 1){			
		int tmp_enc = dpm.encoding;
		tmp_enc &= ~PE_16BIT;
		tmp_enc |= PE_32BIT;
		set_temporary_encoding(tmp_enc);
	}else{
		reset_temporary_encoding();
	}
#endif
#ifdef __IAudioClient2_INTERFACE_DEFINED__	
	{
		IAudioClient2 *pAudioClient2;
		int ver = get_winver();

		if (SUCCEEDED(IAudioClient_QueryInterface(pAudioClient, &tim_IID_IAudioClient2, (void**)&pAudioClient2)))
		{
			timAudioClientProperties acp = {0};
			acp.cbSize = (ver >= 4 ? 16 : 12);
			acp.bIsOffload = FALSE;
			acp.eCategory  = opt_wasapi_stream_category;
		
			if (opt_wasapi_stream_option & 4) {
				if (ver >= 6) // win10à»è„
					acp.Options |= 4 /* AUDCLNT_STREAMOPTIONS_AMBISONICS */;
			}
			if (opt_wasapi_stream_option & 2) {
				if (ver >= 6) // win10à»è„
					acp.Options |= 2 /* AUDCLNT_STREAMOPTIONS_MATCH_FORMAT */;
			}
			if (opt_wasapi_stream_option & 1){
				if(ver >= 4) // win8.1à»è„
					acp.Options |= 1 /* AUDCLNT_STREAMOPTIONS_RAW */;
			}

			hr = IAudioClient2_SetClientProperties(pAudioClient2, (AudioClientProperties *)&acp);
			IAudioClient2_Release(pAudioClient2);
			if (check_hresult_failed(hr, "IAudioClient2::SetClientProperties()"))
				goto error;
		}
	}
#endif
	if(opt_wasapi_priority <= 0 || opt_wasapi_priority > 7)
		ThreadPriorityNum = IsExclusive ? 6 : 1;
	else
		ThreadPriorityNum = opt_wasapi_priority;
	ShareMode = IsExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
	StreamFlags = IsPolling ? 0x0 : AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	StreamFlags |= AUDCLNT_STREAMFLAGS_NOPERSIST;

	LatencyMax = 100000;
	LatencyMin = 30000;
	if(FAILED(IAudioClient_GetDevicePeriod(pAudioClient, &LatencyMax, &LatencyMin))){
		LatencyMax = 100000;
		LatencyMin = 30000;
	}
	if(LatencyMax > 10000000) // 1000ms
		LatencyMax = 10000000;
	else if(LatencyMin < 10000)
		LatencyMin = 10000; // 1ms
	BufferDuration = opt_wasapi_latency * 10000; // ms to 100ns
	if(BufferDuration > LatencyMax)
		BufferDuration = LatencyMax;
	if(BufferDuration < LatencyMin)
		BufferDuration = LatencyMin;
	Periodicity = IsExclusive ? BufferDuration : 0;

	hr = IAudioClient_Initialize(pAudioClient, ShareMode, StreamFlags, BufferDuration, Periodicity,	(WAVEFORMATEX *)&wfe, NULL);
	if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED){
		UINT32 bufferSize;

		if(check_hresult_failed(IAudioClient_GetBufferSize(pAudioClient, &bufferSize), "IAudioClient::GetBufferSize()"))
			goto error;
		IAudioClient_Release(pAudioClient);
		pAudioClient = NULL;
		BufferDuration = (REFERENCE_TIME)(10000.0f * 1000 * bufferSize / pwf->nSamplesPerSec + 0.5);
		Periodicity = IsExclusive ? BufferDuration : 0;
		if(check_hresult_failed(IMMDevice_Activate(pMMDevice, &tim_IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&pAudioClient), "IMMDevice::Activate()"))
			goto error;
		hr = IAudioClient_Initialize(pAudioClient, ShareMode, StreamFlags, BufferDuration, Periodicity,	(WAVEFORMATEX *)&wfe, NULL);
	}
	if(check_hresult_failed(hr, "IAudioClient::Initialize()"))
		goto error;
	if(check_hresult_failed(IAudioClient_GetBufferSize(pAudioClient, &BufferFrames), "IAudioClient::GetBufferSize()"))
		goto error;
	if(!IsPolling)
		if(check_hresult_failed(IAudioClient_SetEventHandle(pAudioClient, hEventTcv), "IAudioClient::SetEventHandle()"))
			goto error;
	if(check_hresult_failed(IAudioClient_GetService(pAudioClient, &tim_IID_IAudioRenderClient, (void**)&pAudioRenderClient), "IAudioClient::GetService(..., IID_IAudioRenderClient, ...)"))
		goto error;
	
	if(dpm.extra_param[0] < 2){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,X", dpm.extra_param[0]);
		dpm.extra_param[0] = 2;
	}	
	if(audio_buffer_size < 5){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: X,%d", audio_buffer_bits);
		audio_buffer_bits = 5;
	}
	QueueSize = audio_buffer_size * FrameBytes * dpm.extra_param[0];
	WaitTime = (double)audio_buffer_size * div_playmode_rate * 1000.0 * DIV_4; // blocktime/4
	if(IsPolling){ // for polling
		ThreadWaitTime = BufferDuration * DIV_10000 * DIV_3; // 100ns to ms
		if(ThreadWaitTime < 1)
			ThreadWaitTime = 1;
	}
	IsOpened = 1;
	IsThreadStart = 0;
	IsThreadExit = 0;
	if(!hRenderThread){
		hRenderThread = (HANDLE)_beginthreadex(NULL, 0, &render_thread, NULL, 0, NULL);
		if(!hRenderThread)
			goto error;
		set_thread_description((ptr_size_t)hRenderThread, "WASAPI Render Thread");
	}
	if(!IsStarted){
		int count = 20; // 200ms
		if(check_hresult_failed(IAudioRenderClient_GetBuffer(pAudioRenderClient, BufferFrames, &buf), "IAudioRenderClient::GetBuffer()"))
			goto error;
		IAudioRenderClient_ReleaseBuffer(pAudioRenderClient, BufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
		while(!IsThreadStart && count > 0){ // 
			Sleep(10);
			--count;
		}
		if(count <= 0) // time out
			goto error;
		if(check_hresult_failed(IAudioClient_Start(pAudioClient), "IAudioClient::Start()"))
			goto error;	
		SetEvent(hEventTcv); // start process
		IsStarted = TRUE;
	}
	return 0;
error:
	close_output();
	return -1;
}

int output_data(const uint8 *buf, size_t nbytes)
{
	int flg = TRUE, i;
	int32 max_count = 64; // wait =  blocktime/4 * max_count
#ifndef USE_TEMP_ENCODE
	uint8 tbuff[2 * (1L << DEFAULT_AUDIO_BUFFER_BITS) * sizeof(int16) * 2] = {0};
#endif

	if(!IsOpened) 
		return -1;	
#ifndef USE_TEMP_ENCODE
	if(CvtMode == 2){ // 24bit 3byte->4byte
		int samples = nbytes / 3;
		uint8 *in = (uint8 *)buf, *out = tbuff;
		for(i = 0; i < samples; i++){
			*out++ = 0;
			*out++ = *(in)++;
			*out++ = *(in)++;
			*out++ = *(in)++;
		}
		buf = (const uint8 *)&tbuff;
		nbytes = samples * 4;
	}else if(CvtMode == 1){ // 16bit 2byte->4byte
		int samples = nbytes / 2;
		uint8 *in = (uint8 *)buf, *out = tbuff;
		for(i = 0; i < samples; i++){
			*out++ = 0;
			*out++ = 0;
			*out++ = *(in)++;
			*out++ = *(in)++;
		}
		buf = (const uint8 *)&tbuff;
		nbytes = samples * 4;
	}
#endif
	for(;;){
		if(!IsOpened)
			return -1; 
		if(get_filled_byte() < QueueSize)
			break;
		if(max_count <= 0){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "WASAPI error: timeout output_data().");
			return -1; 
		}
		Sleep(WaitTime);
		--max_count; 
	}
	EnterCriticalSection(&critSect);
	flg = input_buffer(buf, nbytes);
	LeaveCriticalSection(&critSect);
	if(flg)
		return 0;
	close_output();
	return -1;
}

int acntl(int request, void *arg)
{
	switch(request){
	//case PM_REQ_GETFRAGSIZ:
	//	return 0;
	case PM_REQ_GETQSIZ:
		*(int *)arg = QueueSize;
		return 0;
	case PM_REQ_GETFILLED:
		*(int *)arg = get_filled_byte();
		return 0;
	case PM_REQ_FLUSH: // thru
		while (!is_buffer_empty())
			WaitForSingleObject(hRenderThread, 10);	
	case PM_REQ_DISCARD:
		close_output();
		open_output();
		return 0;
	case PM_REQ_PLAY_START:
		return 0;
	case PM_REQ_PLAY_END:
		return 0;
	default:
		return -1;
	}
}

int detect(void)
{
	IMMDevice *pMMDevice = NULL;
	int result;
	
	if(!get_winver())
		return -1;
	result = get_default_device(&pMMDevice);
	if(pMMDevice)
		IMMDevice_Release(pMMDevice);
	return result;
}

/*****************************************************************************************************************************/

int wasapi_device_list(WASAPI_DEVICELIST *device)
{
	int i;
	UINT num;
	HRESULT hr;
	IMMDeviceEnumerator *pde = NULL;
	IMMDeviceCollection *pdc = NULL;
	IPropertyStore *pps = NULL;
	IAudioClient *tmpClient = NULL;	
	REFERENCE_TIME LatencyMax, LatencyMin;
	IMMDevice *defdev = NULL;
	
	device[0].deviceID = -1;
	device[0].LatencyMax = 10;
	device[0].LatencyMin = 3;
	strcpy(device[0].name, "Default Render Device");
	
	if(!get_winver())
		goto error0;
	if(detect() == FALSE)
		goto error0;	
	if(!get_default_device(&defdev))
		goto error0;	
	if(check_hresult_failed(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void **)&pde), "CoCreateInstance(CLSID_MMDeviceEnumerator, ...)"))
		goto error1;
	if(check_hresult_failed(IMMDeviceEnumerator_EnumAudioEndpoints(pde, eRender, DEVICE_STATE_ACTIVE, &pdc), "IMMDeviceEnumerator::EnumAudioEndpoints()"))
		goto error1;	
	LatencyMax = 100000;
	LatencyMin = 30000;
	if(SUCCEEDED(IMMDevice_Activate(defdev, &tim_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&tmpClient))){
		if (FAILED(IAudioClient_GetDevicePeriod(tmpClient, &LatencyMax, &LatencyMin))) {
			LatencyMax = 100000;
			LatencyMin = 30000;
		}
	}
	LatencyMax /= 10000; // hns to ms
	LatencyMin /= 10000; // hns to ms
	if(LatencyMax > 1000)
		LatencyMax = 1000;
	if(LatencyMin < 0)
		LatencyMin = 1;
	device[0].LatencyMax = LatencyMax;
	device[0].LatencyMin = LatencyMin;
	if(tmpClient){
		IAudioClient_Release(tmpClient);
		tmpClient = NULL;
	}	
	if(defdev){
		IMMDevice_Release(defdev);
		defdev = NULL;
	}
	if(check_hresult_failed(IMMDeviceCollection_GetCount(pdc, &num), "IMMDeviceCollection::GetCount()"))
		goto error1;
	if(num <= 0)
		goto error1;
	if(num > WASAPI_DEVLIST_MAX - 2)
		num = WASAPI_DEVLIST_MAX - 2;
	for(i = 0; i < num; i++){ // -1, 0
		IMMDevice *dev = NULL;
		PROPVARIANT value;
		IAudioClient *tmpClient = NULL;

		if(check_hresult_failed(IMMDeviceCollection_Item(pdc, i, &dev), "IMMDeviceCollection::Item()"))
			goto error1;	
		device[i+1].deviceID = i;
		if(check_hresult_failed(IMMDevice_OpenPropertyStore(dev, STGM_READ, &pps), "IMMDevice::OpenPropertyStore()"))
			goto error1;
		PropVariantInit(&value);
		if(check_hresult_failed(IPropertyStore_GetValue(pps, &PKEY_Device_FriendlyName, &value), "IPropertyStore::GetValue()")) {
			PropVariantClear(&value);
		}else{
			if(value.pwszVal)
#ifdef UNICODE
				WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, (int)wcslen(value.pwszVal), device[i+1].name, WASAPI_DEVLIST_LEN - 1, 0, 0);
#else
				WideCharToMultiByte(CP_ACP, 0, value.pwszVal, (int)wcslen(value.pwszVal), device[i+1].name, WASAPI_DEVLIST_LEN - 1, 0, 0);
#endif
			else
				_snprintf(device[i+1].name, WASAPI_DEVLIST_LEN - 1, "Device Error %d", i);
		}
		PropVariantClear(&value);
		LatencyMax = 100000;
		LatencyMin = 30000;
		if(SUCCEEDED(IMMDevice_Activate(dev, &tim_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&tmpClient))){
			if (FAILED(IAudioClient_GetDevicePeriod(tmpClient, &LatencyMax, &LatencyMin))) {
				LatencyMax = 100000;
				LatencyMin = 30000;
			}
		}
		LatencyMax /= 10000; // hns to ms
		LatencyMin /= 10000; // hns to ms
		if(LatencyMax > 1000)
			LatencyMax = 1000;
		if(LatencyMin < 0)
			LatencyMin = 1;
		device[i+1].LatencyMax = LatencyMax;
		device[i+1].LatencyMin = LatencyMin;
		if(tmpClient){
			IAudioClient_Release(tmpClient);
			tmpClient = NULL;
		}		
		if(dev){
			IMMDevice_Release(dev);
			dev = NULL;
		}
		if(pps){
			IPropertyStore_Release(pps);
			pps = NULL;
		}
	}
	if(pdc)
		IMMDeviceCollection_Release(pdc);
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return num + 1; // +1 def dev

error1:
	if(tmpClient)
		IAudioClient_Release(tmpClient);
	if(pdc){
		IMMDeviceCollection_Release(pdc);
	}
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return 1;
error0:
	return 0;
}


#endif /* AU_WASAPI */


