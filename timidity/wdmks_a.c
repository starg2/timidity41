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

#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <process.h>
#include <tchar.h>

#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "output.h"
#include "wdmks_a.h"

int opt_wdmks_device_id = 0;
int opt_wdmks_format_ext = 0;
int opt_wdmks_latency = 20; //  ms
int opt_wdmks_polling = 0; // 0:event 1:polling
int opt_wdmks_thread_priority = 1; // winbase.h Priority flags
int opt_wdmks_rt_priority = 1; // 0:None, 1:Audio, 2:Capture, 3:Distribution, 4:Games, 5:Playback, 6:ProAudio, 7:WindowManager
int opt_wdmks_pin_priority = 2; // 0:low 1:normal 2:high 3:exclusive

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
    "WDM Kernel Streaming", 'k',
    NULL,
    &open_output,
    &close_output,
    &output_data,
    &acntl,
    &detect
};

/*****************************************************************************************************************************/

typedef struct KSBufferBlock_t {
	struct KSBufferBlock_t *pNext;
	size_t CurrentSize;
	size_t Capacity;
	uint8 Data[1];
} KSBufferBlock;

typedef struct KSBuffer_t {
	KSBufferBlock *pHead;
	KSBufferBlock *pTail;
	KSBufferBlock *pFree;
	uint32 FilledByte;
} KSBuffer;

static KSBuffer Buffers = {NULL, NULL, NULL, 0};

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
	KSBufferBlock *block = Buffers.pHead;

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
	KSBufferBlock *block = NULL;

	clear_buffer();
	block = Buffers.pFree;
	while(block){
		KSBufferBlock* pNext = block->pNext;
		free(block);
		block = pNext;
	}
	Buffers.pFree = NULL;	
}

/* if *pbuf == NULL, appends `size` count of zeros */
static void input_buffer_partial(KSBufferBlock *block, const uint8 **pbuf, size_t *bytes)
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
			KSBufferBlock *block = Buffers.pFree;

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
			KSBufferBlock *new_block = (KSBufferBlock *)safe_malloc(sizeof(KSBufferBlock) + capacity);

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
	KSBufferBlock *block = Buffers.pHead;

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
			KSBufferBlock *pNext = Buffers.pHead->pNext;
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

#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <winioctl.h>
#include <winbase.h>
#include <initguid.h>
#include <objbase.h>
#include <SetupAPI.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

#define WAVE_FORMAT_UNKNOWN      0x0000
#define WAVE_FORMAT_PCM          0x0001
#define WAVE_FORMAT_ADPCM        0x0002
#define WAVE_FORMAT_IEEE_FLOAT   0x0003
#define WAVE_FORMAT_ALAW         0x0006
#define WAVE_FORMAT_MULAW        0x0007
#define WAVE_FORMAT_EXTENSIBLE   0xFFFE

#define SPEAKER_FRONT_LEFT 0x1
#define SPEAKER_FRONT_RIGHT 0x2
#define SPEAKER_FRONT_CENTER 0x4
#define SPEAKER_MONO (SPEAKER_FRONT_CENTER)
#define SPEAKER_STEREO (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)

#ifndef STATIC_KSCATEGORY_REALTIME
#define STATIC_KSCATEGORY_REALTIME \
    0xEB115FFCL, 0x10C8, 0x4964, 0x83, 0x1D, 0x6D, 0xCB, 0x02, 0xE6, 0xF2, 0x3F
DEFINE_GUIDSTRUCT("EB115FFC-10C8-4964-831D-6DCB02E6F23F", KSCATEGORY_REALTIME);
#define KSCATEGORY_REALTIME DEFINE_GUIDNAMED(KSCATEGORY_REALTIME)
#endif

#ifndef STATIC_KSPROPSETID_RtAudio
#define STATIC_KSPROPSETID_RtAudio\
    0xa855a48c, 0x2f78, 0x4729, 0x90, 0x51, 0x19, 0x68, 0x74, 0x6b, 0x9e, 0xef
DEFINE_GUIDSTRUCT("A855A48C-2F78-4729-9051-1968746B9EEF", KSPROPSETID_RtAudio);
#define KSPROPSETID_RtAudio DEFINE_GUIDNAMED(KSPROPSETID_RtAudio)
#endif

#define KSPROPERTY_RTAUDIO_GETPOSITIONFUNCTION 0
#define KSPROPERTY_RTAUDIO_BUFFER 1
#define KSPROPERTY_RTAUDIO_HWLATENCY 2
#define KSPROPERTY_RTAUDIO_POSITIONREGISTER 3
#define KSPROPERTY_RTAUDIO_CLOCKREGISTER 4
#define KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION 5
#define KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT 6
#define KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT 7
#define KSPROPERTY_RTAUDIO_QUERY_NOTIFICATION_SUPPORT 8

#ifndef _MSC_VER
typedef struct {
    KSPROPERTY  Property;
    PVOID       BaseAddress;
    ULONG       RequestedBufferSize;
} KSRTAUDIO_BUFFER_PROPERTY, *PKSRTAUDIO_BUFFER_PROPERTY;

typedef struct {
    KSPROPERTY  Property;
    PVOID       BaseAddress;
    ULONG       RequestedBufferSize;
    ULONG       NotificationCount;
} KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, *PKSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION;

typedef struct {
    PVOID   BufferAddress;
    ULONG   ActualBufferSize;
    BOOL    CallMemoryBarrier;
} KSRTAUDIO_BUFFER, *PKSRTAUDIO_BUFFER;

typedef struct {
    ULONG   FifoSize;
    ULONG   ChipsetDelay;
    ULONG   CodecDelay;
} KSRTAUDIO_HWLATENCY, *PKSRTAUDIO_HWLATENCY;

typedef struct {
    KSPROPERTY  Property;
    PVOID       BaseAddress;
} KSRTAUDIO_HWREGISTER_PROPERTY, *PKSRTAUDIO_HWREGISTER_PROPERTY;

typedef struct {
    PVOID       Register;
    ULONG       Width;
    ULONGLONG   Numerator;
    ULONGLONG   Denominator;
    ULONG       Accuracy;
} KSRTAUDIO_HWREGISTER, *PKSRTAUDIO_HWREGISTER;

typedef struct {
    KSPROPERTY  Property;
    HANDLE      NotificationEvent;
} KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY, *PKSRTAUDIO_NOTIFICATION_EVENT_PROPERTY;
#endif

/*****************************************************************************************************************************/

typedef DWORD WINAPI KSCREATEPIN(HANDLE, PKSPIN_CONNECT, ACCESS_MASK, PHANDLE);
static HINSTANCE hDllKsUser = NULL;
static KSCREATEPIN *fKsCreatePin = NULL;

static int load_ksuser(void)
{
    if(!hDllKsUser)
        hDllKsUser = LoadLibrary(TEXT("ksuser.dll"));
    if(!hDllKsUser)
        return FALSE;	
    fKsCreatePin = (KSCREATEPIN *)GetProcAddress(hDllKsUser, "KsCreatePin");
    if(!fKsCreatePin)
        return FALSE;
	return TRUE;
}

static void free_ksuser(void)
{
    if(hDllKsUser)
        FreeLibrary(hDllKsUser);
	hDllKsUser = NULL;	
	fKsCreatePin = NULL;
}

/*****************************************************************************************************************************/

typedef HANDLE (WINAPI *fAvSetMmThreadCharacteristics)(LPCSTR,LPDWORD);
typedef BOOL   (WINAPI *fAvRevertMmThreadCharacteristics)(HANDLE);

static HINSTANCE hDllAvrt = NULL;
static fAvSetMmThreadCharacteristics pAvSetMmThreadCharacteristics = NULL;
static fAvRevertMmThreadCharacteristics pAvRevertMmThreadCharacteristics = NULL;

static int load_avrt(void)
{
	if(!hDllAvrt)
		hDllAvrt = LoadLibrary(_T("avrt.dll"));
    if(!hDllAvrt)
        return FALSE;	
#ifdef UNICODE
	pAvSetMmThreadCharacteristics = (fAvSetMmThreadCharacteristics)GetProcAddress(hDllAvrt, "AvSetMmThreadCharacteristicsW");
#else
	pAvSetMmThreadCharacteristics = (fAvSetMmThreadCharacteristics)GetProcAddress(hDllAvrt, "AvSetMmThreadCharacteristicsA");
#endif
	pAvRevertMmThreadCharacteristics = (fAvRevertMmThreadCharacteristics)GetProcAddress(hDllAvrt, "AvRevertMmThreadCharacteristics");
	return (int)pAvSetMmThreadCharacteristics && pAvRevertMmThreadCharacteristics;
}

static void free_avrt(void)
{
    if(hDllAvrt)
        FreeLibrary(hDllAvrt);
	hDllAvrt = NULL;	
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
		ver = 0; // NT
		break;
	case 5:
		ver = 1; // 2000, XP
		break;
	default:
		ver = 2; // vista, and later
		break;
	}
	WinVer = ver;
	return WinVer;
}

static void TrimString(wchar_t *str, size_t length)
{
	wchar_t *s = str, *e = NULL;

	while(iswspace(*s)) ++s;
	e = s + min(length, wcslen(s)) - 1;
	while(e > s && iswspace(*e)) --e;
	++e;
	length = e - s;
	memmove(str, s, length * sizeof(wchar_t));
	str[length] = 0;
}

/*****************************************************************************************************************************/

static const GUID guid_WAVE_FORMAT = {WAVE_FORMAT_UNKNOWN,0x0000,0x0010,{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID guid_KSDATAFORMAT_TYPE_AUDIO = {STATIC_KSDATAFORMAT_TYPE_AUDIO};
static const GUID guid_KSDATAFORMAT_SUBTYPE_PCM = {STATIC_KSDATAFORMAT_SUBTYPE_PCM};
static const GUID guid_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX = {STATIC_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX};
static const GUID guid_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT};
static const GUID guid_KSDATAFORMAT_SUBTYPE_WILDCARD = {STATIC_KSDATAFORMAT_SUBTYPE_WILDCARD};
static const GUID guid_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX = {STATIC_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX};
static const GUID guid_KSCATEGORY_AUDIO = {STATIC_KSCATEGORY_AUDIO};
static const GUID guid_KSCATEGORY_RENDER = {STATIC_KSCATEGORY_RENDER};
static const GUID guid_KSCATEGORY_REALTIME = {STATIC_KSCATEGORY_REALTIME};
static const GUID guid_KSPROPSETID_Pin = {STATIC_KSPROPSETID_Pin};
static const GUID guid_KSPROPSETID_Topology = {STATIC_KSPROPSETID_Topology};
static const GUID guid_KSPROPSETID_Connection = {STATIC_KSPROPSETID_Connection};
static const GUID guid_KSPROPSETID_Audio = {STATIC_KSPROPSETID_Audio};
static const GUID guid_KSPROPSETID_RtAudio = {STATIC_KSPROPSETID_RtAudio};
static const GUID guid_KSINTERFACESETID_Standard = {STATIC_KSINTERFACESETID_Standard};
static const GUID guid_KSMEDIUMSETID_Standard = {STATIC_KSMEDIUMSETID_Standard};

#define WDMKS_ERROR_InsufficientMemory (-9999)
#define WDMKS_FILTER_COUNT_MAX (32)

struct _WDMKS_Filter;
typedef struct _WDMKS_Filter WDMKS_Filter;
struct _WDMKS_Pin;
typedef struct _WDMKS_Pin WDMKS_Pin;
typedef int (*FunctionGetPinAudioPosition)(WDMKS_Pin*, ULONG*);
typedef void (*FunctionMemoryBarrier)(void);
struct _WDMKS_ProcessThreadInfo;
typedef struct _WDMKS_ProcessThreadInfo WDMKS_ProcessThreadInfo;
typedef int (*FunctionPinHandler)(WDMKS_ProcessThreadInfo* pInfo, UINT eventIndex);

typedef enum WDMKS_Type {
    Type_kNotUsed,
    Type_kWaveCyclic,
    Type_kWaveRT,
    Type_kCnt,
} WDMKS_Type;

typedef enum WDMKS_SubType {
    SubType_kUnknown,
    SubType_kNotification,
    SubType_kPolled,
    SubType_kCnt,
} WDMKS_SubType;

typedef struct _WDMKS_DeviceInfo {
    wchar_t filterPath[MAX_PATH];
    wchar_t topologyPath[MAX_PATH];
    WDMKS_Type streamingType;
} WDMKS_DeviceInfo;

struct _WDMKS_Pin {
    HANDLE handle;
    UINT inputCount;
    wchar_t friendlyName[MAX_PATH];
    WDMKS_Filter *parentFilter;
    WDMKS_SubType pinKsSubType;
    ULONG pinId;
    ULONG endpointPinId;
    KSPIN_CONNECT *pinConnect;
    ULONG pinConnectSize;
    KSDATAFORMAT_WAVEFORMATEX *ksDataFormatWfx;
    KSPIN_COMMUNICATION communication;
    KSDATARANGE *dataRanges;
    KSMULTIPLE_ITEM *dataRangesItem;
    KSPIN_DATAFLOW dataFlow;
    KSPIN_CINSTANCES instances;
    int maxChannels;
	int isFloat;
    ULONG minBits;
    ULONG maxBits;
    ULONG minSampleRate;
    ULONG maxSampleRate;
    ULONG *positionRegister;
    ULONG hwLatency;
};

struct _WDMKS_Filter {
    HANDLE handle;
    WDMKS_DeviceInfo devInfo;
    DWORD deviceNode;
    int pinCount;
    WDMKS_Pin **pins;
    WDMKS_Filter *topologyFilter;
    wchar_t friendlyName[MAX_PATH];
    int validPinCount;
    int usageCount;
    KSMULTIPLE_ITEM *connections;
    KSMULTIPLE_ITEM *nodes;
    int filterRefCount;
};

static int filter_use(WDMKS_Filter* filter);
static void filter_release(WDMKS_Filter* filter);
static WDMKS_Filter *filter_new(WDMKS_Type type, DWORD devNode, const wchar_t *filterName, const wchar_t *friendlyName, int *error);

static int sync_ioctl(HANDLE handle, ULONG ioctlNumber, void *inBuffer, ULONG inBufferCount, 
	void *outBuffer, ULONG outBufferCount, ULONG* bytesReturned)
{
	int result = 0;
	ULONG dummyBytesReturned = 0;

	if(!bytesReturned)
		bytesReturned = &dummyBytesReturned;
	if(!DeviceIoControl(handle, ioctlNumber, inBuffer, inBufferCount, outBuffer, outBufferCount, bytesReturned, NULL)){
		ULONG error = GetLastError();
		if(!((error == ERROR_INSUFFICIENT_BUFFER || error == ERROR_MORE_DATA)
			&& ioctlNumber == IOCTL_KS_PROPERTY && outBufferCount == 0))
			result = 1;
	}
	return result;
}

static int set_property_simple(HANDLE handle, const GUID* const guidPropertySet, ULONG property,
	void *value, ULONG valueCount, void *instance, ULONG instanceCount)
{
	KSPROPERTY* ksProperty;
	ULONG propertyCount  = 0;

	propertyCount = sizeof(KSPROPERTY) + instanceCount;
	ksProperty = (KSPROPERTY*)_alloca( propertyCount );
	if(!ksProperty)
		return 1;
	ksProperty->Set = *guidPropertySet;
	ksProperty->Id = property;
	ksProperty->Flags = KSPROPERTY_TYPE_SET;
	if(instance)
		memcpy((void*)((char*)ksProperty + sizeof(KSPROPERTY)), instance, instanceCount);
	return sync_ioctl(handle, IOCTL_KS_PROPERTY, ksProperty, propertyCount, value, valueCount, NULL);
}

static int get_property_simple(HANDLE handle, const GUID* const guidPropertySet, ULONG property,
	void *value, ULONG valueCount)
{
	KSPROPERTY ksProperty;

	ksProperty.Set = *guidPropertySet;
	ksProperty.Id = property;
	ksProperty.Flags = KSPROPERTY_TYPE_GET;
	return sync_ioctl(handle, IOCTL_KS_PROPERTY, &ksProperty, sizeof(KSPROPERTY), value, valueCount, NULL);
}

static int get_property_multi(HANDLE handle, const GUID* const guidPropertySet, ULONG property, 
	KSMULTIPLE_ITEM **ksMultipleItem)
{
	int result;
	ULONG multipleItemSize = 0;
	KSPROPERTY ksProp;

	ksProp.Set = *guidPropertySet;
	ksProp.Id = property;
	ksProp.Flags = KSPROPERTY_TYPE_GET;
	if(result = sync_ioctl(handle, IOCTL_KS_PROPERTY, &ksProp, sizeof(KSPROPERTY), NULL, 0, &multipleItemSize))
		return result;
	*ksMultipleItem = (KSMULTIPLE_ITEM *)safe_malloc(multipleItemSize);
	if(!*ksMultipleItem )
		return WDMKS_ERROR_InsufficientMemory;
	memset(*ksMultipleItem, 0, multipleItemSize);
	if(result = sync_ioctl(handle, IOCTL_KS_PROPERTY, &ksProp, sizeof(KSPROPERTY), 
		(void*)*ksMultipleItem, multipleItemSize, NULL))
		safe_free(ksMultipleItem);
	return result;
}

static int get_pin_property_simple(HANDLE handle, ULONG pinId, const GUID* const guidPropertySet, ULONG property, 
	void *value, ULONG valueCount, ULONG *byteCount)
{
	KSP_PIN ksPProp;

	ksPProp.Property.Set = *guidPropertySet;
	ksPProp.Property.Id = property;
	ksPProp.Property.Flags = KSPROPERTY_TYPE_GET;
	ksPProp.PinId = pinId;
	ksPProp.Reserved = 0;
	return sync_ioctl(handle, IOCTL_KS_PROPERTY, &ksPProp, sizeof(KSP_PIN), value, valueCount, byteCount);
}

static int get_pin_property_multi(HANDLE handle, ULONG pinId, const GUID* const guidPropertySet, ULONG property,
	KSMULTIPLE_ITEM **ksMultipleItem)
{
	int result;
	ULONG multipleItemSize = 0;
	KSP_PIN ksPProp;

	ksPProp.Property.Set = *guidPropertySet;
	ksPProp.Property.Id = property;
	ksPProp.Property.Flags = KSPROPERTY_TYPE_GET;
	ksPProp.PinId = pinId;
	ksPProp.Reserved = 0;
	if(result = sync_ioctl(handle, IOCTL_KS_PROPERTY, &ksPProp.Property, sizeof(KSP_PIN), NULL, 0, &multipleItemSize))
		return result;
	*ksMultipleItem = (KSMULTIPLE_ITEM*)safe_malloc(multipleItemSize);
	if(!*ksMultipleItem)
		return WDMKS_ERROR_InsufficientMemory;
	memset(*ksMultipleItem, 0, multipleItemSize);
	if(result = sync_ioctl(handle, IOCTL_KS_PROPERTY, &ksPProp, sizeof(KSP_PIN),
		(void*)*ksMultipleItem, multipleItemSize, NULL))
		safe_free(ksMultipleItem);
	return result;
}

static int pin_set_state(WDMKS_Pin *pin, KSSTATE state)
{
	int result = 0;
	KSPROPERTY prop;
	
	if(!pin)
		return 1;
	if(!pin->handle)
		return 1;
	prop.Set = guid_KSPROPSETID_Connection;
	prop.Id  = KSPROPERTY_CONNECTION_STATE;
	prop.Flags = KSPROPERTY_TYPE_SET;
	return sync_ioctl(pin->handle, IOCTL_KS_PROPERTY, &prop, sizeof(KSPROPERTY), &state, sizeof(KSSTATE), NULL);
}


static int pin_get_audio_position(WDMKS_Pin *pPin, ULONG *pos)
{
	KSPROPERTY propIn;
	KSAUDIO_POSITION propOut;
	ULONG bytes = 0;
	
	if(pPin->positionRegister){ // pin_register_position_register()
		*pos = *(pPin->positionRegister);
		return 0;
	}
	propIn.Set = guid_KSPROPSETID_Audio;
	propIn.Id = KSPROPERTY_AUDIO_POSITION;
	propIn.Flags = KSPROPERTY_TYPE_GET;	
	if(DeviceIoControl(pPin->handle, IOCTL_KS_PROPERTY, (LPVOID)&propIn, sizeof(KSPROPERTY), 
		&propOut, sizeof(KSAUDIO_POSITION), &bytes, NULL)){
		*pos = propOut.PlayOffset;
		return 0;
	}
	return 1;
}

static int pin_register_position_register(WDMKS_Pin *pPin) 
{
	int result = 0;
	KSRTAUDIO_HWREGISTER_PROPERTY propIn;
	KSRTAUDIO_HWREGISTER propOut;

	pPin->positionRegister = NULL;
	propIn.BaseAddress = NULL;
	propIn.Property.Set = guid_KSPROPSETID_RtAudio;
	propIn.Property.Id = KSPROPERTY_RTAUDIO_POSITIONREGISTER;
	propIn.Property.Flags = KSPROPERTY_TYPE_SET;
	if(!(result = sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &propIn, sizeof(KSRTAUDIO_HWREGISTER_PROPERTY),
		&propOut, sizeof(KSRTAUDIO_HWREGISTER), NULL))) 
		pPin->positionRegister = (ULONG *)propOut.Register;
	return result;
}

static int pin_register_notification_handle(WDMKS_Pin *pPin, HANDLE handle) 
{
	KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY prop;

	prop.NotificationEvent = handle;
	prop.Property.Set = guid_KSPROPSETID_RtAudio;
	prop.Property.Id = KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT;
	prop.Property.Flags = KSPROPERTY_TYPE_SET;
	return sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &prop, sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY),
		&prop, sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY), NULL);
}

static int pin_unregister_notification_handle(WDMKS_Pin *pPin, HANDLE handle) 
{
	KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY prop;

	if(!handle)
		return 0;
	prop.NotificationEvent = handle;
	prop.Property.Set = guid_KSPROPSETID_RtAudio;
	prop.Property.Id = KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT;
	prop.Property.Flags = KSPROPERTY_TYPE_SET;
	return sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &prop, sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY),
		&prop, sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY), NULL);
}

static int pin_get_HwLatency(WDMKS_Pin *pPin, ULONG *pFifoSize)
{
	int result = 0;
	KSPROPERTY propIn;
	KSRTAUDIO_HWLATENCY propOut;

	propIn.Set = guid_KSPROPSETID_RtAudio;
	propIn.Id = KSPROPERTY_RTAUDIO_HWLATENCY;
	propIn.Flags = KSPROPERTY_TYPE_GET;
	if(!(result = sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &propIn, sizeof(KSPROPERTY), 
		&propOut, sizeof(KSRTAUDIO_HWLATENCY), NULL))){
		*pFifoSize = propOut.FifoSize;
	}
	return result;
}

static int pin_query_notification_support(WDMKS_Pin *pPin, int *pbResult)
{
	KSPROPERTY propIn;

	propIn.Set = guid_KSPROPSETID_RtAudio;
	propIn.Id = KSPROPERTY_RTAUDIO_QUERY_NOTIFICATION_SUPPORT;
	propIn.Flags = KSPROPERTY_TYPE_GET;
	return sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &propIn, sizeof(KSPROPERTY), pbResult, sizeof(int), NULL);
}

static ULONG WDMKS_GCD(ULONG a, ULONG b )
{
	return (b == 0) ? a : WDMKS_GCD(b, a % b);
}

static int pin_get_buffer(WDMKS_Pin *pPin, void **pBuffer, DWORD *pReqBufSize, int *pbCallMemBarrier)
{
	int result = 0;
	int k;
	int bSupportsNotification = FALSE;

	if(!pin_query_notification_support(pPin, &bSupportsNotification))
		pPin->pinKsSubType = bSupportsNotification ? SubType_kNotification : SubType_kPolled;
	for(k = 0; k < 1000; k++){
		if(pPin->pinKsSubType != SubType_kPolled){			
			KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION propIn;
			KSRTAUDIO_BUFFER propOut;

			propIn.BaseAddress = NULL;
			propIn.NotificationCount = 2;
			propIn.RequestedBufferSize = *pReqBufSize;
			propIn.Property.Set = guid_KSPROPSETID_RtAudio;
			propIn.Property.Id = KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION;
			propIn.Property.Flags = KSPROPERTY_TYPE_GET;
			if(!(result = sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &propIn, 
				sizeof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION), &propOut, sizeof(KSRTAUDIO_BUFFER), NULL))) {
				*pBuffer = propOut.BufferAddress;
				*pReqBufSize = propOut.ActualBufferSize;
				*pbCallMemBarrier = propOut.CallMemoryBarrier;
				pPin->pinKsSubType = SubType_kNotification;
                break;
			}
		}
		{
			KSRTAUDIO_BUFFER_PROPERTY propIn;
			KSRTAUDIO_BUFFER propOut;

			propIn.BaseAddress = NULL;
			propIn.RequestedBufferSize = *pReqBufSize;
			propIn.Property.Set = guid_KSPROPSETID_RtAudio;
			propIn.Property.Id = KSPROPERTY_RTAUDIO_BUFFER;
			propIn.Property.Flags = KSPROPERTY_TYPE_GET;
			if(!(result = sync_ioctl(pPin->handle, IOCTL_KS_PROPERTY, &propIn, sizeof(KSRTAUDIO_BUFFER_PROPERTY),
				&propOut, sizeof(KSRTAUDIO_BUFFER), NULL)))
			{
				*pBuffer = propOut.BufferAddress;
				*pReqBufSize = propOut.ActualBufferSize;
				*pbCallMemBarrier = propOut.CallMemoryBarrier;
				pPin->pinKsSubType = SubType_kPolled;
				break;
			}
		}
		if(((*pReqBufSize) % 128UL) == 0){
			break;
		}else{
			const UINT gcd = WDMKS_GCD(128UL, pPin->ksDataFormatWfx->WaveFormatEx.nBlockAlign);
			const UINT lcm = (128UL * pPin->ksDataFormatWfx->WaveFormatEx.nBlockAlign) / gcd;
			DWORD dwOldSize = *pReqBufSize;
			*pReqBufSize = ((*pReqBufSize + lcm - 1) / lcm) * lcm;
		}
	}
	return result;
}

static int pin_open(WDMKS_Pin *pin, const WAVEFORMATEX *wfex, int Priority)
{
    ULONG size, fsize;
    KSPIN_CONNECT *newConnect = NULL;

	if(!pin)
		return -1;
	fsize = sizeof(WAVEFORMATEX);
    if(wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        fsize += wfex->cbSize;	
	size = fsize + sizeof(KSPIN_CONNECT) + sizeof(KSDATAFORMAT_WAVEFORMATEX) - sizeof(WAVEFORMATEX);
    newConnect = (KSPIN_CONNECT *)safe_malloc(size);
    if(!newConnect)
		return -1;
	memcpy(newConnect, pin->pinConnect, min(pin->pinConnectSize, size));
	switch(Priority){
	case 0:
		newConnect->Priority.PriorityClass = KSPRIORITY_LOW;
		break;
	case 1:
	default:
		newConnect->Priority.PriorityClass = KSPRIORITY_NORMAL;
		break;
	case 2:
		newConnect->Priority.PriorityClass = KSPRIORITY_HIGH;
		break;
	case 3:
		newConnect->Priority.PriorityClass = KSPRIORITY_EXCLUSIVE;
		break;
	}
	safe_free(pin->pinConnect);
	pin->pinConnect = (KSPIN_CONNECT *)newConnect;
	pin->pinConnectSize = size;
	pin->ksDataFormatWfx = (KSDATAFORMAT_WAVEFORMATEX *)((KSPIN_CONNECT *)newConnect + 1);
	pin->ksDataFormatWfx->DataFormat.FormatSize = size - sizeof(KSPIN_CONNECT);
	memcpy(&(pin->ksDataFormatWfx->WaveFormatEx), wfex, fsize);
	pin->ksDataFormatWfx->DataFormat.SampleSize = (USHORT)(wfex->nChannels * (wfex->wBitsPerSample / 8));
	if(!pin->parentFilter)
		return -1;
    if(filter_use(pin->parentFilter))
		return -1;
    if(fKsCreatePin(pin->parentFilter->handle, pin->pinConnect,
		GENERIC_WRITE | GENERIC_READ, &pin->handle) != ERROR_SUCCESS){
        filter_release(pin->parentFilter);
        pin->handle = NULL;
        return -1;
    }
	if(!pin->handle){
        filter_release(pin->parentFilter);
		return -1;
	}
    return 0;
}

static void pin_close(WDMKS_Pin *pin)
{
	if(!pin)
		return;
	if(!pin->handle)
		return;
	pin_set_state(pin, KSSTATE_PAUSE);
	pin_set_state(pin, KSSTATE_STOP);
	CloseHandle(pin->handle);
	pin->handle = NULL;
	filter_release(pin->parentFilter);
}

static void pin_free(WDMKS_Pin *pin)
{
	UINT i;

	if(!pin)
		return;
	pin_close(pin);
	if(pin->pinConnect)
		safe_free(pin->pinConnect);
	if(pin->dataRangesItem)
		safe_free(pin->dataRangesItem);
	safe_free(pin);
}

typedef struct _UsbGuidName {
    USHORT usbGUID;
    wchar_t name[64];
} UsbGuidName;

static const UsbGuidName kNames[] = {
	/* Output terminal types */
	{ 0x0301, L"Speakers" },
	{ 0x0302, L"Headphones" },
	{ 0x0303, L"Head Mounted Display Audio" },
	{ 0x0304, L"Desktop Speaker" },
	{ 0x0305, L"Room Speaker" },
	{ 0x0306, L"Communication Speaker" },
	{ 0x0307, L"LFE Speakers" },
	/* External terminal types */
	{ 0x0601, L"Analog" },
	{ 0x0602, L"Digital" },
	{ 0x0603, L"Line" },
	{ 0x0604, L"Audio" },
	{ 0x0605, L"SPDIF" },
};

static int usb_guid_name_comp(const void *ls, const void *rs)
{
	const UsbGuidName *pL = (const UsbGuidName *)ls;
	const UsbGuidName *pR = (const UsbGuidName *)rs;
	return ((int)(pL->usbGUID) - (int)(pR->usbGUID));
}

static int get_name_from_category(const GUID *pGUID, wchar_t *name, UINT length)
{
	int result = 1;
	USHORT usbTerminalGUID = (USHORT)(pGUID->Data1 - 0xDFF219E0);

	if(usbTerminalGUID >= 0x201 && usbTerminalGUID < 0x300)
		usbTerminalGUID = 0x603;
	if(usbTerminalGUID >= 0x201 && usbTerminalGUID < 0x713){
		UsbGuidName s = {usbTerminalGUID};
		const UsbGuidName* ptr = bsearch(&s, kNames, sizeof(kNames)/sizeof(UsbGuidName), 
			sizeof(UsbGuidName), usb_guid_name_comp);
		if(ptr != 0){
			if(name != NULL && length > 0){
				int n = _snwprintf(name, length, L"%s", ptr->name);
				if(usbTerminalGUID >= 0x601 && usbTerminalGUID < 0x700)
					_snwprintf(name + n, length - n, L" %s", L"Out");
			}
			result = 0;
		}
	}
	return result;
}

static ULONG get_connected_pin(ULONG startPin, WDMKS_Filter *filter)
{
	UINT i, k;
	const KSTOPOLOGY_CONNECTION *conn = NULL, *conn_tmp, *conn_retval;

	for(k = 0; k < 1000; k++){
		conn_tmp = (const KSTOPOLOGY_CONNECTION *)(filter->connections + 1);
		if(!conn){
			for(i = 0; i < filter->connections->Count; ++i){
				const KSTOPOLOGY_CONNECTION *pConn = conn_tmp + i;
				if(!pConn)
					continue;
				if(pConn->FromNode == KSFILTER_NODE && pConn->FromNodePin == startPin){
					conn = pConn;
					break;
				}
			}
		}else{
			conn_retval = NULL;
			for (i = 0; i < filter->connections->Count; ++i){
				const KSTOPOLOGY_CONNECTION *pConn = conn_tmp + i;
				if(!pConn)
					continue;
				if(pConn == conn)
					continue;
				if(pConn->FromNode == conn->ToNode){
					conn_retval = pConn;
					break;
				}
			}
			conn = conn_retval;
		}
		if(!conn)
			break;
		if(conn->ToNode == KSFILTER_NODE)
			return conn->ToNodePin;
	}
	return KSFILTER_NODE;
}

static WDMKS_Pin *pin_new(WDMKS_Filter *parentFilter, ULONG pinId, int *error)
{
	WDMKS_Pin* pin;
	int result = -1;
	ULONG i;
	KSMULTIPLE_ITEM* item = NULL;
	KSIDENTIFIER* identifier;
	KSDATARANGE* dataRange;
	const ULONG streamingId = (parentFilter->devInfo.streamingType == Type_kWaveRT) ? 
		KSINTERFACE_STANDARD_LOOPED_STREAMING : KSINTERFACE_STANDARD_STREAMING;
	ULONG topoPinId = 0;
	const wchar_t kOutputName[] = L"Output";

	pin = (WDMKS_Pin *)safe_malloc(sizeof(WDMKS_Pin));
	if(!pin){
		result = WDMKS_ERROR_InsufficientMemory;
		goto error;
	}
	memset(pin, 0, sizeof(WDMKS_Pin));
	pin->parentFilter = parentFilter;
	pin->pinId = pinId;
	pin->pinConnectSize = sizeof(KSPIN_CONNECT) + sizeof(KSDATAFORMAT_WAVEFORMATEX);
	pin->pinConnect = (KSPIN_CONNECT *)safe_malloc(pin->pinConnectSize);
	if(!pin->pinConnect){
		result = WDMKS_ERROR_InsufficientMemory;
		goto error;
	}
	memset(pin->pinConnect, 0, pin->pinConnectSize);
	pin->pinConnect->Interface.Set = guid_KSINTERFACESETID_Standard;
	pin->pinConnect->Interface.Id = streamingId;
	pin->pinConnect->Interface.Flags = 0;
	pin->pinConnect->Medium.Set = guid_KSMEDIUMSETID_Standard;
	pin->pinConnect->Medium.Id = KSMEDIUM_TYPE_ANYINSTANCE;
	pin->pinConnect->Medium.Flags = 0;
	pin->pinConnect->PinId  = pinId;
	pin->pinConnect->PinToHandle = NULL;
	pin->pinConnect->Priority.PriorityClass = KSPRIORITY_NORMAL;
	pin->pinConnect->Priority.PrioritySubClass = 1;
	pin->ksDataFormatWfx = (KSDATAFORMAT_WAVEFORMATEX*)(pin->pinConnect + 1);
	pin->ksDataFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEX);
	pin->ksDataFormatWfx->DataFormat.Flags = 0;
	pin->ksDataFormatWfx->DataFormat.Reserved = 0;
	pin->ksDataFormatWfx->DataFormat.MajorFormat = guid_KSDATAFORMAT_TYPE_AUDIO;
	pin->ksDataFormatWfx->DataFormat.SubFormat = guid_KSDATAFORMAT_SUBTYPE_PCM;
	pin->ksDataFormatWfx->DataFormat.Specifier = guid_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
	if(result = get_pin_property_simple(parentFilter->handle, pinId, &guid_KSPROPSETID_Pin, 
		KSPROPERTY_PIN_COMMUNICATION, &pin->communication, sizeof(KSPIN_COMMUNICATION), NULL))
		goto error;
	if(pin->communication != KSPIN_COMMUNICATION_SINK && pin->communication != KSPIN_COMMUNICATION_BOTH){
		result = 1;
		goto error;
	}
	if(result = get_pin_property_simple(parentFilter->handle, pinId, &guid_KSPROPSETID_Pin, 
		KSPROPERTY_PIN_DATAFLOW, &pin->dataFlow, sizeof(KSPIN_DATAFLOW), NULL))
		goto error;
	if(pin->dataFlow != KSPIN_DATAFLOW_IN){
		result = 2; // only output
		goto error;
	}
	if(result = get_pin_property_multi(parentFilter->handle, pinId, &guid_KSPROPSETID_Pin,
		KSPROPERTY_PIN_INTERFACES, &item))
		goto error;
	identifier = (KSIDENTIFIER *)(item + 1);
	result = 1;
	for(i = 0; i < item->Count; i++)
		if(IsEqualGUID(&identifier[i].Set, &guid_KSINTERFACESETID_Standard)
			&& (identifier[i].Id == streamingId)){
			result = 0;
			break;
		}
	if(result != 0)
		goto error;
	safe_free( item );
	item = NULL;
	if(result = get_pin_property_multi(parentFilter->handle, pinId, &guid_KSPROPSETID_Pin, 
		KSPROPERTY_PIN_MEDIUMS, &item))
		goto error;
	identifier = (KSIDENTIFIER*)(item + 1);
	result = 1;
	for( i = 0; i < item->Count; i++ )
		if(IsEqualGUID(&identifier[i].Set, &guid_KSMEDIUMSETID_Standard)
			&& (identifier[i].Id == KSMEDIUM_STANDARD_DEVIO)){
			result = 0;
			break;
		}
	if(result != 0)
		goto error;
	safe_free( item );
	item = NULL;
	if(result = get_pin_property_multi(parentFilter->handle, pinId, &guid_KSPROPSETID_Pin, 
		KSPROPERTY_PIN_DATARANGES, &pin->dataRangesItem))
		goto error;
	pin->dataRanges = (KSDATARANGE *)(pin->dataRangesItem + 1);
	result = 0;
	dataRange = pin->dataRanges;
	pin->maxChannels = 0;
	pin->isFloat = 0;
    pin->minBits = 65535;
    pin->maxBits = 0;
    pin->minSampleRate = 4000000;
    pin->maxSampleRate = 0;
	for(i = 0; i < pin->dataRangesItem->Count; i++){
		if((!memcmp(((PUSHORT)&guid_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX) + 1,
			((PUSHORT)&dataRange->SubFormat) + 1, sizeof(GUID) - sizeof(USHORT))) ||
			IsEqualGUID(&dataRange->SubFormat, &guid_KSDATAFORMAT_SUBTYPE_PCM) ||
			IsEqualGUID(&dataRange->SubFormat, &guid_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) ||
			IsEqualGUID(&dataRange->SubFormat, &guid_KSDATAFORMAT_SUBTYPE_WILDCARD) ||
			IsEqualGUID(&dataRange->MajorFormat, &guid_KSDATAFORMAT_TYPE_AUDIO)){
			KSDATARANGE_AUDIO *aRange = (KSDATARANGE_AUDIO *)dataRange;

			if(aRange->MaximumChannels == (ULONG) -1)
				pin->maxChannels = 256;
			else if((int)aRange->MaximumChannels > pin->maxChannels)
				pin->maxChannels = (int)aRange->MaximumChannels;
			if(aRange->MinimumBitsPerSample != (ULONG) -1)
				if(aRange->MinimumBitsPerSample < pin->minBits)
					pin->minBits = aRange->MinimumBitsPerSample;			
			if(aRange->MaximumBitsPerSample != (ULONG) -1)
				if(aRange->MaximumBitsPerSample > pin->maxBits)
					pin->maxBits = aRange->MaximumBitsPerSample;	
			if(aRange->MinimumSampleFrequency != (ULONG) -1)
				if(aRange->MinimumSampleFrequency < pin->minSampleRate)
					pin->minSampleRate = aRange->MinimumSampleFrequency;
			if(aRange->MaximumSampleFrequency != (ULONG) -1)
				if(aRange->MaximumSampleFrequency > pin->maxSampleRate)
					pin->maxSampleRate = aRange->MaximumSampleFrequency;
			if(IsEqualGUID(&dataRange->SubFormat, &guid_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
				pin->isFloat = 1;
		}
		dataRange = (KSDATARANGE *)(((char *)dataRange) + dataRange->FormatSize);
	}
	if(pin->maxChannels == 0 || pin->minBits == 65535 || pin->maxBits == 0
		|| pin->minSampleRate == 4000000 || pin->maxSampleRate == 0){
		result = 1;
		goto error;
	}
	result = get_pin_property_simple(parentFilter->handle, pinId, &guid_KSPROPSETID_Pin,
		KSPROPERTY_PIN_CINSTANCES, &pin->instances, sizeof(KSPIN_CINSTANCES), NULL);
	if(result != 0)
		goto error;
	topoPinId = get_connected_pin(pinId, parentFilter);
	if(topoPinId != KSFILTER_NODE){
		ULONG cbBytes = 0;
		if(result = get_pin_property_simple(parentFilter->handle, topoPinId, &guid_KSPROPSETID_Pin,
			KSPROPERTY_PIN_PHYSICALCONNECTION, 0, 0, &cbBytes)){
			if(result = get_pin_property_simple(parentFilter->handle, topoPinId, &guid_KSPROPSETID_Pin,
				KSPROPERTY_PIN_NAME, pin->friendlyName, MAX_PATH, NULL)){
				GUID category = {0};
				if(!(result = get_pin_property_simple(parentFilter->handle, topoPinId, &guid_KSPROPSETID_Pin,
					KSPROPERTY_PIN_CATEGORY, &category, sizeof(GUID), NULL)))
					result = get_name_from_category(&category, pin->friendlyName, MAX_PATH);
			}
			if(wcslen(pin->friendlyName) == 0)
				wcscpy(pin->friendlyName, kOutputName);
			pin->endpointPinId = pinId;
		}else{
			KSPIN_PHYSICALCONNECTION *pc = (KSPIN_PHYSICALCONNECTION *)safe_malloc(cbBytes + 2);
			ULONG pcPin;
			wchar_t symbLinkName[MAX_PATH];

			if(!pc){
				result = WDMKS_ERROR_InsufficientMemory;
				goto error;
			}
			memset(pc, 0, cbBytes + 2);
			result = get_pin_property_simple(parentFilter->handle, topoPinId, &guid_KSPROPSETID_Pin,
				KSPROPERTY_PIN_PHYSICALCONNECTION, pc, cbBytes, NULL);
			pcPin = pc->Pin;
			wcsncpy(symbLinkName, pc->SymbolicLinkName, MAX_PATH);
			safe_free(pc);
			if(result != 0)
				goto error;
			if(symbLinkName[1] == TEXT('?'))
				symbLinkName[1] = TEXT('\\');
			if(!pin->parentFilter->topologyFilter){
				pin->parentFilter->topologyFilter = filter_new(Type_kNotUsed, 0, symbLinkName, L"", &result);
				if(!pin->parentFilter->topologyFilter){
					result = 1;
					goto error;
				}
				wcsncpy(pin->parentFilter->devInfo.topologyPath, symbLinkName, MAX_PATH);
			}else{
				if((wcscmp(symbLinkName, pin->parentFilter->topologyFilter->devInfo.filterPath) == 0)){
					result = 1;
					goto error;
				}
			}
			if(!filter_use(pin->parentFilter->topologyFilter)){
				GUID category = {0};
				ULONG endpointPinId;

				endpointPinId = get_connected_pin(pcPin, pin->parentFilter->topologyFilter);
				if(endpointPinId == KSFILTER_NODE){
					result = 1;
					goto error;
				}
				if(!(result = get_pin_property_simple(pin->parentFilter->topologyFilter->handle, endpointPinId, 
					&guid_KSPROPSETID_Pin, KSPROPERTY_PIN_CATEGORY, &category, sizeof(GUID), NULL)))
					result = get_name_from_category(&category, pin->friendlyName, MAX_PATH);
				if(wcslen(pin->friendlyName) == 0)
					wcscpy(pin->friendlyName, kOutputName);
				pin->endpointPinId = pcPin;
			}
		}
	}else{
		wcscpy(pin->friendlyName, kOutputName);
	}
	if(pin->parentFilter->topologyFilter && pin->parentFilter->topologyFilter->handle != NULL)
		filter_release(pin->parentFilter->topologyFilter);
	*error = 0;
	return pin;

error:
	if(pin->parentFilter->topologyFilter && pin->parentFilter->topologyFilter->handle != NULL)
		filter_release(pin->parentFilter->topologyFilter);
	safe_free(item);
	pin_free(pin);
	*error = result;
	return NULL;
}

static int filter_init_pins(WDMKS_Filter *filter)
{
	int result = 0;
	int pinId;

	if(filter->devInfo.streamingType == Type_kNotUsed)
		return 0;
	if(filter->pins != NULL)
		return 0;
	filter->pins = (WDMKS_Pin **)safe_malloc(sizeof(WDMKS_Pin *) * filter->pinCount);
	if(!filter->pins){
		result = WDMKS_ERROR_InsufficientMemory;
		goto error;
	}
	memset(filter->pins, 0, sizeof(WDMKS_Pin *) * filter->pinCount);
	for(pinId = 0; pinId < filter->pinCount; pinId++){
		WDMKS_Pin *newPin = pin_new(filter, pinId, &result);
		if(result == WDMKS_ERROR_InsufficientMemory)
			goto error;
		if(newPin != NULL){
			filter->pins[pinId] = newPin;
			++filter->validPinCount;
		}
	}
	if(filter->validPinCount == 0){
		result = -1;
		goto error;
	}
	return 0;
error:
	if(filter->pins){
		for(pinId = 0; pinId < filter->pinCount; ++pinId){
			if(filter->pins[pinId]){
				pin_free(filter->pins[pinId]);
				filter->pins[pinId] = 0;
			}
		}
		safe_free(filter->pins);
		filter->pins = 0;
	}
	return result;
}

static void filter_free(WDMKS_Filter* filter)
{
	if(!filter)
		return;
	if(--filter->filterRefCount > 0)
		return;
	if(filter->topologyFilter){
		filter_free(filter->topologyFilter);
		filter->topologyFilter = 0;
	}
	if(filter->pins){
		int pinId;
		for(pinId = 0; pinId < filter->pinCount; pinId++)
			pin_free(filter->pins[pinId]);
		safe_free( filter->pins );
		filter->pins = 0;
	}
	if(filter->connections){
		safe_free(filter->connections);
		filter->connections = 0;
	}
	if(filter->nodes){
		safe_free(filter->nodes);
		filter->nodes = 0;
	}
	if(filter->handle)
		CloseHandle(filter->handle);
	safe_free(filter); 
}

static int filter_use(WDMKS_Filter* filter)
{
	if(!filter)
		return 1;
	if(!filter->handle){
		filter->handle = CreateFileW(filter->devInfo.filterPath, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
		if(!filter->handle)
			return -1;
	}
	filter->usageCount++;
	return 0;
}

static void filter_release(WDMKS_Filter* filter)
{
	if(!filter)
		return;
	if(filter->usageCount <= 0)
		return;
	if(filter->topologyFilter != NULL && filter->topologyFilter->handle != NULL)
		filter_release(filter->topologyFilter);
	filter->usageCount--;
	if(filter->usageCount != 0 )
		return;
	if(!filter->handle)
		return;
	CloseHandle( filter->handle );
	filter->handle = NULL;
}

static WDMKS_Filter *filter_new(WDMKS_Type type, DWORD devNode, const wchar_t *filterName, const wchar_t *friendlyName, int *error)
{
	WDMKS_Filter *filter = NULL;
	int result;

	filter = (WDMKS_Filter *)safe_malloc( sizeof(WDMKS_Filter));
	if(!filter){
		result = WDMKS_ERROR_InsufficientMemory;
		goto error;
	}
	memset(filter, 0, sizeof(WDMKS_Filter));
	filter->devInfo.streamingType = type;
	filter->deviceNode = devNode;
	wcsncpy(filter->devInfo.filterPath, filterName, MAX_PATH);
	wcsncpy(filter->friendlyName, friendlyName, MAX_PATH);
	if(result = filter_use(filter))
		goto error;
	if(result = get_pin_property_simple(filter->handle, 0, &guid_KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, 
		&filter->pinCount, sizeof(filter->pinCount), NULL))
		goto error;
	if(result = get_property_multi(filter->handle, &guid_KSPROPSETID_Topology,	KSPROPERTY_TOPOLOGY_CONNECTIONS, 
		&filter->connections))
		goto error;
	if(result = get_property_multi(filter->handle, &guid_KSPROPSETID_Topology,	KSPROPERTY_TOPOLOGY_NODES, 
		&filter->nodes))
		goto error;
	if(type != Type_kNotUsed)
		if(result = filter_init_pins(filter))
			goto error;
	filter_release(filter);
	*error = 0;
	return filter;
error:
	filter_free(filter);
	*error = result;
	return NULL;
}

static int get_filter_list(WDMKS_Filter **ppFilters, int *pFilterCount, int *pDeviceCount)
{
	HDEVINFO handle = NULL;
	SP_DEVICE_INTERFACE_DATA ifData;
	SP_DEVICE_INTERFACE_DATA aliasData;
	SP_DEVINFO_DATA devInfoData;
	const int sizeIf = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + (MAX_PATH * sizeof(WCHAR));
	unsigned char ifDetailsArray[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + (MAX_PATH * sizeof(WCHAR))];
	SP_DEVICE_INTERFACE_DETAIL_DATA_W *devIfDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)ifDetailsArray;
	DWORD aliasFlags;
	WDMKS_Type streamingType;
	int device, count_max, FilterCount = 0, DeviceCount = 0;
	const wchar_t kUsbPrefix[] = L"\\\\?\\USB";
	const wchar_t kUsbNamePrefix[] = L"USB Audio";
	
	devIfDetails->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
	count_max = *pFilterCount; // copy max filter count
	*pFilterCount = 0;
	*pDeviceCount = 0;
	handle = SetupDiGetClassDevs(&guid_KSCATEGORY_AUDIO, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if(handle == INVALID_HANDLE_VALUE)
		return -1;
	for(device = 0; ; device++)
	{ /************************ block 1 ************************/
	ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	ifData.Reserved = 0;
	aliasData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	aliasData.Reserved = 0;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	devInfoData.Reserved = 0;
	streamingType = Type_kWaveCyclic;
		
	if(!SetupDiEnumDeviceInterfaces(handle, NULL, &guid_KSCATEGORY_AUDIO, device, &ifData))
		break;
	aliasFlags = 0;
	if(SetupDiGetDeviceInterfaceAlias(handle, &ifData, &guid_KSCATEGORY_RENDER, &aliasData))
		if(aliasData.Flags && (!(aliasData.Flags & SPINT_REMOVED)))
			aliasFlags = 1;
	if(!aliasFlags)
		continue;
	else if(SetupDiGetDeviceInterfaceAlias(handle, &ifData, &guid_KSCATEGORY_REALTIME, &aliasData))
		streamingType = Type_kWaveRT;
	if(SetupDiGetDeviceInterfaceDetailW(handle, &ifData, devIfDetails, sizeIf, NULL, &devInfoData)){
		DWORD type;
		WCHAR fName[MAX_PATH] = {0};
		DWORD size_fName = sizeof(fName);
		WDMKS_Filter *newFilter = 0;
		int result = 0;

		if(get_winver() < 2
			&& !_wcsnicmp(devIfDetails->DevicePath, kUsbPrefix, sizeof(kUsbPrefix)/sizeof(kUsbPrefix[0])))
			if(!SetupDiGetDeviceRegistryPropertyW(handle, &devInfoData, SPDRP_LOCATION_INFORMATION,
				&type, (BYTE*)fName, sizeof(fName), NULL))
				fName[0] = 0;
		if(fName[0] == 0
			|| !_wcsnicmp(fName, kUsbNamePrefix, sizeof(kUsbNamePrefix)/sizeof(kUsbNamePrefix[0]))){
			HKEY hkey=SetupDiOpenDeviceInterfaceRegKey(handle, &ifData, 0, KEY_QUERY_VALUE);
			if(hkey != (HKEY)INVALID_HANDLE_VALUE)
				if(RegQueryValueExW(hkey, L"FriendlyName", 0, &type, (BYTE*)fName, &size_fName) == ERROR_SUCCESS)
					RegCloseKey(hkey);
				else
					fName[0] = 0;
		}
		TrimString(fName, size_fName);
		newFilter = filter_new(streamingType, devInfoData.DevInst, devIfDetails->DevicePath, fName, &result);
		if(result == 0){
			int pin;
			UINT filterIOs = 0;

			for(pin = 0; pin < newFilter->pinCount; ++pin){
				WDMKS_Pin *pPin = newFilter->pins[pin];
				if(!pPin)
					continue;
				filterIOs += max(1, pPin->inputCount);
			}
			DeviceCount += filterIOs;
			ppFilters[FilterCount] = newFilter;
			FilterCount++;
			if(FilterCount >= count_max)
				break;
		}
    }
    } /************************ block 1 ************************/	
	if(handle != NULL)
		SetupDiDestroyDeviceInfoList(handle);
	*pFilterCount = FilterCount;
	*pDeviceCount = DeviceCount;
	return 0;
}

static void filter_add_ref(WDMKS_Filter *filter)
{
    if (filter != 0)
        filter->filterRefCount++;
}

/*****************************************************************************************************************************/

typedef struct _WDMKS_Device {
	WDMKS_Pin *pin;
    char name[MAX_PATH];
	int isWaveRT;
	int isFloat;
    int32 minBits;
    int32 maxBits;
    int32 minSampleRate;
    int32 maxSampleRate;
	int32 minLatency;
	int32 maxLatency;
} WDMKS_Device;

static WDMKS_Device Devices[WDMKS_DEVLIST_MAX] = {0};
static int DeviceCount = -2;
static WDMKS_Pin *cPin = NULL;
static char *hostBuffer = NULL;
static UINT32 FrameBytes = 0;
static UINT32 BufferBytes = 0;
static UINT32 BlockBytes = 0;
static HANDLE hEventTcv[2] = {NULL};
static HANDLE hRenderThread = NULL;
static KSSTREAM_HEADER Header[2] = {0};
static OVERLAPPED Signal[2] = {0};
static UINT32 AudioPos = -1;
static UINT32 BufferPos = -1;
static int ThreadPriorityNum = 0;
static int RTPriorityNum = 0;
static int PinPriorityNum = 1;
static int IsWaveRT = 0;
static int IsPolling = 0;
static int IsMemoryBarrier = 0;
static int IsMemoryMapped = 0;
static int IsStarted = 0;
static int IsOpened = 0;
static int IsThreadStart = 0;
static int IsThreadExit = 0;
static int CvtMode = 0; // 0:no convert 1:convert 24bit(3byte->4byte)
static uint32 QueueSize = sizeof(int16) * 2 * (1L << DEFAULT_AUDIO_BUFFER_BITS) * 2;
static uint32 WaitTime = 1;
static uint32 ThreadWaitTime = 1;

static void free_devices(void)
{
	int i, id_device;	

	if(DeviceCount > 0){
		for(id_device = 0; id_device < DeviceCount; id_device++){
			WDMKS_Device *dev = &Devices[id_device];
			WDMKS_Filter *pFilter = NULL;

			if(!dev)
				continue;
			if(dev->pin)
				pFilter = dev->pin->parentFilter;
			if(!pFilter)
				continue;
			filter_free(pFilter);
		}
	}
	DeviceCount = -2;
}

static int create_device_list(void)
{
	WDMKS_Filter **ppFilters = NULL;
	int filterCount = 0;
	int totalDeviceCount = 0;
	int result = 0;
	int i, id_filter, id_device;

	if(DeviceCount != -2)
		return DeviceCount;	
	ppFilters  = (WDMKS_Filter **)safe_malloc(sizeof(WDMKS_Filter *) * WDMKS_FILTER_COUNT_MAX);
	if(!ppFilters)
		return -1;
	memset(ppFilters, 0, sizeof(WDMKS_Filter *) * WDMKS_FILTER_COUNT_MAX);
	filterCount = WDMKS_FILTER_COUNT_MAX;
	if(get_filter_list(ppFilters, &filterCount, &totalDeviceCount))
		goto error;
	if(totalDeviceCount < 1)
		goto error;
	memset(Devices, 0, sizeof(Devices));
	id_device = 0;
	for(id_filter = 0; id_filter < filterCount; ++id_filter){
		WDMKS_Filter *pFilter = ppFilters[id_filter];
		if(!pFilter)
			continue;
		if(id_device >= WDMKS_DEVLIST_MAX){
			if(pFilter->filterRefCount == 0)
				filter_free(pFilter);
			continue;
		}
		for (i = 0; i < pFilter->pinCount; ++i) {
			UINT m;
			ULONG nameIndex = 0;
			ULONG nameIndexHash = 0;
			WDMKS_Pin *pin = pFilter->pins[i];

			if(!pin)
				continue;
			for(m = 0; m < max(1, pin->inputCount); ++m){
				WDMKS_Device *dev = &Devices[id_device];
				wchar_t buf[MAX_PATH];
				UINT Index = 1;
				size_t n = 0;
				int error = 0;

				dev->pin = pin;
				_snwprintf(buf, MAX_PATH, L"%s :%d (%s)", pin->friendlyName, Index, pFilter->friendlyName);
				++Index;
#ifdef UNICODE
				WideCharToMultiByte(CP_UTF8, 0, buf, -1, dev->name, MAX_PATH, NULL, NULL);
#else
				WideCharToMultiByte(CP_ACP, 0, buf, -1, dev->name, MAX_PATH, NULL, NULL);
#endif
				switch(pFilter->devInfo.streamingType){
				case Type_kWaveCyclic:
					dev->isWaveRT = 0;
					dev->minLatency = get_winver() < 2 ? 20 : 10;
					break;
				case Type_kWaveRT:
					dev->isWaveRT = 1;
					dev->minLatency = 3;
					break;
				default:
					goto error;
					break;
				}
				dev->maxLatency = floor((double)4096 * div_playmode_rate * 1000);
				dev->isFloat = pin->isFloat;
				dev->minBits = pin->minBits;
				dev->maxBits = pin->maxBits;
				dev->minSampleRate = pin->minSampleRate;
				dev->maxSampleRate = pin->maxSampleRate;
				filter_add_ref(pFilter);
				++id_device;
				if(id_device > totalDeviceCount)
					goto error;
				if(id_device >= WDMKS_DEVLIST_MAX)
					break;
			}
			if(id_device >= WDMKS_DEVLIST_MAX)
				break;
		}
		if(pFilter->filterRefCount == 0){
			filter_free(pFilter);
		}
	}
	if(ppFilters){
		safe_free(ppFilters);
		ppFilters = NULL;
	}
	DeviceCount = totalDeviceCount;
	return totalDeviceCount;
error:
	if(ppFilters){
		safe_free(ppFilters);
		ppFilters = NULL;
	}
	free_devices();
	return -1;
}

static void print_device_list(void)
{
	int i, devnum = 0;
	
	for(i = 0; i < devnum; i++){
		WDMKS_Device *dev = &Devices[i];

		if(!dev)
			continue;
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%2d %s", i, dev->name);
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, " StreamingType: %s", dev->isWaveRT ? "WaveRT" : "WaveCyclic");
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, " Bits min:%d max:%d float:%s", 
			dev->minBits, dev->maxBits, dev->isFloat ? "support" : "not supoort");
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, " SampleRate min:%d max:%d", 
			dev->minSampleRate, dev->maxSampleRate);
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, " Latency min:%d max:%d", 
			dev->minLatency, dev->maxLatency);
	}
	return;
error0:
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "WDMKS output device not found");
	return;
}

static int write_buffer_cyclic(int ev)
{	
	size_t out_bytes;
    ULONG dummy = 0;
		
	out_bytes = calc_output_bytes(BlockBytes);
	if(out_bytes < BlockBytes)
		if(!input_buffer(NULL, BlockBytes - out_bytes))
			return FALSE;
	output_buffer((uint8 *)Header[ev].Data, BlockBytes);
    if(DeviceIoControl(cPin->handle, IOCTL_KS_WRITE_STREAM, NULL, 0, 
		&Header[ev], Header[ev].Size, &dummy, &Signal[ev]))
		return FALSE;
	else if(GetLastError() != ERROR_IO_PENDING)
        return FALSE;
	return TRUE;
}

static int write_buffer_rt_event(void)
{			
	ULONG pos;
	size_t out_bytes;
	
	if(IsMemoryBarrier)
		_WriteBarrier();
	if(pin_get_audio_position(cPin, &pos))
		return FALSE;
    pos += cPin->hwLatency;
    pos %= BufferBytes;
    pos &= ~(FrameBytes - 1);
	pos = pos < BlockBytes ? 1 : 0;	
	if(pos == BufferPos)
		pos = 1 - pos;
	BufferPos = pos;
	out_bytes = calc_output_bytes(BlockBytes);
	if(out_bytes < BlockBytes)
		if(!input_buffer(NULL, BlockBytes - out_bytes))
			return FALSE;
	output_buffer((uint8 *)(hostBuffer + pos * BlockBytes), BlockBytes);
	return TRUE;
}

static int write_buffer_rt_polling(void)
{		
	ULONG pos;
	size_t write_bytes, out_bytes;
	
	if(IsMemoryBarrier)
		_WriteBarrier();
	if(pin_get_audio_position(cPin, &pos))
		return FALSE;
    pos += cPin->hwLatency;
    pos %= BufferBytes;
    pos &= ~(FrameBytes - 1);
	pos = pos < BlockBytes ? 1 : 0;
	if(pos == AudioPos)
		return TRUE;
	AudioPos = pos;
	if(pos == BufferPos)
		pos = 1 - pos;
	BufferPos = pos;
	out_bytes = calc_output_bytes(BlockBytes);
	if(out_bytes < BlockBytes)
		if(!input_buffer(NULL, BlockBytes - out_bytes))
			return FALSE;
	output_buffer((uint8 *)(hostBuffer + pos * BlockBytes), BlockBytes);
	return TRUE;
}

static UINT WINAPI render_thread(void *arglist)
{
	int ret = 1;
	HANDLE hMmCss = NULL;
	DWORD mmCssTaskIndex = 0;
    HANDLE hTimer = NULL;	
		
	if(!IsWaveRT)
		SetThreadPriority(hRenderThread, ThreadPriorityNum);
	else if(pAvSetMmThreadCharacteristics && RTPriorityNum){
		hMmCss = (pAvSetMmThreadCharacteristics)(RTThreadPriorityName[RTPriorityNum], &mmCssTaskIndex);
		if(!hMmCss){			
			ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : Failed to set RT Priority.");
			goto thread_exit;
		}
	}
	IsThreadStart = 1;
	WaitForSingleObject(hEventTcv[0], INFINITE); // wait initialize open_output
	ResetEvent(hEventTcv[0]);
	if(!IsWaveRT){ // WaveCyclic event
		SetEvent(hEventTcv[0]);
		SetEvent(hEventTcv[1]);
		for(;;){ // event			
			UINT wait = WaitForMultipleObjects(2, hEventTcv, FALSE, INFINITE);
			int ev = wait - WAIT_OBJECT_0;
			if(ev < 0 || ev > 1) // error
				break;
			ResetEvent(hEventTcv[ev]);
			if(IsThreadExit) break;
			EnterCriticalSection(&critSect);
			write_buffer_cyclic(ev);
			LeaveCriticalSection(&critSect);
		}
	}else if(!IsPolling){ // WaveRT event
		for(;;){ // event
			WaitForSingleObject(hEventTcv[0], INFINITE);
			ResetEvent(hEventTcv[0]);
			if(IsThreadExit) break;		
			EnterCriticalSection(&critSect);
			write_buffer_rt_event();
			LeaveCriticalSection(&critSect);
		}
	}else{ // WaveRT polling
		for(;;){ // polling
			WaitForSingleObject(hEventTcv[0], ThreadWaitTime);
			ResetEvent(hEventTcv[0]);
			if(IsThreadExit) break;
			EnterCriticalSection(&critSect);
			write_buffer_rt_polling();
			LeaveCriticalSection(&critSect);
		}
	}		
	ret = 0;
thread_exit:
	if(hMmCss && pAvRevertMmThreadCharacteristics)
		(pAvRevertMmThreadCharacteristics)(hMmCss);
    if(hTimer){
        CancelWaitableTimer(hTimer);
        CloseHandle(hTimer);
        hTimer = NULL;
    }
	IsThreadStart = 0;
	return ret;
}

/*****************************************************************************************************************************/
/* interface function */

void close_output(void)
{
	int i;

	if(IsStarted){
		if(cPin){
			pin_set_state(cPin, KSSTATE_PAUSE);
			pin_set_state(cPin, KSSTATE_STOP);
		}
		IsStarted = FALSE;
	}
	IsThreadExit = 1;
	if(hRenderThread){
		if(hEventTcv[0])
			SetEvent(hEventTcv[0]);
		if(hEventTcv[1])
			SetEvent(hEventTcv[1]);		
		WaitForSingleObject(hRenderThread, INFINITE);
		CloseHandle(hRenderThread);
		hRenderThread = NULL;
	}
	IsThreadExit = 0;
	free_buffer();
    if(cPin){
		pin_unregister_notification_handle(cPin, cPin->positionRegister);
        pin_close(cPin);
		cPin = NULL;
	}
	if(hostBuffer){
		if(!IsWaveRT)
			safe_free(hostBuffer);
		hostBuffer = NULL;
	}
	if(hEventTcv[0]){
		CloseHandle(hEventTcv[0]);
		hEventTcv[0] = NULL;
	}
	if(hEventTcv[1]){
		CloseHandle(hEventTcv[1]);
		hEventTcv[1] = NULL;
	}
	free_devices();
	free_avrt();
	free_ksuser();
#ifdef USE_TEMP_ENCODE
	reset_temporary_encoding();
#endif
	IsOpened = 0;
}

/* 0=success, 1=warning, -1=fatal error */
int open_output(void)
{
	int i, devnum, dev_id, result;
	WDMKS_Device *dev = NULL;
    WDMKS_Pin *pin;
	int include_enc, exclude_enc;
	WAVEFORMATEXTENSIBLE wfe = {0};
	WAVEFORMATEX *pwf = NULL;
	UINT32 buf_dur = 20, max_dur;
	
	close_output();	
		
	if(!get_winver()){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! WDMKS require Windows 2000 and later.");
		goto error;
	}
	if(!load_ksuser()){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! KSUSER.DLL function failed.");
		goto error;
	}
	if(get_winver() > 1 && !load_avrt()){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! AVRT.DLL function failed.");
		goto error;
	}
	devnum = create_device_list();
	if(devnum < 1){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : get_device() failed.");
		goto error;
	}	
	dev_id = opt_output_device_id == -3 ? opt_wdmks_device_id : opt_output_device_id;
	if(dev_id == -2){
		print_device_list();
        return -1;
	}
	if(dev_id < 0 || dev_id >= devnum){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : opt_wdmks_device_id failed.");
		goto error;
	}	
	hEventTcv[0] = CreateEvent(NULL,FALSE,FALSE,NULL); // reset manual
	if(!hEventTcv[0])
		goto error;	
	hEventTcv[1] = CreateEvent(NULL,FALSE,FALSE,NULL); // reset manual
	if(!hEventTcv[1])
		goto error;	
	dev = &Devices[dev_id];
	if(!dev)
		goto error;
	pin = dev->pin;	
	if(!pin)
		goto error;
	IsWaveRT = dev->isWaveRT;
	if( opt_wdmks_thread_priority == THREAD_PRIORITY_IDLE ||
		opt_wdmks_thread_priority == THREAD_PRIORITY_LOWEST ||
		opt_wdmks_thread_priority == THREAD_PRIORITY_BELOW_NORMAL ||
		opt_wdmks_thread_priority == THREAD_PRIORITY_NORMAL ||
		opt_wdmks_thread_priority == THREAD_PRIORITY_ABOVE_NORMAL ||
		opt_wdmks_thread_priority == THREAD_PRIORITY_HIGHEST ||
		opt_wdmks_thread_priority == THREAD_PRIORITY_TIME_CRITICAL)
		ThreadPriorityNum = opt_wdmks_thread_priority;
	else
		ThreadPriorityNum = THREAD_PRIORITY_NORMAL;
	if(opt_wdmks_rt_priority < 0 || opt_wdmks_rt_priority > 7)
		RTPriorityNum = 0;
	else
		RTPriorityNum = opt_wdmks_rt_priority;
	if(opt_wdmks_pin_priority < 0 || opt_wdmks_pin_priority > 3)
		PinPriorityNum = 1;
	else
		PinPriorityNum = opt_wdmks_pin_priority;	
	include_enc = PE_SIGNED;
	exclude_enc = PE_BYTESWAP | PE_ULAW | PE_ALAW;	
	if(!(dpm.encoding & (PE_F64BIT | PE_64BIT | PE_F32BIT | PE_32BIT | PE_24BIT | PE_16BIT))) // 8bit
		include_enc |= PE_16BIT;
	dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);	
	if(opt_wdmks_format_ext){
		pwf = &wfe.Format;
		memcpy(&wfe.SubFormat, &guid_WAVE_FORMAT, sizeof(GUID));
		pwf->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		pwf->cbSize       = (WORD)22;
	}else{
		pwf = (WAVEFORMATEX *)&wfe;
		pwf->cbSize       = (WORD)0;
	}
	CvtMode = 0;
	if(dpm.encoding & PE_16BIT){
		if(opt_wdmks_format_ext){			
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 16;
			pwf->wBitsPerSample = (WORD) 16;
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 16;
		}
	}else if(dpm.encoding & PE_24BIT){
		if(opt_wdmks_format_ext){
			wfe.SubFormat.Data1 = WAVE_FORMAT_PCM;
			wfe.Samples.wValidBitsPerSample = 24;
			pwf->wBitsPerSample = (WORD) 32;
			CvtMode = 2;
		}else{
			pwf->wFormatTag = WAVE_FORMAT_PCM;
			pwf->wBitsPerSample = (WORD) 24;
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
		if(opt_wdmks_format_ext){
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
	FrameBytes = pwf->nBlockAlign;
	if(pin_open(pin, (WAVEFORMATEX *)&wfe, PinPriorityNum)){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : pin_open() failed.");
		goto error;	
	}
	cPin = pin;
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
	buf_dur = opt_wdmks_latency;
	if(buf_dur > 200)
		buf_dur = 200;
	else if(buf_dur < 1)
		buf_dur = 1;
	max_dur = floor((double)4096 * div_playmode_rate * 1000);
	if(buf_dur > max_dur)
		buf_dur = max_dur;
	if(IsWaveRT){		
		const DWORD dwTotalSize = ceil((double)buf_dur * playmode_rate_ms) * FrameBytes * 2;
		DWORD dwReqSize = dwTotalSize;
		int bCallMemoryBarrier = FALSE;
		ULONG hwFifoLatency = 0;

		if(pin_get_buffer(pin, (void **)&hostBuffer, &dwReqSize, &bCallMemoryBarrier)){
			ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : Failed to get output buffer RT.");
			goto error;
		}
		if(dwReqSize != dwTotalSize){
			UINT32 len = ceil((double)(dwReqSize >> 1) / FrameBytes * div_playmode_rate * 1000.0);
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, 
				"WDMKS WaveRT : Buffer length changed %u to %u.", dwTotalSize, dwReqSize);
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, 
				"WDMKS WaveRT : Latency changed %u[ms] to %u[ms].", buf_dur, len);
			buf_dur = len;
		}
		memset(hostBuffer, 0, dwReqSize);
		BufferBytes = dwReqSize;
        BlockBytes = dwReqSize >> 1;
		if(pin->pinKsSubType == SubType_kNotification)
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, "WDMKS WaveRT : Support Notification. (Event)");
		else
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, "WDMKS WaveRT : Not Support Notification (Polling).");
		if(pin->pinKsSubType == SubType_kPolled || opt_wdmks_polling)
			IsPolling = 1;
		else
			IsPolling = 0;
		IsMemoryBarrier = bCallMemoryBarrier ? 1 : 0;		
		if(!pin_get_HwLatency(pin, &hwFifoLatency)){
			pin->hwLatency = hwFifoLatency;			
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, 
				"WDMKS WaveRT : hwLatency : %d[bytes] : %f[ms].", hwFifoLatency,
				(double)hwFifoLatency/FrameBytes * div_playmode_rate * 1000);
		}else
			pin->hwLatency = 0;
        if(!IsPolling){
            if(pin_register_notification_handle(pin, hEventTcv[0])){
				ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : Failed to register rendering notification handle.");
				goto error;
			}
        }
        if(!pin_register_position_register(pin)){
			ctl->cmsg(CMSG_WARNING, VERB_NOISY, "WDMKS RT : Use Position Register.");
		}else{		
            ULONG pos = -0x01db1ade;	
			
			if(pin_get_audio_position(cPin, &pos) || pos != 0x0){
				ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : Failed to read render position register (IOCTL).");
				goto error;
            }
        }
	}else{
		KSALLOCATOR_FRAMING ksaf;
		KSALLOCATOR_FRAMING_EX ksafex;
		ULONG frameSize = 0;
        ULONG size = ceil((double)buf_dur * playmode_rate_ms) * FrameBytes;
		ULONG TotalSize = size * 2; // double buffer

		if(get_property_simple(pin->handle, &guid_KSPROPSETID_Connection, 
			KSPROPERTY_CONNECTION_ALLOCATORFRAMING, &ksaf, sizeof(ksaf))){
			if(!get_property_simple(pin->handle, &guid_KSPROPSETID_Connection,
				KSPROPERTY_CONNECTION_ALLOCATORFRAMING_EX, &ksafex, sizeof(ksafex)))
				frameSize = ksafex.FramingItem[0].FramingRange.Range.MinFrameSize;
		}else
			frameSize = ksaf.FrameSize;
		if(frameSize){
			if(size < frameSize){
				UINT32 len = ceil((double)frameSize / FrameBytes * div_playmode_rate * 1000.0);
				ctl->cmsg(CMSG_WARNING, VERB_NOISY, 
					"WDMKS WaveCyclic : Buffer length changed %u[ms] to %u[ms].", buf_dur, len);
				buf_dur = len;
				size = ceil((double)buf_dur * playmode_rate_ms * FrameBytes);
				TotalSize = size * 2; // 2=noOfPackets
			}
		}
        hostBuffer = (char *)safe_malloc(TotalSize);
        if(!hostBuffer){
			ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "WDMKS ERROR! : Failed to get output buffer CC.");
            goto error;
        }
		memset(hostBuffer, 0, TotalSize);
        for(i = 0; i < 2; ++i){
            Signal[i].hEvent = hEventTcv[i];
            Header[i].Data = hostBuffer + (i * size);
            Header[i].FrameExtent = size;
            Header[i].DataUsed = size;
            Header[i].Size = sizeof(KSSTREAM_HEADER);
            Header[i].PresentationTime.Numerator = 1;
            Header[i].PresentationTime.Denominator = 1;
        }
		BufferBytes = TotalSize;
        BlockBytes = size;
		IsPolling = 0;
    }	
	AudioPos = -1;
	BufferPos = -1;
	if(dpm.extra_param[0] < 2){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,X", dpm.extra_param[0]);
		dpm.extra_param[0] = 2;
	}	
	if(audio_buffer_size < 5){
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: X,%d", audio_buffer_bits);
		audio_buffer_bits = 5;
	}
	QueueSize = audio_buffer_size * FrameBytes * dpm.extra_param[0];
	WaitTime = (double)audio_buffer_size * div_playmode_rate * 1000.0 * DIV_2; // blocktime/2
	if(WaitTime < 1)
		WaitTime = 1;
	if(IsPolling){ // for polling
		ThreadWaitTime = floor(buf_dur * DIV_3);
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
	}
	if(!IsStarted){
		int count = 20; // 200ms
		while(!IsThreadStart && count > 0){ // 
			Sleep(10);
			--count;
		}
		if(count <= 0) // time out
			goto error;		
		if(pin_set_state(pin, KSSTATE_ACQUIRE))
			goto error;
		if(pin_set_state(pin, KSSTATE_PAUSE))
			goto error;
		if(pin_set_state(cPin, KSSTATE_RUN))
			goto error;
		SetEvent(hEventTcv[0]); // start process
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
	case PM_REQ_PLAY_END:
		return 0;
	default:
		return -1;
	}
}

int detect(void)
{
	if(!get_winver())
		return -1;
	if(create_device_list() < 1)
		return -1;
	return 1;
}

/*****************************************************************************************************************************/

int wdmks_device_list(WDMKS_DEVICELIST *device)
{	
	int i, devnum = 0;
	
	if(!get_winver())
		goto error0;
	devnum = create_device_list();
	if(devnum < 1)
		goto error0;
	memset(device, 0, sizeof(device));
	for(i = 0; i < devnum; i++){
		WDMKS_Device *dev = &Devices[i];
		device[i].deviceID = i;
		device[i].isWaveRT = dev->isWaveRT;
		device[i].isFloat = dev->isFloat;
		device[i].minBits = dev->minBits;
		device[i].maxBits = dev->maxBits;
		device[i].minSampleRate = dev->minSampleRate;
		device[i].maxSampleRate = dev->maxSampleRate;
		device[i].minLatency = dev->minLatency;
		device[i].maxLatency = dev->maxLatency;
		memcpy(device[i].name, dev->name, sizeof(char) * WDMKS_DEVLIST_LEN);
	}
	return devnum;
error0:
	return 0; // no device 
}


#endif /* AU_WDMKS */


