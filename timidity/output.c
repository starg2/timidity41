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

    output.c

    Audio output (to file / device) functions.
*/


#define AU_WRITE_MIDI
#define AU_LIST
#define AU_MODMIDI
//#define AU_VOLUME_CALC

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef STDC_HEADERS
#include <string.h>
#include <ctype.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "tables.h"
#include "controls.h"
#include "audio_cnv.h"

#if (defined(_MSC_VER) && !defined(__POCC__)) || \
	defined(__BORLANDC__)
#define CALLINGCONV __fastcall
#elif defined(__GNUC__)
#define CALLINGCONV __attribute__((fastcall))
#else
#define CALLINGCONV /**/
#endif

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
typedef struct _Channel *Channel;
#include "w32g.h"
#endif /* IA_W32GUI || IA_W32G_SYN */

///r
static FLOAT_T output_volume;
static int32 output_volumei; /* 4.28 */
int opt_output_device_id = -3;
int audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;

/* These are very likely mutually exclusive.. */
#if defined(AU_AUDRIV)
extern PlayMode audriv_play_mode;
#define DEV_PLAY_MODE &audriv_play_mode

#elif defined(AU_SUN)
extern PlayMode sun_play_mode;
#define DEV_PLAY_MODE &sun_play_mode

#elif defined(AU_OSS)
extern PlayMode oss_play_mode;
#define DEV_PLAY_MODE &oss_play_mode

#elif defined(AU_HPUX_AUDIO)
extern PlayMode hpux_play_mode;
#define DEV_PLAY_MODE &hpux_play_mode

#elif defined(AU_W32)
extern PlayMode w32_play_mode;
#define DEV_PLAY_MODE &w32_play_mode

#elif defined(AU_BSDI)
extern PlayMode bsdi_play_mode;
#define DEV_PLAY_MODE &bsdi_play_mode

#elif defined(__MACOS__)
extern PlayMode mac_play_mode;
#define DEV_PLAY_MODE &mac_play_mode

#elif defined(AU_DARWIN)
extern PlayMode darwin_play_mode;
#define DEV_PLAY_MODE &darwin_play_mode
#endif

#ifdef AU_ALSA
extern PlayMode alsa_play_mode;
#endif /* AU_ALSA */

#ifdef AU_HPUX_ALIB
extern PlayMode hpux_nplay_mode;
#endif /* AU_HPUX_ALIB */

#ifdef AU_ARTS
extern PlayMode arts_play_mode;
#endif /* AU_ARTS */

#ifdef AU_ESD
extern PlayMode esd_play_mode;
#endif /* AU_ESD */

#ifdef AU_WASAPI
extern PlayMode wasapi_play_mode;
#endif /* AU_WASAPI */

#ifdef AU_PORTAUDIO
#ifndef AU_PORTAUDIO_DLL
extern PlayMode portaudio_play_mode;
#else
extern PlayMode portaudio_asio_play_mode;
#ifdef PORTAUDIO_V19
extern PlayMode portaudio_win_wasapi_play_mode;
extern PlayMode portaudio_win_wdmks_play_mode;
#endif
extern PlayMode portaudio_win_ds_play_mode;
extern PlayMode portaudio_win_wmme_play_mode;
extern PlayMode portaudio_win_wasapi_play_mode;
#endif
#endif /* AU_PORTAUDIO */

#ifdef AU_NPIPE
extern PlayMode npipe_play_mode;
#endif /* AU_NPIPE */

#ifdef AU_JACK
extern PlayMode jack_play_mode;
#endif /* AU_NAS */

#ifdef AU_NAS
extern PlayMode nas_play_mode;
#endif /* AU_NAS */

#ifdef AU_AO
extern PlayMode ao_play_mode;
#endif /* AU_AO */

#ifndef __MACOS__
/* These are always compiled in. */
#if !defined(KBTIM) && !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP) && !defined(WINVSTI)
extern PlayMode raw_play_mode, wave_play_mode, au_play_mode, aiff_play_mode;
#endif
#if defined(AU_LIST)
extern PlayMode list_play_mode;
#endif /* AU_LIST */
#ifdef AU_VORBIS
extern PlayMode vorbis_play_mode;
#endif /* AU_VORBIS */
#ifdef AU_FLAC
extern PlayMode flac_play_mode;
#endif /* AU_FLAC */
#ifdef AU_OPUS
extern PlayMode opus_play_mode;
#endif /* AU_OPUS */
#ifdef AU_SPEEX
extern PlayMode speex_play_mode;
#endif /* AU_SPEEX */
#ifdef AU_GOGO
extern PlayMode gogo_play_mode;
#endif /* AU_GOGO */
#endif /* !__MACOS__ */

#if defined(AU_WRITE_MIDI)
extern PlayMode midi_play_mode;
#endif /* AU_WRITE_MIDI */
#if defined(AU_MODMIDI)
 extern PlayMode modmidi_play_mode;
#endif /* AU_MODMIDI */

#ifdef AU_LAME
extern PlayMode lame_play_mode;
#endif
#if defined(AU_VOLUME_CALC)
extern PlayMode soundfont_vol_calc;
#endif /* AU_VOLUME_CALC */
#if defined(AU_BENCHMARK)
extern PlayMode benchmark_mode;
#endif /* AU_BENCHMARK */

PlayMode *play_mode_list[] = {
#if defined(AU_AO) /* Try libao first as that will give us pulseaudio */
  &ao_play_mode,
#endif /* AU_AO */

#if defined(AU_ARTS)
  &arts_play_mode,
#endif /* AU_ARTS */

#if defined(AU_ESD)
  &esd_play_mode,
#endif /* AU_ESD */

#ifdef AU_ALSA /* Try alsa (aka DEV_PLAY_MODE 2 on Linux) first */
  &alsa_play_mode,
#endif /* AU_ALSA */

#ifdef DEV_PLAY_MODE /* OS dependent direct hardware access, OSS on Linux */
  DEV_PLAY_MODE,
#endif

#ifdef AU_HPUX_ALIB
  &hpux_nplay_mode,
#endif /* AU_HPUX_ALIB */
  
#if defined(AU_WASAPI)
  &wasapi_play_mode,
#endif /* AU_WASAPI */

#if defined(AU_PORTAUDIO)
#ifndef AU_PORTAUDIO_DLL
  &portaudio_play_mode,
#else
  &portaudio_asio_play_mode,
  &portaudio_win_wasapi_play_mode,
  &portaudio_win_wdmks_play_mode,
  &portaudio_win_ds_play_mode,
  &portaudio_win_wmme_play_mode,
#endif
#endif /* AU_PORTAUDIO */
  
#ifdef AU_LAME
  &lame_play_mode,
#endif

#if defined(AU_NPIPE)
  &npipe_play_mode,
#endif /*AU_NPIPE*/

#if defined(AU_JACK)
  &jack_play_mode,
#endif /* AU_PORTAUDIO */

#if defined(AU_NAS)
  &nas_play_mode,
#endif /* AU_NAS */

#ifndef __MACOS__
#if !defined(KBTIM) && !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP) && !defined(WINVSTI)
  &wave_play_mode,
  &raw_play_mode,
  &au_play_mode,
  &aiff_play_mode,
#endif
#ifdef AU_VORBIS
  &vorbis_play_mode,
#endif /* AU_VORBIS */
#ifdef AU_FLAC
  &flac_play_mode,
#endif /* AU_FLAC */
#ifdef AU_OPUS
  &opus_play_mode,
#endif /* AU_OPUS */
#ifdef AU_SPEEX
  &speex_play_mode,
#endif /* AU_SPEEX */
#ifdef AU_GOGO
  &gogo_play_mode,
#endif /* AU_GOGO */
#if defined(AU_LIST)
  &list_play_mode,
#endif /* AU_LIST */
#endif /* __MACOS__ */
#if defined(AU_WRITE_MIDI)
  &midi_play_mode,
#endif /* AU_WRITE_MIDI */
#if defined(AU_MODMIDI)
  &modmidi_play_mode,
#endif /* AU_MODMIDI */
#if defined(AU_VOLUME_CALC)
  &soundfont_vol_calc,
#endif /* AU_VOLUME_CALC */
#if defined(AU_BENCHMARK)
  &benchmark_mode,
#endif /* AU_BENCHMARK */
  NULL
};


PlayMode *play_mode = NULL;

PlayMode *target_play_mode = NULL;


double div_playmode_rate;
double playmode_rate_div2;
double playmode_rate_div3;
double playmode_rate_div4;
double playmode_rate_div6;
double playmode_rate_div8;
double playmode_rate_ms;
double playmode_rate_dms;
double playmode_rate_us;


void init_output(void){
	div_playmode_rate = (double) 1.0 / play_mode->rate;
	playmode_rate_div2 = (double) play_mode->rate * DIV_2;
	playmode_rate_div3 = (double) play_mode->rate * DIV_3;
	playmode_rate_div4 = (double) play_mode->rate * DIV_4;
	playmode_rate_div6 = (double) play_mode->rate * DIV_6;
	playmode_rate_div8 = (double) play_mode->rate * DIV_8;
	playmode_rate_ms = (double) play_mode->rate * DIV_1000;
	playmode_rate_dms = (double) play_mode->rate * DIV_10000;
	playmode_rate_us = (double) play_mode->rate * DIV_1000000;
}

void change_output_volume(int32 vol)
{
    /* output volume (master volume) */
    output_volume = (FLOAT_T)(vol) * DIV_100;
    if (output_volume > 7.999f) output_volume = 7.999f - 0.0001f;
    output_volumei = TIM_FSCALE(output_volume, 28);
}

static int use_temp_encoding = 0;
static uint32 temp_encoding = 0;

// called open_output()
void set_temporary_encoding(uint32 enc)
{	
	temp_encoding = enc;
	use_temp_encoding = 1;
}

// called close_output()
void reset_temporary_encoding(void)
{
	use_temp_encoding = 0;
	temp_encoding = 0;
}



/*****************************************************************/
/* Some functions to convert signed 32-bit data to other formats */

///r
#define MAX_8BIT_SIGNED (127)
#define MIN_8BIT_SIGNED (-128)
#define MAX_16BIT_SIGNED (32767)
#define MIN_16BIT_SIGNED (-32768)
#define MAX_24BIT_SIGNED (8388607)
#define MIN_24BIT_SIGNED (-8388608)
#define MAX_32BIT_SIGNED (2147483647)
#define MIN_32BIT_SIGNED (-2147483648)
#define MAX_64BIT_SIGNED (9223372036854775807)
#define MIN_64BIT_SIGNED (-9223372036854775808)
#define MAX_GUARD_SIGNED ((1 << (MAX_BITS - GUARD_BITS - 1)) - 1)
#define MIN_GUARD_SIGNED (~ MAX_GUARD_SIGNED)

#define STORE_S24_LE(cp, l) *cp++ = l & 0xFF, *cp++ = l >> 8 & 0xFF, *cp++ = l >> 16
#define STORE_S24_BE(cp, l) *cp++ = l >> 16, *cp++ = l >> 8 & 0xFF, *cp++ = l & 0xFF
#define STORE_U24_LE(cp, l) *cp++ = l & 0xFF, *cp++ = l >> 8 & 0xFF, *cp++ = l >> 16 ^ 0x80
#define STORE_U24_BE(cp, l) *cp++ = l >> 16 ^ 0x80, *cp++ = l >> 8 & 0xFF, *cp++ = l & 0xFF

#ifdef LITTLE_ENDIAN
  #define STORE_S24  STORE_S24_LE
  #define STORE_S24X STORE_S24_BE
  #define STORE_U24  STORE_U24_LE
  #define STORE_U24X STORE_U24_BE
#else
  #define STORE_S24  STORE_S24_BE
  #define STORE_S24X STORE_S24_LE
  #define STORE_U24  STORE_U24_BE
  #define STORE_U24X STORE_U24_LE
#endif

const int32 max_guard_signed = MAX_GUARD_SIGNED;
const int32 min_guard_signed = MIN_GUARD_SIGNED;
const double div_max_guard_signed = 1 / (double)(MAX_GUARD_SIGNED + 1);

static int32 convert_count, convert_bytes;
static void (CALLINGCONV *convert_fnc)(DATA_T*, int32);

#if defined(DATA_T_DOUBLE) || defined(DATA_T_FLOAT) // DATA_T buf

#ifdef EFFECT_LEVEL_FLOAT // level float 
#define	INPUT_GAIN (output_volume)
#else // level int16
#define	INPUT_GAIN (div_max_guard_signed * output_volume)
#endif


#if (USE_X86_EXT_INTRIN >= 8)
__m256d d256max = {1.0, 1.0, 1.0, 1.0,};
__m256d d256min = {-1.0, -1.0, -1.0, -1.0,};
#define D256_CLIP_INPUT(ptr, d256gain) _mm256_max_pd(_mm256_min_pd(_mm256_mul_pd(_mm256_load_pd(ptr), d256gain), d256max), d256min)
#define D256_CLIP_MM(d256in, d256gain) _mm256_max_pd(_mm256_min_pd(_mm256_mul_pd(d256in, d256gain), d256max), d256min)
#endif

#if (USE_X86_EXT_INTRIN >= 3)
__m128d d128max = {1.0, 1.0,};
__m128d d128min = {-1.0, -1.0,};
#define D128_CLIP_INPUT(ptr, d128gain) _mm_max_pd(_mm_min_pd(_mm_mul_pd(_mm_load_pd(ptr), d128gain), d128max), d128min)
#define D128_CLIP_MM(d128in, d128gain) _mm_max_pd(_mm_min_pd(_mm_mul_pd(d128in, d128gain), d128max), d128min)
#endif

#if (USE_X86_EXT_INTRIN >= 8)
__m256 f256max = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,};
__m256 f256min = {-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0,};
#define F256_CLIP_INPUT(ptr, f256gain) _mm256_max_ps(_mm256_min_ps(_mm256_mul_ps(_mm256_load_ps(ptr), f256gain), f256max), f256min)
#define F256_CLIP_MM(f256in, f256gain) _mm256_max_ps(_mm256_min_ps(_mm256_mul_ps(f256in, f256gain), f256max), f256min)
#endif

#if (USE_X86_EXT_INTRIN >= 2)
__m128 f128max = {1.0, 1.0, 1.0, 1.0,};
__m128 f128min = {-1.0, -1.0, -1.0, -1.0,};
#define F128_CLIP_INPUT(ptr, f128gain) _mm_max_ps(_mm_min_ps(_mm_mul_ps(_mm_load_ps(ptr), f128gain), f128max), f128min)
#define F128_CLIP_MM(f128in, f128gain) _mm_max_ps(_mm_min_ps(_mm_mul_ps(f128in, f128gain), f128max), f128min)
#endif

static inline DATA_T clip_input(DATA_T *input){
	*input *= INPUT_GAIN;
	if(*input > 1.0)
		return 1.0;
	else if(*input < -1.0)
		return -1.0;
	else
		return *input;
}

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos8(DATA_T *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_8BIT_SIGNED);
	for(i = 0; i < c; i += 8){ // d5 d5
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		__m128i vec_i8 = _mm_packs_epi16(vec_i16, _mm_setzero_si128());
		_mm_storeu_si128((__m128i *)&cp[i], vec_i8); // L64bit=8bit*8 , H64bit next unaligned	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos8(DATA_T *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_8BIT_SIGNED);
	for(i = 0; i < c; i += 8){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f21 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 4]));
		__m128 vec_f22 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 6]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128 vec_f2 = _mm_shuffle_ps(vec_f21, vec_f22, 0x44);
		__m128i vec_i32_1 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul));
		__m128i vec_i32_2 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f2, gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		__m128i vec_i8 = _mm_packs_epi16(vec_i16, _mm_setzero_si128());
		_mm_storeu_si128((__m128i *)&cp[i], vec_i8); // L64bit=8bit*8 , H64bit next unaligned
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos8(DATA_T *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_8BIT_SIGNED);	
	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1);
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2);
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		__m128i vec_i8 = _mm_packs_epi16(vec_i16, _mm_setzero_si128());
		_mm_storeu_si128((__m128i *)&cp[i], vec_i8); // L64bit=8bit*8 , H64bit next unaligned
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos8(DATA_T *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_8BIT_SIGNED);	
	for(i = 0; i < c; i += 4){
		__m128 vec_f = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		float *out = (float *)vec_f;
		cp[i] = (int8)(out[0]);
		cp[i] = (int8)(out[1]);
		cp[i] = (int8)(out[2]);
		cp[i] = (int8)(out[3]);	
		}
#else
		cp[i] = (int8)(vec_f.m128_f32[0]);
		cp[i] = (int8)(vec_f.m128_f32[1]);
		cp[i] = (int8)(vec_f.m128_f32[2]);
		cp[i] = (int8)(vec_f.m128_f32[3]);	
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64tos8(DATA_T *lp, int32 c)
{
    int8 *cp=(int8 *)(lp);
	int32 i;

    for(i = 0; i < c; i++)
		cp[i] = (int8)(clip_input(&lp[i]) * MAX_8BIT_SIGNED);
}
#endif


#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tou8(DATA_T *lp, int32 c)
{
	uint8 *cp=(uint8 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_8BIT_SIGNED);
	__m128i vex = _mm_set1_epi8(0x80);
	for(i = 0; i < c; i += 8){ // d5 d5
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		__m128i vec_i8 = _mm_packs_epi16(vec_i16, _mm_setzero_si128());
		vec_i8 = _mm_xor_si128(vex, vec_i8);
		_mm_storeu_si128((__m128i *)&cp[i], vec_i8); // L64bit=8bit*8 , H64bit next unaligned	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tou8(DATA_T *lp, int32 c)
{
	uint8 *cp=(uint8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_8BIT_SIGNED);
	__m128i vex = _mm_set1_epi8(0x80);
	for(i = 0; i < c; i += 8){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f21 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 4]));
		__m128 vec_f22 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 6]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128 vec_f2 = _mm_shuffle_ps(vec_f21, vec_f22, 0x44);
		__m128i vec_i32_1 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul));
		__m128i vec_i32_2 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f2, gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		__m128i vec_i8 = _mm_packs_epi16(vec_i16, _mm_setzero_si128());
		vec_i8 = _mm_xor_si128(vex, vec_i8);
		_mm_storeu_si128((__m128i *)&cp[i], vec_i8); // L64bit=8bit*8 , H64bit next unaligned
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tou8(DATA_T *lp, int32 c)
{
	uint8 *cp=(uint8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_8BIT_SIGNED);
	__m128i vex = _mm_set1_epi8(0x80);	
	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1);
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2);
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		__m128i vec_i8 = _mm_packs_epi16(vec_i16, _mm_setzero_si128());
		vec_i8 = _mm_xor_si128(vex, vec_i8);
		_mm_storeu_si128((__m128i *)&cp[i], vec_i8); // L64bit=8bit*8 , H64bit next unaligned
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tou8(DATA_T *lp, int32 c)
{
	uint8 *cp=(uint8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_8BIT_SIGNED);
	__m128i vex = _mm_set1_epi8(0x80);	
	for(i = 0; i < c; i += 4){
		__m128 vec_f = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		float *out = (float *)vec_f;
		cp[i] = 0x80 ^ (uint8)(out[0]);
		cp[i] = 0x80 ^ (uint8)(out[1]);
		cp[i] = 0x80 ^ (uint8)(out[2]);
		cp[i] = 0x80 ^ (uint8)(out[3]);	
		}
#else
		cp[i] = 0x80 ^ (uint8)(vec_f.m128_f32[0]);
		cp[i] = 0x80 ^ (uint8)(vec_f.m128_f32[1]);
		cp[i] = 0x80 ^ (uint8)(vec_f.m128_f32[2]);
		cp[i] = 0x80 ^ (uint8)(vec_f.m128_f32[3]);
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64tou8(DATA_T *lp, int32 c)
{
	uint8 *cp=(uint8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		cp[i] = 0x80 ^ (uint8)(clip_input(&lp[i]) * MAX_8BIT_SIGNED);
}
#endif

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64toulaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_16BIT_SIGNED);	
	for(i = 0; i < c; i += 4){
		__m128i vec0 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		int32 *out = (int32 *)vec0;
		up[i] = AUDIO_S2U(out[0]);
		up[i + 1] = AUDIO_S2U(out[1]);
		up[i + 2] = AUDIO_S2U(out[2]);
		up[i + 3] = AUDIO_S2U(out[3]);
		}
#else
		up[i] = AUDIO_S2U(vec0.m128i_i32[0]);
		up[i + 1] = AUDIO_S2U(vec0.m128i_i32[1]);
		up[i + 2] = AUDIO_S2U(vec0.m128i_i32[2]);
		up[i + 3] = AUDIO_S2U(vec0.m128i_i32[3]);
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64toulaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);
	for(i = 0; i < c; i += 4){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128i vec_i32 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul));
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		int32 *out = (int32 *)vec_i32;
		up[i] = AUDIO_S2U(out[0]);
		up[i + 1] = AUDIO_S2U(out[1]);
		up[i + 2] = AUDIO_S2U(out[2]);
		up[i + 3] = AUDIO_S2U(out[3]);
		}
#else
		up[i] = AUDIO_S2U(vec_i32.m128i_i32[0]);
		up[i + 1] = AUDIO_S2U(vec_i32.m128i_i32[1]);
		up[i + 2] = AUDIO_S2U(vec_i32.m128i_i32[2]);
		up[i + 3] = AUDIO_S2U(vec_i32.m128i_i32[3]);
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}	
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64toulaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);
	for(i = 0; i < c; i += 4){
		__m128i vec0 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul));
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		int32 *out = (int32 *)vec0;
		up[i] = AUDIO_S2U(out[0]);
		up[i + 1] = AUDIO_S2U(out[1]);
		up[i + 2] = AUDIO_S2U(out[2]);
		up[i + 3] = AUDIO_S2U(out[3]);
		}
#else
		up[i] = AUDIO_S2U(vec0.m128i_i32[0]);
		up[i + 1] = AUDIO_S2U(vec0.m128i_i32[1]);
		up[i + 2] = AUDIO_S2U(vec0.m128i_i32[2]);
		up[i + 3] = AUDIO_S2U(vec0.m128i_i32[3]);
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64toulaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		up[i] = AUDIO_S2U((int32)(clip_input(&lp[i]) * MAX_16BIT_SIGNED));
}
#endif

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64toalaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_16BIT_SIGNED);		
	for(i = 0; i < c; i += 4){
		__m128i vec0 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		int32 *out = (int32 *)vec0;
		up[i] = AUDIO_S2A(out[0]);
		up[i + 1] = AUDIO_S2A(out[1]);
		up[i + 2] = AUDIO_S2A(out[2]);
		up[i + 3] = AUDIO_S2A(out[3]);
		}
#else
		up[i] = AUDIO_S2A(vec0.m128i_i32[0]);
		up[i + 1] = AUDIO_S2A(vec0.m128i_i32[1]);
		up[i + 2] = AUDIO_S2A(vec0.m128i_i32[2]);
		up[i + 3] = AUDIO_S2A(vec0.m128i_i32[3]);
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64toalaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);
	for(i = 0; i < c; i += 4){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128i vec_i32 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul));
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		int32 *out = (int32 *)vec_i32;
		up[i] = AUDIO_S2A(out[0]);
		up[i + 1] = AUDIO_S2A(out[1]);
		up[i + 2] = AUDIO_S2A(out[2]);
		up[i + 3] = AUDIO_S2A(out[3]);
		}
#else
		up[i] = AUDIO_S2A(vec_i32.m128i_i32[0]);
		up[i + 1] = AUDIO_S2A(vec_i32.m128i_i32[1]);
		up[i + 2] = AUDIO_S2A(vec_i32.m128i_i32[2]);
		up[i + 3] = AUDIO_S2A(vec_i32.m128i_i32[3]);
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64toalaw(DATA_T *lp, int32 c)
{
	int8 *up=(int8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		up[i] = AUDIO_S2A((int32)(clip_input(&lp[i]) * MAX_16BIT_SIGNED));
}
#endif

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos16(DATA_T *lp, int32 c)
{
	int16 *sp=(int16 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_16BIT_SIGNED);
	for(i = 0; i < c; i += 8){ // d5 d5
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		_mm_store_si128((__m128i *)&sp[i], vec_i16); // 128bit=16bit*8	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos16(DATA_T *lp, int32 c)
{
	int16 *sp=(int16 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);
	for(i = 0; i < c; i += 8){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f21 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 4]));
		__m128 vec_f22 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 6]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128 vec_f2 = _mm_shuffle_ps(vec_f21, vec_f22, 0x44);
		__m128i vec_i32_1 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul));
		__m128i vec_i32_2 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f2, gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		_mm_store_si128((__m128i *)&sp[i], vec_i16); // 128bit=16bit*8	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos16(DATA_T *lp, int32 c)
{
	int16 *sp=(int16 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);	
	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1);
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2);
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		_mm_store_si128((__m128i *)&sp[i], vec_i16); // 128bit=16bit*8	
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos16(DATA_T *lp, int32 c)
{
	int16 *sp=(int16 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);	
	for(i = 0; i < c; i += 4){
		__m128 vec_f = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		float *out = (float *)vec_f;
		sp[i] = (int16)(out[0]);
		sp[i] = (int16)(out[1]);
		sp[i] = (int16)(out[2]);
		sp[i] = (int16)(out[3]);	
		}
#else
		sp[i] = (int16)(vec_f.m128_f32[0]);
		sp[i] = (int16)(vec_f.m128_f32[1]);
		sp[i] = (int16)(vec_f.m128_f32[2]);
		sp[i] = (int16)(vec_f.m128_f32[3]);		
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64tos16(DATA_T *lp, int32 c)
{
	int16 *sp=(int16 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = (int16)(clip_input(&lp[i]) * MAX_16BIT_SIGNED);
}
#endif


#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tou16(DATA_T *lp, int32 c)
{
	uint16 *sp=(uint16 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_16BIT_SIGNED);
	__m128i vex = _mm_set1_epi16(0x8000);	
	for(i = 0; i < c; i += 8){ // d5 d5
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		vec_i16 = _mm_xor_si128(vex, vec_i16);
		_mm_store_si128((__m128i *)&sp[i], vec_i16); // 128bit=16bit*8	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tou16(DATA_T *lp, int32 c)
{
	uint16 *sp=(uint16 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);
	__m128i vex = _mm_set1_epi16(0x8000);	
	for(i = 0; i < c; i += 8){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f21 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 4]));
		__m128 vec_f22 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 6]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128 vec_f2 = _mm_shuffle_ps(vec_f21, vec_f22, 0x44);
		__m128i vec_i32_1 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul));
		__m128i vec_i32_2 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f2, gain), vmul));
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		vec_i16 = _mm_xor_si128(vex, vec_i16);
		_mm_store_si128((__m128i *)&sp[i], vec_i16); // 128bit=16bit*8	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tou16(DATA_T *lp, int32 c)
{
	uint16 *sp=(uint16 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_16BIT_SIGNED);	
	__m128i vex = _mm_set1_epi16(0x8000);	
	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1);
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2);
		__m128i vec_i16 = _mm_packs_epi32(vec_i32_1, vec_i32_2);
		vec_i16 = _mm_xor_si128(vex, vec_i16);
		_mm_store_si128((__m128i *)&sp[i], vec_i16); // 128bit=16bit*8	
	}
}
#else
static void CALLINGCONV f64tou16(DATA_T *lp, int32 c)
{
	uint16 *sp=(uint16 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = 0x8000 ^ (uint16)(clip_input(&lp[i]) * MAX_16BIT_SIGNED);
}
#endif

static void CALLINGCONV f64tos16x(DATA_T *lp, int32 c)
{
	int16 *sp=(int16 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = XCHG_SHORT((int16)(clip_input(&lp[i]) * MAX_16BIT_SIGNED));
}

static void CALLINGCONV f64tou16x(DATA_T *lp, int32 c)
{
	uint16 *sp=(uint16 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = XCHG_SHORT(0x8000 ^ (uint16)(clip_input(&lp[i]) * MAX_16BIT_SIGNED));
}

#if defined(LITTLE_ENDIAN) && (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos24(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;
#if (USE_X86_EXT_INTRIN >= 8)
	const __m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	const __m256d vmul = _mm256_set1_pd((double)MAX_24BIT_SIGNED);
#else
	const __m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	const __m128 vmul = _mm_set1_ps((float)MAX_24BIT_SIGNED);
#endif
	const __m128i vm_24l = _mm_set_epi32(0x00000000,0x00FFFFFF,0x00000000,0x00FFFFFF);
	const __m128i vm_24h = _mm_set_epi32(0x00FFFFFF,0x00000000,0x00FFFFFF,0x00000000);
	const __m128i vm_48l = _mm_set_epi32(0x00000000,0x00000000,0x0000FFFF,0xFFFFFFFF);
	const __m128i vm_48h = _mm_set_epi32(0x0000FFFF,0xFFFFFFFF,0x00000000,0x00000000);

	for(i = 0; i < c; i += 8){
#if (USE_X86_EXT_INTRIN >= 8)
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
#else
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f21 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 4]));
		__m128 vec_f22 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 6]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128 vec_f2 = _mm_shuffle_ps(vec_f21, vec_f22, 0x44);
		__m128i vec_i32_1 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f1, gain), vmul)); // (24bit+8bit)*4
		__m128i vec_i32_2 = _mm_cvttps_epi32(_mm_mul_ps(F128_CLIP_MM(vec_f2, gain), vmul)); // (24bit+8bit)*4
#endif
		__m128i vec_i24_1l = _mm_and_si128(vec_i32_1, vm_24l); 
		__m128i vec_i24_2l = _mm_and_si128(vec_i32_2, vm_24l); 
		__m128i vec_i24_1h = _mm_and_si128(vec_i32_1, vm_24h); 
		__m128i vec_i24_2h = _mm_and_si128(vec_i32_2, vm_24h); 
		__m128i vec_i48_1 =  _mm_or_si128(vec_i24_1l, _mm_srli_si128(vec_i24_1h, 1)); // (48bit+16bit) * 2
		__m128i vec_i48_2 =  _mm_or_si128(vec_i24_2l, _mm_srli_si128(vec_i24_2h, 1)); // (48bit+16bit) * 2
		__m128i vec_i48_1l = _mm_and_si128(vec_i48_1, vm_48l); 
		__m128i vec_i48_2l = _mm_and_si128(vec_i48_2, vm_48l); 
		__m128i vec_i48_1h = _mm_and_si128(vec_i48_1, vm_48h); 
		__m128i vec_i48_2h = _mm_and_si128(vec_i48_2, vm_48h); 
		__m128i vec_i96_1 = _mm_or_si128(vec_i48_1l, _mm_srli_si128(vec_i48_1h, 2)); // 96bit+32bit
		__m128i vec_i96_2 = _mm_or_si128(vec_i48_2l, _mm_srli_si128(vec_i48_2h, 2)); // 96bit+32bit
		__m128i vec_i128_1 = _mm_or_si128(vec_i96_1, _mm_slli_si128(vec_i96_2, 12)); // 24bit*4+(24bit+8bit) = 128bit
		__m128i vec_i128_2 = _mm_srli_si128(vec_i96_2, 4); // 16bit+24bit*2 = 64bit
#if (USE_X86_EXT_INTRIN >= 9)
		__m256i vec_i192 = MM256_SET2X_SI256(vec_i128_1, vec_i128_2); // 24bit*8 + emp64bit
		_mm256_storeu_si256((__m256i *)cp, vec_i192); // 192bit/256bit unalign ’´‚¦‚é•ª‚Í–³Ž‹	
		cp += 24; // 192bit = 24byte
#else
		_mm_storeu_si128((__m128i *)cp, vec_i128_1); // 128bit unalign
		cp += 16; // 128bit = 16byte
		_mm_storeu_si128((__m128i *)cp, vec_i128_2); // 64bit/128bit unalign ’´‚¦‚é•ª‚Í–³Ž‹	
		cp += 8; // 64bit = 8byte
#endif
	}
}
#elif defined(LITTLE_ENDIAN) && (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos24(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;
	const __m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	const __m128 vmul = _mm_set1_ps((float)MAX_24BIT_SIGNED);
	const __m128i vm_24l = _mm_set_epi32(0x00000000,0x00FFFFFF,0x00000000,0x00FFFFFF);
	const __m128i vm_24h = _mm_set_epi32(0x00FFFFFF,0x00000000,0x00FFFFFF,0x00000000);
	const __m128i vm_48l = _mm_set_epi32(0x00000000,0x00000000,0x0000FFFF,0xFFFFFFFF);
	const __m128i vm_48h = _mm_set_epi32(0x0000FFFF,0xFFFFFFFF,0x00000000,0x00000000);

	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1); // (24bit+8bit)*4
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2); // (24bit+8bit)*4
		__m128i vec_i24_1l = _mm_and_si128(vec_i32_1, vm_24l); 
		__m128i vec_i24_2l = _mm_and_si128(vec_i32_2, vm_24l); 
		__m128i vec_i24_1h = _mm_and_si128(vec_i32_1, vm_24h); 
		__m128i vec_i24_2h = _mm_and_si128(vec_i32_2, vm_24h); 
		__m128i vec_i48_1 =  _mm_or_si128(vec_i24_1l, _mm_srli_si128(vec_i24_1h, 1)); // (48bit+16bit) * 2
		__m128i vec_i48_2 =  _mm_or_si128(vec_i24_2l, _mm_srli_si128(vec_i24_2h, 1)); // (48bit+16bit) * 2
		__m128i vec_i48_1l = _mm_and_si128(vec_i48_1, vm_48l); 
		__m128i vec_i48_2l = _mm_and_si128(vec_i48_2, vm_48l); 
		__m128i vec_i48_1h = _mm_and_si128(vec_i48_1, vm_48h); 
		__m128i vec_i48_2h = _mm_and_si128(vec_i48_2, vm_48h); 
		__m128i vec_i96_1 = _mm_or_si128(vec_i48_1l, _mm_srli_si128(vec_i48_1h, 2)); // 96bit+32bit
		__m128i vec_i96_2 = _mm_or_si128(vec_i48_2l, _mm_srli_si128(vec_i48_2h, 2)); // 96bit+32bit
		__m128i vec_i128_1 = _mm_or_si128(vec_i96_1, _mm_slli_si128(vec_i96_2, 12)); // 24bit*4+(24bit+8bit) = 128bit
		__m128i vec_i128_2 = _mm_srli_si128(vec_i96_2, 4); // 16bit+24bit*2 = 64bit
#if (USE_X86_EXT_INTRIN >= 9)
		__m256i vec_i192 = MM256_SET2X_SI256(vec_i128_1, vec_i128_2); // 24bit*8 + emp64bit
		_mm256_storeu_si256((__m256i *)cp, vec_i192); // 192bit/256bit unalign ’´‚¦‚é•ª‚Í–³Ž‹	
		cp += 24; // 192bit = 24byte
#else
		_mm_storeu_si128((__m128i *)cp, vec_i128_1); // 128bit unalign
		cp += 16; // 128bit = 16byte
		_mm_storeu_si128((__m128i *)cp, vec_i128_2); // 64bit/128bit unalign ’´‚¦‚é•ª‚Í–³Ž‹	
		cp += 8; // 64bit = 8byte
#endif
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos24(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_24BIT_SIGNED);
	for(i = 0; i < c; i += 4){ // 108 inst in loop
		__m128 vec_f = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		float *out = (float *)vec_f;
		STORE_S24(cp, (int32)(out[0]));
		STORE_S24(cp, (int32)(out[1]));
		STORE_S24(cp, (int32)(out[2]));
		STORE_S24(cp, (int32)(out[3]));
		}
#else
		STORE_S24(cp, (int32)(vec_f.m128_f32[0]));
		STORE_S24(cp, (int32)(vec_f.m128_f32[1]));
		STORE_S24(cp, (int32)(vec_f.m128_f32[2]));
		STORE_S24(cp, (int32)(vec_f.m128_f32[3]));	
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64tos24(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		STORE_S24(cp, (int32)(clip_input(&lp[i]) * MAX_24BIT_SIGNED));
}
#endif

static void CALLINGCONV f64tou24(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)	
		STORE_U24(cp, (int32)(clip_input(&lp[i]) * MAX_24BIT_SIGNED));
}

static void CALLINGCONV f64tos24x(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		STORE_S24X(cp, (int32)(clip_input(&lp[i]) * MAX_24BIT_SIGNED));
}

static void CALLINGCONV f64tou24x(DATA_T *lp, int32 c)
{
	uint8 *cp = (uint8 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		STORE_U24X(cp, (int32)(clip_input(&lp[i]) * MAX_24BIT_SIGNED));
}


#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos32(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_32BIT_SIGNED);
	for(i = 0; i < c; i += 8){ // d5 d5
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
		_mm_store_si128((__m128i *)&sp[i], vec_i32_1); // 128bit=32bit*4		
		_mm_store_si128((__m128i *)&sp[i + 4], vec_i32_2); // 128bit=32bit*4	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tos32(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;
	__m128d gain = _mm_set1_pd((double)INPUT_GAIN);
	__m128d vmul = _mm_set1_pd((double)MAX_32BIT_SIGNED);
	for(i = 0; i < c; i += 8){
		__m128i vec_i32_11 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_12 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i + 2], gain), vmul));
		__m128i vec_i32_21 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i + 4], gain), vmul));
		__m128i vec_i32_22 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i + 6], gain), vmul));
		__m128i vec_i32_1 = _mm_or_si128(vec_i32_11, _mm_slli_si128(vec_i32_12, 8));
		__m128i vec_i32_2 = _mm_or_si128(vec_i32_21, _mm_slli_si128(vec_i32_22, 8));
		_mm_store_si128((__m128i *)&sp[i], vec_i32_1); // 128bit=32bit*4	
		_mm_store_si128((__m128i *)&sp[i + 4], vec_i32_2); // 128bit=32bit*4	
	}
}
#elif (USE_X86_EXT_INTRIN >= 9) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos32(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;
	__m256 gain = _mm256_set1_ps((float)INPUT_GAIN);
	__m256 vmul = _mm256_set1_ps((float)MAX_32BIT_SIGNED);	
	for(i = 0; i < c; i += 8){
		__m256i vec_i32 = _mm256_cvttps_epi32(_mm256_mul_ps(F256_CLIP_INPUT(&lp[i], gain), vmul));
		_mm256_store_si256((__m256i *)&sp[i], vec_i32); // 256bit=32bit*16	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos32(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_32BIT_SIGNED);	
	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1);
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2);
		_mm_store_si128((__m128i *)&sp[i], vec_i32_1); // 128bit=32bit*4		
		_mm_store_si128((__m128i *)&sp[i + 4], vec_i32_2); // 128bit=32bit*4	
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tos32(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_32BIT_SIGNED);	
	for(i = 0; i < c; i += 4){
		__m128 vec_f = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		float *out = (float *)vec_f;
		sp[i] = (int32)(out[0]);
		sp[i] = (int32)(out[1]);
		sp[i] = (int32)(out[2]);
		sp[i] = (int32)(out[3]);	
		}
#else
		sp[i] = (int32)(vec_f.m128_f32[0]);
		sp[i] = (int32)(vec_f.m128_f32[1]);
		sp[i] = (int32)(vec_f.m128_f32[2]);
		sp[i] = (int32)(vec_f.m128_f32[3]);	
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#else
static void CALLINGCONV f64tos32(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = (int32)(clip_input(&lp[i]) * MAX_32BIT_SIGNED);
}
#endif


#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tou32(DATA_T *lp, int32 c)
{
	uint32 *sp=(uint32 *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);
	__m256d vmul = _mm256_set1_pd((double)MAX_32BIT_SIGNED);
	__m128i vex = _mm_set1_epi32(0x80000000);
	for(i = 0; i < c; i += 8){ // d5 d5
		__m128i vec_i32_1 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_2 = _mm256_cvttpd_epi32(_mm256_mul_pd(D256_CLIP_INPUT(&lp[i + 4], gain), vmul));
		vec_i32_1 = _mm_xor_si128(vex, vec_i32_1);
		vec_i32_2 = _mm_xor_si128(vex, vec_i32_2);
		_mm_store_si128((__m128i *)&sp[i], vec_i32_1); // 128bit=32bit*4		
		_mm_store_si128((__m128i *)&sp[i + 4], vec_i32_2); // 128bit=32bit*4	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tou32(DATA_T *lp, int32 c)
{
	uint32 *sp=(uint32 *)(lp);
	int32 i;
	__m128d gain = _mm_set1_pd((double)INPUT_GAIN);
	__m128d vmul = _mm_set1_pd((double)MAX_32BIT_SIGNED);
	__m128i vex = _mm_set1_epi32(0x80000000);
	for(i = 0; i < c; i += 8){
		__m128i vec_i32_11 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i], gain), vmul));
		__m128i vec_i32_12 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i + 2], gain), vmul));
		__m128i vec_i32_21 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i + 4], gain), vmul));
		__m128i vec_i32_22 = _mm_cvttpd_epi32(_mm_mul_pd(D128_CLIP_INPUT(&lp[i + 6], gain), vmul));
		__m128i vec_i32_1 = _mm_or_si128(vec_i32_11, _mm_slli_si128(vec_i32_12, 8));
		__m128i vec_i32_2 = _mm_or_si128(vec_i32_21, _mm_slli_si128(vec_i32_22, 8));
		vec_i32_1 = _mm_xor_si128(vex, vec_i32_1);
		vec_i32_2 = _mm_xor_si128(vex, vec_i32_2);
		_mm_store_si128((__m128i *)&sp[i], vec_i32_1); // 128bit=32bit*4	
		_mm_store_si128((__m128i *)&sp[i + 4], vec_i32_2); // 128bit=32bit*4	
	}
}
#elif (USE_X86_EXT_INTRIN >= 9) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tou32(DATA_T *lp, int32 c)
{
	uint32 *sp=(uint32 *)(lp);
	int32 i;
	__m256 gain = _mm256_set1_ps((float)INPUT_GAIN);
	__m256 vmul = _mm256_set1_ps((float)MAX_32BIT_SIGNED);	
	__m256i vex = _mm256_set1_epi32(0x80000000);
	for(i = 0; i < c; i += 8){
		__m256i vec_i32 = _mm256_cvttps_epi32(_mm256_mul_ps(F256_CLIP_INPUT(&lp[i], gain), vmul));
		vec_i32 = _mm256_xor_si256(vex, vec_i32);
		_mm256_store_si256((__m256i *)&sp[i], vec_i32); // 256bit=32bit*16	
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tou32(DATA_T *lp, int32 c)
{
	uint32 *sp=(uint32 *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	__m128 vmul = _mm_set1_ps((float)MAX_32BIT_SIGNED);	
	__m128i vex = _mm_set1_epi32(0x80000000);
	for(i = 0; i < c; i += 8){
		__m128 vec_f1 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i], gain), vmul);
		__m128 vec_f2 = _mm_mul_ps(F128_CLIP_INPUT(&lp[i + 4], gain), vmul);
		__m128i vec_i32_1 = _mm_cvttps_epi32(vec_f1);
		__m128i vec_i32_2 = _mm_cvttps_epi32(vec_f2);
		vec_i32_1 = _mm_xor_si128(vex, vec_i32_1);
		vec_i32_2 = _mm_xor_si128(vex, vec_i32_2);
		_mm_store_si128((__m128i *)&sp[i], vec_i32_1); // 128bit=32bit*4		
		_mm_store_si128((__m128i *)&sp[i + 4], vec_i32_2); // 128bit=32bit*4	
	}
}
#else
static void CALLINGCONV f64tou32(DATA_T *lp, int32 c)
{
	uint32 *sp=(uint32 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = 0x80000000 ^ (uint32)(clip_input(&lp[i]) * MAX_32BIT_SIGNED);
}
#endif

static void CALLINGCONV f64tos32x(DATA_T *lp, int32 c)
{
	int32 *sp=(int32 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = XCHG_LONG((int32)(clip_input(&lp[i]) * MAX_32BIT_SIGNED));
}

static void CALLINGCONV f64tou32x(DATA_T *lp, int32 c)
{
	uint32 *sp=(uint32 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = XCHG_LONG(0x80000000 ^ (uint32)(clip_input(&lp[i]) * MAX_32BIT_SIGNED));
}

#ifdef TIMIDITY_HAVE_INT64

static void CALLINGCONV f64tos64(DATA_T *lp, int32 c)
{
	int64 *sp=(int64 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = (int64)(clip_input(&lp[i]) * MAX_64BIT_SIGNED);
}

static void CALLINGCONV f64tou64(DATA_T *lp, int32 c)
{
	uint64 *sp=(uint64 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = 0x8000000000000000 ^ (uint64)(clip_input(&lp[i]) * MAX_64BIT_SIGNED);
}

static void CALLINGCONV f64tos64x(DATA_T *lp, int32 c)
{
	int64 *sp=(int64 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = XCHG_LONGLONG((int64)(clip_input(&lp[i]) * MAX_64BIT_SIGNED));
}

static void CALLINGCONV f64tou64x(DATA_T *lp, int32 c)
{
	uint64 *sp=(uint64 *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = XCHG_LONGLONG(0x8000000000000000 ^ (uint64)(clip_input(&lp[i]) * MAX_64BIT_SIGNED));
}

#else

static void CALLINGCONV f64tos64(DATA_T *lp, int32 c)
{
    const DATA_T *end = lp;
    int32 *sp = (int32*)(lp) + (c << 1);
    double d;

    lp += c;
    while (lp > end)
    {
	d = clip_input(*--lp);
#ifdef LITTLE_ENDIAN
	*--sp = (int32)(d * MAX_32BIT_SIGNED); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#else
	*--sp = 0; /* LODWORD */
	*--sp = (int32)(d * MAX_32BIT_SIGNED); /* HIDWORD */
#endif /* LITTLE_ENDIAN */
    }
}

static void CALLINGCONV f64tou64(DATA_T *lp, int32 c)
{
    const DATA_T *end = lp;
    uint32 *sp = (uint32*)(lp) + (c << 1);
    double d;

    lp += c;
    while (lp > end)
    {
	d = clip_input(*--lp);
#ifdef LITTLE_ENDIAN
	*--sp = 0x80000000UL ^ (uint32)(d * MAX_32BIT_SIGNED); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#else
	*--sp = 0; /* LODWORD */
	*--sp = 0x80000000UL ^ (uint32)(d * MAX_32BIT_SIGNED); /* HIDWORD */
#endif /* LITTLE_ENDIAN */
    }
}

static void CALLINGCONV f64tos64x(DATA_T *lp, int32 c)
{
    const DATA_T *end = lp;
    int32 *sp = (int32*)(lp) + (c << 1);
    double d;
    int32 l;

    lp += c;
    while (lp > end)
    {
	d = clip_input(*--lp);
	l = (int32)(d * MAX_32BIT_SIGNED);
#ifdef LITTLE_ENDIAN
	*--sp = 0; /* LODWORD */
	*--sp = XCHG_LONG(l); /* HIDWORD */
#else
	*--sp = XCHG_LONG(l); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#endif /* LITTLE_ENDIAN */
    }
}

static void CALLINGCONV f64tou64x(DATA_T *lp, int32 c)
{
    const DATA_T *end = lp;
    uint32 *sp = (uint32*)(lp) + (c << 1);
    double d;
    int32 l;

    lp += c;
    while (lp > end)
    {
	d = clip_input(*--lp);
	l = 0x80000000UL ^ (uint32)(d * MAX_32BIT_SIGNED);
#ifdef LITTLE_ENDIAN
	*--sp = 0; /* LODWORD */
	*--sp = XCHG_LONG(l); /* HIDWORD */
#else
	*--sp = XCHG_LONG(l); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#endif /* LITTLE_ENDIAN */
    }
}

#endif /* TIMIDITY_HAVE_INT64 */

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tof32(DATA_T *lp, int32 c)
{
	float *sp=(float *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd((double)INPUT_GAIN);	
	for(i = 0; i < c; i += 4)
		_mm_store_ps(&sp[i], _mm256_cvtpd_ps(D256_CLIP_INPUT(&lp[i], gain)));
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tof32(DATA_T *lp, int32 c)
{
	float *sp=(float *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	for(i = 0; i < c; i += 8){
		__m128 vec_f11 = _mm_cvtpd_ps(_mm_load_pd(&lp[i]));
		__m128 vec_f12 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 2]));
		__m128 vec_f21 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 4]));
		__m128 vec_f22 = _mm_cvtpd_ps(_mm_load_pd(&lp[i + 6]));
		__m128 vec_f1 = _mm_shuffle_ps(vec_f11, vec_f12, 0x44);
		__m128 vec_f2 = _mm_shuffle_ps(vec_f21, vec_f22, 0x44);
		_mm_store_ps(&sp[i], F128_CLIP_MM(vec_f1, gain));
		_mm_store_ps(&sp[i + 4], F128_CLIP_MM(vec_f2, gain));
	}
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tof32(DATA_T *lp, int32 c)
{
	float *sp=(float *)(lp);
	int32 i;
	__m256 gain = _mm256_set1_ps((float)INPUT_GAIN);	
	for(i = 0; i < c; i += 8)
		_mm256_store_ps(&sp[i], F256_CLIP_INPUT(&lp[i], gain));
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tof32(DATA_T *lp, int32 c)
{
	float *sp=(float *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	for(i = 0; i < c; i += 8){
		_mm_store_ps(&sp[i], F128_CLIP_INPUT(&lp[i], gain));
		_mm_store_ps(&sp[i + 4], F128_CLIP_INPUT(&lp[i + 4], gain));
	}
}
#else
static void CALLINGCONV f64tof32(DATA_T *lp, int32 c)
{
	float *sp=(float *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = clip_input(&lp[i]);
}
#endif

#if (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;
	__m256d gain = _mm256_set1_pd(INPUT_GAIN);	
	for(i = 0; i < c; i += 8){
		_mm256_store_pd(&sp[i], D256_CLIP_INPUT(&lp[i], gain));
		_mm256_store_pd(&sp[i + 4], D256_CLIP_INPUT(&lp[i + 4], gain));
	}
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;
	__m128d gain = _mm_set1_pd(INPUT_GAIN);
	for(i = 0; i < c; i += 8){
		_mm_store_pd(&sp[i], D128_CLIP_INPUT(&lp[i], gain));
		_mm_store_pd(&sp[i + 2], D128_CLIP_INPUT(&lp[i + 2], gain));
		_mm_store_pd(&sp[i + 4], D128_CLIP_INPUT(&lp[i + 4], gain));
		_mm_store_pd(&sp[i + 6], D128_CLIP_INPUT(&lp[i + 6], gain));
	}
}
#elif (USE_X86_EXT_INTRIN >= 8) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);	
	for(i = c - 4; i >= 0; i -= 4)
		_mm256_store_pd(&sp[i], _mm256_cvtps_pd(F128_CLIP_INPUT(&lp[i], gain)));
}
#elif (USE_X86_EXT_INTRIN >= 3) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	for(i = c - 4; i >= 0; i -= 4){
		__m128 vec0 = F128_CLIP_INPUT(&lp[i], gain);
		__m128 vec1 = _mm_movehl_ps(vec0, vec0);
		_mm_store_pd(&sp[i], _mm_cvtps_pd(vec0));
		_mm_store_pd(&sp[i + 2], _mm_cvtps_pd(vec1));
	}
}
#elif (USE_X86_EXT_INTRIN >= 2) && defined(DATA_T_FLOAT)
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;
	__m128 gain = _mm_set1_ps((float)INPUT_GAIN);
	for(i = c - 4; i >= 0; i -= 4){
		__m128 vec_f = F128_CLIP_INPUT(&lp[i], gain);
#if !(defined(_MSC_VER) || defined(MSC_VER))
		{
		float *out = (float *)vec_f;
		sp[i] = (double)(out[0]);
		sp[i] = (double)(out[1]);
		sp[i] = (double)(out[2]);
		sp[i] = (double)(out[3]);	
		}
#else
		sp[i] = (double)(vec_f.m128_f32[0]);
		sp[i] = (double)(vec_f.m128_f32[1]);
		sp[i] = (double)(vec_f.m128_f32[2]);
		sp[i] = (double)(vec_f.m128_f32[3]);		
#endif //  !(defined(_MSC_VER) || defined(MSC_VER))
	}
}
#elif defined(DATA_T_DOUBLE)
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;

	for(i = 0; i < c; i++)
		sp[i] = clip_input(&lp[i]);
}
#else
static void CALLINGCONV f64tof64(DATA_T *lp, int32 c)
{
	double *sp=(double *)(lp);
	int32 i;

	for(i = c - 1; i >= 0; i--)
		sp[i] = clip_input(&lp[i]);
}
#endif


void general_output_convert_setup(void)
{
	uint32 penc = use_temp_encoding ? temp_encoding : play_mode->encoding; 
	
    convert_count = 1; //def 8bit mono
    if(!(penc & PE_MONO))
		convert_count *= 2; /* Stereo samples */

	convert_bytes = convert_count;
	if(penc & PE_16BIT)
    {
		convert_bytes *= 2;
		if(penc & PE_BYTESWAP)
		{
			if(penc & PE_SIGNED)
				convert_fnc = f64tos16x;
			else
				convert_fnc = f64tou16x;
		}
		else if(penc & PE_SIGNED)
			convert_fnc = f64tos16;
		else
			convert_fnc = f64tou16;
    }
	else if(penc & PE_24BIT) {
		convert_bytes *= 3;
		if(penc & PE_BYTESWAP)
		{
			if(penc & PE_SIGNED)
				convert_fnc = f64tos24x;
			else
				convert_fnc = f64tou24x;
		} else if(penc & PE_SIGNED)
			convert_fnc = f64tos24;
		else
			convert_fnc = f64tou24;
    }
    else if(penc & PE_32BIT) {
		convert_bytes *= 4;
		if(penc & PE_BYTESWAP)
		{
			if(penc & PE_SIGNED)
				convert_fnc = f64tos32x;
			else
				convert_fnc = f64tou32x;
		} else if(penc & PE_SIGNED)
			convert_fnc = f64tos32;
		else
			convert_fnc = f64tou32;
    }
	else if(penc & PE_F32BIT) {
		convert_bytes *= 4;
		convert_fnc = f64tof32;
    }
    else if(penc & PE_64BIT) {
		convert_bytes *= 8;
		if(penc & PE_BYTESWAP)
		{
			if(penc & PE_SIGNED)
				convert_fnc = f64tos64x;
			else
				convert_fnc = f64tou64x;
		} else if(penc & PE_SIGNED)
			convert_fnc = f64tos64;
		else
			convert_fnc = f64tou64;
    }
	else if(penc & PE_F64BIT) {
		convert_bytes *= 8;
		convert_fnc = f64tof64;
    }
	else if(penc & PE_ULAW)
		convert_fnc = f64toulaw;
    else if(penc & PE_ALAW)
		convert_fnc = f64toalaw;
    else if(penc & PE_SIGNED)
		convert_fnc = f64tos8;
    else
		convert_fnc = f64tou8;
    return;
}


#else // DATA_T_INT32

static const int shift_8bit = MAX_BITS - GUARD_BITS - 8; // right shift
static const int shift_16bit = MAX_BITS - GUARD_BITS - 16; // right shift
static const int shift_24bit = MAX_BITS - GUARD_BITS - 24; // right shift
static const int shift_32bit = GUARD_BITS; // left shift
static const int shift_64bit = GUARD_BITS + 32; // left shift

#define CLIP_GUARD_MAX(x) if (x > max_guard_signed) { x = max_guard_signed; } else if (x < min_guard_signed) { x = min_guard_signed; }

#if OPT_MODE == 1 /* fixed-point implementation */

#define GAIN_INT32(x, amp) (imuldiv28(x, amp))

static void CALLINGCONV s32toulaw(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int8 *up = (int8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*up++ = AUDIO_S2U(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32toalaw(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int8 *up = (int8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*up++ = AUDIO_S2A(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32tos8(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int8 *cp = (int8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*cp++ = (int8)(l >> shift_8bit);
    } while (lp < end);
}

static void CALLINGCONV s32tou8(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*cp++ = 0x80U ^ ((uint8)(l >> shift_8bit));
    } while (lp < end);
}

static void CALLINGCONV s32tos16(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int16 *sp = (int16*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*sp++ = (int16)(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32tou16(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint16 *sp = (uint16*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*sp++ = 0x8000U ^ (uint16)(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32tos16x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int16 *sp = (int16*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l = (int16)(l >> shift_16bit);
	*sp++ = XCHG_SHORT(l);
    } while (lp < end);
}

static void CALLINGCONV s32tou16x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint16 *sp = (uint16*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l = 0x8000U ^ (uint16)(l >> shift_16bit);
	*sp++ = XCHG_SHORT(l);
    } while (lp < end);
}

static void CALLINGCONV s32tos24(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_S24(cp, l);
    } while (lp < end);
}

static void CALLINGCONV s32tou24(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_U24(cp, l);
    } while (lp < end);
}

static void CALLINGCONV s32tos24x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_S24X(cp, l);
    } while (lp < end);
}

static void CALLINGCONV s32tou24x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_U24X(cp, l);
    } while (lp < end);
}

///r int32
static void CALLINGCONV s32tos32(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int32 *sp = (int32*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*sp++ = (int32)(l << shift_32bit);
    } while (lp < end);
}

static void CALLINGCONV s32tou32(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint32 *sp = (uint32*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	*sp++ = 0x80000000UL ^ (uint32)(l << shift_32bit);
    } while (lp < end);
}

static void CALLINGCONV s32tos32x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int32 *sp = (int32*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l = (int32)(l << shift_32bit);
	*sp++ = XCHG_LONG(l);
    } while (lp < end);
}

static void CALLINGCONV s32tou32x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint32 *sp = (uint32*)(lp);
    int32 l;
    const int32 leveli = output_volumei;

    do
    {
	l = GAIN_INT32(*lp++, leveli);
	CLIP_GUARD_MAX(l)
	l = 0x80000000UL ^ (uint32)(l << shift_32bit);
	*sp++ = XCHG_LONG(l);
    } while (lp < end);
}

#ifdef TIMIDITY_HAVE_INT64

///r int64
static void CALLINGCONV s32tos64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int64 *sp = (int64*)(lp) + c;
    int64 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	*--sp = (int64)(l << shift_64bit);
    } while (lp > end);
}

static void CALLINGCONV s32tou64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint64 *sp = (uint64*)(lp) + c;
    int64 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	*--sp = 0x8000000000000000ULL ^ (uint64)(l << shift_64bit);
    } while (lp > end);
}

static void CALLINGCONV s32tos64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int64 *sp = (int64*)(lp) + c;
    int64 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	l = (int64)(l << shift_64bit);
	*--sp = XCHG_LONGLONG(l);
    } while (lp > end);
}

static void CALLINGCONV s32tou64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint64 *sp = (uint64*)(lp) + c;
    int64 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	l = 0x8000000000000000ULL ^ (uint64)(l << shift_64bit);
	*--sp = XCHG_LONGLONG(l);
    } while (lp > end);
}

#else /* TIMIDITY_HAVE_INT64 */

static void CALLINGCONV s32tos64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int32 *sp = (int32*)(lp) + (c << 1);
    int32 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	l = (int32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = l; /* HIDWORD */
	*--sp = 0; /* LODWORD */
#else
	*--sp = 0; /* LODWORD */
	*--sp = l; /* HIDWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

static void CALLINGCONV s32tou64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint32 *sp = (uint32*)(lp) + (c << 1);
    uint32 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	l = 0x80000000UL ^ (uint32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = l; /* HIDWORD */
	*--sp = 0; /* LODWORD */
#else
	*--sp = 0; /* LODWORD */
	*--sp = l; /* HIDWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

static void CALLINGCONV s32tos64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int32 *sp = (int32*)(lp) + (c << 1);
    int32 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	l = (int32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = 0; /* LODWORD */
	*--sp = XCHG_LONG(l); /* HIDWORD */
#else
	*--sp = XCHG_LONG(l); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

static void CALLINGCONV s32tou64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint32 *sp = (uint32*)(lp) + (c << 1);
    int32 l;
    const int32 leveli = output_volumei;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, leveli);
	CLIP_GUARD_MAX(l)
	l = 0x80000000UL ^ (uint32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = 0; /* LODWORD */
	*--sp = XCHG_LONG(l); /* HIDWORD */
#else
	*--sp = XCHG_LONG(l); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

#endif /* TIMIDITY_HAVE_INT64 */

///r float
static void CALLINGCONV s32tof32(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    float *sp = (float*)(lp);
    float l;
    const int32 leveli = output_volumei;

    do
    {
	l = (float)GAIN_INT32(*lp++, leveli) * (float)div_max_guard_signed;
	*sp++ = (float)(l);
    } while (lp < end);
}

static void CALLINGCONV s32tof64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    double *sp = (double*)(lp) + c;
    const int32 leveli = output_volumei;

    lp += c;
    while (lp > end)
	*--sp = (double)GAIN_INT32(*--lp, leveli) * div_max_guard_signed;
}

#else /* floating-point implementation */

#define GAIN_INT32(x, amp) ((x) * amp)

static void CALLINGCONV s32toulaw(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int8 *up = (int8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*up++ = AUDIO_S2U(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32toalaw(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int8 *up = (int8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*up++ = AUDIO_S2A(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32tos8(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int8 *cp = (int8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*cp++ = (int8)(l >> shift_8bit);
    } while (lp < end);
}

static void CALLINGCONV s32tou8(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*cp++ = 0x80U ^ ((uint8)(l >> shift_8bit));
    } while (lp < end);
}

static void CALLINGCONV s32tos16(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int16 *sp = (int16*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*sp++ = (int16)(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32tou16(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint16 *sp = (uint16*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*sp++ = 0x8000U ^ (uint16)(l >> shift_16bit);
    } while (lp < end);
}

static void CALLINGCONV s32tos16x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int16 *sp = (int16*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l = (int16)(l >> shift_16bit);
	*sp++ = XCHG_SHORT(l);
    } while (lp < end);
}

static void CALLINGCONV s32tou16x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint16 *sp = (uint16*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l = 0x8000U ^ (uint16)(l >> shift_16bit);
	*sp++ = XCHG_SHORT(l);
    } while (lp < end);
}

static void CALLINGCONV s32tos24(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_S24(cp, l);
    } while (lp < end);
}

static void CALLINGCONV s32tou24(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_U24(cp, l);
    } while (lp < end);
}

static void CALLINGCONV s32tos24x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_S24X(cp, l);
    } while (lp < end);
}

static void CALLINGCONV s32tou24x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint8 *cp = (uint8*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l >>= shift_24bit;
	STORE_U24X(cp, l);
    } while (lp < end);
}

///r int32
static void CALLINGCONV s32tos32(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int32 *sp = (int32*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*sp++ = (int32)(l << shift_32bit);
    } while (lp < end);
}

static void CALLINGCONV s32tou32(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint32 *sp = (uint32*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	*sp++ = 0x80000000UL ^ (uint32)(l << shift_32bit);
    } while (lp < end);
}

static void CALLINGCONV s32tos32x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    int32 *sp = (int32*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l = (int32)(l << shift_32bit);
	*sp++ = XCHG_LONG(l);
    } while (lp < end);
}

static void CALLINGCONV s32tou32x(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    uint32 *sp = (uint32*)(lp);
    int32 l;
    const FLOAT_T level = output_volume;

    do
    {
	l = GAIN_INT32(*lp++, level);
	CLIP_GUARD_MAX(l)
	l = 0x80000000UL ^ (uint32)(l << shift_32bit);
	*sp++ = XCHG_LONG(l);
    } while (lp < end);
}

#ifdef TIMIDITY_HAVE_INT64

///r int64
static void CALLINGCONV s32tos64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int64 *sp = (int64*)(lp) + c;
    int64 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	*--sp = (int64)(l << shift_64bit);
    } while (lp > end);
}

static void CALLINGCONV s32tou64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint64 *sp = (uint64*)(lp) + c;
    int64 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	*--sp = 0x8000000000000000ULL ^ (uint64)(l << shift_64bit);
    } while (lp > end);
}

static void CALLINGCONV s32tos64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int64 *sp = (int64*)(lp) + c;
    int64 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	l = (int64)(l << shift_64bit);
	*--sp = XCHG_LONGLONG(l);
    } while (lp > end);
}

static void CALLINGCONV s32tou64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint64 *sp = (uint64*)(lp) + c;
    int64 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	l = 0x8000000000000000ULL ^ (uint64)(l << shift_64bit);
	*--sp = XCHG_LONGLONG(l);
    } while (lp > end);
}

#else /* TIMIDITY_HAVE_INT64 */

static void CALLINGCONV s32tos64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int32 *sp = (int32*)(lp) + (c << 1);
    int32 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	l = (int32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = l; /* HIDWORD */
	*--sp = 0; /* LODWORD */
#else
	*--sp = 0; /* LODWORD */
	*--sp = l; /* HIDWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

static void CALLINGCONV s32tou64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint32 *sp = (uint32*)(lp) + (c << 1);
    uint32 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	l = 0x80000000UL ^ (uint32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = l; /* HIDWORD */
	*--sp = 0; /* LODWORD */
#else
	*--sp = 0; /* LODWORD */
	*--sp = l; /* HIDWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

static void CALLINGCONV s32tos64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    int32 *sp = (int32*)(lp) + (c << 1);
    int32 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	l = (int32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = 0; /* LODWORD */
	*--sp = XCHG_LONG(l); /* HIDWORD */
#else
	*--sp = XCHG_LONG(l); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

static void CALLINGCONV s32tou64x(int32 *lp, int32 c)
{
    const int32 *end = lp;
    uint32 *sp = (uint32*)(lp) + (c << 1);
    int32 l;
    const FLOAT_T level = output_volume;

    lp += c;
    do
    {
	l = GAIN_INT32(*--lp, level);
	CLIP_GUARD_MAX(l)
	l = 0x80000000UL ^ (uint32)(l << shift_32bit);
#ifdef LITTLE_ENDIAN
	*--sp = 0; /* LODWORD */
	*--sp = XCHG_LONG(l); /* HIDWORD */
#else
	*--sp = XCHG_LONG(l); /* HIDWORD */
	*--sp = 0; /* LODWORD */
#endif /* LITTLE_ENDIAN */
    } while (lp > end);
}

#endif /* TIMIDITY_HAVE_INT64 */

///r float
static void CALLINGCONV s32tof32(int32 *lp, int32 c)
{
    const int32 *end = lp + c;
    float *sp = (float*)(lp);
    float l;
    const FLOAT_T level = output_volume;

    do
    {
	l = (float)GAIN_INT32(*lp++, level) * (float)div_max_guard_signed;
	*sp++ = (float)(l);
    } while (lp < end);
}

static void CALLINGCONV s32tof64(int32 *lp, int32 c)
{
    const int32 *end = lp;
    double *sp = (double*)(lp) + c;
    const FLOAT_T level = output_volume;

    lp += c;
    while (lp > end)
	*--sp = (double)GAIN_INT32(*--lp, level) * div_max_guard_signed;
}

#endif /* OPT_MODE == 1 */

void general_output_convert_setup(void)
{
	uint32 penc = use_temp_encoding ? temp_encoding : play_mode->encoding; 

    convert_count = 1; //def 8bit mono
    if (!(penc & PE_MONO))
	convert_count *= 2; /* Stereo samples */

    convert_bytes = convert_count;
    if (penc & PE_16BIT)
    {
	convert_bytes *= 2;
	if (penc & PE_BYTESWAP)
	{
	    if (penc & PE_SIGNED)
		convert_fnc = s32tos16x;
	    else
		convert_fnc = s32tou16x;
	}
	else if (penc & PE_SIGNED)
	    convert_fnc = s32tos16;
	else
	    convert_fnc = s32tou16;
    }
    else if (penc & PE_24BIT) {
	convert_bytes *= 3;
	if (penc & PE_BYTESWAP)
	{
	    if (penc & PE_SIGNED)
		convert_fnc = s32tos24x;
	    else
		convert_fnc = s32tou24x;
	} else if (penc & PE_SIGNED)
	    convert_fnc = s32tos24;
	else
	    convert_fnc = s32tou24;
    }
    else if (penc & PE_32BIT) {
	convert_bytes *= 4;
	if (penc & PE_BYTESWAP)
	{
	    if (penc & PE_SIGNED)
		convert_fnc = s32tos32x;
	    else
		convert_fnc = s32tou32x;
	} else if (penc & PE_SIGNED)
	    convert_fnc = s32tos32;
	else
	    convert_fnc = s32tou32;
    }
    else if (penc & PE_F32BIT) {
	convert_bytes *= 4;
	convert_fnc = s32tof32;
    }
    else if (penc & PE_64BIT) {
	convert_bytes *= 8;
	if (penc & PE_BYTESWAP)
	{
	    if (penc & PE_SIGNED)
		convert_fnc = s32tos64x;
	    else
		convert_fnc = s32tou64x;
	} else if (penc & PE_SIGNED)
	    convert_fnc = s32tos64;
	else
	    convert_fnc = s32tou64;
    }
    else if (penc & PE_F64BIT) {
	convert_bytes *= 8;
	convert_fnc = s32tof64;
    }
    else if (penc & PE_ULAW)
	convert_fnc = s32toulaw;
    else if (penc & PE_ALAW)
	convert_fnc = s32toalaw;
    else if (penc & PE_SIGNED)
	convert_fnc = s32tos8;
    else
	convert_fnc = s32tou8;

    return;
}

#endif

/* return: number of bytes */
int32 general_output_convert(DATA_T *buf, int32 count)
{
    (*convert_fnc)(buf, (count * convert_count));
    return (count * convert_bytes);
}

///r
int get_encoding_sample_size(int32 enc)
{	
	uint32 penc = use_temp_encoding ? temp_encoding : enc;
	int size = (penc & PE_MONO) ? 1 : 2;

	if(penc & PE_16BIT)
		size *= 2;
	else if (penc & PE_24BIT)
		size *= 3;
	else if (penc & PE_32BIT)
		size *= 4;
	else if (penc & PE_F32BIT)
		size *= 4;
	else if (penc & PE_64BIT)
		size *= 8;
	else if (penc & PE_F64BIT)
		size *= 8;
	return size;
}

///r
int validate_encoding(int enc, int include_enc, int exclude_enc)
{
    const char *orig_enc_name, *enc_name;

    orig_enc_name = output_encoding_string(enc);
    enc |= include_enc;
    enc &= ~exclude_enc;
    if(enc & (PE_ULAW|PE_ALAW))
		enc &= ~(PE_F64BIT|PE_F32BIT|PE_64BIT|PE_32BIT|PE_24BIT|PE_16BIT|PE_SIGNED|PE_BYTESWAP);
    if(!(enc & PE_16BIT || enc & PE_24BIT || enc & PE_32BIT	|| enc & PE_64BIT
		|| enc & PE_F32BIT || enc & PE_F64BIT))
		enc &= ~PE_BYTESWAP;

	if(enc & PE_F64BIT)
		enc &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT);	
	else if(enc & PE_F32BIT)
		enc &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F64BIT);
	else if(enc & PE_64BIT)
		enc &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_F32BIT | PE_F64BIT);
	else if(enc & PE_32BIT)
		enc &= ~(PE_16BIT | PE_24BIT | PE_64BIT | PE_F32BIT | PE_F64BIT);
	else if(enc & PE_24BIT)
		enc &= ~(PE_16BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT);
	else if(enc & PE_16BIT)
		enc &= ~(PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT);

    enc_name = output_encoding_string(enc);
    if(strcmp(orig_enc_name, enc_name) != 0)
	ctl->cmsg(CMSG_WARNING, VERB_NOISY,
		  "Notice: Audio encoding is changed `%s' to `%s'",
		  orig_enc_name, enc_name);
    return enc;
}
///r
int32 apply_encoding(int32 old_enc, int32 new_enc)
{
	const int32 mutex_flags[] = {
		PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW,
		PE_BYTESWAP | PE_ULAW | PE_ALAW,
		PE_SIGNED | PE_ULAW | PE_ALAW,
	};
	int i;

	for (i = 0; i < sizeof mutex_flags / sizeof mutex_flags[0]; i++) {
		if (new_enc & mutex_flags[i])
			old_enc &= ~mutex_flags[i];
	}
	return old_enc | new_enc;
}
///r
const char *output_encoding_string(int enc)
{
    if(enc & PE_MONO)
    {
		if(enc & PE_16BIT)
		{
			if(enc & PE_SIGNED)
			return "16bit (mono)";
			else
			return "unsigned 16bit (mono)";
		}
		else if(enc & PE_24BIT)
		{
			if(enc & PE_SIGNED)
			return "24bit (mono)";
			else
			return "unsigned 24bit (mono)";
		}
		else if(enc & PE_32BIT)
		{
			if(enc & PE_SIGNED)
			return "32bit (mono)";
			else
			return "unsigned 32bit (mono)";
		}
		else if(enc & PE_F32BIT)
		{
			return "Float 32bit (mono)";
		}
		else if(enc & PE_64BIT)
		{
			if(enc & PE_SIGNED)
			return "64bit (mono)";
			else
			return "unsigned 64bit (mono)";
		}
		else if(enc & PE_F64BIT)
		{
			return "Float 64bit (mono)";
		}
		else
		{
			if(enc & PE_ULAW)
			return "U-law (mono)";
			else if(enc & PE_ALAW)
			return "A-law (mono)";
			else if(enc & PE_SIGNED)
			return "8bit (mono)";
			else
			return "unsigned 8bit (mono)";
		}
    }
    else if(enc & PE_16BIT)
    {
		if(enc & PE_BYTESWAP)
		{
			if(enc & PE_SIGNED)
			return "16bit (swap)";
			else
			return "unsigned 16bit (swap)";
		}
		else if(enc & PE_SIGNED)
			return "16bit";
		else
			return "unsigned 16bit";
    }
    else if(enc & PE_24BIT)
    {
		if(enc & PE_SIGNED)
			return "24bit";
		else
			return "unsigned 24bit";
    }
	 else if(enc & PE_32BIT)
    {
		if(enc & PE_BYTESWAP)
		{
			if(enc & PE_SIGNED)
			return "32bit (swap)";
			else
			return "unsigned 32bit (swap)";
		}
		else if(enc & PE_SIGNED)
			return "32bit";
		else
			return "unsigned 32bit";
    }
	else if(enc & PE_F32BIT)
    {
		return "Float 32bit";
    }
    else if(enc & PE_64BIT)
    {
		if(enc & PE_BYTESWAP)
		{
			if(enc & PE_SIGNED)
			return "64bit (swap)";
			else
			return "unsigned 64bit (swap)";
		}
		else if(enc & PE_SIGNED)
			return "64bit";
		else
			return "unsigned 64bit";
    }
    else if(enc & PE_F64BIT)
    {
		return "Float 64bit";
    }
    else if(enc & PE_ULAW)
	    return "U-law";
	else if(enc & PE_ALAW)
	    return "A-law";
	else if(enc & PE_SIGNED)
	    return "8bit";
	else
	    return "unsigned 8bit";
    /*NOTREACHED*/
}


#if !defined(KBTIM) && !defined(WINVSTI)
/* mode
  0,1: Default mode.
  2: Remove the directory path of input_filename, then add output_dir.
  3: Replace directory separator characters ('/','\',':') with '_', then add output_dir.
 */
char *create_auto_output_name(const char *input_filename, const char *ext_str, char *output_dir, int mode)
{
  char *output_filename;
  char *ext, *p;
  int32 dir_len = 0;
  char ext_str_tmp[65];

#if defined(__W32G__) || defined(IA_W32GUI) || defined(IA_W32G_SYN)
  output_dir = w32g_output_dir;
  mode = w32g_auto_output_mode;
#endif /* __W32G__ || IA_W32GUI || IA_W32G_SYN */

  output_filename = (char *)malloc((output_dir!=NULL?strlen(output_dir):0) + strlen(input_filename) + 6);
  if(output_filename==NULL)
    return NULL;
  output_filename[0] = '\0';
  if(output_dir!=NULL && (mode==2 || mode==3)) {
    strcat(output_filename,output_dir);
    dir_len = strlen(output_filename);
#ifndef __W32__
    if(dir_len>0 && output_filename[dir_len-1]!=PATH_SEP){
#else
      if(dir_len>0 && output_filename[dir_len-1]!='/' && output_filename[dir_len-1]!='\\' && output_filename[dir_len-1]!=':'){
#endif
	strcat(output_filename,PATH_STRING);
	dir_len++;
      }
    }
    strcat(output_filename, input_filename);

    if((ext = strrchr(output_filename, '.')) == NULL)
      ext = output_filename + strlen(output_filename);
    else {
      /* strip ".gz" */
      if(strcasecmp(ext, ".gz") == 0) {
	*ext = '\0';
	if((ext = strrchr(output_filename, '.')) == NULL)
	  ext = output_filename + strlen(output_filename);
      }
    }

    /* replace '\' , '/' or PATH_SEP between '#' and ext */
    p = strrchr(output_filename,'#');
    if(p!=NULL){
      char *p1;
#ifdef _mbsrchr
#define STRCHR(a,b) _mbschr(a,b)
#else
#define STRCHR(a,b) strchr(a,b)
#endif
#ifndef __W32__
      p1 = p + 1;
      while((p1 = STRCHR(p1,PATH_SEP))!=NULL && p1<ext){
        *p1 = '_';
	p1++;
      }
#else
      p1 = p + 1;
      while((p1 = STRCHR(p1,'\\'))!=NULL && p1<ext){
      	*p1 = '_';
	p1++;
      }
      p1 = p;
      while((p1 = STRCHR(p1,'/'))!=NULL && p1<ext){
	*p1 = '_';
	p1++;
      }
#endif
#undef STRCHR
    }

    /* replace '.' and '#' before ext */
    for(p = output_filename; p < ext; p++)
#ifndef __W32__
      if(*p == '.' || *p == '#')
#else
	if(*p == '#')
#endif
	  *p = '_';

    if(mode==2){
      char *p1,*p2,*p3;
#ifndef __W32__
      p = strrchr(output_filename+dir_len,PATH_SEP);
#else
#ifdef _mbsrchr
#define STRRCHR _mbsrchr
#else
#define STRRCHR strrchr
#endif
      p1 = STRRCHR(output_filename+dir_len,'/');
      p2 = STRRCHR(output_filename+dir_len,'\\');
      p3 = STRRCHR(output_filename+dir_len,':');
#undef STRRCHR
      p1>p2 ? (p1>p3 ? (p = p1) : (p = p3)) : (p2>p3 ? (p = p2) : (p = p3));
#endif
      if(p!=NULL){
	for(p1=output_filename+dir_len,p2=p+1; *p2; p1++,p2++)
	  *p1 = *p2;
	*p1 = '\0';
      }
    }

    if(mode==3){
      for(p=output_filename+dir_len; *p; p++)
#ifndef __W32__
	if(*p==PATH_SEP)
#else
	  if(*p=='/' || *p=='\\' || *p==':')
#endif
	    *p = '_';
    }

    if((ext = strrchr(output_filename, '.')) == NULL)
      ext = output_filename + strlen(output_filename);
    if(*ext){
      strncpy(ext_str_tmp,ext_str,64);
      ext_str_tmp[64]=0;
      if(isupper(*(ext + 1))){
	for(p=ext_str_tmp;*p;p++)
	  *p = toupper(*p);
	*p = '\0';
      } else {
	for(p=ext_str_tmp;*p;p++)
	  *p = tolower(*p);
	*p = '\0';
      }
      strcpy(ext+1,ext_str_tmp);
    }
    return output_filename;
}
#endif