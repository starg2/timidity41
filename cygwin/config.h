#ifndef CYGWIN_CONFIG_HEADER
#define CYGWIN_CONFIG_HEADER

/* ../config.h.  Generated from configure by autoheader.  */
#include "../config.h"
#undef VERSION
#undef TIMID_VERSION
#undef PACKAGE_VERSION
#undef PACKAGE_STRING
#undef VERSION_DATA_T


/* Define to 1 if you have the `vwprintw' function. */
#define HAVE_VWPRINTW 1


#ifndef RC_INVOKED
/* Shift-JIS (MBCS) */

#ifndef NDEBUG
#pragma GCC diagnostic ignored "-Wunused" // 一度も使用していない変数
#pragma GCC diagnostic ignored "-Wsign-compare" // signed unsigned の比較
#pragma GCC diagnostic ignored "-Wunknown-pragmas" // 不明なプラグマ

#pragma GCC diagnostic warning "-Wshadow" // 変数名の重複
#pragma GCC diagnostic warning "-Wundef" // 未定義の識別子
#pragma GCC diagnostic warning "-Wfloat-equal" // 浮動小数点数の比較
#endif

#endif



/* #define FLOAT64_BUFFER 1 */
#if !defined(DATA_T_INT32) && !defined(DATA_T_FLOAT) && !defined(DATA_T_DOUBLE)
#define DATA_T_INT32 1
/* #define DATA_T_FLOAT 1 */
/* #define DATA_T_DOUBLE 1 */
#endif

#ifdef DATA_T_INT32
#undef SUPPORT_SOUNDSPEC
#undef W32SOUNDSPEC
#else
#ifndef W32SOUNDSPEC
#define W32SOUNDSPEC
#endif
#endif

#if 0 //defined(DATA_T_FLOAT) || defined(DATA_T_DOUBLE)
#define EFFECT_LEVEL_FLOAT 1
#endif

#if defined(__x86_64__) || defined(__amd64__)
#define OPT_MODE 0
#elif defined(__i386__)
#define OPT_MODE 1
#endif

#define CONFIG_FILE DEFAULT_PATH "timidity.cfg"
#define AU_W32 1
#define AU_NPIPE 1
#define AU_VORBIS 1
#define AU_VORBIS_DLL 1
#define AU_GOGO 1
#define AU_GOGO_DLL 1
#define AU_LAME 1
#define AU_FLAC 1
#define AU_FLAC_DLL 1
#define AU_OGGFLAC 1
/* #define AU_OGGFLAC_DLL 1 */
#define AU_OPUS 1
#define AU_OPUS_DLL 1
#define AU_SPEEX 1
/* #define AU_AO 1 */
#define AU_PORTAUDIO 1
#define AU_PORTAUDIO_DLL 1
#define AU_BENCHMARK 1
#define PORTAUDIO_V19 1
#define VST_LOADER_ENABLE 1
#define VSTWRAP_EXT 1
#define SMFCONV 1
#define WINSOCK 1
#define __W32READDIR__ 1
#define ANOTHER_MAIN 1
#define FLAC__NO_DLL 1

#ifndef WINVER
#define WINVER 0x0400
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif

/* for Cygwin */
#define WIN32GCC 1
#define __USE_W32_SOCKETS 1
#define USE_SYS_TYPES_FD_SET 1

#include <winsock2.h>
/* #include <Ws2tcpip.h> */
/* #include <Wspiapi.h> */
#include <windows.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif
#define fchdir(fd) (0)



/* Win32GUI Synthesizer */
#ifdef TWSYNG32
#define DEFAULT_VOICES               64 /* 64 voices */
#define DEFAULT_AUDIO_BUFFER_BITS     9 /* 512sample 11ms (44.1kHz) */
#define DEFAULT_AUDIO_BUFFER_NUM     13 /* 150ms over */
#define DEFAULT_COMPUTE_BUFFER_BITS   8 /* ratio 5ms (44.1kHz) */
#define TWSYNG32INI 1 // use twsyng32.ini or timpp32g.ini ??
/* #undef AU_NPIPE */
#undef AU_VORBIS
#undef AU_VORBIS_DLL
#undef AU_GOGO
#undef AU_GOGO_DLL
/* #undef AU_PORTAUDIO */
/* #undef AU_PORTAUDIO_DLL */
#undef AU_LAME
/* #undef AU_FLAC */
/* #undef AU_FLAC_DLL */
/* #undef AU_OGGFLAC */
/* #undef AU_OGGFLAC_DLL */
/* #undef AU_OPUS */
/* #undef AU_OPUS_DLL */
#undef AU_SPEEX
/* #undef AU_AO */
#undef AU_WRITE_MIDI
/* #undef AU_LIST */
#undef AU_MODMIDI
#undef AU_VOLUME_CALC
#undef AU_BENCHMARK
/* #undef VST_LOADER_ENABLE */
/* #undef VSTWRAP_EXT */
#undef SMFCONV
#undef SUPPORT_SOCKET
/* #undef SUPPORT_SOUNDSPEC */
/* #undef W32SOUNDSPEC */
#endif

/* Win32 Synthesizer Service */
#ifdef TWSYNSRV
#define DEFAULT_VOICES               64 /* 64 voices */
#define DEFAULT_AUDIO_BUFFER_BITS     9 /* 512sample 11ms (44.1kHz) */
#define DEFAULT_AUDIO_BUFFER_NUM     13 /* 150ms over */
#define DEFAULT_COMPUTE_BUFFER_BITS   8 /* ratio 5ms (44.1kHz) */
#define TWSYNG32INI 1 // use twsyng32.ini or timpp32g.ini ??
/* #undef AU_NPIPE */
#undef AU_VORBIS
#undef AU_VORBIS_DLL
#undef AU_GOGO
#undef AU_GOGO_DLL
/* #undef AU_PORTAUDIO */
/* #undef AU_PORTAUDIO_DLL */
#undef AU_LAME
#undef AU_FLAC
#undef AU_FLAC_DLL
#undef AU_OGGFLAC
#undef AU_OGGFLAC_DLL
#undef AU_OPUS
#undef AU_OPUS_DLL
#undef AU_SPEEX
/* #undef AU_AO */
#undef AU_WRITE_MIDI
#undef AU_LIST
#undef AU_MODMIDI
#undef AU_VOLUME_CALC
#undef AU_BENCHMARK
/* #undef VST_LOADER_ENABLE */
/* #undef VSTWRAP_EXT */
#undef SMFCONV
#undef SUPPORT_SOCKET
#undef SUPPORT_SOUNDSPEC
#undef W32SOUNDSPEC
#undef __W32G__	/* for Win32 GUI */
#endif

/* Win32 Driver */
#ifdef WINDRV
#define TWSYNG32 1
#define TWSYNSRV 1
#define DEFAULT_VOICES               64 /* 64 voices */
#define DEFAULT_AUDIO_BUFFER_BITS     9 /* 512sample 11ms (44.1kHz) */
#define DEFAULT_AUDIO_BUFFER_NUM     13 /* 150ms over */
#define DEFAULT_COMPUTE_BUFFER_BITS   8 /* ratio 5ms (44.1kHz) */
#define TIMDRVINI 1 // use timdrv.ini or twsyng32.ini or timpp32g.ini ??
#undef AU_NPIPE
#undef AU_VORBIS
#undef AU_VORBIS_DLL
#undef AU_GOGO
#undef AU_GOGO_DLL
/* #undef AU_PORTAUDIO */
/* #undef AU_PORTAUDIO_DLL */
#undef AU_LAME
#undef AU_FLAC
#undef AU_FLAC_DLL
#undef AU_OGGFLAC
#undef AU_OGGFLAC_DLL
#undef AU_OPUS
#undef AU_OPUS_DLL
#undef AU_SPEEX
#undef AU_AO
#undef AU_WRITE_MIDI
#undef AU_LIST
#undef AU_MODMIDI
#undef AU_VOLUME_CALC
#undef AU_BENCHMARK
#undef VST_LOADER_ENABLE
#undef VSTWRAP_EXT
#undef SMFCONV
#undef SUPPORT_SOCKET
#undef SUPPORT_SOUNDSPEC
#undef W32SOUNDSPEC
#undef __W32G__	/* for Win32 GUI */
#endif

/* Win32GUI Standalone */
#if defined(__W32G__) && !TWSYNG32 && !KBTIM
#define DEFAULT_AUDIO_BUFFER_BITS    12 /* 4096sample 92ms (44.1kHz) */
#define DEFAULT_AUDIO_BUFFER_NUM     17 /* 1500ms over */
/* #undef AU_NPIPE */
#undef AU_WRITE_MIDI
/* #undef AU_LIST */
#undef AU_MODMIDI
#define FORCE_TIME_PERIOD 1
#endif

/* Win32 Console */
#if defined(__W32__) && !defined(__W32G__) && !TWSYNSRV
#define TIM_CUI 1
#define DEFAULT_AUDIO_BUFFER_BITS    9 /* 512sample 11ms (44.1kHz) */
#define DEFAULT_AUDIO_BUFFER_NUM     65 /* 750ms over */
#define AU_VOLUME_CALC 1
#undef ANOTHER_MAIN
#undef AU_WRITE_MIDI
/* #undef AU_LIST */
/* #undef AU_MODMIDI */
#undef IA_W32G_SYN	/* for Win32 GUI */
#define FORCE_TIME_PERIOD 1
#endif

/* Windows x64 */
#if defined(WIN64) || defined(_WIN64)
#ifndef _AMD64_
#define _AMD64_ 1
#endif

/* #undef AU_NPIPE */
/* #undef AU_VORBIS */
/* #undef AU_VORBIS_DLL */
#undef AU_GOGO
#undef AU_GOGO_DLL
/* #undef AU_PORTAUDIO */
/* #undef AU_PORTAUDIO_DLL */
#undef AU_LAME
/* #undef AU_FLAC */
/* #undef AU_FLAC_DLL */
/* #undef AU_OGGFLAC */
/* #undef AU_OGGFLAC_DLL */
#undef AU_OPUS
#undef AU_OPUS_DLL
#undef AU_SPEEX
#undef AU_AO
#undef VST_LOADER_ENABLE
#undef VSTWRAP_EXT
#undef SMFCONV
#endif

/* cfgforsf */
#ifdef CFG_FOR_SF
#undef AU_NPIPE
#undef AU_VORBIS
#undef AU_VORBIS_DLL
#undef AU_GOGO
#undef AU_GOGO_DLL
#undef AU_PORTAUDIO
#undef AU_PORTAUDIO_DLL
#undef AU_LAME
#undef AU_FLAC
#undef AU_FLAC_DLL
#undef AU_OGGFLAC
#undef AU_OGGFLAC_DLL
#undef AU_OPUS
#undef AU_OPUS_DLL
#undef AU_SPEEX
#undef AU_AO
#undef AU_WRITE_MIDI
#undef AU_LIST
#undef AU_MODMIDI
#undef AU_VOLUME_CALC
#undef AU_BENCHMARK
#undef VST_LOADER_ENABLE
#undef VSTWRAP_EXT
#undef SMFCONV
#undef SUPPORT_SOCKET
#undef SUPPORT_SOUNDSPEC
#undef W32SOUNDSPEC
#undef __W32G__
#undef HAVE_POPEN
#endif

/* KbTim and in_timidity */
#ifdef KBTIM /*added by Kobarin*/
#include "kbtim/kbtim_config.h"
#define KBTIM32 1

#undef AU_NPIPE
#undef AU_VORBIS
#undef AU_VORBIS_DLL
#undef AU_GOGO
#undef AU_GOGO_DLL
#undef AU_PORTAUDIO
#undef AU_PORTAUDIO_DLL
#undef AU_LAME
#undef AU_FLAC
#undef AU_FLAC_DLL
#undef AU_OGGFLAC
#undef AU_OGGFLAC_DLL
#undef AU_OPUS
#undef AU_OPUS_DLL
#undef AU_SPEEX
#undef AU_AO
#undef AU_VOLUME_CALC
#undef AU_BENCHMARK
#undef VST_LOADER_ENABLE
#undef VSTWRAP_EXT
#ifndef IN_TIMIDITY
#undef SMFCONV
#endif
#undef SUPPORT_SOCKET
#undef SUPPORT_SOUNDSPEC
#undef W32SOUNDSPEC
#undef HAVE_POPEN
#endif

#endif /* !CYGWIN_CONFIG_HEADER */

