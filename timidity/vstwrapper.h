#ifndef VSTWRAPPER_H
#define VSTWRAPPER_H

#ifdef _WIN32

#include <windows.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#define VST_CHANNEL_MAX 32

///r
#if VSTWRAP_EXT
// Master VST int64/double
typedef void (WINAPI *mix_vst_effect64)(long long *buff, long count, float freq);
typedef void (WINAPI *mix_vst_effectD)(double *buff, long count, float freq);
void __stdcall effectProcessingInt64(long long *buff, long count, float freq);
void __stdcall effectProcessingDouble(double *buff, long count, float freq);
void __stdcall effectProcessingInt64Mono(long long *buff, long count, float freq);
void __stdcall effectProcessingDoubleMono(double *buff, long count, float freq);
// Channel VST float/double
typedef void (WINAPI *channel_vst_sendF)(float **buff, long count, float freq, float *directbuff);
typedef void (WINAPI *channel_vst_sendD)(double **buff, long count, float freq, double *directbuff);
typedef float * (WINAPI *channel_vst_returnF)(void);
typedef double * (WINAPI *channel_vst_returnD)(void);
typedef void (WINAPI *mix_vst_channel_effectF)(const unsigned long c, float *buff, long count, float freq);
typedef void (WINAPI *mix_vst_channel_effectD)(const unsigned long c, double *buff, long count, float freq);
void __stdcall channelEffect_InOutF(float **buff, long count, float freq, float *directbuff);
void __stdcall channelEffect_InOutD(double **buff, long count, float freq, double *directbuff);
float * __stdcall channelEffect_GetInOutResultBufferF(void);
double * __stdcall channelEffect_GetInOutResultBufferD(void);
void __stdcall effectChannelProcessingFloat(const unsigned long c, float *buff, long count, float freq);
void __stdcall effectChannelProcessingDouble(const unsigned long c, double *buff, long count, float freq);
// Reverb VST int32/float/double
typedef void (WINAPI *reverb_vst_effect32)(long *buff, long count, float freq);
typedef void (WINAPI *reverb_vst_effectF)(float *buff, long count, float freq);
typedef void (WINAPI *reverb_vst_effectD)(double *buff, long count, float freq);
void __stdcall ReverbEffectProcessingInt32(long *buff, long count, float freq);
void __stdcall ReverbEffectProcessingFloat(float *buff, long count, float freq);
void __stdcall ReverbEffectProcessingDouble(double *buff, long count, float freq);
// Chorus VST int32/float/double
typedef void (WINAPI *chorus_vst_effect32)(long *buff, long count, float freq);
typedef void (WINAPI *chorus_vst_effectF)(float *buff, long count, float freq);
typedef void (WINAPI *chorus_vst_effectD)(double *buff, long count, float freq);
void __stdcall ChorusEffectProcessingInt32(long *buff, long count, float freq);
void __stdcall ChorusEffectProcessingFloat(float *buff, long count, float freq);
void __stdcall ChorusEffectProcessingDouble(double *buff, long count, float freq);
#endif

// Master VST int8/int16/int24/int32/float
typedef void (WINAPI *mix_vst_effect8)(char *buff, long count, float freq);
typedef void (WINAPI *mix_vst_effect16)(short *buff, long count, float freq);
typedef void (WINAPI *mix_vst_effect24)(char *buff, long count, float freq);
typedef void (WINAPI *mix_vst_effect32)(long *buff, long count, float freq);
typedef void (WINAPI *mix_vst_effectF)(float *buff, long count, float freq);
void __stdcall effectProcessingInt8(char *buff, long count, float freq);
void __stdcall effectProcessingInt16(short *buff, long count, float freq);
void __stdcall effectProcessingInt24(char *buff, long count, float freq);
void __stdcall effectProcessingInt32(long *buff, long count, float freq);
void __stdcall effectProcessingFloat(float *buff, long count, float freq);
void __stdcall effectProcessingInt8Mono(char *buff, long count, float freq);
void __stdcall effectProcessingInt16Mono(short *buff, long count, float freq);
void __stdcall effectProcessingInt24Mono(char *buff, long count, float freq);
void __stdcall effectProcessingInt32Mono(long *buff, long count, float freq);
void __stdcall effectProcessingFloatMono(float *buff, long count, float freq);
// Channel VST int32
typedef void (WINAPI *channel_vst_send32)(long **buff, long count, float freq, long *directbuff);
typedef long * (WINAPI *channel_vst_return32)(void);
typedef void (WINAPI *channel_vst_effect32)(const unsigned long c, long *buff, long count, float freq);	
void __stdcall channelEffect_InOut(long **buff, long count, float freq, long *directbuff);
long * __stdcall channelEffect_GetInOutResultBuffer(void);
void __stdcall effectChannelProcessingInt32(const unsigned long c, long *buff, long count, float freq);

void __stdcall openEffectEditorAll(HWND hwnd);
typedef void (WINAPI *vst_open_config_all)(HWND);
typedef int (WINAPI *vst_open)(void);
typedef void (WINAPI *vst_close)(void);
typedef void (WINAPI *vst_processing_init)(void);
typedef void (WINAPI *open_vst_mgr)(HWND hwnd);


#endif /* _WIN32 */

#endif /* !VSTWRAPPER_H */

