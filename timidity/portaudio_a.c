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

    portaudio_a.c by skeishi <s_keishi@mutt.freemail.ne.jp>
    based on esd_a.c

    Functions to play sound through Portaudio
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */


#ifdef AU_PORTAUDIO


#define PORTAUDIO_V19 1

#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t
#define _GNU_SOURCE
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <fcntl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef __W32__
#include <windows.h>
#endif /* __W32__ */
#include <portaudio.h>
#ifdef PORTAUDIO_V19
#include <pa_asio.h>
///r
#ifdef __W32__
#include <pa_win_ds.h>
#include <pa_win_wasapi.h>
#include <pa_win_waveformat.h>
#include <pa_win_wmme.h>
#endif /* __W32__ */
#endif /* PORTAUDIO_V19 */

#ifdef AU_PORTAUDIO_DLL
#include "w32_portaudio.h"
#endif /* AU_PORTAUDIO_DLL */

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

///r
#include "portaudio_a.h"

#ifndef PORTAUDIO_V19
typedef int PaDeviceIndex;
typedef int PaHostApiIndex;
#define paStreamIsStopped paNoError
#define Pa_GetDeviceCount Pa_CountDevices
#define Pa_HostApiTypeIdToHostApiIndex(n) (0)
#endif /* !PORTAUDIO_V19 */

int opt_pa_wmme_device_id = -1;
int opt_pa_ds_device_id = -1;
int opt_pa_asio_device_id = -1;
int opt_pa_wdmks_device_id = -1;
int opt_pa_wasapi_device_id = -1;
int opt_pa_wasapi_flag = 0;
int opt_pa_wasapi_stream_category = 0;
int opt_pa_wasapi_stream_option = 0;

static int opt_pa_device_id = -1;

//#define DATA_BLOCK_SIZE     (27648*4) /* WinNT Latency is 600 msec read pa_dsound.c */
#define SAMPLE_SIZE_MAX  (8) /* Maxmum sample size (byte) = 4byte(32bit) * 2ch(stereo) */
#define BUFFER_NUM_MAX   (1000)
#define BUFFER_SIZE_MAX  (AUDIO_BUFFER_SIZE * SAMPLE_SIZE_MAX * BUFFER_NUM_MAX)
#define SAMPLE_RATE      (44100)

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(const uint8 *buf, size_t nbytes);
static int acntl(int request, void *arg);

static void print_device_list(void);

static int stereo=2;
static int conv16_32 = 0;
static int conv24_32 = 0;
static int data_nbyte;
static int numBuffers;
static int framesPerBuffer=128;
static unsigned int framesPerInBuffer;
static unsigned int bytesPerInBuffer=0;
static unsigned int bytesPerBuffer;
//static int  firsttime;
static int pa_active=0;
static int first=1;



#if PORTAUDIO_V19
PaHostApiTypeId HostApiTypeId;
PaHostApiIndex HostApiIndex;
const PaHostApiInfo  *HostApiInfo;
PaDeviceIndex DeviceIndex;
const PaDeviceInfo *DeviceInfo;
PaStreamParameters StreamParameters;
PaStream *stream;
PaError  err;
///r
PaStreamFlags paStreamFlags;
PaWasapiStreamInfo wasapiStreamInfo;
#else
PaDeviceID DeviceID;
const PaDeviceInfo *DeviceInfo;
PortAudioStream *stream;
PaError  err;
#endif
typedef struct {
///r
	char buf[BUFFER_SIZE_MAX]; // 
	uint32 samplesToGo;
	char *bufpoint;
	char *bufepoint;
} padata_t;
padata_t pa_data;

/* export the playback mode */

#ifdef AU_PORTAUDIO_DLL
static int open_output_asio(void);
static int open_output_win_wasapi(void);
static int open_output_win_wdmks(void);
static int open_output_win_ds(void);
static int open_output_win_wmme(void);
PlayMode portaudio_asio_play_mode = {
	(SAMPLE_RATE),
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT|PF_CAN_TRACE,
    -1,
    {32}, //def 32 /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"PortAudio(ASIO)", 'o',
    NULL,
    open_output_asio,
    close_output,
    output_data,
    acntl
};
#ifdef PORTAUDIO_V19
PlayMode portaudio_win_wasapi_play_mode = {
	(SAMPLE_RATE),
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT|PF_CAN_TRACE,
    -1,
    {32}, //def 1 /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"PortAudio(WASAPI)", 'W',
    NULL,
    open_output_win_wasapi,
    close_output,
    output_data,
    acntl
};
PlayMode portaudio_win_wdmks_play_mode = {
	(SAMPLE_RATE),
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT|PF_CAN_TRACE,
    -1,
    {32}, //def 1 /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"PortAudio(KernelStreaming)", 'K',
    NULL,
    open_output_win_wdmks,
    close_output,
    output_data,
    acntl
};
#endif
PlayMode portaudio_win_ds_play_mode = {
	(SAMPLE_RATE),
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT|PF_CAN_TRACE,
    -1,
    {32}, //def 32 /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"PortAudio(DirectSound)", 'P',
    NULL,
    open_output_win_ds,
    close_output,
    output_data,
    acntl
};
PlayMode portaudio_win_wmme_play_mode = {
	(SAMPLE_RATE),
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT|PF_CAN_TRACE,
    -1,
    {32}, //def 32 /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"PortAudio(WMME)", 'p',
    NULL,
    open_output_win_wmme,
    close_output,
    output_data,
    acntl
};
PlayMode * volatile portaudio_play_mode = &portaudio_win_wmme_play_mode;
#define dpm (*portaudio_play_mode)

#else

#define dpm portaudio_play_mode

PlayMode dpm = {
	(SAMPLE_RATE),
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT|PF_CAN_TRACE,
    -1,
    {32}, /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"Portaudio Driver", 'p',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};
#endif

#ifdef PORTAUDIO_V19

void processor_output(
	void *inputBuffer, long inputFrames, 
	void *outputBuffer, long outputFrames, 
	void *userData)
{
    unsigned int i;
	size_t samplesToGo;
	uint8 *bufpoint;
    uint8 *out;
	size_t datalength;
	uint8 * buflimit;
	size_t samples1, samples2;  
	
#if defined(__W32__)
	EnterCriticalSection(&critSect);
#endif
    out = (uint8*)outputBuffer;
	datalength = outputFrames * data_nbyte * stereo;
	buflimit = (uint8*)pa_data.buf + bytesPerInBuffer;
	samplesToGo = pa_data.samplesToGo;
	bufpoint = (uint8*)pa_data.bufpoint;

#ifndef USE_TEMP_ENCODE		
	if(conv24_32){ // Int24
		samples1 = outputFrames * stereo;
		if(samplesToGo < datalength){
			samples2 = samplesToGo / 3;
			for(i=0; i < samples2; i++){
				*out++ = 0;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				if(buflimit <= bufpoint){
					bufpoint = (uint8*)pa_data.buf;
				}
			}
			samplesToGo = 0;
			for(; i < samples1; i++){
				*out++ = 0;
				*out++ = 0;
				*out++ = 0;
				*out++ = 0;
			}
		}else{
			for(i = 0; i < samples1; i++){
				*out++ = 0;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				if(buflimit <= bufpoint){
					bufpoint = (uint8*)pa_data.buf;
				}
			}
			samplesToGo -= datalength;
		}
	}else if(conv16_32){
		if(samplesToGo < datalength  ){		
			for(i = 0; i < (samplesToGo >> 1); i++){
				*out++ = 0;
				*out++ = 0;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				if(buflimit <= bufpoint){
					bufpoint = (uint8*)pa_data.buf;
				}
			}
			samplesToGo = 0;
			for(; i < (datalength>>1); i++){
				*out++ = 0;
				*out++ = 0;
				*out++ = 0;
				*out++ = 0;
			}
		}else{
			for(i = 0; i < (datalength >> 1); i++){
				*out++ = 0;
				*out++ = 0;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				if(buflimit <= bufpoint){
					bufpoint = (uint8*)pa_data.buf;
				}
			}
			samplesToGo -= datalength;
		}
	}else
#endif
	{
		if(samplesToGo < datalength ){
			if(bufpoint+samplesToGo <= buflimit){
				memcpy(out, bufpoint, samplesToGo);
				bufpoint += samplesToGo;
			}else if(buflimit - bufpoint >= 0) {
				size_t send;
				send = buflimit - bufpoint;
				if (send !=0) memcpy(out, bufpoint, send);
				out +=send;
				memcpy(out, pa_data.buf, samplesToGo -send);
				bufpoint = (uint8*)pa_data.buf + samplesToGo -send;
				out += samplesToGo - send;
			}
			memset(out, 0x0, datalength - samplesToGo);
			samplesToGo=0;
		}else{
			if(bufpoint + datalength <= buflimit){
				memcpy(out, bufpoint, datalength);
				bufpoint += datalength;
			}else if(buflimit-bufpoint >= 0) {
				size_t send;
				send = buflimit - bufpoint;
				if (send !=0) memcpy(out, bufpoint, send);
				out += send;
				memcpy(out, pa_data.buf, datalength -send);
				bufpoint = (uint8*)pa_data.buf + datalength -send;
			}
			samplesToGo -= datalength;
		}
	}
	pa_data.samplesToGo = samplesToGo;
	pa_data.bufpoint = (char *)bufpoint;
#if defined(__W32__)
	LeaveCriticalSection(&critSect);
#endif
}

int paCallback(  const void *inputBuffer, void *outputBuffer,
                     unsigned long outputFrames,
                     const PaStreamCallbackTimeInfo* timeInfo,
                     PaStreamCallbackFlags statusFlags,
	                 void *userData )
#else
int paCallback(  void *inputBuffer, void *outputBuffer,
                     unsigned long framesPerBuffer,
                     PaTimestamp outTime, 
                     void *userData )
#endif
{

    unsigned int i;
	int finished = 0;

/* Cast data passed through stream to our structure type. */
//    pa_data_t pa_data = (pa_data_t*)userData;
	
	size_t samplesToGo;
	uint8 *bufpoint;
    uint8 *out;
	size_t datalength;
	uint8 * buflimit;
	
#if defined(__W32__)
	EnterCriticalSection(&critSect);
#endif
    out = (uint8*)outputBuffer;
	datalength = outputFrames * data_nbyte * stereo;
//	buflimit = pa_data.buf+bytesPerInBuffer*2;
//	buflimit = pa_data.buf+DATA_BLOCK_SIZE*2;
	buflimit = (uint8*)pa_data.buf+bytesPerInBuffer;
	samplesToGo = pa_data.samplesToGo;
	bufpoint = (uint8*)pa_data.bufpoint;
	
#ifndef USE_TEMP_ENCODE	
	if(conv16_32){
		if(samplesToGo < datalength  ){		
			for(i=0;i<(samplesToGo>>1);i++){
				*out++ = 0;
				*out++ = 0;
				*out++ = *(bufpoint)++;
				*out++ = *(bufpoint)++;
				if( buflimit <= bufpoint ){
					bufpoint = (uint8*)pa_data.buf;
				}
			}
			samplesToGo=0;
			for(;i<(datalength>>1);i++){
				*out++ = 0;
				*out++ = 0;
				*out++ = 0;
				*out++ = 0;
			}
			finished = 0;
		}else{
			for(i=0;i<(datalength>>1);i++){
				*out++ = 0;
				*out++ = 0;
				*out++=*(bufpoint)++;
				*out++=*(bufpoint)++;
				if( buflimit <= bufpoint ){
					bufpoint = (uint8*)pa_data.buf;
				}
			}
			samplesToGo -= datalength;
		}
	}else
#endif
	{
		if(samplesToGo < datalength  ){
			if(bufpoint+samplesToGo <= buflimit){
				memcpy(out, bufpoint, samplesToGo);
				bufpoint += samplesToGo;
			}else if(buflimit-bufpoint >= 0) {
				size_t send;
				send = buflimit-bufpoint;
				if (send !=0) memcpy(out, bufpoint, send);
				out +=send;
				memcpy(out, pa_data.buf, samplesToGo -send);
				bufpoint = (uint8*)pa_data.buf+samplesToGo -send;
				out += samplesToGo -send;
			}
			memset(out, 0x0, datalength-samplesToGo);
			samplesToGo=0;
			finished = 0;
		}else{
			if(bufpoint + datalength <= buflimit){
				memcpy(out, bufpoint, datalength);
				bufpoint += datalength;
			}else if(buflimit-bufpoint >= 0) {
				size_t send;
				send = buflimit-bufpoint;
				if (send !=0) memcpy(out, bufpoint, send);
				out += send;
				memcpy(out, pa_data.buf, datalength -send);
				bufpoint = (uint8*)pa_data.buf+datalength -send;
			}
			samplesToGo -= datalength;
		}
	}
	pa_data.samplesToGo=samplesToGo;
	pa_data.bufpoint = (char *)bufpoint;
#if defined(__W32__)
	LeaveCriticalSection(&critSect);
#endif
	return finished;
}

#ifdef AU_PORTAUDIO_DLL
static int open_output_asio(void)
{
	portaudio_play_mode = &portaudio_asio_play_mode;
	return open_output();
}
#ifdef PORTAUDIO_V19
static int open_output_win_wasapi(void)
{
	portaudio_play_mode = &portaudio_win_wasapi_play_mode;
	return open_output();
}
static int open_output_win_wdmks(void)
{
	portaudio_play_mode = &portaudio_win_wdmks_play_mode;
	return open_output();
}
#endif
static int open_output_win_ds(void)
{
	portaudio_play_mode = &portaudio_win_ds_play_mode;
	return open_output();
}
static int open_output_win_wmme(void)
{
	portaudio_play_mode = &portaudio_win_wmme_play_mode;
	return open_output();
}
#endif


static int open_output(void)
{
	double rate;
	int n, nrates, include_enc, exclude_enc, ret;
	int tmp;
	PaSampleFormat SampleFormat, nativeSampleFormats;
/*
	if( dpm.name != NULL)
		ret = sscanf(dpm.name, "%d", &opt_pa_device_id);
	if (dpm.name == NULL || ret == 0 || ret == EOF)
		opt_pa_device_id = -2;
*/

#ifdef AU_PORTAUDIO_DLL
#if PORTAUDIO_V19
  {
///r
		if(&dpm == &portaudio_asio_play_mode){
			opt_pa_device_id = opt_output_device_id != -3 ? opt_output_device_id : opt_pa_asio_device_id;
			HostApiTypeId = paASIO;
		} else if(&dpm == &portaudio_win_wdmks_play_mode){
			opt_pa_device_id = opt_output_device_id != -3 ? opt_output_device_id : opt_pa_wdmks_device_id;
			HostApiTypeId = paWDMKS;
		} else if(&dpm == &portaudio_win_wasapi_play_mode){
			opt_pa_device_id = opt_output_device_id != -3 ? opt_output_device_id : opt_pa_wasapi_device_id;
			HostApiTypeId = paWASAPI;
		} else if(&dpm == &portaudio_win_ds_play_mode){
			opt_pa_device_id = opt_output_device_id != -3 ? opt_output_device_id : opt_pa_ds_device_id;
			HostApiTypeId = paDirectSound;
		} else if(&dpm == &portaudio_win_wmme_play_mode){
			opt_pa_device_id = opt_output_device_id != -3 ? opt_output_device_id : opt_pa_wmme_device_id;
			HostApiTypeId = paMME;
		} else {
			opt_pa_device_id = opt_pa_wmme_device_id;
			return -1;
		}
		if (load_portaudio_dll(0)) {
#ifdef _WIN64
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "DLL load failed: %s", "portaudio_x64.dll");
#else
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "DLL load failed: %s", "portaudio_x86.dll");
#endif
			return -1;
		}
  }
#else
  {
		if(&dpm == &portaudio_asio_play_mode){
			opt_pa_device_id = opt_pa_asio_device_id;
			if (load_portaudio_dll(PA_DLL_ASIO)) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					  "DLL load failed: %s", "pa_asio.dll");
				return -1;
			}
		} else if(&dpm == &portaudio_win_ds_play_mode){
			opt_pa_device_id = opt_pa_ds_device_id;
			if (load_portaudio_dll(PA_DLL_WIN_DS)) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					  "DLL load failed: %s", "pa_win_ds.dll");
				return -1;
			}
		} else if(&dpm == &portaudio_win_wmme_play_mode){
			opt_pa_device_id = opt_pa_wmme_device_id;
			if (load_portaudio_dll(PA_DLL_WIN_WMME)) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					  "DLL load failed: %s", "pa_win_wmme.dll");
				return -1;
			}
		} else {
			opt_pa_device_id = opt_pa_wmme_device_id;
			return -1;
		}
  }
#endif
#endif
	/* if call twice Pa_OpenStream causes paDeviceUnavailable error  */
	if(pa_active == 1) return 0; 
	if(pa_active == 0){
		err = Pa_Initialize();
		if( err != paNoError ) goto error;
		pa_active = 1;
	}

	if (opt_pa_device_id == -2){
		print_device_list();
		goto error2;
	}
#ifdef PORTAUDIO_V19
///r
    include_enc = 0;
	exclude_enc = PE_BYTESWAP |PE_ULAW |PE_ALAW | PE_F64BIT | PE_64BIT;
    if(dpm.encoding & (PE_F32BIT | PE_32BIT | PE_24BIT | PE_16BIT))
		include_enc |= PE_SIGNED;
    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

#ifdef AU_PORTAUDIO_DLL
    {	
        PaHostApiIndex i, ApiCount;
		i = 0;
		ApiCount = Pa_GetHostApiCount();
		do{
			HostApiInfo=Pa_GetHostApiInfo(i);
			if( HostApiInfo->type == HostApiTypeId ) break;
			i++;
		}while ( i < ApiCount );
		if ( i == ApiCount ) goto error;
	
		DeviceIndex = HostApiInfo->defaultOutputDevice;
		if(DeviceIndex==paNoDevice) goto error;
    }
#else
	DeviceIndex = Pa_GetDefaultOutputDevice();
	if(DeviceIndex==paNoDevice) goto error;
#endif
	DeviceInfo = Pa_GetDeviceInfo( DeviceIndex);
	if(DeviceInfo==NULL) goto error;

	if(opt_pa_device_id != -1){
		const PaDeviceInfo *id_DeviceInfo;
    	id_DeviceInfo=Pa_GetDeviceInfo((PaDeviceIndex)opt_pa_device_id);
		if(id_DeviceInfo==NULL) goto error;
		if( DeviceInfo->hostApi == id_DeviceInfo->hostApi){
			DeviceIndex=(PaDeviceIndex)opt_pa_device_id;
			DeviceInfo = id_DeviceInfo;
		}
    }
///r
	if (dpm.encoding & PE_F32BIT) {
		SampleFormat = paFloat32;
		data_nbyte = 4;
	}else if (dpm.encoding & PE_32BIT) {
		SampleFormat = paInt32;
		data_nbyte = 4;
	}else if (dpm.encoding & PE_24BIT) {
		SampleFormat = paInt24;
		data_nbyte = 3;
	}else if (dpm.encoding & PE_16BIT) {
		SampleFormat = paInt16;
		data_nbyte = 2;
	}else if (dpm.encoding & PE_SIGNED) {
		SampleFormat = paInt8;
		data_nbyte = 1;
	}else {
		SampleFormat = paUInt8;
		data_nbyte = 1;
	}
	stereo = (dpm.encoding & PE_MONO) ? 1 : 2;

	pa_data.samplesToGo = 0;
	pa_data.bufpoint = pa_data.buf;
	pa_data.bufepoint = pa_data.buf;
//	firsttime = 1;
///r
/*
	numBuffers = 1; //Pa_GetMinNumBuffers( framesPerBuffer, dpm.rate );
	framesPerInBuffer = numBuffers * framesPerBuffer;
	if (framesPerInBuffer < 4096) framesPerInBuffer = 4096;
	bytesPerInBuffer = framesPerInBuffer * data_nbyte * stereo;
*/

	if (dpm.extra_param[0] < 2)
	{
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,X", dpm.extra_param[0]);
		dpm.extra_param[0] = 2; // MinNumBuffers
	}
	if (audio_buffer_bits < 5)
    {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: X,%d", audio_buffer_bits);
        audio_buffer_bits = 5; // block size 32
    }
 // check buffer size
#if 0
	tmp = dpm.extra_param[0] * audio_buffer_size;
	if(tmp < 4096)
    {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d,%d", dpm.extra_param[0], audio_buffer_bits);
		dpm.extra_param[0] = DEFAULT_AUDIO_BUFFER_NUM;
		audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;
    }
#endif


	/* set StreamParameters */
	StreamParameters.device = DeviceIndex;
	StreamParameters.channelCount = stereo;
#if 1 // c201-
	if(opt_realtime_playing)
		StreamParameters.suggestedLatency = DeviceInfo->defaultLowOutputLatency;
	else
		StreamParameters.suggestedLatency = DeviceInfo->defaultHighOutputLatency;
#else // -c200
	if(ctl->id_character != 'r' && ctl->id_character != 'A' && ctl->id_character != 'W' && ctl->id_character != 'N' && ctl->id_character != 'P'){
		StreamParameters.suggestedLatency = DeviceInfo->defaultHighOutputLatency;
	}else{
		StreamParameters.suggestedLatency = DeviceInfo->defaultLowOutputLatency;
	}
#endif
///r
	if( SampleFormat == paInt16){
		StreamParameters.sampleFormat = paInt16;
		if( paFormatIsSupported != Pa_IsFormatSupported( NULL , &StreamParameters,(double) dpm.rate )){
			StreamParameters.sampleFormat = paInt32;
			conv16_32 = 1;
		} else {
			StreamParameters.sampleFormat = paInt16;
			conv16_32 = 0;
		}
	}else{
		StreamParameters.sampleFormat = SampleFormat;
	}
///r	
	if(HostApiTypeId == paWASAPI){
		wasapiStreamInfo.size = sizeof(wasapiStreamInfo);
		wasapiStreamInfo.hostApiType = paWASAPI;
		wasapiStreamInfo.version = 1;
		wasapiStreamInfo.flags = opt_pa_wasapi_flag;
		wasapiStreamInfo.channelMask = 0;
		wasapiStreamInfo.hostProcessorOutput = (PaWasapiHostProcessorCallback)processor_output;
		wasapiStreamInfo.hostProcessorInput = NULL;
		wasapiStreamInfo.threadPriority = eThreadPriorityNone;
#ifdef PORTAUDIO_V19_6
		wasapiStreamInfo.streamCategory = (PaWasapiStreamCategory)opt_pa_wasapi_stream_category;
		wasapiStreamInfo.streamOption = (PaWasapiStreamOption)opt_pa_wasapi_stream_option;
#endif
		StreamParameters.hostApiSpecificStreamInfo = &wasapiStreamInfo;
		if(SampleFormat == paInt24 && (opt_pa_wasapi_flag & paWinWasapiRedirectHostProcessor) )
			conv24_32 = 1; // only use RedirectHostProcessor
		else
			conv24_32 = 0;
	}else{
		StreamParameters.hostApiSpecificStreamInfo = NULL;
		conv24_32 = 0;
	}
	
#ifdef USE_TEMP_ENCODE
	if(conv24_32){			
		int tmp_enc = dpm.encoding;
		tmp_enc &= ~PE_24BIT;
		tmp_enc |= PE_32BIT;
		set_temporary_encoding(tmp_enc);
		data_nbyte = data_nbyte * 4 / 3;
	}else if(conv16_32 == 2){			
		int tmp_enc = dpm.encoding;
		tmp_enc &= ~PE_16BIT;
		tmp_enc |= PE_32BIT;
		set_temporary_encoding(tmp_enc);
		data_nbyte *= 2;
	}else{
		reset_temporary_encoding();
	}
#endif

	numBuffers = dpm.extra_param[0];
	framesPerBuffer = audio_buffer_size;
	framesPerInBuffer = numBuffers * framesPerBuffer;	
	bytesPerBuffer = framesPerBuffer * data_nbyte * stereo; ///r
	bytesPerInBuffer = framesPerInBuffer * data_nbyte * stereo;

	paStreamFlags = paNoFlag;

	err = Pa_IsFormatSupported( NULL , &StreamParameters, (double) dpm.rate );
	if ( err != paNoError) goto error;
	err = Pa_OpenStream(    
		& stream,			/* passes back stream pointer */
		NULL,			 	/* inputStreamParameters */
		&StreamParameters,	/* outputStreamParameters */
		(double) dpm.rate,	/* sample rate */
		paFramesPerBufferUnspecified,	/* frames per buffer */
///r
		paStreamFlags,	    /* PaStreamFlags */
		paCallback,			/* specify our custom callback */
		&pa_data			/* pass our data through to callback */
		);
//		Pa_Sleeep(1);
	if ( err != paNoError) goto error;
	return 0;
	
#else
	if(opt_pa_device_id == -1){
		DeviceID = Pa_GetDefaultOutputDeviceID();
	    if(DeviceID==paNoDevice) goto error2;
	}else{
		DeviceID = opt_pa_device_id;
	}
	DeviceInfo = Pa_GetDeviceInfo( DeviceID);	
	if(DeviceInfo==NULL) goto error2;
	nativeSampleFormats = DeviceInfo->nativeSampleFormats;
///r
    include_enc = 0;
	exclude_enc = PE_BYTESWAP |PE_ULAW |PE_ALAW | PE_F64BIT | PE_64BIT;
    if(dpm.encoding & (PE_F32BIT | PE_32BIT | PE_24BIT | PE_16BIT))
		include_enc |= PE_SIGNED;
	if (!(nativeSampleFormats & paInt16) && !(nativeSampleFormats & paInt32)) {exclude_enc |= PE_16BIT;}
	if (!(nativeSampleFormats & paInt24)) {exclude_enc |= PE_24BIT;}
	if (!(nativeSampleFormats & paInt32)) {exclude_enc |= PE_32BIT;}
    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

	if (dpm.encoding & PE_F32BIT) {
		SampleFormat = paFloat32;
		data_nbyte = 4;
	}else if (dpm.encoding & PE_32BIT) {
		SampleFormat = paInt32;
		data_nbyte = 4;
	}else if (dpm.encoding & PE_24BIT) {
		SampleFormat = paInt24;
		data_nbyte = 3;
	}else if (dpm.encoding & PE_16BIT) {
		if(nativeSampleFormats & paInt16){
			SampleFormat = paInt16;
			data_nbyte = 2;
		}else{
			SampleFormat = paInt32;
			conv16_32 = 1;
			data_nbyte = 2;
		}
	}else if (dpm.encoding & PE_SIGNED) {
		SampleFormat = paInt8;
		data_nbyte = 1;
	}else {
		SampleFormat = paUInt8;
		data_nbyte = 1;
	}

	stereo = (dpm.encoding & PE_MONO) ? 1 : 2;
//	data_nbyte = (dpm.encoding & PE_16BIT) ? 2 : 1;
//	data_nbyte = (dpm.encoding & PE_24BIT) ? 3 : data_nbyte;

	nrates = DeviceInfo->numSampleRates;
	if (nrates == -1) {	/* range supported */
		rate = dpm.rate;
		if (dpm.rate < DeviceInfo->sampleRates[0]) rate = DeviceInfo->sampleRates[0];
		if (dpm.rate > DeviceInfo->sampleRates[1]) rate = DeviceInfo->sampleRates[1];
	} else {
		rate = DeviceInfo->sampleRates[nrates-1];
		for (n = nrates - 1; n >= 0; n--) {	/* find nearest sample rate */
			if (dpm.rate <= DeviceInfo->sampleRates[n]) rate=DeviceInfo->sampleRates[n];
		}
	}
	dpm.rate = (int32)rate;
	
	pa_data.samplesToGo = 0;
	pa_data.bufpoint = pa_data.buf;
	pa_data.bufepoint = pa_data.buf;
//	firsttime = 1;
///r	
#ifdef USE_TEMP_ENCODE
	if(conv24_32){			
		int tmp_enc = dpm.encoding;
		tmp_enc &= ~PE_24BIT;
		tmp_enc |= PE_32BIT;
		set_temporary_encoding(tmp_enc);
		data_nbyte = data_nbyte * 4 / 3;
	}else if(conv16_32 == 2){			
		int tmp_enc = dpm.encoding;
		tmp_enc &= ~PE_16BIT;
		tmp_enc |= PE_32BIT;
		set_temporary_encoding(tmp_enc);
		data_nbyte *= 2;
	}else{
		reset_temporary_encoding();
	}
#endif
	numBuffers = Pa_GetMinNumBuffers( framesPerBuffer, dpm.rate );
	//framesPerInBuffer = numBuffers * framesPerBuffer;
	//if (framesPerInBuffer < 4096) framesPerInBuffer = 4096;
	//bytesPerInBuffer = framesPerInBuffer * data_nbyte * stereo;
	if (numBuffers < dpm.extra_param[0])
		numBuffers = dpm.extra_param[0]
	framesPerInBuffer = numBuffers * framesPerBuffer;
	if (framesPerInBuffer < 2048)
		framesPerInBuffer = 2048;
	bytesPerBuffer = framesPerBuffer * data_nbyte * stereo;
	bytesPerInBuffer = framesPerInBuffer * data_nbyte * stereo;

	err = Pa_OpenDefaultStream(
    	&stream,        /* passes back stream pointer */
    	0,              /* no input channels */
    	stereo,              /* 2:stereo 1:mono output */
    	SampleFormat,      /* 24bit 16bit 8bit output */
		(double)dpm.rate,          /* sample rate */
    	framesPerBuffer,            /* frames per buffer */
    	numBuffers,              /* number of buffers, if zero then use default minimum */
    	paCallback, /* specify our custom callback */
    	&pa_data);   /* pass our data through to callback */
	if ( err != paNoError && err != paHostError) goto error;
	return 0;

#endif		

error:
	ctl->cmsg(  CMSG_ERROR, VERB_NORMAL, "PortAudio error: %s\n", Pa_GetErrorText( err ) );
error2:
	Pa_Terminate(); pa_active = 0;
#ifdef AU_PORTAUDIO_DLL
#ifndef PORTAUDIO_V19
  free_portaudio_dll();
#endif
#endif
#ifdef USE_TEMP_ENCODE
	reset_temporary_encoding();
#endif

	return -1;
}

static int output_data(const uint8 *buf, size_t nbytes)
{
	unsigned int i;
	size_t samplesToGo;
	char *bufepoint;
	int32 max_count = 2000; // 2sec
	unsigned int update_bytes = bytesPerInBuffer - bytesPerBuffer; 

///r
#if 1 // c211
	for(;;){
		if(pa_active == 0) return -1; 
#if defined(__W32__)
		EnterCriticalSection(&critSect);
#endif
		if(pa_data.samplesToGo <= update_bytes)
			break;
#if defined(__W32__)
		LeaveCriticalSection(&critSect);
#endif
		if(max_count <= 0){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "PortAudio error: timeout output_data().");
			return -1; 
		}
		Pa_Sleep(1); -- max_count; 
	}
#else
    if(pa_active == 0) return -1; 
//	while((pa_active==1) && (pa_data.samplesToGo > bytesPerInBuffer) && (max_count > 0)){ Pa_Sleep(1); -- max_count; };
//	while((pa_active==1) && (pa_data.samplesToGo > (bytesPerInBuffer>>1)) && (max_count > 0)){
	while((pa_active==1) && (pa_data.samplesToGo > update_bytes) && (max_count > 0)){
	if(max_count <= 0){
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "PortAudio error: timeout output_data().");
		return -1; 
	}
#if defined(__W32__)
		EnterCriticalSection(&critSect);
#endif
#endif

//	if(pa_data.samplesToGo > DATA_BLOCK_SIZE){ 
//		Sleep(  (pa_data.samplesToGo - DATA_BLOCK_SIZE)/dpm.rate/4  );
//	}
	samplesToGo=pa_data.samplesToGo;
	bufepoint=pa_data.bufepoint;

//	if (pa_data.buf+DATA_BLOCK_SIZE*2 >= bufepoint + nbytes){
//	if (pa_data.buf+bytesPerInBuffer*2 >= bufepoint + nbytes){
	if (pa_data.buf+bytesPerInBuffer >= bufepoint + nbytes){
		memcpy(bufepoint, buf, nbytes);
		bufepoint += nbytes;
		//buf += nbytes;
	}else{
//		int32 send = pa_data.buf+BUFFER_SIZE_MAX - bufepoint;
//		int32 send = pa_data.buf+DATA_BLOCK_SIZE*2 - bufepoint;
//		int32 send = pa_data.buf+bytesPerInBuffer*2 - bufepoint;
		int32 send = pa_data.buf+bytesPerInBuffer - bufepoint;
		if (send > 0) memcpy(bufepoint, buf, send);
		buf += send;
		memcpy(pa_data.buf, buf, nbytes - send);
		bufepoint = pa_data.buf + nbytes - send;
		//buf += nbytes-send;
	}
	samplesToGo += nbytes;

	pa_data.samplesToGo=samplesToGo;
	pa_data.bufepoint=bufepoint;
	
#if defined(__W32__)
	LeaveCriticalSection(&critSect);
#endif
/*
	if(firsttime==1){
		err = Pa_StartStream( stream );

		if( err != paNoError ) goto error;
		firsttime=0;
	}
*/
#ifdef PORTAUDIO_V19
	if( 0==Pa_IsStreamActive(stream)){
#else
	if( 0==Pa_StreamActive(stream)){
#endif
		err = Pa_StartStream( stream );

		if( err != paNoError ) goto error;
	}
		
//	if(ctl->id_character != 'r' && ctl->id_character != 'A' && ctl->id_character != 'W' && ctl->id_character != 'P')
//	    while((pa_active==1) && (pa_data.samplesToGo > bytesPerInBuffer)){ Pa_Sleep(1);};
//	Pa_Sleep( (pa_data.samplesToGo - bytesPerInBuffer)/dpm.rate * 1000);
	return 0;

error:
	Pa_Terminate(); pa_active=0;
#ifdef USE_TEMP_ENCODE
	reset_temporary_encoding();
#endif
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "PortAudio error: %s\n", Pa_GetErrorText( err ) );
	return -1;
}

#define WAIT_TIME (100) // ms

static void close_output(void)
{	
	int cnt, i, wait_ms;

	if( pa_active==0) return;
	
///r
#ifdef PORTAUDIO_V19
//	if(Pa_IsStreamActive(stream))	Pa_Sleep(  bytesPerInBuffer/dpm.rate*1000  );

	memset(&(pa_data.buf), 0, sizeof(pa_data.buf));
	i = 0;
	if(dpm.rate && data_nbyte)
		wait_ms = 1000 * bytesPerInBuffer / dpm.rate / data_nbyte / 2;
	else
		wait_ms = 1000;
	cnt = wait_ms / WAIT_TIME + 1;
	while (!Pa_IsStreamActive(stream) && i < cnt){
		Pa_Sleep(WAIT_TIME);
		i++;
	}
#else
//	if(Pa_StreamActive(stream))	Pa_Sleep(  bytesPerInBuffer/dpm.rate*1000  );

	memset(&(pa_data.buf), 0, sizeof(pa_data.buf));
	i = 0;
	if(dpm.rate && data_nbyte)
		wait_ms = 1000 * bytesPerInBuffer / dpm.rate / data_nbyte / 2;
	else
		wait_ms = 1000;
	cnt = wait_ms / WAIT_TIME + 1;
	while (!Pa_StreamActive(stream) && i < cnt){
		Pa_Sleep(WAIT_TIME);
		i++;
	}
#endif
	err = Pa_AbortStream( stream );
    if( (err!=paStreamIsStopped) && (err!=paNoError) ) goto error;
	err = Pa_CloseStream( stream );
//	if( err != paNoError ) goto error;
	Pa_Terminate(); 
	pa_active=0;		
#ifdef USE_TEMP_ENCODE
	reset_temporary_encoding();
#endif
#ifdef AU_PORTAUDIO_DLL
#ifndef PORTAUDIO_V19
  free_portaudio_dll();
#endif
#endif

	return;

error:
	Pa_Terminate(); pa_active=0;
#ifdef USE_TEMP_ENCODE
	reset_temporary_encoding();
#endif
#ifdef AU_PORTAUDIO_DLL
#ifndef PORTAUDIO_V19
  free_portaudio_dll();
#endif
#endif
	ctl->cmsg(  CMSG_ERROR, VERB_NORMAL, "PortAudio error: %s\n", Pa_GetErrorText( err ) );
	return;
}


static int acntl(int request, void *arg)
{
    switch(request)
    {
///r
      case PM_REQ_GETQSIZ:
//		 *(int *)arg = bytesPerInBuffer*2;
		 *(int *)arg = bytesPerInBuffer;
    	return 0;
		//break;
/*
// audio buffer not fill ??
      case PM_REQ_GETFILLABLE:
//		 *(int *)arg = bytesPerInBuffer*2-pa_data.samplesToGo;
		 *(int *)arg = bytesPerInBuffer-pa_data.samplesToGo;
    	return 0;
		//break;
*/
      case PM_REQ_GETFILLED:
		 *(int *)arg = pa_data.samplesToGo;
    	return 0;
		//break;
 	
   	case PM_REQ_DISCARD:
    case PM_REQ_FLUSH:
    	pa_data.samplesToGo=0;
    	pa_data.bufpoint=pa_data.bufepoint;
///r
//		memset(&(pa_data.buf), 0, sizeof(pa_data.buf));

    	err = Pa_AbortStream( stream );
    	if( (err!=paStreamIsStopped) && (err!=paNoError) ) goto error;
    	err = Pa_StartStream( stream );
    	if(err!=paNoError) goto error;
		return 0;

		//break;

    case PM_REQ_RATE:  
    	{
    		int i;
    		double sampleRateBack;
    		i = *(int *)arg; //* sample rate in and out *
    		close_output();
    		sampleRateBack=dpm.rate;
    		dpm.rate=i;
    		if(0==open_output()){
    			return 0;
    		}else{    		
    			dpm.rate=sampleRateBack;
    			open_output();
    			return -1;
    		}
    	}
    	//break;

//    case PM_REQ_RATE: 
//          return -1;
    	
    case PM_REQ_PLAY_START: //* Called just before playing *
    case PM_REQ_PLAY_END: //* Called just after playing *
        return 0;
      
	default:
    	return -1;

    }
	return -1;
error:
/*
	Pa_Terminate(); pa_active=0;
#ifdef AU_PORTAUDIO_DLL
  free_portaudio_dll();
#endif
*/
	ctl->cmsg(  CMSG_ERROR, VERB_NORMAL, "PortAudio error in acntl : %s\n", Pa_GetErrorText( err ) );
	return -1;
}

#if defined(__W32__) && defined(PORTAUDIO_V19)
static char *convert_device_name_dup(const char *name, PaHostApiIndex hostApi)
{
	const PaHostApiIndex paIdxDirectSound = Pa_HostApiTypeIdToHostApiIndex(paDirectSound);
	const PaHostApiIndex paIdxMME = Pa_HostApiTypeIdToHostApiIndex(paMME);
	const PaHostApiIndex paIdxASIO = Pa_HostApiTypeIdToHostApiIndex(paASIO);
	const PaHostApiIndex paIdxWDMKS = Pa_HostApiTypeIdToHostApiIndex(paWDMKS);
	const PaHostApiIndex paIdxWASAPI = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);

	if (hostApi == paIdxWDMKS || hostApi == paIdxWASAPI /* || hostApi == paIdxASIO */
#if defined(_UNICODE) || defined(UNICODE)
	    || hostApi == paIdxDirectSound || hostApi == paIdxMME
#endif /* UNICODE */
	    )
	{
		return (char *)w32_utf8_to_mbs(name);
	}

	return safe_strdup(name);
}
#else
static char *convert_device_name_dup(const char *name, PaHostApiIndex hostApi)
{
	return safe_strdup(name);
}
#endif /* __W32__ && PORTAUDIO_V19 */

static void print_device_list(void)
{
	PaDeviceIndex maxDeviceIndex, i;
	PaHostApiIndex HostApiIndex;
	const PaDeviceInfo *DeviceInfo;

	HostApiIndex = Pa_HostApiTypeIdToHostApiIndex(HostApiTypeId);

	maxDeviceIndex = Pa_GetDeviceCount();

	for (i = 0; i < maxDeviceIndex; i++) {
		DeviceInfo = Pa_GetDeviceInfo(i);
#ifdef PORTAUDIO_V19
		if (DeviceInfo->hostApi == HostApiIndex)
#endif /* PORTAUDIO_V19 */
		{
			if (DeviceInfo->maxOutputChannels > 0) {
				char *name;
#ifdef PORTAUDIO_V19
				name = convert_device_name_dup(DeviceInfo->name, DeviceInfo->hostApi);
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%2d %2d %s", i, DeviceInfo->hostApi, name);
#else
				name = safe_strdup(DeviceInfo->name);
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%2d %s", i, name);
#endif /* PORTAUDIO_V19 */
				safe_free(name);
			}
		}
	}
}

int pa_device_list(PA_DEVICELIST *device, int HostApiTypeId)
{
	int i, num;
	PaDeviceIndex maxDeviceIndex;
	PaHostApiIndex HostApiIndex;
	const PaDeviceInfo *DeviceInfo;
	PaError err;
	int buffered_data = 0;
	const int err_unknown = -1, err_nodll = -2, err_opened = -3;

#ifdef AU_PORTAUDIO_DLL
	if (play_mode == portaudio_play_mode
#ifdef PORTAUDIO_V19
	    || play_mode == &portaudio_win_wasapi_play_mode
	    || play_mode == &portaudio_win_wdmks_play_mode
#endif /* PORTAUDIO_V19 */
	    || play_mode == &portaudio_win_ds_play_mode
	    || play_mode == &portaudio_win_wmme_play_mode
	    || play_mode == &portaudio_asio_play_mode)
	{
		play_mode->acntl(PM_REQ_GETFILLED, &buffered_data);
		if (buffered_data != 0) return err_opened;
		play_mode->close_output();
	}
#ifdef PORTAUDIO_V19
	if (load_portaudio_dll(0)) return err_nodll;
#else
  	if (HostApiTypeId == paASIO) {
		if (load_portaudio_dll(PA_DLL_ASIO))
			return err_nodll;
	} else if (HostApiTypeId == paDirectSound) {
		if (load_portaudio_dll(PA_DLL_WIN_DS))
			return err_nodll;
	} else if (HostApiTypeId == paMME) {
		if (load_portaudio_dll(PA_DLL_WIN_WMME))
			return err_nodll;
	} else {
		return -1;
	}
#endif /* PORTAUDIO_V19 */
#else
	if (play_mode == &portaudio_play_mode) {
		play_mode->acntl(PM_REQ_GETFILLED, &buffered_data);
		if (buffered_data != 0) return err_opened;
		play_mode->close_output();
	}
#endif /* AU_PORTAUDIO_DLL */
	err = Pa_Initialize();
	if (err != paNoError) goto error1;

	HostApiIndex = Pa_HostApiTypeIdToHostApiIndex((PaHostApiTypeId)HostApiTypeId);

	num = 0;
	maxDeviceIndex = Pa_GetDeviceCount();
	for (i = 0; i < maxDeviceIndex && i < PA_DEVLIST_MAX; i++) {
		DeviceInfo = Pa_GetDeviceInfo(i);
#ifdef PORTAUDIO_V19
		if (DeviceInfo->hostApi == HostApiIndex)
#endif /* PORTAUDIO_V19 */
		{
			if (DeviceInfo->maxOutputChannels > 0) {
				device[num].deviceID = i;
				if (!strlen(DeviceInfo->name)) {
					strcpy(device[num].name, PA_DEVLIST_DEFAULT_NAME);
				}
				else {
					char *name;
#ifdef PORTAUDIO_V19
					name = convert_device_name_dup(DeviceInfo->name, DeviceInfo->hostApi);
#else
					name = safe_strdup(DeviceInfo->name);
#endif /* PORTAUDIO_V19 */
					memset(device[num].name, 0, PA_DEVLIST_LEN * sizeof(device[num].name[0]));
					strlcpy(device[num].name, name, PA_DEVLIST_LEN);
//					ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%2d %2d %s", i, DeviceInfo->hostApi, device[num].name);
					safe_free(name);
				}
				num++;
			}
		}
	}
	return num;
error1:
//  	free_portaudio_dll();
#ifdef __W32__
	MessageBox(NULL, Pa_GetErrorText(err), "Port Audio error", MB_OK | MB_ICONEXCLAMATION);
#else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "PortAudio error in acntl : %s\n", Pa_GetErrorText(err));
#endif /* __W32__ */
	Pa_Terminate();
	return err_unknown;
}




#endif /* AU_PORTAUDIO */