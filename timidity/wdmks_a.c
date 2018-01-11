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

    wdmks_a.c
	
	Functions to play sound using WDM Kernel Streeming (Windows 2000).
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */


#ifdef AU_WDMKS

#ifdef __W32__
#include "interface.h"
#endif
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
#include "wdmks_a.h"

int opt_wdmks_device_id = -1;
int opt_wdmks_format_ext = 1;
int opt_wdmks_exclusive = 0;
int opt_wdmks_priority = 0;

/*****************************************************************************************************************************/

static int  open_output     (void); /* 0=success, 1=warning, -1=fatal error */
static void close_output    (void);
static int  output_data     (const uint8 *buf, size_t nbytes);
static int  acntl           (int request, void *arg);
static int  detect          (void);

/*****************************************************************************************************************************/

#define dpm wdmks_play_mode

PlayMode dpm = {
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "WDM Kernel Streeming", 'k',
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
#include <initguid.h>
#include <objbase.h>
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#include <Avrt.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#define INITGUID
#include <mmdeviceapi.h>
#include <functiondiscoverykeys.h>
#undef INITGUID
#endif

#define SPEAKER_FRONT_LEFT        0x1
#define SPEAKER_FRONT_RIGHT       0x2
#define SPEAKER_FRONT_CENTER      0x4
#define SPEAKER_MONO	          (SPEAKER_FRONT_CENTER)
#define SPEAKER_STEREO	          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)

/*****************************************************************************************************************************/
/* RenderBuffer */

typedef struct RenderBufferNode
{
	struct RenderBufferNode *pNext;
	size_t CurrentSize;
	size_t Capacity;
	char Data[1];
} RenderBufferNode;

typedef struct RenderBuffer
{
	RenderBufferNode *pHead;
	RenderBufferNode *pTail;
	RenderBufferNode *pFree;
} RenderBuffer;

static int IsRenderBufferEmpty(RenderBuffer *pRenderBuffer)
{
	return !pRenderBuffer->pHead;
}

static void ClearRenderBuffer(RenderBuffer *pRenderBuffer)
{
	if(pRenderBuffer->pTail){
		/* prepend [pHead, ...,  pTail] to [pFree, ...] */
		pRenderBuffer->pTail->pNext = pRenderBuffer->pFree;
		pRenderBuffer->pFree = pRenderBuffer->pHead;
		pRenderBuffer->pHead = NULL;
		pRenderBuffer->pTail = NULL;
	}
}

static void DeleteRenderBuffer(RenderBuffer *pRenderBuffer)
{
	RenderBufferNode *pNode = NULL;

	ClearRenderBuffer(pRenderBuffer);
	pNode = pRenderBuffer->pFree;
	while(pNode){
		RenderBufferNode* pNext = pNode->pNext;
		free(pNode);
		pNode = pNext;
	}
	pRenderBuffer->pFree = NULL;
}

/* if *ppData == NULL, appends `size` count of zeros */
static void PushToRenderBufferPartial(RenderBufferNode *pNode, const uint8 **ppData, size_t *pSize)
{
	size_t pushLength = pNode->Capacity - pNode->CurrentSize;

	if(pushLength > *pSize)
		pushLength = *pSize;
	if(*ppData){
		memcpy(pNode->Data + pNode->CurrentSize, *ppData, pushLength);
		*ppData += pushLength;
	}else{
		memset(pNode->Data + pNode->CurrentSize, 0, pushLength);
	}
	*pSize -= pushLength;
	pNode->CurrentSize += pushLength;
}

/* if pData == NULL, appends `size` count of zeros */
static int PushToRenderBuffer(RenderBuffer *pRenderBuffer, const uint8 *pData, size_t size)
{
	while (size > 0){
		if(pRenderBuffer->pTail && pRenderBuffer->pTail->CurrentSize < pRenderBuffer->pTail->Capacity){
			PushToRenderBufferPartial(pRenderBuffer->pTail, &pData, &size);
		}else if(pRenderBuffer->pFree){
			RenderBufferNode *pNode = pRenderBuffer->pFree;

			pRenderBuffer->pFree = pRenderBuffer->pFree->pNext;
			pNode->CurrentSize = 0;
			pNode->pNext = NULL;
			if(pRenderBuffer->pTail){
				pRenderBuffer->pTail->pNext = pNode;
				pRenderBuffer->pTail = pNode;
			}else{
				pRenderBuffer->pHead = pNode;
				pRenderBuffer->pTail = pNode;
			}
			PushToRenderBufferPartial(pNode, &pData, &size);
		}else{
			size_t capacity = size * 4;
			RenderBufferNode *pNewNode = (RenderBufferNode *)safe_malloc(sizeof(RenderBufferNode) + capacity);

			if(!pNewNode)
				return FALSE;
			if(pRenderBuffer->pTail)
				pRenderBuffer->pTail->pNext = pNewNode;
			else
				pRenderBuffer->pHead = pNewNode;
			pRenderBuffer->pTail = pNewNode;
			pRenderBuffer->pTail->pNext = NULL;
			pRenderBuffer->pTail->Capacity = capacity;
			pRenderBuffer->pTail->CurrentSize = 0;
			PushToRenderBufferPartial(pRenderBuffer->pTail, &pData, &size);
		}
	}
	return TRUE;
}

static int PushZeroToRenderBuffer(RenderBuffer *pRenderBuffer, size_t size)
{
	return PushToRenderBuffer(pRenderBuffer, NULL, size);
}

static size_t CalculateRenderBufferSize(RenderBuffer *pRenderBuffer, size_t maxSize)
{
	size_t size = 0;
	RenderBufferNode *pNode = pRenderBuffer->pHead;

	while(size < maxSize && pNode){
		size += pNode->CurrentSize;
		pNode = pNode->pNext;
	}
	if(size > maxSize)
		size = maxSize;
	return size;
}

static size_t ReadRenderBuffer(RenderBuffer *pRenderBuffer, char *pBuffer, size_t size)
{
	size_t copySize, copiedLength = 0;
	RenderBufferNode *pNode = pRenderBuffer->pHead;

	while(size > 0 && pNode){
		copySize = pNode->CurrentSize;
		if(copySize > size)
			copySize = size;
		memcpy(pBuffer, pNode->Data, copySize);
		pBuffer += copySize;
		size -= copySize;
		copiedLength += copySize;
		pNode = pNode->pNext;
	}
	return copiedLength;
}

static size_t PopRenderBuffer(RenderBuffer *pRenderBuffer, size_t size)
{
	size_t popSize, actualLength = 0;

	while (size > 0 && pRenderBuffer->pHead){
		popSize = pRenderBuffer->pHead->CurrentSize;
		if(pRenderBuffer->pHead->CurrentSize > size){
			popSize = size;
			memmove(pRenderBuffer->pHead->Data, pRenderBuffer->pHead->Data + size, pRenderBuffer->pHead->CurrentSize - size);
			pRenderBuffer->pHead->CurrentSize -= size;
		}else{
			/* prepend pHead to [pFree, ...] */
			RenderBufferNode *pNext = pRenderBuffer->pHead->pNext;
			pRenderBuffer->pHead->pNext = pRenderBuffer->pFree;
			pRenderBuffer->pFree = pRenderBuffer->pHead;
			pRenderBuffer->pHead = pNext;
			if(!pNext)
				pRenderBuffer->pTail = NULL;
		}
		size -= popSize;
		actualLength += popSize;
	}
	return actualLength;
}

/*****************************************************************************************************************************/

static HANDLE hEventTcv = NULL;
static HANDLE hRenderThread = NULL;
static IMMDevice* pMMDevice = NULL;
static IAudioClient* pAudioClient = NULL;
static IAudioRenderClient* pAudioRenderClient = NULL;
static RenderBuffer Buffers = {0, 0, 0};
static UINT32 FrameBytes = 0;
static UINT32 BufferFrames = 0;
static int IsExclusive = 0;
static int IsStarted = 0;
static int IsOpened = 0;
static int IsThreadExit = 0;

static int WriteBuffer(void)
{
	UINT32 padding = 0;
	size_t numberOfBytesToCopy, numberOfFramesToCopy;
	BYTE *pData = NULL;

	if(!IsExclusive)
		if(FAILED(IAudioClient_GetCurrentPadding(pAudioClient, &padding)))
			return FALSE;
	numberOfBytesToCopy = CalculateRenderBufferSize(&Buffers, (BufferFrames - padding) * FrameBytes);
	numberOfFramesToCopy = numberOfBytesToCopy / FrameBytes;
	if(IsExclusive && numberOfFramesToCopy < BufferFrames){
		if(!PushZeroToRenderBuffer(&Buffers, BufferFrames * FrameBytes - numberOfBytesToCopy))
			return FALSE;
		numberOfBytesToCopy = BufferFrames * FrameBytes;
		numberOfFramesToCopy = numberOfBytesToCopy / FrameBytes;
	}else if(numberOfFramesToCopy == 0){
		if(!PushZeroToRenderBuffer(&Buffers, FrameBytes - numberOfBytesToCopy))
			return FALSE;
		numberOfBytesToCopy = FrameBytes;
		numberOfFramesToCopy = 1;
	}
	if(FAILED(IAudioRenderClient_GetBuffer(pAudioRenderClient, numberOfFramesToCopy, &pData)))
		return FALSE;
	PopRenderBuffer(&Buffers, ReadRenderBuffer(&Buffers, (char*)pData, numberOfFramesToCopy * FrameBytes));
	IAudioRenderClient_ReleaseBuffer(pAudioRenderClient, numberOfFramesToCopy, 0);
	return TRUE;
}

static unsigned int WINAPI RenderThread(void *arglist)
{
	int ret = 1;
	HANDLE waitArray[2] = {hEventTcv, hEventTcv};
	HANDLE hMmCss = NULL;
	DWORD mmCssTaskIndex = 0;
	
	if(FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
		return 1;
	hMmCss = AvSetMmThreadCharacteristics(_T("Audio"), &mmCssTaskIndex);
	if(!hMmCss)
		goto thread_exit;	
	for(;;){		
		WaitForSingleObject(hEventTcv, INFINITE);
		ResetEvent(hEventTcv);
		if(IsThreadExit) break;		
		EnterCriticalSection(&critSect);
		WriteBuffer();
		LeaveCriticalSection(&critSect);
	}	
	ret = 0;
thread_exit:
	if(hMmCss)
		AvRevertMmThreadCharacteristics(hMmCss);
	CoUninitialize();
	return ret;
}

static void stop(void)
{
	if(!IsOpened)
		return;
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
	DeleteRenderBuffer(&Buffers);
}

static void reset(void)
{
	stop();
	BufferFrames = 0;
	FrameBytes = 0;
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
}

static int start(void)
{
	BYTE *pData;

	IsThreadExit = 0;
	if(!hRenderThread){
		hRenderThread = (HANDLE)_beginthreadex(NULL, 0, &RenderThread, NULL, 0, NULL);
		if(!hRenderThread)
			goto error;
	}
	if(!IsStarted){
		if(IsRenderBufferEmpty(&Buffers)){
			if(FAILED(IAudioRenderClient_GetBuffer(pAudioRenderClient, BufferFrames, &pData)))
				goto error;
			IAudioRenderClient_ReleaseBuffer(pAudioRenderClient, BufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
		}else{
			if(!WriteBuffer())
				goto error;
		}
		if(FAILED(IAudioClient_Start(pAudioClient)))
			goto error;
		IsStarted = TRUE;
	}
	return 0;
error:
	reset();
	return -1;
}

/*****************************************************************************************************************************/

/* returns TRUE on success */
static int get_default_device(IMMDevice** ppMMDevice)
{
	IMMDeviceEnumerator *pEnumerator = NULL;

	if(FAILED(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void**)&pEnumerator)))
		goto error;
	if(FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, ppMMDevice)))
		goto error;
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

	if(devnum > WDMKS_DEVLIST_MAX - 2)
		devnum = -1;
	if(devnum < 0)
		return get_default_device(ppMMDevice);		
	if(FAILED(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void **)&pde)))
		goto error;
	if(FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(pde, eRender, DEVICE_STATE_ACTIVE, &pdc)))
		goto error;	
	if(FAILED(IMMDeviceCollection_GetCount(pdc, &num)))
		goto error;
	if(num <= 0)
		goto error;
	if(num > WDMKS_DEVLIST_MAX - 2)
		num = WDMKS_DEVLIST_MAX - 2;
	if(devnum > num)
		goto error;
	if(FAILED(IMMDeviceCollection_Item(pdc, devnum, &pdev)))
		goto error;  
	if(FAILED(IMMDevice_GetId(pdev, &pszDeviceId)))
		goto error;
	if(FAILED(IMMDeviceEnumerator_GetDevice(pde, pszDeviceId, ppMMDevice)))
		goto error; 		
	if(pszDeviceId)
		CoTaskMemFree(pszDeviceId);
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return TRUE;

error:	
	if(pszDeviceId)
		CoTaskMemFree(pszDeviceId);
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return FALSE;
}

static int get_winver()
{
	DWORD winver, major, minor;
	int ver = 0;

	winver = GetVersion();		
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
	return ver;
}

/*****************************************************************************************************************************/
/* interface function */

/* 0=success, 1=warning, -1=fatal error */
int open_output(void)
{
	HRESULT hr;
	int include_enc, exclude_enc;
	WAVEFORMATEXTENSIBLE wfe = {0};
	WAVEFORMATEX *pwf = NULL;
	AUDCLNT_SHAREMODE ShareMode;
	uint32 StreamFlags;
	REFERENCE_TIME BufferDuration, Periodicity;	
	GUID guid_WAVE_FORMAT = {WAVE_FORMAT_UNKNOWN,0x0000,0x0010,{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

	
    DWORD defaultInDevPathSize = 0;
    DWORD defaultOutDevPathSize = 0;
    wchar_t* defaultInDevPath = 0;
    wchar_t* defaultOutDevPath = 0;


	reset();

	
    if( waveOutMessage(0, DRV_QUERYDEVICEINTERFACESIZE, (DWORD_PTR)&defaultOutDevPathSize, 0 ) == MMSYSERR_NOERROR )
    {
        defaultOutDevPath = (wchar_t *)PaUtil_AllocateMemory((defaultOutDevPathSize + 1) * sizeof(wchar_t));
        waveOutMessage(0, DRV_QUERYDEVICEINTERFACE, (DWORD_PTR)defaultOutDevPath, defaultOutDevPathSize);
    }

	//hEventTcv = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	hEventTcv = CreateEvent(NULL,FALSE,FALSE,NULL); // reset manual
	if(!hEventTcv)
		goto error;
	if(!get_device(&pMMDevice, opt_output_device_id == -3 ? opt_wdmks_device_id : opt_output_device_id))
		goto error;
	if(FAILED(IMMDevice_Activate(pMMDevice, &tim_IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&pAudioClient)))
		goto error;	

#if 0 // test format
	{
	IPropertyStore *pps = NULL;
	PROPVARIANT value;
	int cpsz;

	if(FAILED(IMMDevice_OpenPropertyStore(pMMDevice, STGM_READ, &pps)))
		goto error;
    PropVariantInit(&value);
    if(FAILED(IPropertyStore_GetValue(pps, &PKEY_AudioEngine_DeviceFormat, &value)))
		goto error;
	cpsz = min(sizeof(WAVEFORMATEXTENSIBLE), value.blob.cbSize);
	memcpy(&wfe, value.blob.pBlobData, cpsz);
    PropVariantClear(&value);
	if(pps){
		pps->lpVtbl->Release(pps); 
		pps = NULL;
	}
	if(cpsz == sizeof(WAVEFORMATEX)){
		pwf = (WAVEFORMATEX *)&wfe;
		opt_wdmks_format_ext = 0;
	}else{
		pwf = &wfe.Format;
		opt_wdmks_format_ext = 1;
	}
	}
#endif

	include_enc = PE_SIGNED;
	exclude_enc = PE_BYTESWAP | PE_ULAW | PE_ALAW;	
	if(!(dpm.encoding & (PE_F64BIT | PE_64BIT | PE_F32BIT | PE_32BIT | PE_24BIT | PE_16BIT))) // 8bit
		include_enc |= PE_16BIT;
	if(opt_wdmks_format_ext && (dpm.encoding & PE_24BIT))
		include_enc |= PE_4BYTE;
	else
		exclude_enc |= PE_4BYTE;
	dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);
	
	if(opt_wdmks_format_ext){
		pwf = &wfe.Format;
		memcpy(&wfe.SubFormat, &guid_WAVE_FORMAT, sizeof(GUID));
	}else{
		pwf = (WAVEFORMATEX *)&wfe;
		memset(pwf, 0, sizeof(WAVEFORMATEX));
	}

	if(dpm.encoding & PE_16BIT){
		if(dpm.encoding & PE_4BYTE){
			if(opt_wdmks_format_ext){
				wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
				wfe.Samples.wValidBitsPerSample = 16;
				pwf->wBitsPerSample = (WORD) 32;			
			}else{
				pwf->wFormatTag = WAVE_FORMAT_PCM;
				pwf->wBitsPerSample = (WORD) 16;
			}
		}else{
			if(opt_wdmks_format_ext){
				wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
				wfe.Samples.wValidBitsPerSample = 16;
				pwf->wBitsPerSample = (WORD) 16;			
			}else{
				pwf->wFormatTag = WAVE_FORMAT_PCM;
				pwf->wBitsPerSample = (WORD) 16;
			}
		}
	}else if(dpm.encoding & PE_24BIT){
		if(dpm.encoding & PE_4BYTE){
			if(opt_wdmks_format_ext){
				wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
				wfe.Samples.wValidBitsPerSample = 24;
				pwf->wBitsPerSample = (WORD) 32;			
			}else{
				pwf->wFormatTag = WAVE_FORMAT_PCM;
				pwf->wBitsPerSample = (WORD) 24;
			}
		}else{
			if(opt_wdmks_format_ext){
				wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
				wfe.Samples.wValidBitsPerSample = 24;
				pwf->wBitsPerSample = (WORD) 24;			
			}else{
				pwf->wFormatTag = WAVE_FORMAT_PCM;
				pwf->wBitsPerSample = (WORD) 24;
			}
		}
	}else if(dpm.encoding & PE_32BIT){
		if(opt_wdmks_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 32;
			pwf->wBitsPerSample = (WORD) 32;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 32;
		}
	}else if(dpm.encoding & PE_F32BIT){
		if(opt_wdmks_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
			wfe.Samples.wValidBitsPerSample = 32;
			pwf->wBitsPerSample = (WORD) 32;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			pwf->wBitsPerSample = (WORD) 32;
		}
	}else if(dpm.encoding & PE_64BIT){
		if(opt_wdmks_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 64;
			pwf->wBitsPerSample = (WORD) 64;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 64;
		}
	}else if(dpm.encoding & PE_F64BIT){
		if(opt_wdmks_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
			wfe.Samples.wValidBitsPerSample = 64;
			pwf->wBitsPerSample = (WORD) 64;			
		}else{
			pwf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			pwf->wBitsPerSample = (WORD) 64;
		}
	}else{ // 8bit // error
		pwf->wFormatTag = WAVE_FORMAT_PCM; // only WAVEFORMATEX
		pwf->wBitsPerSample = (WORD) 8;
	}
	pwf->nChannels       = (WORD)dpm.encoding & PE_MONO ? 1 : 2;
	pwf->nSamplesPerSec  = (DWORD)dpm.rate;
	pwf->nBlockAlign     = (WORD)(pwf->nChannels * pwf->wBitsPerSample / 8);
	pwf->nAvgBytesPerSec = (DWORD)pwf->nSamplesPerSec * pwf->nBlockAlign;
	if(opt_wdmks_format_ext){
		wfe.Samples.wValidBitsPerSample = pwf->wBitsPerSample; // union
		wfe.Samples.wSamplesPerBlock = 1;
		wfe.Samples.wReserved = wfe.Samples.wValidBitsPerSample;
		wfe.dwChannelMask = pwf->nChannels==1 ? SPEAKER_MONO : SPEAKER_STEREO;
		pwf->cbSize       = (WORD)22;
	}else{
		pwf->cbSize       = (WORD)0;
	}

	FrameBytes = pwf->nBlockAlign;

	if(dpm.extra_param[0] < 2){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,X", dpm.extra_param[0]);
		dpm.extra_param[0] = 2;
	}
	if(audio_buffer_bits < 5){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: X,%d", audio_buffer_bits);
		audio_buffer_bits = 5;
	}

#ifdef __IAudioClient2_INTERFACE_DEFINED__	
	{
		int ver = get_winver();

		if(ver >= 3) // win8ˆÈã
		{
			AudioClientProperties acp = {0};
			acp.cbSize     = sizeof(AudioClientProperties);
			acp.bIsOffload = FALSE;
			acp.eCategory  = opt_wdmks_stream_category;
		
			if(opt_wdmks_stream_option >= 2){
				if(ver >= 6) // win10ˆÈã
					acp.Options = AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;
			}else if(opt_wdmks_stream_option == 1){
				if(ver >= 6) // win8.1ˆÈã
					acp.Options = AUDCLNT_STREAMOPTIONS_RAW;
			}
			hr = IAudioClient2_SetClientProperties((IAudioClient2 *)pAudioClient, (AudioClientProperties *)&acp);
				goto error;
		}
	}
#endif

	IsExclusive = opt_wdmks_exclusive;
	
	ShareMode = IsExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
	StreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;// | AUDCLNT_STREAMFLAGS_NOPERSIST;
	BufferDuration = (IsExclusive ? 10 : 30) * 10000; // 10ms,30ms
	Periodicity = IsExclusive ? BufferDuration : 0;

	hr = IAudioClient_Initialize(pAudioClient, ShareMode, StreamFlags, BufferDuration, Periodicity,	(WAVEFORMATEX *)&wfe, NULL);
	if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED){
		UINT32 bufferSize;

		if(FAILED(IAudioClient_GetBufferSize(pAudioClient, &bufferSize)))
			goto error;
		IAudioClient_Release(pAudioClient);
		pAudioClient = NULL;
		BufferDuration = (REFERENCE_TIME)(10000.0f * 1000 * bufferSize / pwf->nSamplesPerSec + 0.5);
		if(FAILED(IMMDevice_Activate(pMMDevice, &tim_IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&pAudioClient)))
			goto error;
		hr = IAudioClient_Initialize(pAudioClient, ShareMode, StreamFlags, BufferDuration, Periodicity,	(WAVEFORMATEX *)&wfe, NULL);
	}
	if(FAILED(hr))
		goto error;
	if(FAILED(IAudioClient_GetBufferSize(pAudioClient, &BufferFrames)))
		goto error;
	if(FAILED(IAudioClient_SetEventHandle(pAudioClient, hEventTcv)))
		goto error;
	if(FAILED(IAudioClient_GetService(pAudioClient, &tim_IID_IAudioRenderClient, (void**)&pAudioRenderClient)))
		goto error;
	IsOpened = 1;
	return 0;
error:
	reset();
	return -1;
}

void close_output(void)
{
	reset();
	IsOpened = 0;
}

int output_data(const uint8 *buf, size_t nbytes)
{
	int flg = 0;

	if(!IsOpened) return -1;
	EnterCriticalSection(&critSect);
	flg = PushToRenderBuffer(&Buffers, buf, nbytes);
	LeaveCriticalSection(&critSect);
	if(flg)
		return 0;
	reset();
	return -1;
}

int acntl(int request, void *arg)
{
	switch(request){
	//case PM_REQ_GETFRAGSIZ:
	//	return 0;
	//case PM_REQ_GETQSIZ:
	//	if(BufferFrames <= 0)
	//		return -1;
	//	*(int *)arg = BufferFrames ;
	//	return 0;
//	case PM_REQ_GETFILLED:
//		return 0;
	case PM_REQ_DISCARD:
		stop();
		return 0;
	case PM_REQ_FLUSH:
		while (!IsRenderBufferEmpty(&Buffers))
			WaitForSingleObject(hRenderThread, 10);
		return 0;
	case PM_REQ_PLAY_START:
		return start();
	case PM_REQ_PLAY_END:
		stop();
		return 0;
	default:
		return -1;
	}
}

int detect(void)
{
	IMMDevice *pMMDevice = NULL;
	int result = get_default_device(&pMMDevice);

	if(pMMDevice)
		IMMDevice_Release(pMMDevice);
	return result;
}

/*****************************************************************************************************************************/

int wdmks_device_list(WDMKS_DEVICELIST *device)
{
	int i;
	UINT num;
	HRESULT hr;
	IMMDeviceEnumerator *pde = NULL;
	IMMDeviceCollection *pdc = NULL;
	IPropertyStore *pps = NULL;
	
	device[0].deviceID = -1;
//	device[0].LatencyMax = 100000;
//	device[01].LatencyMin = 30000;
	strcpy(device[0].name, "Default Render Device");

	if(detect() == FALSE)
		goto error0;
	if(FAILED(CoCreateInstance(&tim_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &tim_IID_IMMDeviceEnumerator, (void **)&pde)))
		goto error1;	
	if(FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(pde, eRender, DEVICE_STATE_ACTIVE, &pdc)))
		goto error1;
	if(FAILED(IMMDeviceCollection_GetCount(pdc, &num)))
		goto error1;
	if(num <= 0)
		goto error1;
	if(num > WDMKS_DEVLIST_MAX - 2)
		num = WDMKS_DEVLIST_MAX - 2;
	for(i = 0; i < num; i++){ // -1, 0
		IMMDevice *dev = NULL;
		PROPVARIANT value;
	//	IAudioClient *tmpClient = NULL;

		if(FAILED(IMMDeviceCollection_Item(pdc, i, &dev)))
			goto error1;	
		device[i+1].deviceID = i;
		if(FAILED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &pps)))
			goto error1;
		PropVariantInit(&value);
		if(FAILED(IPropertyStore_GetValue(pps, &PKEY_Device_FriendlyName, &value))){
			PropVariantClear(&value);
		}else{
			if(value.pwszVal)
				WideCharToMultiByte(CP_ACP, 0, value.pwszVal, (int)wcslen(value.pwszVal), device[i+1].name, WDMKS_DEVLIST_LEN - 1, 0, 0);
			else
				_snprintf(device[i+1].name, WDMKS_DEVLIST_LEN - 1, "Device Error %d", i);
		}
		PropVariantClear(&value);
		//if(FAILED(IMMDevice_Activate(dev, &tim_IID__IAudioClient, CLSCTX_ALL, NULL, (void **)tmpClient))){
		//	device[i+1].LatencyMax = 100000;
		//	device[i+1].LatencyMin = 30000;
		//}else if(FAILED(IAudioClient_GetDevicePeriod(tmpClient, &device[i+1].LatencyMax, &device[i+1].LatencyMin))){
		//	device[i+1].LatencyMax = 100000;
		//	device[i+1].LatencyMin = 30000;
		//}
		//if(tmpClient)
		//	tmpClient->lpVtbl->Release(tmpClient);
		if(pps){
			pps->lpVtbl->Release(pps); 
			pps = NULL;
		}
	}
	if(pdc)
		pdc->lpVtbl->Release(pdc); 
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return num + 1; // +1 def dev

error1:
	if(pdc)
		pdc->lpVtbl->Release(pdc); 
	if(pde)
		IMMDeviceEnumerator_Release(pde);
	return 1;
error0:
	return 0;
}


#endif /* AU_WDMKS */


