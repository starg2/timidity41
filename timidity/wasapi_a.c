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

Functions to play sound using WASAPI (Windows 7+).

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

#ifdef AU_WASAPI

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
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

#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <avrt.h>
#include "timidity.h"
#include "output.h"

const CLSID MyCLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const IID MyIID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const IID MyIID_IAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
const IID MyIID_IAudioRenderClient = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};

int WASAPIOpenOutputShared(void);
int WASAPIOpenOutputExclusive(void);
void WASAPICloseOutput(void);
int WASAPIOutputData(char* pData, int32 size);
int WASAPIACntl(int request, void* pArg);
int WASAPIDetect(void);

PlayMode wasapi_shared_play_mode = {
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "WASAPI (Shared)", 'x',
    NULL,
    &WASAPIOpenOutputShared,
    &WASAPICloseOutput,
    &WASAPIOutputData,
    &WASAPIACntl,
    &WASAPIDetect
};

PlayMode wasapi_exclusive_play_mode = {
    DEFAULT_RATE,
    PE_16BIT | PE_SIGNED,
    PF_PCM_STREAM | PF_CAN_TRACE | PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "WASAPI (Exclusive)", 'X',
    NULL,
    &WASAPIOpenOutputExclusive,
    &WASAPICloseOutput,
    &WASAPIOutputData,
    &WASAPIACntl,
    &WASAPIDetect
};

PlayMode* GetWASAPIPlayModeInfo(int isExclusive)
{
    return isExclusive ? &wasapi_exclusive_play_mode : &wasapi_shared_play_mode;
}

/* RenderBuffer */

typedef struct RenderBufferNode
{
    struct RenderBufferNode* pNext;
    size_t CurrentSize;
    size_t Capacity;
    char Data[1];
} RenderBufferNode;

typedef struct RenderBuffer
{
    RenderBufferNode* pHead;
    RenderBufferNode* pTail;
    RenderBufferNode* pFree;
} RenderBuffer;

int IsRenderBufferEmpty(RenderBuffer* pRenderBuffer)
{
    return !pRenderBuffer->pHead;
}

void ClearRenderBuffer(RenderBuffer* pRenderBuffer)
{
    if (pRenderBuffer->pTail)
    {
        /* prepend [pHead, ...,  pTail] to [pFree, ...] */
        pRenderBuffer->pTail->pNext = pRenderBuffer->pFree;
        pRenderBuffer->pFree = pRenderBuffer->pHead;
        pRenderBuffer->pHead = NULL;
        pRenderBuffer->pTail = NULL;
    }
}

void DeleteRenderBuffer(RenderBuffer* pRenderBuffer)
{
    ClearRenderBuffer(pRenderBuffer);

    {
        RenderBufferNode* pNode = pRenderBuffer->pFree;

        while (pNode)
        {
            RenderBufferNode* pNext = pNode->pNext;
            free(pNode);
            pNode = pNext;
        }
    }

    pRenderBuffer->pFree = NULL;
}

void PushToRenderBufferPartial(RenderBufferNode* pNode, const char** ppData, size_t* pSize)
{
    size_t copyLength = pNode->Capacity - pNode->CurrentSize;

    if (copyLength > *pSize)
    {
        copyLength = *pSize;
    }

    memcpy(pNode->Data + pNode->CurrentSize, *ppData, copyLength);
    *ppData += copyLength;
    *pSize -= copyLength;
    pNode->CurrentSize += copyLength;
}

int PushToRenderBuffer(RenderBuffer* pRenderBuffer, const char* pData, size_t size)
{
    while (size > 0)
    {
        if (pRenderBuffer->pTail && pRenderBuffer->pTail->CurrentSize < pRenderBuffer->pTail->Capacity)
        {
            PushToRenderBufferPartial(pRenderBuffer->pTail, &pData, &size);
        }
        else if (pRenderBuffer->pFree)
        {
            RenderBufferNode* pNode = pRenderBuffer->pFree;
            pRenderBuffer->pFree = pRenderBuffer->pFree->pNext;
            pNode->CurrentSize = 0;
            pNode->pNext = NULL;

            if (pRenderBuffer->pTail)
            {
                pRenderBuffer->pTail->pNext = pNode;
                pRenderBuffer->pTail = pNode;
            }
            else
            {
                pRenderBuffer->pHead = pNode;
                pRenderBuffer->pTail = pNode;
            }

            PushToRenderBufferPartial(pNode, &pData, &size);
        }
        else
        {
            size_t capacity = size * 4;
            RenderBufferNode* pNewNode = malloc(sizeof(RenderBufferNode) + capacity);

            if (!pNewNode)
            {
                return FALSE;
            }

            if (pRenderBuffer->pTail)
            {
                pRenderBuffer->pTail->pNext = pNewNode;
            }
            else
            {
                pRenderBuffer->pHead = pNewNode;
            }

            pRenderBuffer->pTail = pNewNode;
            pRenderBuffer->pTail->pNext = NULL;
            pRenderBuffer->pTail->Capacity = capacity;
            pRenderBuffer->pTail->CurrentSize = 0;
            PushToRenderBufferPartial(pRenderBuffer->pTail, &pData, &size);
        }
    }

    return TRUE;
}

size_t CalculateRenderBufferSize(RenderBuffer* pRenderBuffer, size_t maxSize)
{
    size_t size = 0;
    RenderBufferNode* pNode = pRenderBuffer->pHead;

    while (size < maxSize && pNode)
    {
        size += pNode->CurrentSize;
        pNode = pNode->pNext;
    }

    if (size > maxSize)
    {
        size = maxSize;
    }

    return size;
}

size_t ReadRenderBuffer(RenderBuffer* pRenderBuffer, char* pBuffer, size_t size)
{
    size_t copiedLength = 0;
    RenderBufferNode* pNode = pRenderBuffer->pHead;

    while (size > 0 && pNode)
    {
        size_t copySize = pNode->CurrentSize;

        if (copySize > size)
        {
            copySize = size;
        }

        memcpy(pBuffer, pNode->Data, copySize);
        pBuffer += copySize;
        size -= copySize;
        copiedLength += copySize;
        pNode = pNode->pNext;
    }

    return copiedLength;
}

size_t PopRenderBuffer(RenderBuffer* pRenderBuffer, size_t size)
{
    size_t actualLength = 0;

    while (size > 0 && pRenderBuffer->pHead)
    {
        size_t popSize = pRenderBuffer->pHead->CurrentSize;

        if (pRenderBuffer->pHead->CurrentSize > size)
        {
            popSize = size;
            memmove(
                pRenderBuffer->pHead->Data,
                pRenderBuffer->pHead->Data + size,
                pRenderBuffer->pHead->CurrentSize - size
            );
            pRenderBuffer->pHead->CurrentSize -= size;
        }
        else
        {
            /* prepend pHead to [pFree, ...] */
            RenderBufferNode* pNext = pRenderBuffer->pHead->pNext;
            pRenderBuffer->pHead->pNext = pRenderBuffer->pFree;
            pRenderBuffer->pFree = pRenderBuffer->pHead;
            pRenderBuffer->pHead = pNext;

            if (!pNext)
            {
                pRenderBuffer->pTail = NULL;
            }
        }

        size -= popSize;
        actualLength += popSize;
    }

    return actualLength;
}

/* WASAPIContext */

typedef struct WASAPIContext
{
    int IsExclusive;
    HANDLE hShutdownEvent;
    HANDLE hAudioSamplesReadyEvent;
    IMMDevice* pMMDevice;
    IAudioClient* pAudioClient;
    IAudioRenderClient* pAudioRenderClient;
    UINT32 FrameSize;
    UINT32 BufferSizeInFrames;
    RenderBuffer Buffer;
    HANDLE hRenderThread;
    int IsStarted;
} WASAPIContext;

WASAPIContext g_WASAPIContext;

void WASAPIDoStop(void)
{
    if (g_WASAPIContext.IsStarted)
    {
        if (g_WASAPIContext.pAudioClient)
        {
            IAudioClient_Stop(g_WASAPIContext.pAudioClient);
        }

        g_WASAPIContext.IsStarted = FALSE;
    }

    if (g_WASAPIContext.hRenderThread)
    {
        if (g_WASAPIContext.hShutdownEvent)
        {
            SetEvent(g_WASAPIContext.hShutdownEvent);
            WaitForSingleObject(g_WASAPIContext.hRenderThread, INFINITE);
        }

        CloseHandle(g_WASAPIContext.hRenderThread);
        g_WASAPIContext.hRenderThread = NULL;
    }

    DeleteRenderBuffer(&g_WASAPIContext.Buffer);
}

void ResetWASAPIContext(void)
{
    WASAPIDoStop();

    g_WASAPIContext.BufferSizeInFrames = 0;
    g_WASAPIContext.FrameSize = 0;

    if (g_WASAPIContext.pAudioRenderClient)
    {
        IAudioRenderClient_Release(g_WASAPIContext.pAudioRenderClient);
        g_WASAPIContext.pAudioRenderClient = NULL;
    }

    if (g_WASAPIContext.pAudioClient)
    {
        IAudioClient_Release(g_WASAPIContext.pAudioClient);
        g_WASAPIContext.pAudioClient = NULL;
    }

    if (g_WASAPIContext.pMMDevice)
    {
        IMMDevice_Release(g_WASAPIContext.pMMDevice);
        g_WASAPIContext.pMMDevice = NULL;
    }

    if (g_WASAPIContext.hAudioSamplesReadyEvent)
    {
        CloseHandle(g_WASAPIContext.hAudioSamplesReadyEvent);
        g_WASAPIContext.hAudioSamplesReadyEvent = NULL;
    }

    if (g_WASAPIContext.hShutdownEvent)
    {
        CloseHandle(g_WASAPIContext.hShutdownEvent);
        g_WASAPIContext.hShutdownEvent = NULL;
    }
}

/* returns TRUE on success */
int WASAPIGetDefaultDevice(IMMDevice** ppMMDevice)
{
    int ret = FALSE;
    IMMDeviceEnumerator* pEnumerator = NULL;

    if (FAILED(CoCreateInstance(&MyCLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &MyIID_IMMDeviceEnumerator, (void**)&pEnumerator)))
    {
        goto LExit;
    }

    if (FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, ppMMDevice)))
    {
        goto LExit;
    }

    ret = TRUE;

LExit:
    if (pEnumerator)
    {
        IMMDeviceEnumerator_Release(pEnumerator);
    }

    return ret;
}

/* return 0 on success, -1 on failure */
int WASAPIOpenOutput(int isExclusive)
{
    int ret = -1;

    ResetWASAPIContext();
    g_WASAPIContext.IsExclusive = isExclusive;

    g_WASAPIContext.hShutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

    if (!g_WASAPIContext.hShutdownEvent)
    {
        ResetWASAPIContext();
        return -1;
    }

    g_WASAPIContext.hAudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

    if (!g_WASAPIContext.hAudioSamplesReadyEvent)
    {
        ResetWASAPIContext();
        return -1;
    }

    if (!WASAPIGetDefaultDevice(&g_WASAPIContext.pMMDevice))
    {
        ResetWASAPIContext();
        return -1;
    }

    if (FAILED(IMMDevice_Activate(g_WASAPIContext.pMMDevice, &MyIID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&g_WASAPIContext.pAudioClient)))
    {
        ResetWASAPIContext();
        return -1;
    }

    {
        REFERENCE_TIME bufferDuration = (isExclusive ? 10 : 30) /* ms */ * 10000;
        PlayMode* pPlayMode = GetWASAPIPlayModeInfo(isExclusive);
        WAVEFORMATEX waveFormat = {0};
        HRESULT hr;

        g_WASAPIContext.FrameSize = get_encoding_sample_size(pPlayMode->encoding);

        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = pPlayMode->encoding & PE_MONO ? 1 : 2;
        waveFormat.nSamplesPerSec = pPlayMode->rate;
        waveFormat.nBlockAlign = g_WASAPIContext.FrameSize;
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        waveFormat.wBitsPerSample = pPlayMode->encoding & PE_24BIT ? 24 : pPlayMode->encoding & PE_16BIT ? 16 : 8;
        waveFormat.cbSize = 0;


        hr = IAudioClient_Initialize(
            g_WASAPIContext.pAudioClient,
            isExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            bufferDuration,
            isExclusive ? bufferDuration : 0,
            &waveFormat,
            NULL
        );

        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
        {
            UINT32 bufferSize;

            if (FAILED(IAudioClient_GetBufferSize(g_WASAPIContext.pAudioClient, &bufferSize)))
            {
                ResetWASAPIContext();
                return -1;
            }

            IAudioClient_Release(g_WASAPIContext.pAudioClient);
            g_WASAPIContext.pAudioClient = NULL;

            bufferDuration = (REFERENCE_TIME)(10000.0f * 1000 * bufferSize / waveFormat.nSamplesPerSec + 0.5);

            if (FAILED(IMMDevice_Activate(g_WASAPIContext.pMMDevice, &MyIID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&g_WASAPIContext.pAudioClient)))
            {
                ResetWASAPIContext();
                return -1;
            }

            hr = IAudioClient_Initialize(
                g_WASAPIContext.pAudioClient,
                isExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                bufferDuration,
                isExclusive ? bufferDuration : 0,
                &waveFormat,
                NULL
            );
        }

        if (FAILED(hr))
        {
            ResetWASAPIContext();
            return -1;
        }
    }

    if (FAILED(IAudioClient_GetBufferSize(g_WASAPIContext.pAudioClient, &g_WASAPIContext.BufferSizeInFrames)))
    {
        ResetWASAPIContext();
        return -1;
    }

    if (FAILED(IAudioClient_SetEventHandle(g_WASAPIContext.pAudioClient, g_WASAPIContext.hAudioSamplesReadyEvent)))
    {
        ResetWASAPIContext();
        return -1;
    }

    if (FAILED(IAudioClient_GetService(g_WASAPIContext.pAudioClient, &MyIID_IAudioRenderClient, (void**)&g_WASAPIContext.pAudioRenderClient)))
    {
        ResetWASAPIContext();
        return -1;
    }

    return 0;
}

int WASAPIDoWriteBuffer(void)
{
    UINT32 padding = 0;

    if (!g_WASAPIContext.IsExclusive)
    {
        if (FAILED(IAudioClient_GetCurrentPadding(g_WASAPIContext.pAudioClient, &padding)))
        {
            return FALSE;
        }
    }

    {
        size_t numberOfBytesToCopy = CalculateRenderBufferSize(&g_WASAPIContext.Buffer, (g_WASAPIContext.BufferSizeInFrames - padding) * g_WASAPIContext.FrameSize);
        size_t numberOfFramesToCopy = numberOfBytesToCopy / g_WASAPIContext.FrameSize;

        if (numberOfFramesToCopy > 0)
        {
            BYTE* pData;
            HRESULT hr = IAudioRenderClient_GetBuffer(g_WASAPIContext.pAudioRenderClient, numberOfFramesToCopy, &pData);

            if (hr == AUDCLNT_E_BUFFER_SIZE_ERROR)
            {
                PopRenderBuffer(&g_WASAPIContext.Buffer, numberOfFramesToCopy * g_WASAPIContext.FrameSize);
                return TRUE;
            }
            else if (FAILED(hr))
            {
                return FALSE;
            }

            PopRenderBuffer(&g_WASAPIContext.Buffer, ReadRenderBuffer(&g_WASAPIContext.Buffer, (char*)pData, numberOfFramesToCopy * g_WASAPIContext.FrameSize));
            IAudioRenderClient_ReleaseBuffer(g_WASAPIContext.pAudioRenderClient, numberOfFramesToCopy, 0);
        }
        else
        {
            ClearRenderBuffer(&g_WASAPIContext.Buffer);
        }
    }

    return TRUE;
}

unsigned int __stdcall WASAPIRenderThread(void* pData)
{
    int ret = 1;
    int stillPlaying = TRUE;
    HANDLE waitArray[2] = {g_WASAPIContext.hShutdownEvent, g_WASAPIContext.hAudioSamplesReadyEvent};
    HANDLE hMmCss = NULL;
    DWORD mmCssTaskIndex = 0;

    (void)pData;

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        return 1;
    }

    hMmCss = AvSetMmThreadCharacteristics(_T("Audio"), &mmCssTaskIndex);

    if (!hMmCss)
    {
        goto LExit;
    }

    while (stillPlaying)
    {
        DWORD waitResult = WaitForMultipleObjects(_countof(waitArray), waitArray, FALSE, INFINITE);

        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0: /* hShutdownEvent */
            ret = 0;
            stillPlaying = FALSE;
            break;

        case WAIT_OBJECT_0 + 1: /* hAudioSamplesReadyEvent */
            WASAPIDoWriteBuffer();
            break;
        }
    }

LExit:
    if (hMmCss)
    {
        AvRevertMmThreadCharacteristics(hMmCss);
    }

    CoUninitialize();
    return ret;
}

int WASAPIPlayStart(void)
{
    if (!g_WASAPIContext.hRenderThread)
    {
        g_WASAPIContext.hRenderThread = (HANDLE)_beginthreadex(
            NULL,
            0,
            &WASAPIRenderThread,
            NULL,
            0,
            NULL
        );

        if (!g_WASAPIContext.hRenderThread)
        {
            ResetWASAPIContext();
            return -1;
        }
    }

    if (!g_WASAPIContext.IsStarted)
    {
        if (IsRenderBufferEmpty(&g_WASAPIContext.Buffer))
        {
            BYTE* pData;

            if (FAILED(IAudioRenderClient_GetBuffer(g_WASAPIContext.pAudioRenderClient, g_WASAPIContext.BufferSizeInFrames, &pData)))
            {
                ResetWASAPIContext();
                return -1;
            }

            IAudioRenderClient_ReleaseBuffer(g_WASAPIContext.pAudioRenderClient, g_WASAPIContext.BufferSizeInFrames, AUDCLNT_BUFFERFLAGS_SILENT);
        }
        else
        {
            if (!WASAPIDoWriteBuffer())
            {
                ResetWASAPIContext();
                return -1;
            }
        }

        if (FAILED(IAudioClient_Start(g_WASAPIContext.pAudioClient)))
        {
            ResetWASAPIContext();
            return -1;
        }

        g_WASAPIContext.IsStarted = TRUE;
    }

    return 0;
}

int WASAPIOpenOutputShared(void)
{
    return WASAPIOpenOutput(FALSE);
}

int WASAPIOpenOutputExclusive(void)
{
    return WASAPIOpenOutput(TRUE);
}

void WASAPICloseOutput(void)
{
    ResetWASAPIContext();
}

int WASAPIOutputData(char* pData, int32 size)
{
    int ret = -1;

    if (!PushToRenderBuffer(&g_WASAPIContext.Buffer, pData, (size_t)size))
    {
        ResetWASAPIContext();
        return -1;
    }
    return 0;
}

int WASAPIACntl(int request, void* pArg)
{
    switch (request)
    {
    case PM_REQ_GETQSIZ:
        *(int*)pArg = (int)(g_WASAPIContext.BufferSizeInFrames * g_WASAPIContext.FrameSize);
        return 0;

    case PM_REQ_DISCARD:
        WASAPIDoStop();
        return 0;

    case PM_REQ_PLAY_START:
        return WASAPIPlayStart();

    case PM_REQ_PLAY_END:
        WASAPIDoStop();
        return 0;

    case PM_REQ_OUTPUT_FINISH:
        while (!IsRenderBufferEmpty(&g_WASAPIContext.Buffer))
        {
            WaitForSingleObject(g_WASAPIContext.hRenderThread, 10);
        }
        return 0;

    default:
        return -1;
    }
}

int WASAPIDetect(void)
{
    IMMDevice* pMMDevice = NULL;
    int result = WASAPIGetDefaultDevice(&pMMDevice);

    if (pMMDevice)
    {
        IMMDevice_Release(pMMDevice);
    }

    return result;
}

#endif /* AU_WASAPI */
