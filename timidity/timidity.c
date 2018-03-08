/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2008 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#endif /* STDC_HEADERS */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif /* !NO_STRING_H */
#ifdef __W32__
#include <windows.h>
#include <io.h>
#include <shlobj.h>
#endif /* __W32__ */
#ifdef __MINGW32__
#define _BSD_SOURCE 1
#endif
#include "tmdy_getopt.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* for open */
#endif /* HAVE_FCNTL_H */

#ifdef BORLANDC_EXCEPTION
#include <excpt.h>
#endif /* BORLANDC_EXCEPTION */
#include <signal.h>

#if defined(__FreeBSD__) && !defined(__alpha__)
#include <floatingpoint.h> /* For FP exceptions */
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <ieeefp.h> /* For FP exceptions */
#endif

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "miditrace.h"
#include "effect.h"
#include "freq.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "resample.h"
#include "recache.h"
#include "arc.h"
#include "strtab.h"
#include "wrd.h"
#define DEFINE_GLOBALS
#include "mid.defs"
#include "aq.h"
#include "mix.h"
#include "unimod.h"
#include "quantity.h"
#include "rtsyn.h"
#include "sndfontini.h"
#include "thread.h"
#include "miditrace.h"
#include "flac_a.h"
#include "sfz.h"
///r
#ifdef __BORLANDC__
#define inline
#endif

#ifdef IA_W32GUI
#include "w32g.h"
#include "w32g_subwin.h"
#include "w32g_utl.h"
#endif

#ifdef WINDRV
/* supress std outputs */
#define fputs(a, b)
#endif

#ifndef __GNUC__
#define __attribute__(x) /* ignore */
#endif

///r
#ifdef AU_W32
#include "w32_a.h"
#endif
#ifdef AU_WDMKS
#include "wdmks_a.h"
#endif
#ifdef AU_WASAPI
#include "wasapi_a.h"
#endif

#ifdef AU_PORTAUDIO
#include "portaudio_a.h"
#endif

#ifdef __W32G__
#include "w32g_utl.h"
#endif


uint8 opt_normal_chorus_plus = 5; // chorusEX

extern DWORD processPriority;
DWORD processPriority = NORMAL_PRIORITY_CLASS;	// プロセスのプライオリティ
#if defined(IA_W32G_SYN) || defined(WINDRV)
DWORD syn_ThreadPriority = THREAD_PRIORITY_NORMAL;
#else
int GUIThreadPriority = THREAD_PRIORITY_NORMAL;
#endif
int PlayerThreadPriority = THREAD_PRIORITY_NORMAL;

/* option enums */
enum {
	TIM_OPT_FIRST = 256,
	/* first entry */
	TIM_OPT_VOLUME = TIM_OPT_FIRST,
	TIM_OPT_MASTER_VOLUME,
	TIM_OPT_DRUM_POWER,
	TIM_OPT_VOLUME_COMP,
	TIM_OPT_ANTI_ALIAS,
	TIM_OPT_BUFFER_FRAGS,
	TIM_OPT_CONTROL_RATIO,
	TIM_OPT_CONFIG_FILE,
	TIM_OPT_DRUM_CHANNEL,
	TIM_OPT_IFACE_PATH,
	TIM_OPT_EXT,
	TIM_OPT_MOD_WHEEL,
	TIM_OPT_PORTAMENTO,
	TIM_OPT_VIBRATO,
	TIM_OPT_CH_PRESS,
	TIM_OPT_MOD_ENV,
	TIM_OPT_TRACE_TEXT,
	TIM_OPT_OVERLAP,
///r
	TIM_OPT_OVERLAP_COUNT,
	TIM_OPT_MAX_CHANNEL_VOICES,

	TIM_OPT_TEMPER_CTRL,
	TIM_OPT_DEFAULT_MID,
	TIM_OPT_SYSTEM_MID,
	TIM_OPT_DEFAULT_BANK,
	TIM_OPT_FORCE_BANK,
	TIM_OPT_DEFAULT_PGM,
	TIM_OPT_FORCE_PGM,
	TIM_OPT_DELAY,
	TIM_OPT_CHORUS,
	TIM_OPT_REVERB,
	TIM_OPT_VOICE_LPF,
	TIM_OPT_VOICE_HPF,
	TIM_OPT_NS,
	TIM_OPT_RESAMPLE,
///r
	TIM_OPT_RESAMPLE_FILTER,
	TIM_OPT_RESAMPLE_OVER_SAMPLING,
	TIM_OPT_PRE_RESAMPLE,
	TIM_OPT_EVIL,
	TIM_OPT_FAST_PAN,
	TIM_OPT_FAST_DECAY,
	TIM_OPT_SEGMENT,
	TIM_OPT_SPECTROGRAM,
	TIM_OPT_KEYSIG,
	TIM_OPT_HELP,
	TIM_OPT_INTERFACE,
	TIM_OPT_VERBOSE,
	TIM_OPT_QUIET,
	TIM_OPT_TRACE,
	TIM_OPT_LOOP,
	TIM_OPT_RANDOM,
	TIM_OPT_SORT,
	TIM_OPT_BACKGROUND,
	TIM_OPT_RT_PRIO,
	TIM_OPT_SEQ_PORTS,
	TIM_OPT_RTSYN_LATENCY,
	TIM_OPT_RTSYN_SKIP_AQ,
	TIM_OPT_REALTIME_LOAD,
	TIM_OPT_ADJUST_KEY,
	TIM_OPT_VOICE_QUEUE,
///r
	TIM_OPT_RESAMPLE_QUEUE,
	TIM_OPT_POLYPHONY_QUEUE,
	TIM_OPT_ADD_PLAY_TIME,
	TIM_OPT_ADD_SILENT_TIME,
	TIM_OPT_EMU_DELAY_TIME,
	TIM_OPT_PROCESS_PRIORITY,
	TIM_OPT_PLAYER_THREAD_PRIORITY,

	TIM_OPT_OD_LEVEL_GS,
	TIM_OPT_OD_DRIVE_GS,
	TIM_OPT_OD_LEVEL_XG,
	TIM_OPT_OD_DRIVE_XG,

	TIM_OPT_PATCH_PATH,
	TIM_OPT_PCM_FILE,
	TIM_OPT_DECAY_TIME,
	TIM_OPT_INTERPOLATION,
	TIM_OPT_OUTPUT_MODE,
	TIM_OPT_OUTPUT_STEREO,
	TIM_OPT_OUTPUT_SIGNED,
	TIM_OPT_OUTPUT_BITWIDTH,
	TIM_OPT_OUTPUT_FORMAT,
	TIM_OPT_OUTPUT_SWAB,
///r
	TIM_OPT_OUTPUT_DEVICE_ID,
	TIM_OPT_WMME_DEVICE_ID,
	TIM_OPT_WAVE_FORMAT_EXT,
	TIM_OPT_WDMKS_DEVICE_ID,
	TIM_OPT_WDMKS_LATENCY,
	TIM_OPT_WDMKS_FORMAT_EXT,
	TIM_OPT_WDMKS_POLLING,
	TIM_OPT_WDMKS_THREAD_PRIORITY,
	TIM_OPT_WDMKS_RT_PRIORITY,
	TIM_OPT_WDMKS_PIN_PRIORITY,
	TIM_OPT_WASAPI_DEVICE_ID,
	TIM_OPT_WASAPI_LATENCY,
	TIM_OPT_WASAPI_FORMAT_EXT,
	TIM_OPT_WASAPI_EXCLUSIVE,
	TIM_OPT_WASAPI_POLLING,
	TIM_OPT_WASAPI_PRIORITY,
	TIM_OPT_WASAPI_STREAM_CATEGORY,
	TIM_OPT_WASAPI_STREAM_OPTION,
	TIM_OPT_PA_WMME_DEVICE_ID,
	TIM_OPT_PA_DS_DEVICE_ID,
	TIM_OPT_PA_ASIO_DEVICE_ID,
	TIM_OPT_PA_WDMKS_DEVICE_ID,
	TIM_OPT_PA_WASAPI_DEVICE_ID,
	TIM_OPT_PA_WASAPI_FLAG,
	TIM_OPT_PA_WASAPI_STREAM_CATEGORY,
	TIM_OPT_PA_WASAPI_STREAM_OPTION,
	
	TIM_OPT_WAVE_EXTENSIBLE,
	TIM_OPT_WAVE_UPDATE_STEP,
	TIM_OPT_FLAC_VERIFY,
	TIM_OPT_FLAC_PADDING,
	TIM_OPT_FLAC_COMPLEVEL,
	TIM_OPT_FLAC_OGGFLAC,
	TIM_OPT_OPUS_NFRAMES,
	TIM_OPT_OPUS_BITRATE,
	TIM_OPT_OPUS_COMPLEXITY,
	TIM_OPT_OPUS_VBR,
	TIM_OPT_OPUS_CVBR,
	TIM_OPT_SPEEX_QUALITY,
	TIM_OPT_SPEEX_VBR,
	TIM_OPT_SPEEX_ABR,
	TIM_OPT_SPEEX_VAD,
	TIM_OPT_SPEEX_DTX,
	TIM_OPT_SPEEX_COMPLEXITY,
	TIM_OPT_SPEEX_NFRAMES,
	TIM_OPT_OUTPUT_FILE,
	TIM_OPT_PATCH_FILE,
	TIM_OPT_POLYPHONY,
	TIM_OPT_POLY_REDUCE,
	TIM_OPT_MUTE,
	TIM_OPT_TEMPER_MUTE,
	TIM_OPT_PRESERVE_SILENCE,
	TIM_OPT_AUDIO_BUFFER,
///r
	TIM_OPT_COMPUTE_BUFFER,
	TIM_OPT_MIX_ENV,
	TIM_OPT_MOD_UPDATE,
	TIM_OPT_CUT_SHORT,
	TIM_OPT_LIMITER,
	TIM_OPT_COMPUTE_THREAD_NUM,
	TIM_OPT_TRACE_MODE_UPDATE,	
	TIM_OPT_LOAD_ALL_INSTRUMENT,
	TIM_OPT_LOOP_REPEAT,

	TIM_OPT_CACHE_SIZE,
	TIM_OPT_SAMPLE_FREQ,
	TIM_OPT_ADJUST_TEMPO,
	TIM_OPT_CHARSET,
	TIM_OPT_UNLOAD_INST,
	TIM_OPT_VOLUME_CALC_RMS,
	TIM_OPT_VOLUME_CALC_TRIM,
	TIM_OPT_VOLUME_CURVE,
	TIM_OPT_VERSION,
	TIM_OPT_WRD,
	TIM_OPT_RCPCV_DLL,
	TIM_OPT_CONFIG_STR,
	TIM_OPT_FREQ_TABLE,
	TIM_OPT_PURE_INT,
	TIM_OPT_MODULE,
	TIM_OPT_DUMMY_SETTING,
	TIM_OPT_INT_SYNTH_RATE,
	TIM_OPT_INT_SYNTH_SINE,
	TIM_OPT_INT_SYNTH_UPDATE,
	/* last entry */
	TIM_OPT_LAST = TIM_OPT_MODULE,
};

#ifdef IA_WINSYN
const char *optcommands =
#else
static const char *optcommands =
#endif
///r
//		"4A:aB:b:C:c:D:d:E:eFfg:H:hI:i:jK:k:L:M:m:N:"
//		"O:o:P:p:Q:q:R:S:s:T:t:UV:vW:"
		"4A:aB:b:C:c:D:d:E:eF:fg:H:hI:i:jK:k:L:lM:m:N:n"
		"O:o:P:p:Q:q:R:rS:s:T:t:UV:vW:"
#ifdef __W32__
		"w:"
#endif
		"xY:Z:";		/* Only GJluXyz are remain... */
#ifdef IA_WINSYN
const struct option longopts[] = {
#else
static const struct option longopts[] = {
#endif
	{ "volume",                 required_argument, NULL, TIM_OPT_VOLUME },
	{ "master-volume",          required_argument, NULL, TIM_OPT_MASTER_VOLUME },
	{ "drum-power",             required_argument, NULL, TIM_OPT_DRUM_POWER },
	{ "no-volume-compensation", optional_argument, NULL, TIM_OPT_VOLUME_COMP },
	{ "volume-compensation",    optional_argument, NULL, TIM_OPT_VOLUME_COMP },
	{ "no-anti-alias",          no_argument,       NULL, TIM_OPT_ANTI_ALIAS },
	{ "anti-alias",             optional_argument, NULL, TIM_OPT_ANTI_ALIAS },
	{ "buffer-fragments",       required_argument, NULL, TIM_OPT_BUFFER_FRAGS },
	{ "control-ratio",          required_argument, NULL, TIM_OPT_CONTROL_RATIO },
	{ "config-file",            required_argument, NULL, TIM_OPT_CONFIG_FILE },
	{ "drum-channel",           required_argument, NULL, TIM_OPT_DRUM_CHANNEL },
	{ "interface-path",         required_argument, NULL, TIM_OPT_IFACE_PATH },
	{ "ext",                    required_argument, NULL, TIM_OPT_EXT },
	{ "no-mod-wheel",           no_argument,       NULL, TIM_OPT_MOD_WHEEL },
	{ "mod-wheel",              optional_argument, NULL, TIM_OPT_MOD_WHEEL },
	{ "no-portamento",          no_argument,       NULL, TIM_OPT_PORTAMENTO },
	{ "portamento",             optional_argument, NULL, TIM_OPT_PORTAMENTO },
	{ "no-vibrato",             no_argument,       NULL, TIM_OPT_VIBRATO },
	{ "vibrato",                optional_argument, NULL, TIM_OPT_VIBRATO },
	{ "no-ch-pressure",         no_argument,       NULL, TIM_OPT_CH_PRESS },
	{ "ch-pressure",            optional_argument, NULL, TIM_OPT_CH_PRESS },
	{ "no-mod-envelope",        no_argument,       NULL, TIM_OPT_MOD_ENV },
	{ "mod-envelope",           optional_argument, NULL, TIM_OPT_MOD_ENV },
	{ "no-trace-text-meta",     no_argument,       NULL, TIM_OPT_TRACE_TEXT },
	{ "trace-text-meta",        optional_argument, NULL, TIM_OPT_TRACE_TEXT },
	{ "no-overlap-voice",       no_argument,       NULL, TIM_OPT_OVERLAP },
	{ "overlap-voice",          optional_argument, NULL, TIM_OPT_OVERLAP },
///r
	{ "overlap-voice-count",    required_argument, NULL, TIM_OPT_OVERLAP_COUNT },
	{ "max-channel-voices",     required_argument, NULL, TIM_OPT_MAX_CHANNEL_VOICES },

	{ "no-temper-control",      no_argument,       NULL, TIM_OPT_TEMPER_CTRL },
	{ "temper-control",         optional_argument, NULL, TIM_OPT_TEMPER_CTRL },
	{ "default-mid",            required_argument, NULL, TIM_OPT_DEFAULT_MID },
	{ "system-mid",             required_argument, NULL, TIM_OPT_SYSTEM_MID },
	{ "default-bank",           required_argument, NULL, TIM_OPT_DEFAULT_BANK },
	{ "force-bank",             required_argument, NULL, TIM_OPT_FORCE_BANK },
	{ "default-program",        required_argument, NULL, TIM_OPT_DEFAULT_PGM },
	{ "force-program",          required_argument, NULL, TIM_OPT_FORCE_PGM },
	{ "delay",                  required_argument, NULL, TIM_OPT_DELAY },
	{ "chorus",                 required_argument, NULL, TIM_OPT_CHORUS },
	{ "reverb",                 required_argument, NULL, TIM_OPT_REVERB },
	{ "voice-lpf",              required_argument, NULL, TIM_OPT_VOICE_LPF },
	{ "voice-hpf",              required_argument, NULL, TIM_OPT_VOICE_HPF },
	{ "noise-shaping",          required_argument, NULL, TIM_OPT_NS },
#ifndef FIXED_RESAMPLATION
	{ "resample",               required_argument, NULL, TIM_OPT_RESAMPLE },
#endif
	{ "resample-filter",        required_argument, NULL, TIM_OPT_RESAMPLE_FILTER },
	{ "resample-over-sampling", required_argument, NULL, TIM_OPT_RESAMPLE_OVER_SAMPLING },
	{ "pre-resample",           required_argument, NULL, TIM_OPT_PRE_RESAMPLE },
	{ "evil",                   no_argument,       NULL, TIM_OPT_EVIL },
	{ "no-fast-panning",        no_argument,       NULL, TIM_OPT_FAST_PAN },
	{ "fast-panning",           optional_argument, NULL, TIM_OPT_FAST_PAN },
	{ "no-fast-decay",          no_argument,       NULL, TIM_OPT_FAST_DECAY },
	{ "fast-decay",             optional_argument, NULL, TIM_OPT_FAST_DECAY },
	{ "segment",                required_argument, NULL, TIM_OPT_SEGMENT },
	{ "spectrogram",            required_argument, NULL, TIM_OPT_SPECTROGRAM },
	{ "force-keysig",           required_argument, NULL, TIM_OPT_KEYSIG },
	{ "help",                   optional_argument, NULL, TIM_OPT_HELP },
	{ "interface",              required_argument, NULL, TIM_OPT_INTERFACE },
	{ "verbose",                optional_argument, NULL, TIM_OPT_VERBOSE },
	{ "quiet",                  optional_argument, NULL, TIM_OPT_QUIET },
	{ "no-trace",               no_argument,       NULL, TIM_OPT_TRACE },
	{ "trace",                  optional_argument, NULL, TIM_OPT_TRACE },
	{ "no-loop",                no_argument,       NULL, TIM_OPT_LOOP },
	{ "loop",                   optional_argument, NULL, TIM_OPT_LOOP },
	{ "no-random",              no_argument,       NULL, TIM_OPT_RANDOM },
	{ "random",                 optional_argument, NULL, TIM_OPT_RANDOM },
	{ "no-sort",                no_argument,       NULL, TIM_OPT_SORT },
	{ "sort",                   optional_argument, NULL, TIM_OPT_SORT },
#ifdef IA_ALSASEQ
	{ "no-background",          no_argument,       NULL, TIM_OPT_BACKGROUND },
	{ "background",             optional_argument, NULL, TIM_OPT_BACKGROUND },
	{ "realtime-priority",      required_argument, NULL, TIM_OPT_RT_PRIO },
	{ "sequencer-ports",        required_argument, NULL, TIM_OPT_SEQ_PORTS },
#endif
	{ "no-realtime-load",       no_argument,       NULL, TIM_OPT_REALTIME_LOAD },
	{ "realtime-load",          optional_argument, NULL, TIM_OPT_REALTIME_LOAD },
	{ "adjust-key",             required_argument, NULL, TIM_OPT_ADJUST_KEY },
	{ "voice-queue",            required_argument, NULL, TIM_OPT_VOICE_QUEUE },
///r
	{ "resample-queue",         required_argument, NULL, TIM_OPT_RESAMPLE_QUEUE },
	{ "polyphony-queue",        required_argument, NULL, TIM_OPT_POLYPHONY_QUEUE },
	{ "add-play-time",			required_argument, NULL, TIM_OPT_ADD_PLAY_TIME },
	{ "add-silent-time",		required_argument, NULL, TIM_OPT_ADD_SILENT_TIME },
	{ "emu-delay-time",			required_argument, NULL, TIM_OPT_EMU_DELAY_TIME },
#if defined(__W32__)
	{ "process-priority",		required_argument, NULL, TIM_OPT_PROCESS_PRIORITY },
#if !(defined(IA_W32G_SYN) || defined(WINDRV))
	{ "player-thread-priority",	required_argument, NULL, TIM_OPT_PLAYER_THREAD_PRIORITY },
#endif
#endif
	{ "od-level-gs",            required_argument, NULL, TIM_OPT_OD_LEVEL_GS },
	{ "od-drive-gs",            required_argument, NULL, TIM_OPT_OD_DRIVE_GS },
	{ "od-level-xg",            required_argument, NULL, TIM_OPT_OD_LEVEL_XG },
	{ "od-drive-xg",            required_argument, NULL, TIM_OPT_OD_DRIVE_XG },
	{ "patch-path",             required_argument, NULL, TIM_OPT_PATCH_PATH },
	{ "pcm-file",               required_argument, NULL, TIM_OPT_PCM_FILE },
	{ "decay-time",             required_argument, NULL, TIM_OPT_DECAY_TIME },
	{ "interpolation",          required_argument, NULL, TIM_OPT_INTERPOLATION },
	{ "output-mode",            required_argument, NULL, TIM_OPT_OUTPUT_MODE },
	{ "output-stereo",          no_argument,       NULL, TIM_OPT_OUTPUT_STEREO },
	{ "output-mono",            no_argument,       NULL, TIM_OPT_OUTPUT_STEREO },
	{ "output-signed",          no_argument,       NULL, TIM_OPT_OUTPUT_SIGNED },
	{ "output-unsigned",        no_argument,       NULL, TIM_OPT_OUTPUT_SIGNED },
	{ "output-16bit",           no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-24bit",           no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
///r
	{ "output-32bit",           no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-f32bit",          no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-float32bit",      no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-64bit",           no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-f64bit",          no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-float64bit",      no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },

	{ "output-8bit",            no_argument,       NULL, TIM_OPT_OUTPUT_BITWIDTH },
	{ "output-linear",          no_argument,       NULL, TIM_OPT_OUTPUT_FORMAT },
	{ "output-ulaw",            no_argument,       NULL, TIM_OPT_OUTPUT_FORMAT },
	{ "output-alaw",            no_argument,       NULL, TIM_OPT_OUTPUT_FORMAT },
	{ "no-output-swab",         no_argument,       NULL, TIM_OPT_OUTPUT_SWAB },
	{ "output-swab",            optional_argument, NULL, TIM_OPT_OUTPUT_SWAB },
#if defined(IA_WINSYN) || defined(IA_W32G_SYN) || defined(IA_W32GUI)
	{ "rtsyn-latency",          required_argument, NULL, TIM_OPT_RTSYN_LATENCY },
	{ "rtsyn-skip-aq",          required_argument, NULL, TIM_OPT_RTSYN_SKIP_AQ },
#endif
///r
	{ "output-device-id",		required_argument, NULL, TIM_OPT_OUTPUT_DEVICE_ID },
#ifdef AU_W32
	{ "wmme-device-id",			required_argument, NULL, TIM_OPT_WMME_DEVICE_ID },
	{ "wave-format-ext",		required_argument, NULL, TIM_OPT_WAVE_FORMAT_EXT },
	{ "wmme-format-ext",		required_argument, NULL, TIM_OPT_WAVE_FORMAT_EXT },
#endif
#ifdef AU_WDMKS
	{ "wdmks-device-id",		required_argument, NULL, TIM_OPT_WDMKS_DEVICE_ID },
	{ "wdmks-latency",			required_argument, NULL, TIM_OPT_WDMKS_LATENCY },
	{ "wdmks-format-ext",		required_argument, NULL, TIM_OPT_WDMKS_FORMAT_EXT },
	{ "wdmks-polling",			required_argument, NULL, TIM_OPT_WDMKS_POLLING },
	{ "wdmks-thread-priority",	required_argument, NULL, TIM_OPT_WDMKS_THREAD_PRIORITY },
	{ "wdmks-rt-priority",		required_argument, NULL, TIM_OPT_WDMKS_RT_PRIORITY },
	{ "wdmks-pin-priority",		required_argument, NULL, TIM_OPT_WDMKS_PIN_PRIORITY },
#endif
#ifdef AU_WASAPI
	{ "wasapi-device-id",		required_argument, NULL, TIM_OPT_WASAPI_DEVICE_ID },
	{ "wasapi-latency",			required_argument, NULL, TIM_OPT_WASAPI_LATENCY },
	{ "wasapi-format-ext",		required_argument, NULL, TIM_OPT_WASAPI_FORMAT_EXT },
	{ "wasapi-exclusive",		required_argument, NULL, TIM_OPT_WASAPI_EXCLUSIVE },
	{ "wasapi-polling",			required_argument, NULL, TIM_OPT_WASAPI_POLLING },
	{ "wasapi-priority",		required_argument, NULL, TIM_OPT_WASAPI_PRIORITY },
	{ "wasapi-stream-category",		required_argument, NULL, TIM_OPT_WASAPI_STREAM_CATEGORY },
	{ "wasapi-stream-option",		required_argument, NULL, TIM_OPT_WASAPI_STREAM_OPTION },
#endif
#ifdef AU_PORTAUDIO	
	{ "pa-wmme-device-id",		required_argument, NULL, TIM_OPT_PA_WMME_DEVICE_ID },
	{ "pa-ds-device-id",		required_argument, NULL, TIM_OPT_PA_DS_DEVICE_ID },
	{ "pa-asio-device-id",		required_argument, NULL, TIM_OPT_PA_ASIO_DEVICE_ID },
#ifdef PORTAUDIO_V19
	{ "pa-wdmks-device-id",		required_argument, NULL, TIM_OPT_PA_WDMKS_DEVICE_ID },
	{ "pa-wasapi-device-id",	required_argument, NULL, TIM_OPT_PA_WASAPI_DEVICE_ID },
	{ "pa-wasapi-flag",			required_argument, NULL, TIM_OPT_PA_WASAPI_FLAG },
	{ "pa-wasapi-stream-category",required_argument, NULL, TIM_OPT_PA_WASAPI_STREAM_CATEGORY },
	{ "pa-wasapi-stream-option",required_argument, NULL, TIM_OPT_PA_WASAPI_STREAM_OPTION },
#endif
#endif
	{ "wave-extensible",        no_argument,       NULL, TIM_OPT_WAVE_EXTENSIBLE },
	{ "wave-update-step",       optional_argument, NULL, TIM_OPT_WAVE_UPDATE_STEP },
#ifdef AU_FLAC
	{ "flac-verify",            no_argument,       NULL, TIM_OPT_FLAC_VERIFY },
	{ "flac-padding",           required_argument, NULL, TIM_OPT_FLAC_PADDING },
	{ "flac-complevel",         required_argument, NULL, TIM_OPT_FLAC_COMPLEVEL },
#ifdef AU_OGGFLAC
	{ "oggflac",                no_argument,       NULL, TIM_OPT_FLAC_OGGFLAC },
#endif /* AU_OGGFLAC */
#endif /* AU_FLAC */
#ifdef AU_OPUS
	{ "opus-nframes",           required_argument, NULL, TIM_OPT_OPUS_NFRAMES },
	{ "opus-bitrate",           required_argument, NULL, TIM_OPT_OPUS_BITRATE },
	{ "opus-complexity",        required_argument, NULL, TIM_OPT_OPUS_COMPLEXITY },
	{ "opus-vbr",               no_argument,       NULL, TIM_OPT_OPUS_VBR },
	{ "opus-cvbr",              no_argument,       NULL, TIM_OPT_OPUS_CVBR },
#endif /* AU_OPUS */
#ifdef AU_SPEEX
	{ "speex-quality",          required_argument, NULL, TIM_OPT_SPEEX_QUALITY },
	{ "speex-vbr",              no_argument,       NULL, TIM_OPT_SPEEX_VBR },
	{ "speex-abr",              required_argument, NULL, TIM_OPT_SPEEX_ABR },
	{ "speex-vad",              no_argument,       NULL, TIM_OPT_SPEEX_VAD },
	{ "speex-dtx",              no_argument,       NULL, TIM_OPT_SPEEX_DTX },
	{ "speex-complexity",       required_argument, NULL, TIM_OPT_SPEEX_COMPLEXITY },
	{ "speex-nframes",          required_argument, NULL, TIM_OPT_SPEEX_NFRAMES },
#endif /* AU_SPEEX */
	{ "output-file",            required_argument, NULL, TIM_OPT_OUTPUT_FILE },
	{ "patch-file",             required_argument, NULL, TIM_OPT_PATCH_FILE },
	{ "polyphony",              required_argument, NULL, TIM_OPT_POLYPHONY },
	{ "no-polyphony-reduction", no_argument,       NULL, TIM_OPT_POLY_REDUCE },
	{ "polyphony-reduction",    optional_argument, NULL, TIM_OPT_POLY_REDUCE },
	{ "mute",                   required_argument, NULL, TIM_OPT_MUTE },
	{ "temper-mute",            required_argument, NULL, TIM_OPT_TEMPER_MUTE },
	{ "preserve-silence",       no_argument,       NULL, TIM_OPT_PRESERVE_SILENCE },
	{ "audio-buffer",           required_argument, NULL, TIM_OPT_AUDIO_BUFFER },
///r
	{ "compute-buffer",         required_argument, NULL, TIM_OPT_COMPUTE_BUFFER },
	{ "mix-envelope",           required_argument, NULL, TIM_OPT_MIX_ENV },
	{ "modulation-update",      required_argument, NULL, TIM_OPT_MOD_UPDATE },
	{ "cut-short-time",         required_argument, NULL, TIM_OPT_CUT_SHORT },
	{ "limiter",                required_argument, NULL, TIM_OPT_LIMITER },
	{ "compute-thread-num",     required_argument, NULL, TIM_OPT_COMPUTE_THREAD_NUM },
	{ "trace-mode-update-time", required_argument, NULL, TIM_OPT_TRACE_MODE_UPDATE },	
	{ "load-all-instrument",    required_argument, NULL, TIM_OPT_LOAD_ALL_INSTRUMENT },	
	{ "loop-repeat",            required_argument, NULL, TIM_OPT_LOOP_REPEAT },

	{ "cache-size",             required_argument, NULL, TIM_OPT_CACHE_SIZE },
	{ "sampling-freq",          required_argument, NULL, TIM_OPT_SAMPLE_FREQ },
	{ "adjust-tempo",           required_argument, NULL, TIM_OPT_ADJUST_TEMPO },
	{ "output-charset",         required_argument, NULL, TIM_OPT_CHARSET },
	{ "no-unload-instruments",  no_argument,       NULL, TIM_OPT_UNLOAD_INST },
	{ "unload-instruments",     optional_argument, NULL, TIM_OPT_UNLOAD_INST },
#if defined(AU_VOLUME_CALC)
	{ "volume-calc-rms",        no_argument,       NULL, TIM_OPT_VOLUME_CALC_RMS },
	{ "volume-calc-trim",       no_argument,       NULL, TIM_OPT_VOLUME_CALC_TRIM },
#endif /* AU_VOLUME_CALC */
	{ "volume-curve",           required_argument, NULL, TIM_OPT_VOLUME_CURVE },
	{ "version",                no_argument,       NULL, TIM_OPT_VERSION },
	{ "wrd",                    required_argument, NULL, TIM_OPT_WRD },
#ifdef __W32__
	{ "rcpcv-dll",              required_argument, NULL, TIM_OPT_RCPCV_DLL },
#endif
	{ "config-string",          required_argument, NULL, TIM_OPT_CONFIG_STR },
	{ "freq-table",             required_argument, NULL, TIM_OPT_FREQ_TABLE },
	{ "pure-intonation",        optional_argument, NULL, TIM_OPT_PURE_INT },
	{ "module",                 required_argument, NULL, TIM_OPT_MODULE },
	{ "disable-chorus-plus",    no_argument,       NULL, TIM_OPT_DUMMY_SETTING},
	
	{ "int-synth-rate",         required_argument, NULL, TIM_OPT_INT_SYNTH_RATE },
	{ "int-synth-sine",         required_argument, NULL, TIM_OPT_INT_SYNTH_SINE },
	{ "int-synth-update",       required_argument, NULL, TIM_OPT_INT_SYNTH_UPDATE },

	{ NULL,                     no_argument,       NULL, '\0'     }
};
#define INTERACTIVE_INTERFACE_IDS "kmqagrwAWNP"



/* main interfaces (To be used another main) */
#if defined(main) || defined(ANOTHER_MAIN) || defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
#define MAIN_INTERFACE
#else
#define MAIN_INTERFACE static
#endif /* main */

MAIN_INTERFACE void timidity_start_initialize(void);
MAIN_INTERFACE int timidity_pre_load_configuration(void);
MAIN_INTERFACE int timidity_post_load_configuration(void);
MAIN_INTERFACE void timidity_init_player(void);
MAIN_INTERFACE int timidity_play_main(int nfiles, char **files);
MAIN_INTERFACE int got_a_configuration;
char *wrdt_open_opts = NULL;
char *opt_aq_max_buff = NULL,
     *opt_aq_fill_buff = NULL;
void timidity_init_aq_buff(void);
int opt_control_ratio = 0; /* Save -C option */
///r
char *opt_reduce_voice_threshold = NULL;
char *opt_reduce_quality_threshold = NULL;
char *opt_reduce_polyphony_threshold = NULL;


int set_extension_modes(char *);
int set_ctl(char *);
int set_play_mode(char *);
int set_wrd(char *);
MAIN_INTERFACE int set_tim_opt_short(int c, const char *);
MAIN_INTERFACE int set_tim_opt_long(int, const char *, int);
static inline int parse_opt_A(const char *);
static inline int parse_opt_master_volume(const char *arg);
static inline int parse_opt_drum_power(const char *);
static inline int parse_opt_volume_comp(const char *);
static inline int parse_opt_a(const char *);
static inline int parse_opt_B(const char *);
static inline int parse_opt_C(const char *);
static inline int parse_opt_c(const char *);
static inline int parse_opt_D(const char *);
static inline int parse_opt_d(const char *);
static inline int parse_opt_E(const char *);
static inline int parse_opt_mod_wheel(const char *);
static inline int parse_opt_portamento(const char *);
static inline int parse_opt_vibrato(const char *);
static inline int parse_opt_ch_pressure(const char *);
static inline int parse_opt_mod_env(const char *);
static inline int parse_opt_trace_text(const char *);
static inline int parse_opt_overlap_voice(const char *);
///r
static inline int parse_opt_overlap_voice_count(const char *);
static inline int parse_opt_max_channel_voices(const char *);

static inline int parse_opt_temper_control(const char *);
static inline int parse_opt_default_mid(const char *);
static inline int parse_opt_system_mid(const char *);
static inline int parse_opt_default_bank(const char *);
static inline int parse_opt_force_bank(const char *);
static inline int parse_opt_default_program(const char *);
static inline int parse_opt_force_program(const char *);
static inline int set_default_program(int);
static inline int parse_opt_delay(const char *arg);
static inline int parse_opt_chorus(const char *);
static inline int parse_opt_reverb(const char *);
static int parse_opt_reverb_freeverb(const char *arg, char type);
static inline int parse_opt_voice_lpf(const char *);
static inline int parse_opt_voice_hpf(const char *);
static inline int parse_opt_noise_shaping(const char *);
static inline int parse_opt_resample(const char *);
static inline int parse_opt_e(const char *);
static inline int parse_opt_F(const char *);
static inline int parse_opt_f(const char *);
static inline int parse_opt_G(const char*);
static inline int parse_opt_G1(const char*);
static int parse_segment(TimeSegment*, const char*);
static int parse_segment2(TimeSegment*, const char*);
static int parse_time(FLOAT_T*, const char*);
static int parse_time2(Measure*, const char*);
static inline int parse_opt_g(const char *);
static inline int parse_opt_H(const char *);
__attribute__((noreturn))
static inline int parse_opt_h(const char *);
#ifdef IA_DYNAMIC
static inline void list_dyna_interface(FILE *, char *, char *);
ControlMode *dynamic_interface_module(int);
#endif
static inline int parse_opt_i(const char *);
static inline int parse_opt_verbose(const char *);
static inline int parse_opt_quiet(const char *);
static inline int parse_opt_trace(const char *);
static inline int parse_opt_loop(const char *);
static inline int parse_opt_random(const char *);
static inline int parse_opt_sort(const char *);
#ifdef IA_ALSASEQ
static inline int parse_opt_background(const char *);
static inline int parse_opt_rt_prio(const char *);
static inline int parse_opt_seq_ports(const char *);
#endif
#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) || defined(IA_NPSYN) || defined(IA_W32G_SYN) || defined(IA_W32GUI)
static inline int parse_opt_rtsyn_latency(const char *);
static inline int parse_opt_rtsyn_skip_aq(const char *);
#endif
static inline int parse_opt_j(const char *);
static inline int parse_opt_K(const char *);
static inline int parse_opt_k(const char *);
static inline int parse_opt_L(const char *);
static inline int parse_opt_l(const char *);
static inline int parse_opt_resample_over_sampling(const char *);
static inline int parse_opt_pre_resample(const char *);
static inline int parse_opt_M(const char *);
static inline int parse_opt_m(const char *);
static inline int parse_opt_N(const char *);
static inline int parse_opt_n(const char *);
static inline int parse_opt_O(const char *);
static inline int parse_opt_output_stereo(const char *);
static inline int parse_opt_output_signed(const char *);
static inline int parse_opt_output_bitwidth(const char *);
static inline int parse_opt_output_format(const char *);
static inline int parse_opt_output_swab(const char *);
///r
static inline int parse_opt_output_device_id(const char *);
#ifdef AU_W32
static inline int parse_opt_wmme_device_id(const char *);
static inline int parse_opt_wave_format_ext(const char *arg);
#endif
#ifdef AU_WDMKS
static inline int parse_opt_wdmks_device_id(const char *arg);
static inline int parse_opt_wdmks_latency(const char *arg);
static inline int parse_opt_wdmks_format_ext(const char *arg);
static inline int parse_opt_wdmks_polling(const char *arg);
static inline int parse_opt_wdmks_thread_priority(const char *arg);
static inline int parse_opt_wdmks_rt_priority(const char *arg);
static inline int parse_opt_wdmks_pin_priority(const char *arg);
#endif
#ifdef AU_WASAPI
static inline int parse_opt_wasapi_device_id(const char *arg);
static inline int parse_opt_wasapi_latency(const char *arg);
static inline int parse_opt_wasapi_format_ext(const char *arg);
static inline int parse_opt_wasapi_exclusive(const char *arg);
static inline int parse_opt_wasapi_polling(const char *arg);
static inline int parse_opt_wasapi_priority(const char *arg);
static inline int parse_opt_wasapi_stream_category(const char *arg);
static inline int parse_opt_wasapi_stream_option(const char *arg);
#endif
#ifdef AU_PORTAUDIO
static inline int parse_opt_pa_wmme_device_id(const char *);
static inline int parse_opt_pa_ds_device_id(const char *);
static inline int parse_opt_pa_asio_device_id(const char *);
#ifdef PORTAUDIO_V19
static inline int parse_opt_pa_wdmks_device_id(const char *);
static inline int parse_opt_pa_wasapi_device_id(const char *);
static inline int parse_opt_pa_wasapi_flag(const char *);
static inline int parse_opt_pa_wasapi_stream_category(const char *arg);
static inline int parse_opt_pa_wasapi_stream_option(const char *arg);
#endif
#endif

#if defined(AU_PORTAUDIO) || defined(AU_W32)
static inline int parse_opt_output_device(const char*);
#endif
static inline int parse_opt_wave_extensible(const char*);
static inline int parse_opt_wave_update_step(const char*);
#ifdef AU_FLAC
static inline int parse_opt_flac_verify(const char *);
static inline int parse_opt_flac_padding(const char *);
static inline int parse_opt_flac_complevel(const char *);
#ifdef AU_OGGFLAC
static inline int parse_opt_flac_oggflac(const char *);
#endif /* AU_OGGFLAC */
#endif /* AU_FLAC */
#ifdef AU_OPUS
static inline int parse_opt_opus_nframes(const char *);
static inline int parse_opt_opus_bitrate(const char *);
static inline int parse_opt_opus_complexity(const char *);
static inline int parse_opt_opus_vbr(const char *);
static inline int parse_opt_opus_cvbr(const char *);
#endif /* AU_OPUS */
#ifdef AU_SPEEX
static inline int parse_opt_speex_quality(const char *);
static inline int parse_opt_speex_vbr(const char *);
static inline int parse_opt_speex_abr(const char *);
static inline int parse_opt_speex_vad(const char *);
static inline int parse_opt_speex_dtx(const char *);
static inline int parse_opt_speex_complexity(const char *);
static inline int parse_opt_speex_nframes(const char *);
#endif /* AU_SPEEX */
static inline int parse_opt_o(const char *);
static inline int parse_opt_P(const char *);
static inline int parse_opt_p(const char *);
static inline int parse_opt_p1(const char *);
static inline int parse_opt_Q(const char *);
static inline int parse_opt_Q1(const char *);
static inline int parse_opt_preserve_silence(const char *);
static inline int parse_opt_q(const char *);
static inline int parse_opt_R(const char *);
///r
static inline int parse_opt_r(const char *);

static inline int parse_opt_S(const char *);
static inline int parse_opt_s(const char *);
static inline int parse_opt_T(const char *);
static inline int parse_opt_t(const char *);
static inline int parse_opt_U(const char *);
#if defined(AU_VOLUME_CALC)
static inline int parse_opt_volume_calc_rms(const char *);
static inline int parse_opt_volume_calc_trim(const char *);
#endif /* AU_VOLUME_CALC */
static inline int parse_opt_volume_curve(const char *);
__attribute__((noreturn))
static inline int parse_opt_v(const char *);
static inline int parse_opt_W(const char *);
#ifdef __W32__
static inline int parse_opt_w(const char *);
#endif
static inline int parse_opt_x(const char *);
///r
static inline int parse_opt_Y(const char *);
static inline void expand_escape_string(char *);
static inline int parse_opt_Z(const char *);
static inline int parse_opt_Z1(const char *);
static inline int parse_opt_default_module(const char *);
__attribute__((noreturn))
static inline int parse_opt_fail(const char *);
static inline int set_value(int *, int, int, int, const char *);
static inline int set_val_i32(int32 *, int32, int32, int32, const char *);
static int parse_val_float_t(FLOAT_T *param, const char *arg, FLOAT_T low, FLOAT_T high, const char *name);
static inline int set_val_float_t(FLOAT_T *param, FLOAT_T i, FLOAT_T low, FLOAT_T high, const char *name);
static inline int set_channel_flag(ChannelBitMask *, int32, const char *);
static inline int y_or_n_p(const char *);
static inline int set_flag(int32 *, int32, const char *);
static inline FILE *open_pager(void);
static inline void close_pager(FILE *);
static void interesting_message(void);
///r
static inline int parse_opt_add_play_time(const char *arg);
static inline int parse_opt_add_silent_time(const char *arg);
static inline int parse_opt_emu_delay_time(const char *arg);
static inline int parse_opt_mix_envelope(const char *arg);
static inline int parse_opt_modulation_update(const char *arg);

static inline int parse_opt_cut_short_time(const char *arg);
static inline int parse_opt_limiter(const char *arg);
static inline int parse_opt_compute_thread_num(const char *arg);
static inline int parse_opt_trace_mode_update(const char *arg);
static inline int parse_opt_load_all_instrument(const char *arg);
static inline int parse_opt_int_synth_rate(const char *arg);
static inline int parse_opt_int_synth_sine(const char *arg);
static inline int parse_opt_int_synth_update(const char *arg);
#ifdef SUPPORT_LOOPEVENT
static inline int parse_opt_midi_loop_repeat(const char*);
#endif /* SUPPORT_LOOPEVENT */
static inline int parse_opt_od_level_gs(const char *arg);
static inline int parse_opt_od_drive_gs(const char *arg);
static inline int parse_opt_od_level_xg(const char *arg);
static inline int parse_opt_od_drive_xg(const char *arg);

#if defined(__W32__)
static inline int parse_opt_process_priority(const char *arg);
#if !(defined(IA_W32G_SYN) || defined(WINDRV))
static inline int parse_opt_player_thread_priority(const char *arg);
#endif
static void w32_exit(void);
static int w32_atexit = 1;
#endif

extern StringTable wrd_read_opts;

extern int SecondMode;

extern struct URL_module URL_module_file;
#ifndef __MACOS__
extern struct URL_module URL_module_dir;
#endif /* __MACOS__ */
#ifdef SUPPORT_SOCKET
extern struct URL_module URL_module_http;
extern struct URL_module URL_module_ftp;
extern struct URL_module URL_module_news;
extern struct URL_module URL_module_newsgroup;
#endif /* SUPPORT_SOCKET */
#ifdef HAVE_POPEN
extern struct URL_module URL_module_pipe;
#endif /* HAVE_POPEN */

MAIN_INTERFACE struct URL_module *url_module_list[] =
{
    &URL_module_file,
#ifndef __MACOS__
    &URL_module_dir,
#endif /* __MACOS__ */
#ifdef SUPPORT_SOCKET
    &URL_module_http,
    &URL_module_ftp,
    &URL_module_news,
    &URL_module_newsgroup,
#endif /* SUPPORT_SOCKET */
#if !defined(__MACOS__) && defined(HAVE_POPEN)
    &URL_module_pipe,
#endif
#if defined(main) || defined(ANOTHER_MAIN)
    /* You can put some other modules */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#endif /* main */
    NULL
};

#ifdef IA_DYNAMIC
#include "dlutils.h"
#ifndef SHARED_LIB_PATH
#define SHARED_LIB_PATH PKGLIBDIR
#endif /* SHARED_LIB_PATH */
static char *dynamic_lib_root = NULL;
#endif /* IA_DYNAMIC */

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif /* MAXPATHLEN */

int free_instruments_afterwards = 1;
int def_prog = -1;
char def_instr_name[FILEPATH_MAX] = "";
VOLATILE int intr = 0;

#ifdef __W32__
CRITICAL_SECTION critSect;

#pragma argsused
static BOOL WINAPI handler(DWORD dw)
{
#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN)
	if (ctl->id_character == 'W' || ctl->id_character == 'P')
		rtsyn_midiports_close();
#endif /* IA_WINSYN || IA_PORTMIDISYN */
#if 0
#ifdef IA_NPSYN
	if (ctl->id_character == 'N')
		return FALSE;	/* why FALSE need?  It must close by intr++; */
#endif /* IA_NPSYN */
#endif
	printf ("***BREAK" NLS);
	fflush(stdout);
	intr++;
	return TRUE;
}
#endif /* __W32__ */

int effect_lr_mode = -1;
/* 0: left delay
 * 1: right delay
 * 2: rotate
 * -1: not use
 */
int effect_lr_delay_msec = 25;

extern char* pcm_alternate_file;
/* NULL, "none": disabled (default)
 * "auto":       automatically selected
 * filename:     use the one.
 */

#ifndef atof
extern double atof(const char *);
#endif

///r
/*! copy bank and, if necessary, map appropriately */
static void copybank(ToneBank *to, ToneBank *from, int mapid, int bankmapfrom, int bankno)
{
	ToneBankElement *toelm, *fromelm;
	int i;
	int elm;

	if (from == NULL)
		return;
	for(i = 0; i < 128; i++){
		if(from->tone[i][0] == NULL)
			continue;
		for(elm = 0; elm <= from->tone[i][0]->element_num; elm++) {
			toelm = to->tone[i][elm];
			fromelm = from->tone[i][elm];
			if (fromelm->name == NULL)
				continue;
			if(toelm == NULL){					
				if(alloc_tone_bank_element(&to->tone[i][elm])){
					ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "copybank ToneBankElement malloc error ",mapid, bankmapfrom, bankno, elm);
					break;
				}
				toelm = to->tone[i][elm];
			}
			copy_tone_bank_element(toelm, fromelm);
			toelm->instrument = NULL;
			if (mapid != INST_NO_MAP)
				set_instrument_map(mapid, bankmapfrom, i, bankno, i);
		}
	}
}

/*! copy the whole mapped bank. returns 0 if no error. */
static int copymap(int mapto, int mapfrom, int isdrum)
{
	ToneBank **tb = isdrum ? drumset : tonebank;
	int i, bankfrom, bankto;
	
	for(i = 0; i < 128; i++)
	{
		bankfrom = find_instrument_map_bank(isdrum, mapfrom, i);
		if (bankfrom <= 0) /* not mapped */
			continue;
		bankto = alloc_instrument_map_bank(isdrum, mapto, i);
		if (bankto == -1) /* failed */
			return 1;
		copybank(tb[bankto], tb[bankfrom], mapto, i, bankto);
	}
	return 0;
}

static float *config_parse_tune(const char *cp, int *num)
{
	const char *p;
	float *tune_list;
	int i;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	tune_list = (float *) safe_malloc((*num) * sizeof(float));
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		tune_list[i] = atof(p);
		if (! (p = strchr(p, ',')))
			break;
	}
	return tune_list;
}

static int16 *config_parse_int16(const char *cp, int *num)
{
	const char *p;
	int16 *list;
	int i;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	list = (int16 *) safe_malloc((*num) * sizeof(int16));
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		list[i] = atoi(p);
		if (! (p = strchr(p, ',')))
			break;
	}
	return list;
}

static int16 *config_parse_lfo_rate(const char *cp, int *num)
{
	const char *p;
	int16 *list;
	int i;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	list = (int16 *) safe_malloc((*num) * sizeof(int16));
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		if ((! strcmp(p, "off")) || (! strcmp(p, "-0")))
			list[i] = -1;
		else 
			list[i] = atoi(p);
		if (! (p = strchr(p, ',')))
			break;
	}
	return list;
}

static int **config_parse_envelope(const char *cp, int *num)
{
	const char *p, *px;
	int **env_list;
	int i, j;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	env_list = (int **) safe_malloc((*num) * sizeof(int *));
	for (i = 0; i < *num; i++)
		env_list[i] = (int *) safe_malloc(6 * sizeof(int));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < 6; j++)
			env_list[i][j] = -1;
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < 6; j++, p++) {
			if (*p == ':')
				continue;
			env_list[i][j] = atoi(p);
			if (! (p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (! (p = px))
			break;
	}
	return env_list;
}


static int **config_parse_pitch_envelope(const char *cp, int *num)
{
	const char *p, *px;
	int **env_list;
	int i, j;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	env_list = (int **) safe_malloc((*num) * sizeof(int *));
	for (i = 0; i < *num; i++)
		env_list[i] = (int *) safe_malloc(9 * sizeof(int));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < 9; j++)
			env_list[i][j] = 0;
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < 9; j++, p++) {
			if (*p == ':')
				continue;
			env_list[i][j] = atoi(p);
			if (! (p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (! (p = px))
			break;
	}
	return env_list;
}

///r
static int **config_parse_hpfparam(const char *cp, int *num)
{
	const char *p, *px;
	int **hpf_param;
	int i, j;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	hpf_param = (int **) safe_malloc((*num) * sizeof(int *));
	for (i = 0; i < *num; i++)
		hpf_param[i] = (int *) safe_malloc(HPF_PARAM_NUM * sizeof(int));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < HPF_PARAM_NUM; j++)
			hpf_param[i][j] = 0;
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < HPF_PARAM_NUM; j++, p++) {
			if (*p == ':')
				continue;
			hpf_param[i][j] = atoi(p);
			if (! (p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (! (p = px))
			break;
	}
	return hpf_param;
}

static int **config_parse_vfxparam(const char *cp, int *num)
{
	const char *p, *px;
	int **vfx_param;
	int i, j;
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	vfx_param = (int **) safe_malloc((*num) * sizeof(int *));
	for (i = 0; i < *num; i++)
		vfx_param[i] = (int *) safe_malloc(VOICE_EFFECT_PARAM_NUM * sizeof(int));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < VOICE_EFFECT_PARAM_NUM; j++)
			vfx_param[i][j] = 0; // init param
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < VOICE_EFFECT_PARAM_NUM; j++, p++) {
			if (*p == ':')
				continue;
			vfx_param[i][j] = atoi(p);
			if (! (p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (! (p = px))
			break;
	}
	return vfx_param;
}

static Quantity **config_parse_modulation(const char *name, int line, const char *cp, int *num, int mod_type)
{
	const char *p, *px, *err;
	char buf[128], *delim;
	Quantity **mod_list;
	int i, j;
	static const char * qtypestr[] = {"tremolo", "vibrato"};
	static const uint16 qtypes[] = {
		QUANTITY_UNIT_TYPE(LFO_SWEEP), QUANTITY_UNIT_TYPE(LFO_RATE), QUANTITY_UNIT_TYPE(TREMOLO_DEPTH),
		QUANTITY_UNIT_TYPE(LFO_SWEEP), QUANTITY_UNIT_TYPE(LFO_RATE), QUANTITY_UNIT_TYPE(DIRECT_INT)
	};
	
	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	mod_list = (Quantity **) safe_malloc((*num) * sizeof(Quantity *));
	for (i = 0; i < *num; i++)
		mod_list[i] = (Quantity *) safe_malloc(3 * sizeof(Quantity));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < 3; j++)
			INIT_QUANTITY(mod_list[i][j]);
	buf[sizeof buf - 1] = '\0';
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < 3; j++, p++) {
			if (*p == ':')
				continue;
			if ((delim = strpbrk(strncpy(buf, p, sizeof buf - 1), ":,")) != NULL)
				*delim = '\0';
			if (*buf != '\0' && (err = string_to_quantity(buf, &mod_list[i][j], qtypes[mod_type * 3 + j])) != NULL) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: %s: parameter %d of item %d: %s (%s)",
						name, line, qtypestr[mod_type], j+1, i+1, err, buf);
				free_ptr_list(mod_list, *num);
				mod_list = NULL;
				*num = 0;
				return NULL;
			}
			if (! (p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (! (p = px))
			break;
	}
	return mod_list;
}

///r
extern int16 sd_mfx_patch_param[13][9][128][33];
static int config_parse_mfx_patch(char *w[], int words, int mapid, int bank, int prog)
{
    int i, num;

	switch (mapid) {
	case SDXX_TONE80_MAP: num = 0; break; /* Special 1 */
	case SDXX_TONE81_MAP: num = 1; break; /* Special 2 */
	case SDXX_TONE87_MAP: num = 2; break; /* Usr Inst*/ /* SD-50 Preset Inst */
	case SDXX_TONE89_MAP: num = -1; break; /* SD-50 Solo */
	case SDXX_TONE96_MAP: num = 3; break; /* Clsc Inst */
	case SDXX_TONE97_MAP: num = 4; break; /* Cntn Inst */
	case SDXX_TONE98_MAP: num = 5; break; /* Solo Inst */
	case SDXX_TONE99_MAP: num = 6; break; /* Ehnc Inst */
	case SDXX_DRUM86_MAP: num = 7; break; /* Usr Drum */ /* SD-50 Preset Rhythm */
	case SDXX_DRUM104_MAP: num = 8; break; /* Clsc Drum */
	case SDXX_DRUM105_MAP: num = 9; break; /* Cntn Drum */
	case SDXX_DRUM106_MAP: num = 10; break; /* Solo Drum */
	case SDXX_DRUM107_MAP: num = 11; break; /* Ehnc Drum */
	//case GM2_TONE_MAP: num = 12; break; /* GM2 tone */ // test
	default: num = -1; break;
	}
	if(num < 0)
		return 1;
	if(bank > 9)
		return 1;
	memset(sd_mfx_patch_param[num][bank][prog], 0, sizeof(int16) * 33);
	for(i = 0; i < words && i <= 33; i++){
		int32 tmp = atoi(w[i]);
		if(tmp < INT16_MIN || tmp > INT16_MAX)
			return 1;
		sd_mfx_patch_param[num][bank][prog][i] = tmp;
	}
    return 0;
}

///r
static int set_gus_patchconf_opts(char *name,
		int line, char *opts, ToneBankElement *tone)
{
	char *cp;
	int i, k, dup;

	
	if (! (cp = strchr(opts, '='))) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: bad patch option %s", name, line, opts);
		return 1;
	}
	*cp++ = 0;
	if (! strcmp(opts, "amp")) {
#if 1 // float (instrum.c apply_bank_parameter()
		double fk = atof(cp);
		if ((fk < 0 || fk > MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9') && (*cp != '.')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: amplification must be between 0 and %d",
					name, line, MAX_AMPLIFICATION);
			return 1;
		}
		k = fk * DIV_100 * M_12BIT; // max 32768 = 8 * 4096
		if(k > INT16_MAX)
			k = INT16_MAX; // 32767
		tone->amp = k;
#else
		if ((k < 0 || k > MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: amplification must be between 0 and %d",
					name, line, MAX_AMPLIFICATION);
			return 1;
		}
		tone->amp = k;
#endif
	} else if (! strcmp(opts, "amp_normalize")) {
		if (! strcmp(cp, "on"))
			k = 1;
		else if (! strcmp(cp, "off"))
			k = 0;
		else {
			k = atoi(cp);
			if ((k < 0 || k > 1)
					|| (k == 0 && *cp != '-' && (*cp < '0' || *cp > '1'))) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: amp_normalize must be off, on, 0, 1",
						name, line);
				return 1;
			}
		}
		tone->amp_normalize = k;
	} else if (! strcmp(opts, "lokey")) {
		k = atoi(cp);
		if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: lokey must be between 0 and 127",
					name, line);
			return 1;
		}
		tone->lokey = k;
	} else if (! strcmp(opts, "hikey")) {
		k = atoi(cp);
		if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: hikey must be between 0 and 127",
					name, line);
			return 1;
		}
		tone->hikey = k;
	} else if (! strcmp(opts, "lovel")) {
		k = atoi(cp);
		if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: lovel must be between 0 and 127",
					name, line);
			return 1;
		}
		tone->lovel = k;
	} else if (! strcmp(opts, "hivel")) {
		k = atoi(cp);
		if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: hivel must be between 0 and 127",
					name, line);
			return 1;
		}
		tone->hivel = k;
	}else if (! strcmp(opts, "sample_lokey")){
		if(tone->sample_lokeynum)
			safe_free(tone->sample_lokey);
		tone->sample_lokey = config_parse_int16(cp, &tone->sample_lokeynum);
	}else if (! strcmp(opts, "sample_hikey")){
		if(tone->sample_hikeynum)
			safe_free(tone->sample_hikey);
		tone->sample_hikey = config_parse_int16(cp, &tone->sample_hikeynum);
	}else if (! strcmp(opts, "sample_lovel")){
		if(tone->sample_lovelnum)
			safe_free(tone->sample_lovel);
		tone->sample_lovel = config_parse_int16(cp, &tone->sample_lovelnum);
	}else if (! strcmp(opts, "sample_hivel")){
		if(tone->sample_hivelnum)
			safe_free(tone->sample_hivel);
		tone->sample_hivel = config_parse_int16(cp, &tone->sample_hivelnum);
	} else if (! strcmp(opts, "note")) {
		k = atoi(cp);
		if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9')) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: note must be between 0 and 127",
					name, line);
			return 1;
		}
		tone->note = k;
		if(tone->scltunenum)
			safe_free(tone->scltune);
		tone->scltune = config_parse_int16("100", &tone->scltunenum);
///r
	} else if (! strcmp(opts, "pan")) {
		if (! strcmp(cp, "center"))
			k = 64;
		else if (! strcmp(cp, "left"))
			k = 0;
		else if (! strcmp(cp, "right"))
			k = 127;
		else {
			int tmp = atoi(cp);
			if(tmp == 0)
				k = 0x40;
			else if(tmp == 100)
				k = 0x7F;
			else if(tmp == -100)
				k = 0x00;
			else if(tmp > 0)
				k = 0x40 + (tmp * 63) / 100;
			else // tmp < 0
				k = 0x40 + (tmp * 64) / 100;
			if ((k < 0 || k > 127)
					|| (k == 0 && *cp != '-' && (*cp < '0' || *cp > '9'))) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: panning must be left, right, "
						"center, or between -100 and 100",
						name, line);
				return 1;
			}
		}	
		tone->def_pan = k;
///r
	} else if (! strcmp(opts, "sample_pan")) {
		if (! strcmp(cp, "left"))
			k = -200;
		else if (! strcmp(cp, "right"))
			k = 200;
		else {
			k = atoi(cp);
			if ((k < -200 || k > 200)
					|| (k == 0 && *cp != '-' && (*cp < '0' || *cp > '9'))) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: sample_pan must be left, right, "
						"or between -200 and 200",
						name, line);
				return 1;
			}
		}
		tone->sample_pan = k + 200; // offset +200
	} else if (! strcmp(opts, "sample_width")) {		
		if (! strcmp(cp, "center"))
			k = 0;
		else if (! strcmp(cp, "reverse"))
			k = -100;
		else{
			k = atoi(cp);
			if ((k < -800 || k > 800)
					|| (k == 0 && *cp != '-' && (*cp < '0' || *cp > '9'))) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: sample_width must be center, reverse, "
						"or between -800 and 800",
						name, line);
				return 1;
			}
		}
		tone->sample_width = k + 800; // offset +800
	} else if (! strcmp(opts, "tune")){
		if(tone->tunenum)
			safe_free(tone->tune);
		tone->tune = config_parse_tune(cp, &tone->tunenum);
	}else if (! strcmp(opts, "rate")){
		if(tone->envratenum)
			free_ptr_list(tone->envrate, tone->envratenum);
		tone->envrate = config_parse_envelope(cp, &tone->envratenum);
	}else if (! strcmp(opts, "offset")){
		if(tone->envofsnum)
			free_ptr_list(tone->envofs, tone->envofsnum);
		tone->envofs = config_parse_envelope(cp, &tone->envofsnum);
	}else if (! strcmp(opts, "keep")) {
		if (! strcmp(cp, "env"))
			tone->strip_envelope = 0;
		else if (! strcmp(cp, "loop"))
			tone->strip_loop = 0;
		else if (! strcmp(cp, "voice"))
			tone->keep_voice = 1;
		else {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: keep must be env or loop or voice", name, line);
			return 1;
		}
	} else if (! strcmp(opts, "strip")) {
		if (! strcmp(cp, "env"))
			tone->strip_envelope = 1;
		else if (! strcmp(cp, "loop"))
			tone->strip_loop = 1;
		else if (! strcmp(cp, "tail"))
			tone->strip_tail = 1;
		else {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: strip must be env, loop, or tail",
					name, line);
			return 1;
		}
	} else if (! strcmp(opts, "tremolo")){
		if(tone->tremnum)
			free_ptr_list(tone->trem, tone->tremnum);
		if ((tone->trem = config_parse_modulation(name,
				line, cp, &tone->tremnum, 0)) == NULL)
			return 1;
///r
	}else if (! strcmp(opts, "tremdelay")){
		if(tone->tremdelaynum)
			safe_free(tone->tremdelay);
		tone->tremdelay = config_parse_int16(cp, &tone->tremdelaynum);
	}else if (! strcmp(opts, "tremsweep")){
		if(tone->tremsweepnum)
			safe_free(tone->tremsweep);
		tone->tremsweep = config_parse_lfo_rate(cp, &tone->tremsweepnum);
	}else if (! strcmp(opts, "tremfreq")){
		if(tone->tremfreqnum)
			safe_free(tone->tremfreq);
		tone->tremfreq = config_parse_lfo_rate(cp, &tone->tremfreqnum);		
	}else if (! strcmp(opts, "tremamp")){
		if(tone->tremampnum)
			safe_free(tone->tremamp);
		tone->tremamp = config_parse_int16(cp, &tone->tremampnum);
	}else if (! strcmp(opts, "trempitch")){
		if(tone->trempitchnum)
			safe_free(tone->trempitch);
		tone->trempitch = config_parse_int16(cp, &tone->trempitchnum);
	}else if (! strcmp(opts, "tremfc")){
		if(tone->tremfcnum)
			safe_free(tone->tremfc);
		tone->tremfc = config_parse_int16(cp, &tone->tremfcnum);
	} else if (! strcmp(opts, "vibrato")){
		if(tone->vibnum)
			free_ptr_list(tone->vib, tone->vibnum);
		if ((tone->vib = config_parse_modulation(name,
				line, cp, &tone->vibnum, 1)) == NULL)
			return 1;	
///r			
	}else if (! strcmp(opts, "vibdelay")){
		if(tone->vibdelaynum)
			safe_free(tone->vibdelay);
		tone->vibdelay = config_parse_int16(cp, &tone->vibdelaynum);		
	}else if (! strcmp(opts, "vibsweep")){
		if(tone->vibsweepnum)
			safe_free(tone->vibsweep);
		tone->vibsweep = config_parse_lfo_rate(cp, &tone->vibsweepnum);
	}else if (! strcmp(opts, "vibfreq")){
		if(tone->vibfreqnum)
			safe_free(tone->vibfreq);
		tone->vibfreq = config_parse_lfo_rate(cp, &tone->vibfreqnum);	
	}else if (! strcmp(opts, "vibamp")){
		if(tone->vibampnum)
			safe_free(tone->vibamp);
		tone->vibamp = config_parse_int16(cp, &tone->vibampnum);
	}else if (! strcmp(opts, "vibpitch")){
		if(tone->vibpitchnum)
			safe_free(tone->vibpitch);
		tone->vibpitch = config_parse_int16(cp, &tone->vibpitchnum);
	}else if (! strcmp(opts, "vibfc")){
		if(tone->vibfcnum)
			safe_free(tone->vibfc);
		tone->vibfc = config_parse_int16(cp, &tone->vibfcnum);
	} else if (! strcmp(opts, "sclnote")){
		if(tone->sclnotenum)
			safe_free(tone->sclnote);
		tone->sclnote = config_parse_int16(cp, &tone->sclnotenum);
	}else if (! strcmp(opts, "scltune")){
		if(tone->scltunenum)
			safe_free(tone->scltune);
		tone->scltune = config_parse_int16(cp, &tone->scltunenum);
	}else if (! strcmp(opts, "comm")) {
		char *p;
		
		if (!tone->comment)
			tone->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
		strncpy(tone->comment, cp, MAX_TONEBANK_COMM_LEN - 1);
		tone->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
		p = tone->comment;
		while (*p) {
			if (*p == ',')
				*p = ' ';
			p++;
		}
	} else if (! strcmp(opts, "modrate")){
		if(tone->modenvratenum)
			free_ptr_list(tone->modenvrate, tone->modenvratenum);
		tone->modenvrate = config_parse_envelope(cp, &tone->modenvratenum);
	}else if (! strcmp(opts, "modoffset")){
		if(tone->modenvofsnum)
			free_ptr_list(tone->modenvofs, tone->modenvofsnum);
		tone->modenvofs = config_parse_envelope(cp, &tone->modenvofsnum);
	}else if (! strcmp(opts, "envkeyf")){
		if(tone->envkeyfnum)
			free_ptr_list(tone->envkeyf, tone->envkeyfnum);
		tone->envkeyf = config_parse_envelope(cp, &tone->envkeyfnum);
	}else if (! strcmp(opts, "envvelf")){
		if(tone->envvelfnum)
			free_ptr_list(tone->envvelf, tone->envvelfnum);
		tone->envvelf = config_parse_envelope(cp, &tone->envvelfnum);
	}else if (! strcmp(opts, "modkeyf")){
		if(tone->modenvkeyfnum)
			free_ptr_list(tone->modenvkeyf, tone->modenvkeyfnum);
		tone->modenvkeyf = config_parse_envelope(cp, &tone->modenvkeyfnum);
	}else if (! strcmp(opts, "modvelf")){
		if(tone->modenvvelfnum)
			free_ptr_list(tone->modenvvelf, tone->modenvvelfnum);
		tone->modenvvelf = config_parse_envelope(cp, &tone->modenvvelfnum);
	}else if (! strcmp(opts, "modpitch")){
		if(tone->modpitchnum)
			safe_free(tone->modpitch);
		tone->modpitch = config_parse_int16(cp, &tone->modpitchnum);
	}else if (! strcmp(opts, "modfc")){
		if(tone->modfcnum)
			safe_free(tone->modfc);
		tone->modfc = config_parse_int16(cp, &tone->modfcnum);
	}else if (! strcmp(opts, "fc")){
		if(tone->fcnum)
			safe_free(tone->fc);
		tone->fc = config_parse_int16(cp, &tone->fcnum);
	}else if (! strcmp(opts, "q")){
		if(tone->resonum)
			safe_free(tone->reso);
		tone->reso = config_parse_int16(cp, &tone->resonum);
///r
	}else if (! strcmp(opts, "fclow")){
		if(tone->fclownum)
			safe_free(tone->fclow);
		tone->fclow = config_parse_int16(cp, &tone->fclownum);
	}else if (! strcmp(opts, "fcmul")){
		if(tone->fcmulnum)
			safe_free(tone->fcmul);
		tone->fcmul = config_parse_int16(cp, &tone->fcmulnum);
	}else if (! strcmp(opts, "fcadd")){
		if(tone->fcaddnum)
			safe_free(tone->fcadd);
		tone->fcadd = config_parse_int16(cp, &tone->fcaddnum);
	} else if (! strcmp(opts, "pitenv")){
		if(tone->pitenvnum)
			free_ptr_list(tone->pitenv, tone->pitenvnum);
		tone->pitenv = config_parse_pitch_envelope(cp, &tone->pitenvnum);
	}else if (! strcmp(opts, "fckeyf"))		/* filter key-follow */
		tone->key_to_fc = atoi(cp);
	else if (! strcmp(opts, "fcvelf"))		/* filter velocity-follow */
		tone->vel_to_fc = atoi(cp);
	else if (! strcmp(opts, "qvelf"))		/* resonance velocity-follow */
		tone->vel_to_resonance = atoi(cp);
///r
	else if (! strcmp(opts, "perc"))
		tone->rx_note_off = atoi(cp) ? 0 : 1;
	else if (! strcmp(opts, "rxnoteoff"))
		tone->rx_note_off = atoi(cp);
	else if (! strcmp(opts, "lpf"))		/* lpf type */
		tone->lpf_type = atoi(cp);
	else if (! strcmp(opts, "hpf")){		/* hpf */
		if(tone->hpfnum)
			free_ptr_list(tone->hpf, tone->hpfnum);
		tone->hpf = config_parse_hpfparam(cp, &tone->hpfnum);
	}else if (! strcmp(opts, "vfx")){		/* voice effect*/
		for (i = 0; i < VOICE_EFFECT_NUM; i++){
			if(tone->vfxnum[i]) // already use
				continue;
			if(tone->vfx[i]){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,"VFX ERROR: read cfg. vfx pointer already exist.");
				continue; // error check
			}
			tone->vfx[i] = config_parse_vfxparam(cp, &tone->vfxnum[i]);
			break;
		}
///r
	}else {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: bad patch option %s",
				name, line, opts);
		return 1;
	}
	return 0;
}

///r
#define SET_GUS_PATCHCONF_COMMENT
static int set_gus_patchconf(char *name, int line,
			     ToneBankElement *tone, char *pat, char **opts)
{
    int j;
#ifdef SET_GUS_PATCHCONF_COMMENT
		char *old_name = NULL;

		if(tone != NULL && tone->name != NULL)
			old_name = safe_strdup(tone->name);
#endif
//    reinit_tone_bank_element(tone);

    if(strcmp(pat, "%font") == 0 || strcmp(pat, "%sf2") == 0 || strcmp(pat, "%sbk") == 0) /* Font extention */
    {
	/* %font filename bank prog [note-to-use]
	 * %font filename 128 bank key
	 */

		if(opts[0] == NULL || opts[1] == NULL || opts[2] == NULL ||
		   (atoi(opts[1]) == 128 && opts[3] == NULL))
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 1; // sf2
		if(atoi(opts[1]) == 128) /* drum */
		{
			tone->font_bank = 128;
			tone->font_preset = atoi(opts[2]);
			tone->font_keynote = atoi(opts[3]);
			opts += 4;
		}
		else
		{
			tone->font_bank = atoi(opts[1]);
			tone->font_preset = atoi(opts[2]);

			if(opts[3] && isdigit(opts[3][0]))
			{
			tone->font_keynote = atoi(opts[3]);
			opts += 4;
			}
			else
			{
			tone->font_keynote = -1;
			opts += 3;
			}
		}
    }
    else if(strcmp(pat, "%sample") == 0) /* Sample extention */
    {
	/* %sample filename */
		if(opts[0] == NULL)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 2; // wav
		opts++;
    }
///r
#ifdef INT_SYNTH
    else if(strcmp(pat, "%mms") == 0) /* mms extention */
    {
	/* %mms filename num */
		if(opts[0] == NULL || opts[1] == NULL)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 3; // mms
		tone->is_preset = atoi(opts[1]);
		opts += 2;
    }
    else if(strcmp(pat, "%scc") == 0) /* scc extention */
    {
	/* %scc filename num */
		if(opts[0] == NULL || opts[1] == NULL)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 4; // scc
		tone->is_preset = atoi(opts[1]);
		opts += 2;
    }
#endif
#ifdef ENABLE_SFZ
	else if(strcmp(pat, "%sfz") == 0) /* sfz extension */
	{
		/* %sfz filename */
		if (opts[0] == NULL)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 5; // sfz
		opts++;
	}
#endif
    else if(strcmp(pat, "%pat") == 0) /* pat extention */
	{
		tone->instype = 0; // pat
		tone->name = safe_strdup(pat);
	}
    else
    {
		tone->instype = 0; // pat
		tone->name = safe_strdup(pat);
    }

    for(j = 0; opts[j] != NULL; j++)
    {
	int err;
	if((err = set_gus_patchconf_opts(name, line, opts[j], tone)) != 0)
	    return err;
    }
#if 0 // c210 CFG comm
/*
ここでtone->commentをセットする理由が不明
CFGのcommでセットしたかどうかが後でわからなくなる 
CFGのcommを優先するべき
サンプルロードの時に instrum.c *load_instrument() でセットする
*/
#ifdef SET_GUS_PATCHCONF_COMMENT
	if(tone->comment == NULL ||
		(old_name != NULL && strcmp(old_name,tone->comment) == 0))
	{
		tone->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
		strncpy(tone->comment, tone->name, MAX_TONEBANK_COMM_LEN - 1);
		tone->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
	}
	safe_free(old_name);

#else
    if(tone->comment == NULL)
    {
	//tone->comment = safe_strdup(tone->name);
	tone->comment = (char *)safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
	strncpy(tone->comment, tone->name, MAX_TONEBANK_COMM_LEN - 1);
	tone->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
    }
#endif
#endif
    return 0;
}
///r
static int set_patchconf(char *name, int line, ToneBank *bank, char *w[], int dr, int mapid, int bankmapfrom, int bankno, int add)
{
    int i;
	int elm;
    
    i = atoi(w[0]);
    if(!dr)
	i -= progbase;
    if(i < 0 || i > 127)
    {
	if(dr)
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "%s: line %d: Drum number must be between "
		      "0 and 127",
		      name, line);
	else
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "%s: line %d: Program must be between "
		      "%d and %d",
		      name, line, progbase, 127 + progbase);
	return 1;
    }
    if(!bank)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: line %d: Must specify tone bank or drum set "
		  "before assignment", name, line);
	return 1;
    }
	if(add){
		if(bank->tone[i][0] == NULL){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: undeine normal program" "xx: add ", name, line);
			return 1;
		}
		elm = bank->tone[i][0]->element_num + 1; // next add_elm
		if(elm >= MAX_ELEMENT){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Too much program" "%d: add ", name, line, elm);
			return 1;
		}
		if(bank->tone[i][elm] == NULL){
			if(alloc_tone_bank_element(&bank->tone[i][elm])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error" "%d: add ", name, line, elm);
				return 1;
			}
		}
		reinit_tone_bank_element(bank->tone[i][elm]);
		bank->tone[i][0]->element_num = elm;
		if(set_gus_patchconf(name, line, bank->tone[i][elm], w[1], w + 2))
			return 1;
	}else{
		if(bank->tone[i][0] == NULL){
			if(alloc_tone_bank_element(&bank->tone[i][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
				return 1;
			}
		}
		reinit_tone_bank_element(bank->tone[i][0]);
		for(elm = 1; elm < MAX_ELEMENT; elm++){ // delete add_elm
			if(bank->tone[i][elm]){
				free_tone_bank_element(bank->tone[i][elm]);
				safe_free(bank->tone[i][elm]);
				bank->tone[i][elm] = NULL;
			}
		}
		if(set_gus_patchconf(name, line, bank->tone[i][0], w[1], w + 2)){
			return 1;
	}
    if (mapid != INST_NO_MAP)
		set_instrument_map(mapid, bankmapfrom, i, bankno, i);
	}
    return 0;
}

typedef struct {
	const char *name;
	int mapid, isdrum;
} MapNameEntry;

static int mapnamecompare(const void *name, const void *entry)
{
	return strcmp((const char *)name, ((const MapNameEntry *)entry)->name);
}
///r
static int mapname2id(char *name, int *isdrum)
{
	static const MapNameEntry data[] = {
		/* sorted in alphabetical order */
		{"cm32drum",	CM32_DRUM_MAP, 1},
		{"cm32l",		CM32L_TONE_MAP, 0},
		{"cm32p",		CM32P_TONE_MAP, 0},
		{"gm2",         GM2_TONE_MAP, 0},
		{"gm2drum",     GM2_DRUM_MAP, 1},
		{"gm2drum",     GM2_DRUM_MAP, 1},
		{"k05rw000",	K05RW_TONE0_MAP, 0},
		{"k05rw056",	K05RW_TONE56_MAP, 0},
		{"k05rw057",	K05RW_TONE57_MAP, 0},
		{"k05rwdrum062",K05RW_DRUM62_MAP, 1},
		{"mt32",		MT32_TONE_MAP, 0},
		{"mt32drum",	MT32_DRUM_MAP, 1},
		{"nx5r000",		NX5R_TONE0_MAP, 0},
		{"nx5r001",		NX5R_TONE1_MAP, 0},
		{"nx5r002",		NX5R_TONE2_MAP, 0},
		{"nx5r003",		NX5R_TONE3_MAP, 0},
		{"nx5r004",		NX5R_TONE4_MAP, 0},
		{"nx5r005",		NX5R_TONE5_MAP, 0},
		{"nx5r006",		NX5R_TONE6_MAP, 0},
		{"nx5r007",		NX5R_TONE7_MAP, 0},
		{"nx5r008",		NX5R_TONE8_MAP, 0},
		{"nx5r009",		NX5R_TONE9_MAP, 0},
		{"nx5r010",		NX5R_TONE10_MAP, 0},
		{"nx5r011",		NX5R_TONE11_MAP, 0},
		{"nx5r016",		NX5R_TONE16_MAP, 0},
		{"nx5r017",		NX5R_TONE17_MAP, 0},
		{"nx5r018",		NX5R_TONE18_MAP, 0},
		{"nx5r019",		NX5R_TONE19_MAP, 0},
		{"nx5r024",		NX5R_TONE24_MAP, 0},
		{"nx5r025",		NX5R_TONE25_MAP, 0},
		{"nx5r026",		NX5R_TONE26_MAP, 0},
		{"nx5r032",		NX5R_TONE32_MAP, 0},
		{"nx5r033",		NX5R_TONE33_MAP, 0},
		{"nx5r040",		NX5R_TONE40_MAP, 0},
		{"nx5r056",		NX5R_TONE56_MAP, 0},
		{"nx5r057",		NX5R_TONE57_MAP, 0},
		{"nx5r064",		NX5R_TONE64_MAP, 0}, 
		{"nx5r080",		NX5R_TONE80_MAP, 0},
		{"nx5r081",		NX5R_TONE81_MAP, 0},
		{"nx5r082",		NX5R_TONE82_MAP, 0},
		{"nx5r083",		NX5R_TONE83_MAP, 0},
		{"nx5r088",		NX5R_TONE88_MAP, 0},
		{"nx5r089",		NX5R_TONE89_MAP, 0},
		{"nx5r090",		NX5R_TONE90_MAP, 0},
		{"nx5r091",		NX5R_TONE91_MAP, 0},
		{"nx5r125",		NX5R_TONE125_MAP, 0},
		{"nx5rdrum061",	NX5R_DRUM61_MAP, 1},
		{"nx5rdrum062",	NX5R_DRUM61_MAP, 1},
		{"nx5rdrum126",	NX5R_DRUM126_MAP, 1},
		{"nx5rdrum127",	NX5R_DRUM127_MAP, 1},
		{"sc55",        SC_55_TONE_MAP, 0},
		{"sc55drum",    SC_55_DRUM_MAP, 1},
		{"sc88",        SC_88_TONE_MAP, 0},
		{"sc8850",      SC_8850_TONE_MAP, 0},
		{"sc8850drum",  SC_8850_DRUM_MAP, 1},
		{"sc88drum",    SC_88_DRUM_MAP, 1},
		{"sc88pro",     SC_88PRO_TONE_MAP, 0},
		{"sc88prodrum", SC_88PRO_DRUM_MAP, 1},
		{"sd080",		SDXX_TONE80_MAP, 0},
		{"sd081",		SDXX_TONE81_MAP, 0},
		{"sd087",		SDXX_TONE87_MAP, 0},
		{"sd089",		SDXX_TONE89_MAP, 0},
		{"sd096",		SDXX_TONE96_MAP, 0},
		{"sd097",		SDXX_TONE97_MAP, 0},
		{"sd098",		SDXX_TONE98_MAP, 0},
		{"sd099",		SDXX_TONE99_MAP, 0},
		{"sddrum104",	SDXX_DRUM104_MAP, 1}, 
		{"sddrum105",	SDXX_DRUM105_MAP, 1}, 
		{"sddrum106",	SDXX_DRUM106_MAP, 1}, 
		{"sddrum107",	SDXX_DRUM107_MAP, 1}, 
		{"sddrum86",	SDXX_DRUM86_MAP, 1}, 
		{"sn01",		SN01_TONE_MAP, 0},
		{"sn02",		SN02_TONE_MAP, 0},
		{"sn02drum",	SN02_DRUM_MAP, 1},
		{"sn03",		SN03_TONE_MAP, 0},
		{"sn04",		SN04_TONE_MAP, 0},
		{"sn05",		SN05_TONE_MAP, 0},
		{"sn06",		SN06_TONE_MAP, 0},
		{"sn07",		SN07_TONE_MAP, 0},
		{"sn08",		SN08_TONE_MAP, 0},
		{"sn09",		SN09_TONE_MAP, 0},
		{"sn10drum",	SN10_DRUM_MAP, 1},
		{"sn11",		SN11_TONE_MAP, 0},
		{"sn12",		SN12_TONE_MAP, 0},
		{"sn13",		SN13_TONE_MAP, 0},
		{"sn14",		SN14_TONE_MAP, 0},
		{"sn15",		SN15_TONE_MAP, 0},
		{"xg",          XG_NORMAL_MAP, 0},
		{"xg000",       XG_NORMAL_MAP, 0},
		{"xg016",       XG_SAMPLING16_MAP, 0}, 
		{"xg032",       XG_PCM_USER_MAP, 0}, 
		{"xg033",       XG_VA_USER_MAP, 0}, 
		{"xg034",       XG_SG_USER_MAP, 0}, 
		{"xg035",       XG_FM_USER_MAP, 0}, 
		{"xg048",       XG_MU100EXC_MAP, 0}, 
		{"xg063",       XG_FREE_MAP, 0}, 
		{"xg064",       XG_PCM_SFX_MAP, 0}, 
		{"xg065",       XG_VA_SFX_MAP, 0}, 
		{"xg066",       XG_SG_SFX_MAP, 0}, 
		{"xg067",       XG_FM_SFX_MAP, 0}, 
		{"xg080",       XG_PCM_A_MAP, 0}, 
		{"xg081",       XG_VA_A_MAP, 0}, 
		{"xg082",       XG_SG_A_MAP, 0}, 
		{"xg083",       XG_FM_A_MAP, 0}, 
		{"xg096",       XG_PCM_B_MAP, 0}, 
		{"xg097",       XG_VA_B_MAP, 0}, 
		{"xg098",       XG_SG_B_MAP, 0}, 
		{"xg099",       XG_FM_B_MAP, 0},
		{"xg126",       XG_SFX_KIT_MAP, 1},
		{"xg127",       XG_DRUM_KIT_MAP, 1},
		{"xgdrum",      XG_DRUM_KIT_MAP, 1},
		{"xgexclusive48", XG_MU100EXC_MAP, 0}, 
		{"xgfma",       XG_FM_A_MAP, 0},
		{"xgfmb",       XG_FM_B_MAP, 0},
		{"xgfmsfx",     XG_FM_SFX_MAP, 0}, 
		{"xgfmuser",    XG_FM_USER_MAP, 0},
		{"xgfree",      XG_FREE_MAP, 0}, 
		{"xgpcma",      XG_PCM_A_MAP, 0},
		{"xgpcmb",      XG_PCM_B_MAP, 0},
		{"xgpcmsfx",    XG_PCM_SFX_MAP, 0}, 
		{"xgpcmuser",   XG_PCM_USER_MAP, 0}, 
		{"xgsampling126",XG_SAMPLING126_MAP, 1}, 
		{"xgsampling16",XG_SAMPLING16_MAP, 0}, 
		{"xgsfx126",    XG_SFX_KIT_MAP, 1},
		{"xgsfx64",     XG_PCM_SFX_MAP, 0},  // = xgpcmsfx
		{"xgsga",       XG_SG_A_MAP, 0},
		{"xgsgb",       XG_SG_B_MAP, 0},
		{"xgsgsfx",     XG_SG_SFX_MAP, 0}, 
		{"xgsguser",    XG_SG_USER_MAP, 0}, 
		{"xgvaa",       XG_VA_A_MAP, 0},
		{"xgvab",       XG_VA_B_MAP, 0},
		{"xgvasfx",     XG_VA_SFX_MAP, 0}, 
		{"xgvauser",    XG_VA_USER_MAP, 0}, 
	};
	const MapNameEntry *found;
	
	found = (MapNameEntry *)bsearch(name, data, sizeof data / sizeof data[0], sizeof data[0], mapnamecompare);
	if (found != NULL)
	{
		*isdrum = found->isdrum;
		return found->mapid;
	}
	return -1;
}

/* string[0] should not be '#' */
static int strip_trailing_comment(char *string, int next_token_index)
{
    if (string[next_token_index - 1] == '#'	/* strip \1 in /^\S+(#*[ \t].*)/ */
	&& (string[next_token_index] == ' ' || string[next_token_index] == '\t'))
    {
	string[next_token_index] = '\0';	/* new c-string terminator */
	while(string[--next_token_index - 1] == '#')
	    ;
    }
    return next_token_index;
}

static char *expand_variables(char *string, MBlockList *varbuf, const char *basedir)
{
	char *p, *expstr;
	const char *copystr;
	int limlen, copylen, explen, varlen, braced;
	
	if ((p = strchr(string, '$')) == NULL)
		return string;
	varlen = strlen(basedir);
	explen = limlen = 0;
	expstr = NULL;
	copystr = string;
	copylen = p - string;
	string = p;
	for(;;)
	{
		if (explen + copylen + 1 > limlen)
		{
			limlen += copylen + 128;
			expstr = memcpy(new_segment(varbuf, limlen), expstr, explen);
		}
		memcpy(&expstr[explen], copystr, copylen);
		explen += copylen;
		if (*string == '\0')
			break;
		else if (*string == '$')
		{
			braced = *++string == '{';
			if (braced)
			{
				if ((p = strchr(string + 1, '}')) == NULL)
					p = string;	/* no closing brace */
				else
					string++;
			}
			else
				for(p = string; isalnum(*p) || *p == '_'; p++) ;
			if (p == string)	/* empty */
			{
				copystr = "${";
				copylen = 1 + braced;
			}
			else
			{
				if (p - string == 7 && memcmp(string, "basedir", 7) == 0)
				{
					copystr = basedir;
					copylen = varlen;
				}
				else	/* undefined variable */
					copylen = 0;
				string = p + braced;
			}
		}
		else	/* search next */
		{
			p = strchr(string, '$');
			if (p == NULL)
				copylen = strlen(string);
			else
				copylen = p - string;
			copystr = string;
			string += copylen;
		}
	}
	expstr[explen] = '\0';
	return expstr;
}

#define MAXWORDS 130
#define CHECKERRLIMIT \
  if(++errcnt >= 10) { \
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, \
      "Too many errors... Give up read %s", name); \
    reuse_mblock(&varbuf); \
    close_file(tf); return 1; }

#define READ_CONFIG_SUCCESS        0
#define READ_CONFIG_ERROR          1
#define READ_CONFIG_RECURSION      2 /* Too much recursion */
#define READ_CONFIG_FILE_NOT_FOUND 3 /* Returned only w. allow_missing_file */


MAIN_INTERFACE int read_config_file(const char *name, int self, int allow_missing_file) // changed elion static int -> int
{
    struct timidity_file *tf = NULL;
    char buf[2048] = "", *tmp = NULL, *w[MAXWORDS + 1] = { NULL }, *cp = NULL;
    ToneBank *bank = NULL;
    int i, j, k, line = 0, words, errcnt = 0, elm;
    static int rcf_count = 0;
    int dr = 0, bankno = 0, mapid = INST_NO_MAP, origbankno = 0x7FFFFFFF;
    int extension_flag, param_parse_err;
    MBlockList varbuf;
    char *basedir = NULL, *sep = NULL;

    if (rcf_count > 50)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Probable source loop in configuration files");
	return READ_CONFIG_RECURSION;
    }

    if (self)
    {
	tf = open_with_mem(name, (int32)strlen(name), OF_VERBOSE);
	name = "(configuration)";
    }
    else
	tf = open_file(name, 1, allow_missing_file ? OF_NORMAL : OF_VERBOSE);
    if (!tf)
	return allow_missing_file ? READ_CONFIG_FILE_NOT_FOUND :
				    READ_CONFIG_ERROR;

	init_mblock(&varbuf);
	if (!self)
	{
		basedir = strdup_mblock(&varbuf, current_filename);
		if (is_url_prefix(basedir))
			sep = strrchr(basedir, '/');
		else
			sep = pathsep_strrchr(basedir);
	}
	else
		sep = NULL;
	if (!sep)
	{
		#ifndef __MACOS__
		basedir = ".";
		#else
		basedir = "";
		#endif
	}
	else
	{
		if ((cp = strchr(sep, '#')) != NULL)
			sep = cp + 1;	/* inclusive of '#' */
		*sep = '\0';
	}

    errno = 0;
    while (tf_gets(buf, sizeof(buf), tf))
    {
	line++;
	
	if (!strncmp(buf, "#@extension", 11)) {
	    extension_flag = 1;
	    i = 11;
	}else if (!strncmp(buf, "#extension", 10)) {
	    extension_flag = 1;
	    i = 10;
	}
	else
	{
	    extension_flag = 0;
	    i = 0;
	}

	while (isspace(buf[i]))			/* skip /^\s*(?#)/ */
	    i++;
	if (buf[i] == '#' || buf[i] == '\0')	/* /^#|^$/ */
	    continue;
	tmp = expand_variables(buf, &varbuf, basedir);
	j = strcspn(tmp + i, " \t\r\n\240");
	if (j == 0)
		j = strlen(tmp + i);
	j = strip_trailing_comment(tmp + i, j);
	tmp[i + j] = '\0';			/* terminate the first token */
	w[0] = tmp + i;
	i += j + 1;
	words = param_parse_err = 0;
	while (words < MAXWORDS - 1)		/* -1 : next arg */
	{
	    char *terminator;

	    while (isspace(tmp[i]))		/* skip /^\s*(?#)/ */
		i++;
	    if (tmp[i] == '\0'
		    || tmp[i] == '#')		/* /\s#/ */
		break;
	    if ((tmp[i] == '"' || tmp[i] == '\'')
		    && (terminator = strchr(tmp + i + 1, tmp[i])) != NULL)
	    {
		if (!isspace(terminator[1]) && terminator[1] != '\0')
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: line %d: there must be at least one whitespace between "
			"string terminator (%c) and the next parameter", name, line, tmp[i]);
		    CHECKERRLIMIT;
		    param_parse_err = 1;
		    break;
		}
		w[++words] = tmp + i + 1;
		i = terminator - tmp + 1;
		*terminator = '\0';
	    }
	    else	/* not terminated */
	    {
		j = strcspn(tmp + i, " \t\r\n\240");
		if (j > 0)
		    j = strip_trailing_comment(tmp + i, j);
		w[++words] = tmp + i;
		i += j;
		if (tmp[i] != '\0')		/* unless at the end-of-string (i.e. EOF) */
		    tmp[i++] = '\0';		/* terminate the token */
	    }
	}
	if (param_parse_err)
	    continue;
	w[++words] = NULL;

	/*
	 * #extension [something...]
	 */

	/* #extension comm program comment */
	if (!strcmp(w[0], "comm"))
	{
	    char *p;

	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum "
			  "set before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension comm must be "
			  "between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }		
		if(bank->tone[i][0] == NULL){
			if(alloc_tone_bank_element(&bank->tone[i][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
				return 1;
			}
		}
		if (!bank->tone[i][0]->comment)
		bank->tone[i][0]->comment = (char*) safe_malloc(sizeof(char) * MAX_TONEBANK_COMM_LEN);
		strncpy(bank->tone[i][0]->comment, w[2], MAX_TONEBANK_COMM_LEN - 1);
		bank->tone[i][0]->comment[MAX_TONEBANK_COMM_LEN - 1] = '\0';
		p = bank->tone[i][0]->comment;
		while (*p)
		{
		if (*p == ',') *p = ' ';
		p++;
		}
	}
	/* #extension timeout program sec */
	else if (!strcmp(w[0], "timeout"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension timeout "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
		if(bank->tone[i][0])
			bank->tone[i][0]->loop_timeout = atoi(w[2]) & 0xFF;
	}
	/* #extension copydrumset drumset */
	else if (!strcmp(w[0], "copydrumset"))
	{
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No copydrumset number given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension copydrumset "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or "
			  "drum set before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    copybank(bank, drumset[i], mapid, origbankno, bankno);
	}
	/* #extension copybank bank */
	else if (!strcmp(w[0], "copybank"))
	{
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No copybank number given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension copybank "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or "
			  "drum set before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    copybank(bank, tonebank[i], mapid, origbankno, bankno);
	}
	/* #extension copymap tomapid frommapid */
	else if (!strcmp(w[0], "copymap"))
	{
	    int mapto, mapfrom;
	    int toisdrum, fromisdrum;

	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if ((mapto = mapname2id(w[1], &toisdrum)) == -1)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Invalid map name: %s", name, line, w[1]);
		CHECKERRLIMIT;
		continue;
	    }
	    if ((mapfrom = mapname2id(w[2], &fromisdrum)) == -1)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Invalid map name: %s", name, line, w[2]);
		CHECKERRLIMIT;
		continue;
	    }
	    if (toisdrum != fromisdrum)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Map type should be matched", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (copymap(mapto, mapfrom, toisdrum))
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No free %s available to map",
			  name, line, toisdrum ? "drum set" : "tone bank");
		CHECKERRLIMIT;
		continue;
	    }
	}
	/* #extension HTTPproxy hostname:port */
	else if (!strcmp(w[0], "HTTPproxy"))
	{
	    char r_bracket, l_bracket;

	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No proxy name given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    /* If network is not supported, this extension is ignored. */
#ifdef SUPPORT_SOCKET
	    url_http_proxy_host = safe_strdup(w[1]);
	    if ((cp = strrchr(url_http_proxy_host, ':')) == NULL)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    *cp++ = '\0';
	    if ((url_http_proxy_port = atoi(cp)) <= 0)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Port number must be "
			  "positive number", name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    l_bracket = url_http_proxy_host[0];
	    r_bracket = url_http_proxy_host[strlen(url_http_proxy_host) - 1];

	    if (l_bracket == '[' || r_bracket == ']')
	    {
		if (l_bracket != '[' || r_bracket != ']')
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: Malformed IPv6 address",
			      name, line);
		    CHECKERRLIMIT;
		    continue;
		}
		url_http_proxy_host++;
		url_http_proxy_host[strlen(url_http_proxy_host) - 1] = '\0';
	    }
#endif /* SUPPORT_SOCKET */
	}
	/* #extension FTPproxy hostname:port */
	else if (!strcmp(w[0], "FTPproxy"))
	{
	    char l_bracket, r_bracket;

	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No proxy name given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    /* If network is not supported, this extension is ignored. */
#ifdef SUPPORT_SOCKET
	    url_ftp_proxy_host = safe_strdup(w[1]);
	    if ((cp = strrchr(url_ftp_proxy_host, ':')) == NULL)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    *cp++ = '\0';
	    if ((url_ftp_proxy_port = atoi(cp)) <= 0)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Port number "
			  "must be positive number", name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    l_bracket = url_ftp_proxy_host[0];
	    r_bracket = url_ftp_proxy_host[strlen(url_ftp_proxy_host) - 1];

	    if (l_bracket == '[' || r_bracket == ']')
	    {
		if (l_bracket != '[' || r_bracket != ']')
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: Malformed IPv6 address",
			      name, line);
		    CHECKERRLIMIT;
		    continue;
		}
		url_ftp_proxy_host++;
		url_ftp_proxy_host[strlen(url_ftp_proxy_host) - 1] = '\0';
	    }
#endif /* SUPPORT_SOCKET */
	}
	/* #extension mailaddr somebody@someware.domain.com */
	else if (!strcmp(w[0], "mailaddr"))
	{
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No mail address given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (strchr(w[1], '@') == NULL) {
		ctl->cmsg(CMSG_WARNING, VERB_NOISY,
			  "%s: line %d: Warning: Mail address %s is not valid",
			  name, line);
	    }

	    /* If network is not supported, this extension is ignored. */
#ifdef SUPPORT_SOCKET
	    user_mailaddr = safe_strdup(w[1]);
#endif /* SUPPORT_SOCKET */
	}
	/* #extension opt [-]{option}[optarg] */
	else if (!strcmp(w[0], "opt")) {
		int c, longind, err;
		char *p, *cmd, *arg;

		if (words != 2 && words != 3) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Syntax error", name, line);
			CHECKERRLIMIT;
			continue;
		}
		if (*w[1] == '-') {
			int optind_save = optind;
			optind = 0;
			c = getopt_long(words, w, optcommands, longopts, &longind);
			err = set_tim_opt_long(c, optarg, longind);
			optind = optind_save;
		} else {
			/* backward compatibility */
			if ((p = strchr(optcommands, c = *(cmd = w[1]))) == NULL)
				err = 1;
			else {
				if (*(p + 1) == ':')
					arg = (words == 2) ? cmd + 1 : w[2];
				else
					arg = "";
				err = set_tim_opt_short(c, arg);
			}
		}
		if (err) {
			/* error */
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Invalid command line option",
					name, line);
			errcnt += err - 1;
			CHECKERRLIMIT;
			continue;
		}
	}
	/* #extension undef program */
	else if (!strcmp(w[0], "undef"))
	{
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No undef number given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension undef "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or "
			  "drum set before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
		
	    for (elm = 0; elm < MAX_ELEMENT; elm++)
			if(bank->tone[i][elm])
				free_tone_bank_element(bank->tone[i][elm]);
	}
	/* #extension altassign numbers... */
	else if (!strcmp(w[0], "altassign"))
	{
	    ToneBank *bk;

	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before altassign", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No alternate assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    if (!dr) {
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
			  "%s: line %d: Warning: Not a drumset altassign"
			  " (ignored)",
			  name, line);
		continue;
	    }

	    bk = drumset[bankno];
	    bk->alt = add_altassign_string(bk->alt, w + 1, words - 1);
	}
	/* #extension legato [program] [0 or 1] */
	else if (!strcmp(w[0], "legato"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension legato "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
///r
		if(bank->tone[i][0] == NULL){
			if(alloc_tone_bank_element(&bank->tone[i][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
				return READ_CONFIG_ERROR;
			}
		}
		bank->tone[i][0]->legato = atoi(w[2]);

	}	/* #extension damper [program] [0 or 1] */
	else if (!strcmp(w[0], "damper"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension damper "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
///r
		if(bank->tone[i][0] == NULL){
			if(alloc_tone_bank_element(&bank->tone[i][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
				return READ_CONFIG_ERROR;
			}
		}
		bank->tone[i][0]->damper_mode = atoi(w[2]);
	}	/* #extension rnddelay [program] [0 or 1] */
	else if (!strcmp(w[0], "rnddelay"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension rnddelay "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
///r
		if(bank->tone[i][0] == NULL){
			if(alloc_tone_bank_element(&bank->tone[i][0])){
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
				return READ_CONFIG_ERROR;
			}
		}
		bank->tone[i][0]->rnddelay = atoi(w[2]);
	}	/* #extension level program tva_level */
	else if (!strcmp(w[0], "level"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[2]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension level "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    cp = w[1];
	    do {
		if (string_to_7bit_range(cp, &j, &k))
		{
		    while (j <= k) {
				if(bank->tone[j][0] == NULL){
					if(alloc_tone_bank_element(&bank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
						return READ_CONFIG_ERROR;
					}
				}
				bank->tone[j][0]->tva_level = i;
				j++;
		    }
		}
		cp = strchr(cp, ',');
	    } while (cp++);
	}	/* #extension reverbsend */
	else if (!strcmp(w[0], "reverbsend"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[2]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension reverbsend "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    cp = w[1];
	    do {
		if (string_to_7bit_range(cp, &j, &k))
		{
		    while (j <= k) {
				if(bank->tone[j][0] == NULL){
					if(alloc_tone_bank_element(&bank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
						return READ_CONFIG_ERROR;
					}
				}
				bank->tone[j][0]->reverb_send = i;
				j++;
		    }
		}
		cp = strchr(cp, ',');
	    } while (cp++);
	}	/* #extension chorussend */
	else if (!strcmp(w[0], "chorussend"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[2]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension chorussend "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    cp = w[1];
	    do {
		if (string_to_7bit_range(cp, &j, &k))
		{
		    while (j <= k) {
				if(bank->tone[j][0] == NULL){
					if(alloc_tone_bank_element(&bank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
						return READ_CONFIG_ERROR;
					}
				}
				bank->tone[j][0]->chorus_send = i;
				j++;
		    }
		}
		cp = strchr(cp, ',');
	    } while (cp++);
	}	/* #extension delaysend */
	else if (!strcmp(w[0], "delaysend"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    i = atoi(w[2]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension delaysend "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    cp = w[1];
	    do {
		if (string_to_7bit_range(cp, &j, &k))
		{
		    while (j <= k) {
				if(bank->tone[j][0] == NULL){
					if(alloc_tone_bank_element(&bank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
						return READ_CONFIG_ERROR;
					}
				}
				bank->tone[j][0]->delay_send = i;
				j++;
		    }
		}
		cp = strchr(cp, ',');
	    } while (cp++);
	}	
	/* #extension rxnoteoff numbers... */
	else if (!strcmp(w[0], "rxnoteoff"))
	{		
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!dr) {
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
			  "%s: line %d: Warning: Not a drumset rxnoteoff"
			  " (ignored)",
			  name, line);
		continue;
	    }		
	    i = atoi(w[2]);
	    if (i < 0 || i > 1)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension rxnoteoff "
			  "must be 0 or 1", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    cp = w[1];
	    do {
		if (string_to_7bit_range(cp, &j, &k))
		{
		    while (j <= k) {
				if(bank->tone[j][0] == NULL){
					if(alloc_tone_bank_element(&bank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
						return READ_CONFIG_ERROR;
					}
				}
				bank->tone[j][0]->rx_note_off = i;
				j++;
		    }
		}
		cp = strchr(cp, ',');
	    } while (cp++);
	}
	/* #extension playnote */
	else if (!strcmp(w[0], "playnote"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[2]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension playnote"
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    cp = w[1];
	    do {
		if (string_to_7bit_range(cp, &j, &k))
		{
		    while (j <= k) {
				if(bank->tone[j][0] == NULL){
					if(alloc_tone_bank_element(&bank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: ToneBankElement malloc error.", name, line);
						return READ_CONFIG_ERROR;
					}
				}
				bank->tone[j][0]->play_note = i;
				j++;
		    }
		}
		cp = strchr(cp, ',');
	    } while (cp++);
	}	/* #extension fc */
	else if (!strcmp(w[0], "fc"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension fc "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
///r
		for (elm = 0; elm < MAX_ELEMENT; elm++)
			if(bank->tone[i][elm])
				bank->tone[i][elm]->fc = config_parse_int16(w[2], &bank->tone[i][elm]->fcnum);
	}	/* #extension q */
	else if (!strcmp(w[0], "q"))
	{
	    if (words != 3)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: extension fc "
			  "must be between 0 and 127", name, line);
		CHECKERRLIMIT;
		continue;
	    }
///r
		for (elm = 0; elm < MAX_ELEMENT; elm++)
			if(bank->tone[i][elm])
				bank->tone[i][elm]->reso = config_parse_int16(w[2], &bank->tone[i][elm]->resonum);
	}
///r
	else if(!strcmp(w[0], "mfx_patch"))
	{
	    int prog;
	    words--;
	    memmove(&w[0], &w[1], sizeof(w[0]) * words);
	    w[words] = '\0';		/* terminate the token */
	    if (words < 2 || *w[0] < '0' || *w[0] > '9')
	    {
		if (extension_flag)
		    continue;
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    prog = atoi(w[0]);
	    words--;
	    memmove(&w[0], &w[1], sizeof(w[0]) * words);
	    w[words] = '\0';		/* terminate the token */
	    if (!dr)
		prog -= progbase;
	    if (prog < 0 || prog > 127){
		if (dr)
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: Drum number must be between ""0 and 127", name, line);
		else
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: Program must be between ""%d and %d", name, line, progbase, 127 + progbase);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set ""before assignment", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (config_parse_mfx_patch(w, words, mapid, origbankno, prog)) // mfx
	    {
		CHECKERRLIMIT;
		continue;
	    }
	}	/* #extension amp_normalize */
	else if (!strcmp(w[0], "amp_normalize"))
	{
	    if (words != 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!bank)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify tone bank or drum set "
			  "before amp_normalize", name, line);
		CHECKERRLIMIT;
		continue;
	    }
		if (! strcmp(w[1], "on"))
			k = 1;
		else if (! strcmp(w[1], "off"))
			k = 0;
	    else {
			k = atoi(w[1]);
			if (k < 0 || k > 1)	{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: extension amp_normalize "
				  "must be off, on, 0, 1", name, line);
			CHECKERRLIMIT;
			continue;
			}
		}
		for(i = 0; i < (128 + MAP_BANK_COUNT); i++){
			ToneBank *tmpbank = tonebank[i];
			if (!tmpbank)
				continue;
			for (j = 0; j < 128; j++)
				for (elm = 0; elm < MAX_ELEMENT; elm++)
					if(tmpbank->tone[j][elm])
						tmpbank->tone[j][elm]->amp_normalize = k;
		}
		for(i = 0; i < (128 + MAP_BANK_COUNT); i++){
			ToneBank *tmpbank = drumset[i];
			if (!tmpbank)
				continue;
			for (j = 0; j < 128; j++)
				for (elm = 0; elm < MAX_ELEMENT; elm++)
					if(tmpbank->tone[i][elm])
						tmpbank->tone[i][elm]->amp_normalize = k;
		}
	}
	else if (!strcmp(w[0], "soundfont"))
	{
	    int order, cutoff, isremove, reso, amp;
	    char *sf_file;

	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No soundfont file given",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    sf_file = w[1];
	    order = cutoff = reso = amp = -1;
	    isremove = 0;
	    for (j = 2; j < words; j++)
	    {
		if (!strcmp(w[j], "remove"))
		{
		    isremove = 1;
		    break;
		}
		if (!(cp = strchr(w[j], '=')))
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: bad patch option %s",
			      name, line, w[j]);
		    CHECKERRLIMIT;
		    break;
		}
		*cp++ = 0;
		k = atoi(cp);
		if (!strcmp(w[j], "order"))
		{
		    if (k < 0 || (*cp < '0' || *cp > '9'))
		    {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: order must be a digit",
				  name, line);
			CHECKERRLIMIT;
			break;
		    }
		    order = k;
		}
		else if (!strcmp(w[j], "cutoff"))
		{
		    if (k < 0 || (*cp < '0' || *cp > '9'))
		    {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: cutoff must be a digit",
				  name, line);
			CHECKERRLIMIT;
			break;
		    }
		    cutoff = k;
		}
		else if (!strcmp(w[j], "reso"))
		{
		    if (k < 0 || (*cp < '0' || *cp > '9'))
		    {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "%s: line %d: reso must be a digit",
				  name, line);
			CHECKERRLIMIT;
			break;
		    }
		    reso = k;
		}
		else if (!strcmp(w[j], "amp"))
		{
		    amp = k;
		}
	    }
	    if (isremove)
		remove_soundfont(sf_file);
	    else
		add_soundfont(sf_file, order, cutoff, reso, amp);
		// init tonebank
		for(i = 0; i < 128; i++){
			ToneBank *tmpbank = tonebank[i];
			if (!tmpbank){
				tonebank[i] = (ToneBank *)safe_malloc(sizeof(ToneBank));
				memset(tonebank[i], 0, sizeof(ToneBank));
				tmpbank = tonebank[i];
			}
			for (j = 0; j < 128; j++){				
				if(tmpbank->tone[j][0] == NULL)
				{
					if(alloc_tone_bank_element(&tmpbank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "read_config_file: soundfont ToneBankElement malloc error.");
						return READ_CONFIG_ERROR;
					}
				}
				for(elm = 0; elm < MAX_ELEMENT; elm++)
					if(tmpbank->tone[j][elm])
					reinit_tone_bank_element(tmpbank->tone[j][elm]); // need instrum.c apply_bank_parameter()
			}
		}
		for(i = 0; i < 128; i++){
			ToneBank *tmpbank = drumset[i];
			if (!tmpbank){
				drumset[i] = (ToneBank *)safe_malloc(sizeof(ToneBank));
				memset(drumset[i], 0, sizeof(ToneBank));
				tmpbank = drumset[i];
			}
			for (j = 0; j < 128; j++){			
				if(tmpbank->tone[j][0] == NULL)
				{
					if(alloc_tone_bank_element(&tmpbank->tone[j][0])){
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "read_config_file: soundfont ToneBankElement malloc error.");
						return READ_CONFIG_ERROR;
					}
				}
				for(elm = 0; elm < MAX_ELEMENT; elm++)
					if(tmpbank->tone[j][elm])
					reinit_tone_bank_element(tmpbank->tone[j][elm]); // need instrum.c apply_bank_parameter()
			}
		}
	}
	else if (!strcmp(w[0], "font"))
	{
	    int bank, preset, keynote;
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: no font command", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (!strcmp(w[1], "exclude"))
	    {
		if (words < 3)
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: No bank/preset/key is given",
			      name, line);
		    CHECKERRLIMIT;
		    continue;
		}
		bank = atoi(w[2]);
		if (words >= 4)
		    preset = atoi(w[3]) - progbase;
		else
		    preset = -1;
		if (words >= 5)
		    keynote = atoi(w[4]);
		else
		    keynote = -1;
		if (exclude_soundfont(bank, preset, keynote))
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: No soundfont is given",
			      name, line);
		    CHECKERRLIMIT;
		}
	    }
	    else if (!strcmp(w[1], "order"))
	    {
		int order;
		if (words < 4)
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: No order/bank is given",
			      name, line);
		    CHECKERRLIMIT;
		    continue;
		}
		order = atoi(w[2]);
		bank = atoi(w[3]);
		if (words >= 5)
		    preset = atoi(w[4]) - progbase;
		else
		    preset = -1;
		if (words >= 6)
		    keynote = atoi(w[5]);
		else
		    keynote = -1;
		if (order_soundfont(bank, preset, keynote, order))
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: No soundfont is given",
			      name, line);
		    CHECKERRLIMIT;
		}
	    }
	}
	else if (!strcmp(w[0], "progbase"))
	{
	    if (words < 2 || *w[1] < '0' || *w[1] > '9')
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    progbase = atoi(w[1]);
	}
	else if (!strcmp(w[0], "map")) /* map <name> set1 elem1 set2 elem2 */
	{
	    int arg[5], isdrum;

	    if (words != 6)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if ((arg[0] = mapname2id(w[1], &isdrum)) == -1)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Invalid map name: %s", name, line, w[1]);
		CHECKERRLIMIT;
		continue;
	    }
	    for (i = 2; i < 6; i++)
		arg[i - 1] = atoi(w[i]);
	    if (isdrum)
	    {
		arg[1] -= progbase;
		arg[3] -= progbase;
	    }
	    else
	    {
		arg[2] -= progbase;
		arg[4] -= progbase;
	    }

	    for (i = 1; i < 5; i++)
		if (arg[i] < 0 || arg[i] > 127)
		    break;
	    if (i != 5)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Invalid parameter", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    set_instrument_map(arg[0], arg[1], arg[2], arg[3], arg[4]);
	}

	/*
	 * Standard configurations
	 */
	else if (!strcmp(w[0], "dir"))
	{
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No directory given", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    for (i = 1; i < words; i++)
		add_to_pathlist(w[i]);
	}
	else if (!strcmp(w[0], "source") || !strcmp(w[0], "trysource"))
	{
	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No file name given", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    for (i = 1; i < words; i++)
	    {
		int status;
		rcf_count++;
		status = read_config_file(w[i], 0, !strcmp(w[0], "trysource"));
		rcf_count--;
		switch (status) {
		case READ_CONFIG_SUCCESS:
		    break;
		case READ_CONFIG_ERROR:
		    CHECKERRLIMIT;
		    continue;
		case READ_CONFIG_RECURSION:
		    reuse_mblock(&varbuf);
		    close_file(tf);
		    return READ_CONFIG_RECURSION;
		case READ_CONFIG_FILE_NOT_FOUND:
		    break;
		}
	    }
	}
	else if (!strcmp(w[0], "default"))
	{
	    if (words != 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Must specify exactly one patch name",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    strncpy(def_instr_name, w[1], FILEPATH_MAX - 1);
	    def_instr_name[FILEPATH_MAX - 1] = '\0';
	    default_instrument_name = def_instr_name;
	}
	/* drumset [mapid] num */
	else if (!strcmp(w[0], "drumset"))
	{
	    int newmapid, isdrum, newbankno;

	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No drum set number given", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (words != 2 && !isdigit(*w[1]))
	    {
		if ((newmapid = mapname2id(w[1], &isdrum)) == -1 || !isdrum)
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Invalid drum set map name: %s", name, line, w[1]);
		    CHECKERRLIMIT;
		    continue;
		}
		words--;
		memmove(&w[1], &w[2], sizeof w[0] * words);
	    }
	    else
		newmapid = INST_NO_MAP;
	    i = atoi(w[1]) - progbase;
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Drum set must be between %d and %d",
			  name, line,
			  progbase, progbase + 127);
		CHECKERRLIMIT;
		continue;
	    }

	    newbankno = i;
	    i = alloc_instrument_map_bank(1, newmapid, i);
	    if (i == -1)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No free drum set available to map",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    if (words == 2)
	    {
		bank = drumset[i];
		bankno = i;
		mapid = newmapid;
		origbankno = newbankno;
		dr = 1;
	    }
	    else
	    {
		if (words < 4 || *w[2] < '0' || *w[2] > '9')
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: syntax error", name, line);
		    CHECKERRLIMIT;
		    continue;
		}
		if (set_patchconf(name, line, drumset[i], &w[2], 1, newmapid, newbankno, i, 0))
		{
		    CHECKERRLIMIT;
		    continue;
		}
	    }
	}
	/* bank [mapid] num */
	else if (!strcmp(w[0], "bank"))
	{
	    int newmapid, isdrum, newbankno;

	    if (words < 2)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No bank number given", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (words != 2 && !isdigit(*w[1]))
	    {
		if ((newmapid = mapname2id(w[1], &isdrum)) == -1 || isdrum)
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Invalid bank map name: %s", name, line, w[1]);
		    CHECKERRLIMIT;
		    continue;
		}
		words--;
		memmove(&w[1], &w[2], sizeof w[0] * words);
	    }
	    else
		newmapid = INST_NO_MAP;
	    i = atoi(w[1]);
	    if (i < 0 || i > 127)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: Tone bank must be between 0 and 127",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    newbankno = i;
	    i = alloc_instrument_map_bank(0, newmapid, i);
	    if (i == -1)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: No free tone bank available to map",
			  name, line);
		CHECKERRLIMIT;
		continue;
	    }

	    if (words == 2)
	    {
		bank = tonebank[i];
		bankno = i;
		mapid = newmapid;
		origbankno = newbankno;
		dr = 0;
	    }
	    else
	    {
		if (words < 4 || *w[2] < '0' || *w[2] > '9')
		{
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "%s: line %d: syntax error", name, line);
		    CHECKERRLIMIT;
		    continue;
		}
		if (set_patchconf(name, line, tonebank[i], &w[2], 0, newmapid, newbankno, i, 0))
		{
		    CHECKERRLIMIT;
		    continue;
		}
	    }
	}
///r
	else if(!strcmp(w[0], "add"))
	{
		words--;
		memmove(&w[0], &w[1], sizeof w[0] * words);
		w[words] = '\0';		/* terminate the token */
	    if (words < 2 || *w[0] < '0' || *w[0] > '9')
	    {
		if (extension_flag)
		    continue;
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (set_patchconf(name, line, bank, w, dr, mapid, origbankno, bankno, 1))
	    {
		CHECKERRLIMIT;
		continue;
	    }
	}
	else
	{
	    if (words < 2 || *w[0] < '0' || *w[0] > '9')
	    {
		if (extension_flag)
		    continue;
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: line %d: syntax error", name, line);
		CHECKERRLIMIT;
		continue;
	    }
	    if (set_patchconf(name, line, bank, w, dr, mapid, origbankno, bankno, 0))
	    {
		CHECKERRLIMIT;
		continue;
	    }
	}
    }

    if (errno)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Can't read %s: %s", name, strerror(errno));
	errcnt++;
    }
    reuse_mblock(&varbuf);
    close_file(tf);
    return (errcnt == 0) ? READ_CONFIG_SUCCESS : READ_CONFIG_ERROR;
}

#ifdef SUPPORT_SOCKET

#if defined(__W32__) && !defined(MAIL_NAME)
#define MAIL_NAME "anonymous"
#endif /* __W32__ */

#ifdef MAIL_NAME
#define get_username() MAIL_NAME
#else /* MAIL_NAME */
#include <pwd.h>
static char *get_username(void)
{
    char *p;
    struct passwd *pass;

    /* USER
     * LOGIN
     * LOGNAME
     * getpwnam()
     */

    if((p = getenv("USER")) != NULL)
        return p;
    if((p = getenv("LOGIN")) != NULL)
        return p;
    if((p = getenv("LOGNAME")) != NULL)
        return p;

    pass = getpwuid(getuid());
    if(pass == NULL)
        return "nobody";
    return pass->pw_name;
}
#endif /* MAIL_NAME */

static void init_mail_addr(void)
{
    char addr[BUFSIZ];

    sprintf(addr, "%s%s", get_username(), MAIL_DOMAIN);
    user_mailaddr = safe_strdup(addr);
}
#endif /* SUPPORT_SOCKET */

static int read_user_config_file(void)
{
    char *home;
    char path[BUFSIZ];
    int status;

    home = getenv("HOME");
#ifdef __W32__
/* HOME or home */
    if(home == NULL)
	home = getenv("home");
#endif
    if(home == NULL)
    {
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		  "Warning: HOME environment is not defined.");
	return 0;
    }

#ifdef __W32__
/* timidity.cfg or _timidity.cfg or .timidity.cfg*/
    sprintf(path, "%s" PATH_STRING "timidity.cfg", home);
    status = read_config_file(path, 0, 1);
    if (status != READ_CONFIG_FILE_NOT_FOUND)
        return status;

    sprintf(path, "%s" PATH_STRING "_timidity.cfg", home);
    status = read_config_file(path, 0, 1);
    if (status != READ_CONFIG_FILE_NOT_FOUND)
        return status;
#endif

    sprintf(path, "%s" PATH_STRING ".timidity.cfg", home);
    status = read_config_file(path, 0, 1);
    if (status != READ_CONFIG_FILE_NOT_FOUND)
        return status;
	return 0;
}

MAIN_INTERFACE void tmdy_free_config(void)
{
	free_tone_bank();
	free_instrument_map();
	clean_up_pathlist();
}

int set_extension_modes(char *flag)
{
	return parse_opt_E(flag);
}

int set_ctl(char *cp)
{
	return parse_opt_i(cp);
}

int set_play_mode(char *cp)
{
	return parse_opt_O(cp);
}

int set_wrd(char *w)
{
	return parse_opt_W(w);
}



#ifdef __W32__
int opt_evil_mode = 0;
#ifdef SMFCONV
int opt_rcpcv_dll = 0;
#endif	/* SMFCONV */
#endif	/* __W32__ */
static int   try_config_again = 0;
///r tim13-
#ifdef FAST_DECAY
int opt_fast_decay = 1;
#else
int opt_fast_decay = 0;
#endif /* FAST_DECAY */
int32 opt_output_rate = 0;
static char *opt_output_name = NULL;
static StringTable opt_config_string;
#ifdef SUPPORT_SOUNDSPEC
double spectrogram_update_sec = 0.0;
#endif /* SUPPORT_SOUNDSPEC */
#if defined(AU_VOLUME_CALC)
int opt_volume_calc_rms = 0;
int opt_volume_calc_trim = 0;
#endif /* AU_VOLUME_CALC */
int opt_buffer_fragments = -1;
int opt_audio_buffer_bits = -1;
///r
int opt_compute_buffer_bits = -128;

MAIN_INTERFACE int set_tim_opt_short(int c, const char *optarg)
{
	int err = 0;
	
	switch (c) {
	case '4':
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"-4 option is obsoleted.  Please use -N");
		return 1;
	case 'A':
		if (*optarg != ',' && *optarg != 'a')
			err += parse_opt_A(optarg);
		if (strchr(optarg, ','))
			err += parse_opt_drum_power(strchr(optarg, ',') + 1);
		if (strchr(optarg, 'a'))
			opt_amp_compensation = 1;
		return err;
	case 'a':
		antialiasing_allowed = 1;
		break;
	case 'B':
		return parse_opt_B(optarg);
	case 'C':
		return parse_opt_C(optarg);
	case 'c':
		return parse_opt_c(optarg);
	case 'D':
		return parse_opt_D(optarg);
	case 'd':
		return parse_opt_d(optarg);
	case 'E':
		return parse_opt_E(optarg);
	case 'e':
		return parse_opt_e(optarg);
	case 'F':
		adjust_panning_immediately = (adjust_panning_immediately) ? 0 : 1;
		break;
	case 'f':
		fast_decay = (fast_decay) ? 0 : 1;
		break;
	case 'G':
		return parse_opt_G(optarg);
	case 'g':
		return parse_opt_g(optarg);
	case 'H':
		return parse_opt_H(optarg);
	case 'h':
		return parse_opt_h(optarg);
	case 'I':
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"-I option is obsoleted.  Please use -Ei");
		return 1;
	case 'i':
		return parse_opt_i(optarg);
	case 'j':
		opt_realtime_playing = (opt_realtime_playing) ? 0 : 1;
		break;
	case 'K':
		return parse_opt_K(optarg);
	case 'k':
		return parse_opt_k(optarg);
	case 'L':
		return parse_opt_L(optarg);
	case 'M':
		return parse_opt_M(optarg);
	case 'm':
		return parse_opt_m(optarg);
	case 'N':
		return parse_opt_N(optarg);
	case 'n':
		return parse_opt_n(optarg);
	case 'O':
		return parse_opt_O(optarg);
	case 'o':
		return parse_opt_o(optarg);
	case 'P':
		return parse_opt_P(optarg);
	case 'p':
		if (*optarg != 'a')
			err += parse_opt_p(optarg);
		if (strchr(optarg, 'a'))
			auto_reduce_polyphony = (auto_reduce_polyphony) ? 0 : 1;
		return err;
	case 'Q':
		return parse_opt_Q(optarg);
	case 'q':
		return parse_opt_q(optarg);
	case 'R':
		return parse_opt_R(optarg);
///r
	case 'r':
		return parse_opt_r(optarg);
	case 'S':
		return parse_opt_S(optarg);
	case 's':
		return parse_opt_s(optarg);
	case 'T':
		return parse_opt_T(optarg);
	case 't':
		return parse_opt_t(optarg);
	case 'U':
		free_instruments_afterwards = 1;
		break;
	case 'V':
		return parse_opt_volume_curve(optarg);
	case 'v':
		return parse_opt_v(optarg);
	case 'W':
		return parse_opt_W(optarg);
#ifdef __W32__
	case 'w':
		return parse_opt_w(optarg);
#endif
	case 'x':
		return parse_opt_x(optarg);
	case 'Y':
		return parse_opt_Y(optarg);
	case 'Z':
		if (strncmp(optarg, "pure", 4))
			return parse_opt_Z(optarg);
		else
			return parse_opt_Z1(optarg + 4);
	default:
		return 1;
	}
	return 0;
}

#ifdef __W32__
MAIN_INTERFACE int set_tim_opt_short_cfg(int c, const char *optarg)
{
	switch (c) {
	case 'c':
		return parse_opt_c(optarg);
	}
	return 0;
}
#endif

/* -------- getopt_long -------- */
MAIN_INTERFACE int set_tim_opt_long(int c, const char *optarg, int index)
{
	const struct option *the_option = &(longopts[index]);
	char *arg = NULL;
	
	if (c == '?')	/* getopt_long failed parsing */
		parse_opt_fail(optarg);
	else if (c < TIM_OPT_FIRST)
		return set_tim_opt_short(c, optarg);
	if (! strncmp(the_option->name, "no-", 3))
		arg = "no";		/* `reverse' switch */
	else
		arg = optarg;
	switch (c) {
	case TIM_OPT_VOLUME:
		return parse_opt_A(arg);
	case TIM_OPT_MASTER_VOLUME:
		return parse_opt_master_volume(arg);
	case TIM_OPT_DRUM_POWER:
		return parse_opt_drum_power(arg);
	case TIM_OPT_VOLUME_COMP:
		return parse_opt_volume_comp(arg);
	case TIM_OPT_ANTI_ALIAS:
		return parse_opt_a(arg);
	case TIM_OPT_BUFFER_FRAGS:
		return parse_opt_B(arg);
	case TIM_OPT_CONTROL_RATIO:
		return parse_opt_C(arg);
	case TIM_OPT_CONFIG_FILE:
		return parse_opt_c(arg);
	case TIM_OPT_DRUM_CHANNEL:
		return parse_opt_D(arg);
	case TIM_OPT_IFACE_PATH:
		return parse_opt_d(arg);
	case TIM_OPT_EXT:
		return parse_opt_E(arg);
	case TIM_OPT_MOD_WHEEL:
		return parse_opt_mod_wheel(arg);
	case TIM_OPT_PORTAMENTO:
		return parse_opt_portamento(arg);
	case TIM_OPT_VIBRATO:
		return parse_opt_vibrato(arg);
	case TIM_OPT_CH_PRESS:
		return parse_opt_ch_pressure(arg);
	case TIM_OPT_MOD_ENV:
		return parse_opt_mod_env(arg);
	case TIM_OPT_TRACE_TEXT:
		return parse_opt_trace_text(arg);
	case TIM_OPT_OVERLAP:
		return parse_opt_overlap_voice(arg);
///r
	case TIM_OPT_OVERLAP_COUNT:
		return parse_opt_overlap_voice_count(arg);
	case TIM_OPT_MAX_CHANNEL_VOICES:
		return parse_opt_max_channel_voices(arg);

	case TIM_OPT_TEMPER_CTRL:
		return parse_opt_temper_control(arg);
	case TIM_OPT_DEFAULT_MID:
		return parse_opt_default_mid(arg);
	case TIM_OPT_SYSTEM_MID:
		return parse_opt_system_mid(arg);
	case TIM_OPT_DEFAULT_BANK:
		return parse_opt_default_bank(arg);
	case TIM_OPT_FORCE_BANK:
		return parse_opt_force_bank(arg);
	case TIM_OPT_DEFAULT_PGM:
		return parse_opt_default_program(arg);
	case TIM_OPT_FORCE_PGM:
		return parse_opt_force_program(arg);
	case TIM_OPT_DELAY:
		return parse_opt_delay(arg);
	case TIM_OPT_CHORUS:
		return parse_opt_chorus(arg);
	case TIM_OPT_REVERB:
		return parse_opt_reverb(arg);
	case TIM_OPT_VOICE_LPF:
		return parse_opt_voice_lpf(arg);
	case TIM_OPT_VOICE_HPF:
		return parse_opt_voice_hpf(arg);
	case TIM_OPT_NS:
		return parse_opt_noise_shaping(arg);
#ifndef FIXED_RESAMPLATION
	case TIM_OPT_RESAMPLE:
		return parse_opt_resample(arg);
#endif
	case TIM_OPT_EVIL:
		return parse_opt_e(arg);
	case TIM_OPT_FAST_PAN:
		return parse_opt_F(arg);
	case TIM_OPT_FAST_DECAY:
		return parse_opt_f(arg);
	case TIM_OPT_SEGMENT:
		return parse_opt_G(arg);
	case TIM_OPT_SPECTROGRAM:
		return parse_opt_g(arg);
	case TIM_OPT_KEYSIG:
		return parse_opt_H(arg);
	case TIM_OPT_HELP:
		return parse_opt_h(arg);
	case TIM_OPT_INTERFACE:
		return parse_opt_i(arg);
	case TIM_OPT_VERBOSE:
		return parse_opt_verbose(arg);
	case TIM_OPT_QUIET:
		return parse_opt_quiet(arg);
	case TIM_OPT_TRACE:
		return parse_opt_trace(arg);
	case TIM_OPT_LOOP:
		return parse_opt_loop(arg);
	case TIM_OPT_RANDOM:
		return parse_opt_random(arg);
	case TIM_OPT_SORT:
		return parse_opt_sort(arg);
#ifdef IA_ALSASEQ
	case TIM_OPT_BACKGROUND:
		return parse_opt_background(arg);
	case TIM_OPT_RT_PRIO:
		return parse_opt_rt_prio(arg);
	case TIM_OPT_SEQ_PORTS:
		return parse_opt_seq_ports(arg);
#endif
#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) || defined(IA_NPSYN) || defined(IA_W32G_SYN) || defined(IA_W32GUI)
	case TIM_OPT_RTSYN_LATENCY:
		return parse_opt_rtsyn_latency(arg);
	case TIM_OPT_RTSYN_SKIP_AQ:
		return parse_opt_rtsyn_skip_aq(arg);
#endif
	case TIM_OPT_REALTIME_LOAD:
		return parse_opt_j(arg);
	case TIM_OPT_ADJUST_KEY:
		return parse_opt_K(arg);
	case TIM_OPT_VOICE_QUEUE:
		return parse_opt_k(arg);
	case TIM_OPT_PATCH_PATH:
		return parse_opt_L(arg);
	case TIM_OPT_PCM_FILE:
		return parse_opt_M(arg);
///r
	case TIM_OPT_RESAMPLE_FILTER:
		return parse_opt_l(arg);
	case TIM_OPT_RESAMPLE_OVER_SAMPLING:
		return parse_opt_resample_over_sampling(arg);
	case TIM_OPT_PRE_RESAMPLE:
		return parse_opt_pre_resample(arg);
	case TIM_OPT_DECAY_TIME:
		return parse_opt_m(arg);
	case TIM_OPT_INTERPOLATION:
		return parse_opt_N(arg);
	case TIM_OPT_POLYPHONY_QUEUE:
		return parse_opt_n(arg);
	case TIM_OPT_OUTPUT_MODE:
		return parse_opt_O(arg);
	case TIM_OPT_OUTPUT_STEREO:
		if (! strcmp(the_option->name, "output-mono"))
			/* --output-mono == --output-stereo=no */
			arg = "no";
		return parse_opt_output_stereo(arg);
	case TIM_OPT_OUTPUT_SIGNED:
		if (! strcmp(the_option->name, "output-unsigned"))
			/* --output-unsigned == --output-signed=no */
			arg = "no";
		return parse_opt_output_signed(arg);
///r
	case TIM_OPT_OUTPUT_BITWIDTH:
		if (! strcmp(the_option->name, "output-16bit"))
			arg = "16bit";
		else if (! strcmp(the_option->name, "output-24bit"))
			arg = "24bit";
		else if (! strcmp(the_option->name, "output-32bit"))
			arg = "32bit";
		else if (! strcmp(the_option->name, "output-64bit"))
			arg = "64bit";
		else if (! strcmp(the_option->name, "output-8bit"))
			arg = "8bit";
		else if (! strcmp(the_option->name, "output-f32bit"))
			arg = "f32bit";
		else if (! strcmp(the_option->name, "output-float32bit"))
			arg = "f";
		else if (! strcmp(the_option->name, "output-f64bit"))
			arg = "D64bit";
		else if (! strcmp(the_option->name, "output-float64bit"))
			arg = "D";
		return parse_opt_output_bitwidth(arg);
	case TIM_OPT_OUTPUT_FORMAT:
		if (! strcmp(the_option->name, "output-linear"))
			arg = "linear";
		else if (! strcmp(the_option->name, "output-ulaw"))
			arg = "ulaw";
		else if (! strcmp(the_option->name, "output-alaw"))
			arg = "alaw";
		return parse_opt_output_format(arg);
	case TIM_OPT_OUTPUT_SWAB:
		return parse_opt_output_swab(arg);
///r
	case TIM_OPT_OUTPUT_DEVICE_ID:
		return parse_opt_output_device_id(arg);
#ifdef AU_W32
	case TIM_OPT_WMME_DEVICE_ID:
		return parse_opt_wmme_device_id(arg);
	case TIM_OPT_WAVE_FORMAT_EXT:
		return parse_opt_wave_format_ext(arg);
#endif
#ifdef AU_WDMKS
	case TIM_OPT_WDMKS_DEVICE_ID:
		return parse_opt_wdmks_device_id(arg);
	case TIM_OPT_WDMKS_LATENCY:
		return parse_opt_wdmks_latency(arg);
	case TIM_OPT_WDMKS_FORMAT_EXT:
		return parse_opt_wdmks_format_ext(arg);
	case TIM_OPT_WDMKS_POLLING:
		return parse_opt_wdmks_polling(arg);
	case TIM_OPT_WDMKS_THREAD_PRIORITY:
		return parse_opt_wdmks_thread_priority(arg);
	case TIM_OPT_WDMKS_RT_PRIORITY:
		return parse_opt_wdmks_rt_priority(arg);
	case TIM_OPT_WDMKS_PIN_PRIORITY:
		return parse_opt_wdmks_pin_priority(arg);
#endif
#ifdef AU_WASAPI
	case TIM_OPT_WASAPI_DEVICE_ID:
		return parse_opt_wasapi_device_id(arg);
	case TIM_OPT_WASAPI_LATENCY:
		return parse_opt_wasapi_latency(arg);
	case TIM_OPT_WASAPI_FORMAT_EXT:
		return parse_opt_wasapi_format_ext(arg);
	case TIM_OPT_WASAPI_EXCLUSIVE:
		return parse_opt_wasapi_exclusive(arg);
	case TIM_OPT_WASAPI_POLLING:
		return parse_opt_wasapi_polling(arg);
	case TIM_OPT_WASAPI_PRIORITY:
		return parse_opt_wasapi_priority(arg);
	case TIM_OPT_WASAPI_STREAM_CATEGORY:
		return parse_opt_wasapi_stream_category(arg);
	case TIM_OPT_WASAPI_STREAM_OPTION:
		return parse_opt_wasapi_stream_option(arg);
#endif
#ifdef AU_PORTAUDIO
	case TIM_OPT_PA_WMME_DEVICE_ID:
		return parse_opt_pa_wmme_device_id(arg);
	case TIM_OPT_PA_DS_DEVICE_ID:
		return parse_opt_pa_ds_device_id(arg);
	case TIM_OPT_PA_ASIO_DEVICE_ID:
		return parse_opt_pa_asio_device_id(arg);
#ifdef PORTAUDIO_V19
	case TIM_OPT_PA_WDMKS_DEVICE_ID:
		return parse_opt_pa_wdmks_device_id(arg);
	case TIM_OPT_PA_WASAPI_DEVICE_ID:
		return parse_opt_pa_wasapi_device_id(arg);
	case TIM_OPT_PA_WASAPI_FLAG:
		return parse_opt_pa_wasapi_flag(arg);
	case TIM_OPT_PA_WASAPI_STREAM_CATEGORY:
		return parse_opt_pa_wasapi_stream_category(arg);
	case TIM_OPT_PA_WASAPI_STREAM_OPTION:
		return parse_opt_pa_wasapi_stream_option(arg);
#endif
#endif
		
	case TIM_OPT_WAVE_EXTENSIBLE:
		return parse_opt_wave_extensible(arg);
	case TIM_OPT_WAVE_UPDATE_STEP:
		return parse_opt_wave_update_step(arg);
#ifdef AU_FLAC
	case TIM_OPT_FLAC_VERIFY:
		return parse_opt_flac_verify(arg);
	case TIM_OPT_FLAC_PADDING:
		return parse_opt_flac_padding(arg);
	case TIM_OPT_FLAC_COMPLEVEL:
		return parse_opt_flac_complevel(arg);
#ifdef AU_OGGFLAC
	case TIM_OPT_FLAC_OGGFLAC:
		return parse_opt_flac_oggflac(arg);
#endif /* AU_OGGFLAC */
#endif /* AU_FLAC */
#ifdef AU_OPUS
	case TIM_OPT_OPUS_NFRAMES:
		return parse_opt_opus_nframes(arg);
	case TIM_OPT_OPUS_BITRATE:
		return parse_opt_opus_bitrate(arg);
	case TIM_OPT_OPUS_COMPLEXITY:
		return parse_opt_opus_complexity(arg);
	case TIM_OPT_OPUS_VBR:
		return parse_opt_opus_vbr(arg);
	case TIM_OPT_OPUS_CVBR:
		return parse_opt_opus_cvbr(arg);
#endif /* AU_OPUS */
#ifdef AU_SPEEX
	case TIM_OPT_SPEEX_QUALITY:
		return parse_opt_speex_quality(arg);
	case TIM_OPT_SPEEX_VBR:
		return parse_opt_speex_vbr(arg);
	case TIM_OPT_SPEEX_ABR:
		return parse_opt_speex_abr(arg);
	case TIM_OPT_SPEEX_VAD:
		return parse_opt_speex_vad(arg);
	case TIM_OPT_SPEEX_DTX:
		return parse_opt_speex_dtx(arg);
	case TIM_OPT_SPEEX_COMPLEXITY:
		return parse_opt_speex_complexity(arg);
	case TIM_OPT_SPEEX_NFRAMES:
		return parse_opt_speex_nframes(arg);
#endif /* AU_SPEEX */
	case TIM_OPT_OUTPUT_FILE:
		return parse_opt_o(arg);
	case TIM_OPT_PATCH_FILE:
		return parse_opt_P(arg);
	case TIM_OPT_POLYPHONY:
		return parse_opt_p(arg);
	case TIM_OPT_POLY_REDUCE:
		return parse_opt_p1(arg);
	case TIM_OPT_MUTE:
		return parse_opt_Q(arg);
	case TIM_OPT_TEMPER_MUTE:
		return parse_opt_Q1(arg);
	case TIM_OPT_PRESERVE_SILENCE:
		return parse_opt_preserve_silence(arg);
	case TIM_OPT_AUDIO_BUFFER:
		return parse_opt_q(arg);
///r
	case TIM_OPT_RESAMPLE_QUEUE:
		return parse_opt_r(arg);

	case TIM_OPT_CACHE_SIZE:
		return parse_opt_S(arg);
	case TIM_OPT_SAMPLE_FREQ:
		return parse_opt_s(arg);
	case TIM_OPT_ADJUST_TEMPO:
		return parse_opt_T(arg);
	case TIM_OPT_CHARSET:
		return parse_opt_t(arg);
	case TIM_OPT_UNLOAD_INST:
		return parse_opt_U(arg);
#if defined(AU_VOLUME_CALC)
	case TIM_OPT_VOLUME_CALC_RMS:
		return parse_opt_volume_calc_rms(arg);
	case TIM_OPT_VOLUME_CALC_TRIM:
		return parse_opt_volume_calc_trim(arg);
#endif /* AU_VOLUME_CALC */
	case TIM_OPT_VOLUME_CURVE:
		return parse_opt_volume_curve(arg);
	case TIM_OPT_VERSION:
		return parse_opt_v(arg);
	case TIM_OPT_WRD:
		return parse_opt_W(arg);
#ifdef __W32__
	case TIM_OPT_RCPCV_DLL:
		return parse_opt_w(arg);
#endif
	case TIM_OPT_CONFIG_STR:
		return parse_opt_x(arg);
///r
	case TIM_OPT_COMPUTE_BUFFER:
		return parse_opt_Y(arg);
	case TIM_OPT_FREQ_TABLE:
		return parse_opt_Z(arg);
	case TIM_OPT_PURE_INT:
		return parse_opt_Z1(arg);
	case TIM_OPT_MODULE:
		return parse_opt_default_module(arg);
///r
	case TIM_OPT_ADD_PLAY_TIME:
		return parse_opt_add_play_time(arg);
	case TIM_OPT_ADD_SILENT_TIME:
		return parse_opt_add_silent_time(arg);
	case TIM_OPT_EMU_DELAY_TIME:
		return parse_opt_emu_delay_time(arg);
	case TIM_OPT_LIMITER:
		return parse_opt_limiter(arg);		
	case TIM_OPT_MIX_ENV:
		return parse_opt_mix_envelope(arg);
	case TIM_OPT_MOD_UPDATE:
		return parse_opt_modulation_update(arg);		
	case TIM_OPT_CUT_SHORT:
		return parse_opt_cut_short_time(arg);
	case TIM_OPT_COMPUTE_THREAD_NUM:
		return parse_opt_compute_thread_num(arg);	
	case TIM_OPT_TRACE_MODE_UPDATE:
		return parse_opt_trace_mode_update(arg);
	case TIM_OPT_LOAD_ALL_INSTRUMENT:
		return parse_opt_load_all_instrument(arg);
#ifdef SUPPORT_LOOPEVENT
	case TIM_OPT_LOOP_REPEAT:
		return parse_opt_midi_loop_repeat(arg);
#endif /* SUPPORT_LOOPEVENT */
	case TIM_OPT_OD_LEVEL_GS:
		return parse_opt_od_level_gs(arg);
	case TIM_OPT_OD_DRIVE_GS:
		return parse_opt_od_drive_gs(arg);
	case TIM_OPT_OD_LEVEL_XG:
		return parse_opt_od_level_xg(arg);
	case TIM_OPT_OD_DRIVE_XG:
		return parse_opt_od_drive_xg(arg);
				
#if defined(__W32__)
	case TIM_OPT_PROCESS_PRIORITY:
		return parse_opt_process_priority(arg);
#if !(defined(IA_W32G_SYN) || defined(WINDRV))
	case TIM_OPT_PLAYER_THREAD_PRIORITY:
		return parse_opt_player_thread_priority(arg);
#endif
#endif
	case TIM_OPT_DUMMY_SETTING:
		return 0; //dummy call
		
	case TIM_OPT_INT_SYNTH_RATE:
		return parse_opt_int_synth_rate(arg);
	case TIM_OPT_INT_SYNTH_SINE:
		return parse_opt_int_synth_sine(arg);
	case TIM_OPT_INT_SYNTH_UPDATE:
		return parse_opt_int_synth_update(arg);

	default:
		ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
				"[BUG] Inconceivable case branch %d", c);
		abort();
		return 0; //dummy call
	}
}

#ifdef __W32__
MAIN_INTERFACE int set_tim_opt_long_cfg(int c, const char *optarg, int index)
{
	const struct option *the_option = &(longopts[index]);
	char *arg;
	
	if (c == '?')	/* getopt_long failed parsing */
		parse_opt_fail(optarg);
	else if (c < TIM_OPT_FIRST)
		return set_tim_opt_short_cfg(c, optarg);
	if (! strncmp(the_option->name, "no-", 3))
		arg = "no";		/* `reverse' switch */
	else
		arg = optarg;
	switch (c) {
	case TIM_OPT_CONFIG_FILE:
		return parse_opt_c(arg);
	default:
		return 0; //dummy call
	}
}
#endif

static inline int parse_opt_A(const char *arg)
{
	/* amplify volume by n percent */
	if (!arg) return amplification;
	return set_val_i32(&amplification, atoi(arg), 0, MAX_AMPLIFICATION,
			"Amplification");
}

static inline int parse_opt_master_volume(const char *arg)
{
	/* amplify volume by n percent */
	if (!arg) return output_amplification;
	return set_val_i32(&output_amplification, atoi(arg), 0, MAX_AMPLIFICATION,
			"Output Amplification");
}

static inline int parse_opt_drum_power(const char *arg)
{
	/* --drum-power */
	if (!arg) return opt_drum_power;
	return set_val_i32(&opt_drum_power, atoi(arg), 0, MAX_AMPLIFICATION,
			"Drum power");
}

static inline int parse_opt_volume_comp(const char *arg)
{
	/* --[no-]volume-compensation */
	if (!arg) return 0;
	opt_amp_compensation = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_a(const char *arg)
{
	if (!arg) return 0;
	antialiasing_allowed = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_B(const char *arg)
{
	/* --buffer-fragments */
	const char *p;
	if (!arg) return 0;
	
	/* num */
	if (*arg != ',') {
		if (set_value(&opt_buffer_fragments, atoi(arg), 0, 4096,
				"Buffer Fragments (num)"))
			return 1;
	}
	/* bits */
	if ((p = strchr(arg, ',')) != NULL) {
		if (set_value(&opt_audio_buffer_bits, atoi(++p), 1, AUDIO_BUFFER_BITS,
				"Buffer Fragments (bit)"))
			return 1;
	}
	return 0;
}

static inline int parse_opt_C(const char *arg)
{
	if (!arg) return 0;
	if (set_val_i32(&control_ratio, atoi(arg), 0, MAX_CONTROL_RATIO,
			"Control ratio"))
		return 1;
	opt_control_ratio = control_ratio;
	return 0;
}

static inline int parse_opt_c(const char *arg)
{
	if (!arg) return 0;
#ifdef __W32__
	if (got_a_configuration == 1)
		return 0;
#endif
	if (read_config_file(arg, 0, 0))
		return 1;
	got_a_configuration = 1;
	return 0;
}

static inline int parse_opt_D(const char *arg)
{
	if (!arg) return 0;
	return set_channel_flag(&default_drumchannels, atoi(arg), "Drum channel");
}

static inline int parse_opt_d(const char *arg)
{
	/* dynamic lib root */
#ifdef IA_DYNAMIC
	safe_free(dynamic_lib_root);
	if (!arg) arg = ".";
	dynamic_lib_root = safe_strdup(arg);
	return 0;
#else
	ctl->cmsg(CMSG_WARNING, VERB_NOISY, "-d option is not supported");
	return 1;
#endif	/* IA_DYNAMIC */
}

static inline int parse_opt_E(const char *arg)
{
	/* undocumented option --ext */
	int err = 0;
	if (!arg) return err;
	
	while (*arg) {
		switch (*arg) {
		case 'w':
			opt_modulation_wheel = 1;
			break;
		case 'W':
			opt_modulation_wheel = 0;
			break;
		case 'p':
			opt_portamento = 1;
			break;
		case 'P':
			opt_portamento = 0;
			break;
		case 'v':
			opt_nrpn_vibrato = 1;
			break;
		case 'V':
			opt_nrpn_vibrato = 0;
			break;
		case 's':
			opt_channel_pressure = 1;
			break;
		case 'S':
			opt_channel_pressure = 0;
			break;
		case 'e':
			opt_modulation_envelope = 1;
			break;
		case 'E':
			opt_modulation_envelope = 0;
			break;
		case 't':
			opt_trace_text_meta_event = 1;
			break;
		case 'T':
			opt_trace_text_meta_event = 0;
			break;
		case 'o':
			opt_overlap_voice_allow = 1;
			break;
		case 'O':
			opt_overlap_voice_allow = 0;
			break;
		case 'z':
			opt_temper_control = 1;
			break;
		case 'Z':
			opt_temper_control = 0;
			break;
//elion
		case 'd':
			opt_drum_effect = 1;
			break;
		case 'D':
			opt_drum_effect = 0;
			break;
		case 'j':
			opt_insertion_effect = 1;
			break;
		case 'J':
			opt_insertion_effect = 0;
			break;
		case 'q':
			opt_eq_control = 1;
			break;
		case 'Q':
			opt_eq_control = 0;
			break;
		case 'x':
			opt_tva_attack = 1;
			opt_tva_decay = 1;
			opt_tva_release = 1;
			break;
		case 'X':
			opt_tva_attack = 0;
			opt_tva_decay = 0;
			opt_tva_release = 0;
			break;
		case 'c':
			opt_delay_control = 1;
			break;
		case 'C':
			opt_delay_control = 0;
			break;

		case 'm':
			if (parse_opt_default_mid(arg + 1))
				err++;
			arg += 2;
			break;
		case 'M':
			if (parse_opt_system_mid(arg + 1))
				err++;
			arg += 2;
			break;
		case 'b':
			if (parse_opt_default_bank(arg + 1))
				err++;
			while (isdigit(*(arg + 1)))
				arg++;
			break;
		case 'B':
			if (parse_opt_force_bank(arg + 1))
				err++;
			while (isdigit(*(arg + 1)))
				arg++;
			break;
		case 'i':
			if (parse_opt_default_program(arg + 1))
				err++;
			while (isdigit(*(arg + 1)) || *(arg + 1) == '/')
				arg++;
			break;
		case 'I':
			if (parse_opt_force_program(arg + 1))
				err++;
			while (isdigit(*(arg + 1)) || *(arg + 1) == '/')
				arg++;
			break;
		case 'F':
			if (strncmp(arg + 1, "delay=", 6) == 0) {
				if (parse_opt_delay(arg + 7))
					err++;
			} else if (strncmp(arg + 1, "chorus=", 7) == 0) {
				if (parse_opt_chorus(arg + 8))
					err++;
			} else if (strncmp(arg + 1, "reverb=", 7) == 0) {
				if (parse_opt_reverb(arg + 8))
					err++;
			} else if (strncmp(arg + 1, "ns=", 3) == 0) {
				if (parse_opt_noise_shaping(arg + 4))
					err++;
			} else if (strncmp(arg + 1, "vlpf=", 5) == 0) {
				if (parse_opt_voice_lpf(arg + 6))
					err++;
#ifndef FIXED_RESAMPLATION
			} else if (strncmp(arg + 1, "resamp=", 7) == 0) {
				if (parse_opt_resample(arg + 8))
					err++;
#endif
			}
			if (err) {
				ctl->cmsg(CMSG_ERROR,
						VERB_NORMAL, "-E%s: unsupported effect", arg);
				return err;
			}
			return err;
		default:
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"-E: Illegal mode `%c'", *arg);
			err++;
			break;
		}
		arg++;
	}
	return err;
}

static inline int parse_opt_mod_wheel(const char *arg)
{
	/* --[no-]mod-wheel */
	if (!arg) return 0;
	opt_modulation_wheel = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_portamento(const char *arg)
{
	/* --[no-]portamento */
	if (!arg) return 0;
	opt_portamento = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_vibrato(const char *arg)
{
	/* --[no-]vibrato */
	if (!arg) return 0;
	opt_nrpn_vibrato = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_ch_pressure(const char *arg)
{
	/* --[no-]ch-pressure */
	if (!arg) return 0;
	opt_channel_pressure = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_mod_env(const char *arg)
{
	/* --[no-]mod-envelope */
	if (!arg) return 0;
	opt_modulation_envelope = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_trace_text(const char *arg)
{
	/* --[no-]trace-text-meta */
	if (!arg) return 0;
	opt_trace_text_meta_event = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_overlap_voice(const char *arg)
{
	/* --[no-]overlap-voice */
	if (!arg) return 0;
	opt_overlap_voice_allow = y_or_n_p(arg);
	return 0;
}
///r
static inline int parse_opt_overlap_voice_count(const char *arg)
{
	/* --overlap-voice-count */
	int val;
	if (!arg) return 0;
	val = atoi(arg);

	if (! val || val < 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Voice Overlap Count: Illegal value");
		return 1;
	}
	opt_overlap_voice_count = val;
	return 0;
}

static inline int parse_opt_max_channel_voices(const char *arg)
{
	/* --max_channel_voices */
	int val;
	if (!arg) return 0;
	val = atoi(arg);

	if (! val || val < 4 || val > 512) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Max Channel Voices: Illegal value");
		return 1;
	}
	opt_max_channel_voices = val;
	return 0;
}

static inline int parse_opt_temper_control(const char *arg)
{
	/* --[no-]temper-control */
	if (!arg) return 0;
	opt_temper_control = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_default_mid(const char *arg)
{
	/* --default-mid */
	int val;
	if (!arg) return 0;
	val = str2mID(arg);
	
	if (! val) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Manufacture ID: Illegal value");
		return 1;
	}
	opt_default_mid = val;
	return 0;
}

static inline int parse_opt_system_mid(const char *arg)
{
	/* --system-mid */
	int val;
	if (!arg) return 0;
	val = str2mID(arg);
	
	if (! val) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Manufacture ID: Illegal value");
		return 1;
	}
	opt_system_mid = val;
	return 0;
}

static inline int parse_opt_default_bank(const char *arg)
{
	/* --default-bank */
	if (!arg) return 0;
	if (set_value(&default_tonebank, atoi(arg), 0, 0x7f, "Bank number"))
		return 1;
	special_tonebank = -1;
	return 0;
}

static inline int parse_opt_force_bank(const char *arg)
{
	/* --force-bank */
	if (!arg) return 0;
	if (set_value(&special_tonebank, atoi(arg), 0, 0x7f, "Bank number"))
		return 1;
	return 0;
}

static inline int parse_opt_default_program(const char *arg)
{
	/* --default-program */
	int prog, i;
	const char *p;
	if (!arg) return 0;
	
	if (set_value(&prog, atoi(arg), 0, 0x7f, "Program number"))
		return 1;
	if ((p = strchr(arg, '/')) != NULL) {
		if (set_value(&i, atoi(++p), 1, MAX_CHANNELS, "Program channel"))
			return 1;
		default_program[i - 1] = prog;
	} else
		for (i = 0; i < MAX_CHANNELS; i++)
			default_program[i] = prog;
	return 0;
}

static inline int parse_opt_force_program(const char *arg)
{
	/* --force-program */
	const char *p;
	int i;
	if (!arg) return 0;
	
	if (set_value(&def_prog, atoi(arg), 0, 0x7f, "Program number"))
		return 1;
	if (ctl->opened)
		set_default_program(def_prog);
	if ((p = strchr(arg, '/')) != NULL) {
		if (set_value(&i, atoi(++p), 1, MAX_CHANNELS, "Program channel"))
			return 1;
		default_program[i - 1] = SPECIAL_PROGRAM;
	} else
		for (i = 0; i < MAX_CHANNELS; i++)
			default_program[i] = SPECIAL_PROGRAM;
	return 0;
}
///r
static inline int set_default_program(int prog)
{
	int bank;
	Instrument *ip;
	
	bank = (special_tonebank >= 0) ? special_tonebank : default_tonebank;
	if ((ip = play_midi_load_instrument(0, bank, prog, 0, NULL)) == NULL) // elm=0
		return 1;
	default_instrument = ip;
	return 0;
}
///r
static inline int parse_opt_delay(const char *arg)
{
	/* --delay */
	const char *p;
	if (!arg) return 0;
	
	switch (*arg) {
	case '0':
	case 'd':	/* disable */
		opt_delay_control = 0;
		return 0;
	case '1':
	case 'D':	/* normal */
		opt_delay_control = 1;
		return 0;
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid delay parameter.");
		return 1;
	}
	return 0;
}

static inline int parse_opt_chorus(const char *arg)
{
	/* --chorus */
	const char *p;
	if (!arg) return 0;
	
	switch (*arg) {
	case '0':
	case 'd':	/* disable */
		opt_chorus_control = 0;
		opt_surround_chorus = 0;
		break;
	case '1':
	case 'n':	/* normal */
	case '2':
	case 's':	/* surround */
		opt_surround_chorus = (*arg == '2' || *arg == 's') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 0;
		} else {
 			opt_chorus_control = 1;
			opt_normal_chorus_plus = 0;
		}
		break;
	case '3':
	case 'w':
	case '4':
	case 'W':
		opt_surround_chorus = (*arg == '4' || *arg == 'W') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 1;
		} else {
			opt_chorus_control = 1;
			opt_normal_chorus_plus = 1;
		}
		break;
	case '5':
	case 'b':
	case '6':
	case 'B':
		opt_surround_chorus = (*arg == '6' || *arg == 'B') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 2;
		} else {
			opt_chorus_control = 1;
			opt_normal_chorus_plus = 2;
		}
		break;
	case '7':
	case 't':
	case '8':
	case 'T':
		opt_surround_chorus = (*arg == '8' || *arg == 'T') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 3;
		} else {
			opt_chorus_control = 1;
			opt_normal_chorus_plus = 3;
		}
		break;
	case '9':
	case 'h':
	case 'H':
		opt_surround_chorus = ( *arg == 'H') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 4;
		} else {
			opt_chorus_control = 1;
			opt_normal_chorus_plus = 4;
		}
		break;
	case 'e':
	case 'E':
		opt_surround_chorus = ( *arg == 'E') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 5;
		} else {
			opt_chorus_control = 1;
			opt_normal_chorus_plus = 5;
		}
		break;
#if VSTWRAP_EXT
	case 'v':	/* chorus VST */
	case 'V':
		opt_surround_chorus = ( *arg == 'V') ? 1 : 0;
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_chorus_control, atoi(++p), 0, 0x7f,
					"Chorus level"))
				return 1;
			opt_chorus_control = -opt_chorus_control;
			opt_normal_chorus_plus = 6;
		} else {
			opt_chorus_control = 1;
			opt_normal_chorus_plus = 6;
		}
		break;
#endif
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid chorus parameter.");
		return 1;
	}
	return 0;
}

static inline int parse_opt_reverb(const char *arg)
{
	/* --reverb */
	const char *p;
	if (!arg) return 0;
	
	/* option       action                  opt_reverb_control
	 * reverb=0     no reverb                 0
	 * reverb=1     old reverb                1
	 * reverb=1,n   set reverb level to n   (-1 to -127)
	 * reverb=2     "global" old reverb       2
	 * reverb=2,n   set reverb level to n   (-1 to -127) - 128
	 * reverb=3     new reverb                3
	 * reverb=3,n   set reverb level to n   (-1 to -127) - 256
	 * reverb=4     "global" new reverb       4
	 * reverb=4,n   set reverb level to n   (-1 to -127) - 384
	 * 
	 * I think "global" was meant to apply a single global reverb,
	 * without applying any reverb to the channels.  The do_effects()
	 * function in effects.c looks like a good way to do this.
	 * 
	 * This is NOT the "correct" way to implement global reverb, we should
	 * really make a new variable just for that.  But if opt_reverb_control
	 * is already used in a similar fashion, rather than creating a new
	 * variable for setting the channel reverb levels, then I guess
	 * maybe this isn't so bad....  It would be nice to create new
	 * variables for both global reverb and channel reverb level settings
	 * in the future, but this will do for now.
	 */
	
	switch (*arg) {
	case '0':
	case 'd':	/* disable */
		opt_reverb_control = 0;
		break;
	case '1':
	case 'n':	/* normal */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control;
		} else
			opt_reverb_control = 1;
		break;
	case '2':
	case 'g':	/* global */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 128;
		} else
			opt_reverb_control = 2;
		break;
	case '3':
	case 'f':	/* freeverb */
		return parse_opt_reverb_freeverb(arg, 'f');
	case '4':
	case 'G':	/* global freeverb */
		return parse_opt_reverb_freeverb(arg, 'G');
	case '5':	/* reverb ex */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 512;
		} else
			opt_reverb_control = 5;		
		break;
	case '6':	/* global reverb ex */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 640;
		} else
			opt_reverb_control = 6;
		break;
	case '7':	/* reverb ex2 */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 768;
		} else
			opt_reverb_control = 7;		
		break;
	case '8':	/* global reverb ex2 */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 896;
		} else
			opt_reverb_control = 8;
		break;
#if VSTWRAP_EXT
	case 'v': /* reverb VST */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 1024;
		} else
			opt_reverb_control = 9;		
		break;
	case 'V': /* global reverb VST */
		if ((p = strchr(arg, ',')) != NULL) {
			if (set_value(&opt_reverb_control, atoi(++p), 1, 0x7f,
					"Reverb level"))
				return 1;
			opt_reverb_control = -opt_reverb_control - 1152;
		} else
			opt_reverb_control = 10;
		break;
#endif
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid reverb parameter.");
		return 1;
	}
	return 0;
}

static int parse_opt_reverb_freeverb(const char *arg, char type)
{
	const char *p;
	if (!arg) return 0;
	
	if ((p = strchr(arg, ',')) != NULL)
		p++;
	else
		p = "";
	/* reverb level */
	if (*p && *p != ',') {
		if (set_value(&opt_reverb_control, atoi(p), 1, 0x7f,
				"Reverb level"))
			return 1;
		if (type == 'f')
			opt_reverb_control = -opt_reverb_control - 256;
		else
			opt_reverb_control = -opt_reverb_control - 384;
	} else
		opt_reverb_control = (type == 'f') ? 3 : 4;
	if ((p = strchr(p, ',')) == NULL)
		return 0;
	p++;
	/* ranges 0..10 below determined just to reject an extreme value */
	/* scaleroom */
	if (*p && *p != ',') {
		if (parse_val_float_t(&freeverb_scaleroom, p, 0, 10,
				"Freeverb scaleroom"))
			return 1;
	}
	if ((p = strchr(p, ',')) == NULL)
		return 0;
	p++;
	/* offsetroom */
	if (*p && *p != ',') {
		if (parse_val_float_t(&freeverb_offsetroom, p, 0, 10,
				"Freeverb offsetroom"))
			return 1;
	}
	if ((p = strchr(p, ',')) == NULL)
		return 0;
	p++;
	/* predelay factor */
	if (*p && *p != ',') {
		int value;

		if (set_val_i32(&value, atoi(p), 0, 1000,
				"Freeverb predelay factor"))
			return 1;
		reverb_predelay_factor = value * DIV_100;
	}
	return 0;
}

static inline int parse_opt_voice_lpf(const char *arg)
{
	/* --voice-lpf */
	if (!arg) return 0;
	switch (*arg) {
	case '0':
	case 'd':	/* disable */
		opt_lpf_def = 0;
		break;
	case '1':
	case 'c':	/* chamberlin */
		opt_lpf_def = 1;
		break;
	case '2':
	case 'm':	/* moog */
		opt_lpf_def = 2;
		break;
	case '3':
	case 'b':	/* ButterworthFilter */
		opt_lpf_def = 3;
		break;
	case '4':	/* Resonant IIR */
	case 'i':
		opt_lpf_def = 4;
		break;
	case '5':	/* amSynth */
	case 'a':
		opt_lpf_def = 5;
		break;
	case '6':	/* 1 pole 6db/oct */
	case 'o':
		opt_lpf_def = 6;
		break;
	case '7':	/* LPF18 18db/oct */
	case 'e':
		opt_lpf_def = 7;
		break;
	case '8':	/* two first order low-pass filter */
	case 't':
		opt_lpf_def = 8;
		break;
	case '9':	/* HPF ButterworthFilter */
	case 'h':
		opt_lpf_def = 9;
		break;
	case 'B':	/* BPF ButterworthFilter */
		opt_lpf_def = 10;
		break;
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid voice LPF type %s", arg);
		return 1;
	}
	return 0;
}

static inline int parse_opt_voice_hpf(const char *arg)
{
	/* --voice-hpf */
	if (!arg) return 0;
	switch (*arg) {
	case '0':
	case 'd':	/* disable */
		opt_hpf_def = 0;
		break;
	case '1':
	case 'b':	/* ButterworthFilter hpf */
		opt_hpf_def = 1;
		break;
	case '2':
	case 'c':	/* lpf12-3 */
		opt_hpf_def = 2;
		break;
	case '3':
	case 'o':	/* 1 pole 6db/oct */
		opt_hpf_def = 3;
		break;
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid voice HPF type %s", arg);
		return 1;
	}
	return 0;
}

/* Noise Shaping filter from
 * Kunihiko IMAI <imai@leo.ec.t.kanazawa-u.ac.jp>
 */
static inline int parse_opt_noise_shaping(const char *arg)
{
	/* --noise-shaping */
	if (!arg) return 0;
	if (set_value(&noise_sharp_type, atoi(arg), 0, 4, "Noise shaping type"))
		return 1;
	return 0;
}

static inline int parse_opt_resample(const char *arg)
{
	int num;
	/* --resample */
	if (!arg) return 0;
	num = atoi(arg);
	if(num > RESAMPLE_MAX - 1){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid resample type %s", arg);
		opt_resample_type = DEFAULT_RESAMPLATION_NUM;
		return 1;
	}else if(num > 1){
		opt_resample_type = num;
		return 0;
	}
	switch (*arg) {
	case '0':
	case 'd':	/* disable */
		opt_resample_type = RESAMPLE_NONE;
		break;
	case '1':
	case 'l':	/* linear */
		opt_resample_type = RESAMPLE_LINEAR;
		break;
	case '2':
	case 'c':	/* cspline */
		opt_resample_type = RESAMPLE_CSPLINE;
		break;
	case '3':
	case 'L':	/* lagrange */
		opt_resample_type = RESAMPLE_LAGRANGE;
		break;
	case '4':
	case 'n':	/* newton */
		opt_resample_type = RESAMPLE_NEWTON;
		break;
	case '5':
	case 'g':	/* guass */
		opt_resample_type = RESAMPLE_GAUSS;
		break;
///r
	case '6':
	case 's':	/* sharp */
		opt_resample_type = RESAMPLE_SHARP;
		break;
	case '7':
	case 'p':	/* linear % */
		opt_resample_type = RESAMPLE_LINEAR_P;
		break;
	case '8':	/* sine */
		opt_resample_type = RESAMPLE_SINE;
		break;
	case '9':	/* square */
		opt_resample_type = RESAMPLE_SQUARE;
		break;
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid resample type %s", arg);
		opt_resample_type = DEFAULT_RESAMPLATION_NUM;
		return 1;
	}
	return 0;
}

static inline int parse_opt_e(const char *arg)
{
	/* evil */
#ifdef __W32__
	opt_evil_mode = 1;
	return 0;
#else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "-e option is not supported");
	return 1;
#endif /* __W32__ */
}

static inline int parse_opt_F(const char *arg)
{
	if (!arg) return 0;
	adjust_panning_immediately = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_f(const char *arg)
{
	if (!arg) return 0;
	fast_decay = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_G(const char *arg)
{
	/* play just sub-segment(s) (seconds) */
	TimeSegment *sp;
	const char *p = arg;
	int prev_end;
	
	if (strchr(arg, 'm'))
		return parse_opt_G1(arg);
	if (time_segments == NULL) {
		time_segments = (TimeSegment *) safe_malloc(sizeof(TimeSegment));
		time_segments->type = 0;
		if (parse_segment(time_segments, p)) {
			free_time_segments();
			return 1;
		}
		time_segments->prev = time_segments->next = NULL, sp = time_segments;
	} else {
		for (sp = time_segments; sp->next != NULL; sp = sp->next)
			;
		sp->next = (TimeSegment *) safe_malloc(sizeof(TimeSegment));
		sp->next->type = 0;
		if (parse_segment(sp->next, p)) {
			free_time_segments();
			return 1;
		}
		sp->next->prev = sp, sp->next->next = NULL, sp = sp->next;
	}
	while ((p = strchr(p, ',')) != NULL) {
		sp->next = (TimeSegment *) safe_malloc(sizeof(TimeSegment));
		sp->next->type = 0;
		if (parse_segment(sp->next, ++p)) {
			free_time_segments();
			return 1;
		}
		sp->next->prev = sp, sp->next->next = NULL, sp = sp->next;
	}
	prev_end = -1;
	for (sp = time_segments; sp != NULL; sp = sp->next) {
		if (sp->type != 0)
			continue;
		if (sp->begin.s <= prev_end) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Segments must be ordered");
			free_time_segments();
			return 1;
		} else if (sp->end.s != -1 && sp->begin.s >= sp->end.s) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Segment time must be ordered");
			free_time_segments();
			return 1;
		}
		prev_end = sp->end.s;
	}
	return 0;
}

static inline int parse_opt_G1(const char *arg)
{
	/* play just sub-segment(s) (measure) */
	TimeSegment *sp;
	const char *p = arg;
	int prev_end_meas, prev_end_beat;
	
	if (time_segments == NULL) {
		time_segments = (TimeSegment *) safe_malloc(sizeof(TimeSegment));
		time_segments->type = 1;
		if (parse_segment2(time_segments, p)) {
			free_time_segments();
			return 1;
		}
		time_segments->prev = time_segments->next = NULL, sp = time_segments;
	} else {
		for (sp = time_segments; sp->next != NULL; sp = sp->next)
			;
		sp->next = (TimeSegment *) safe_malloc(sizeof(TimeSegment));
		sp->next->type = 1;
		if (parse_segment2(sp->next, p)) {
			free_time_segments();
			return 1;
		}
		sp->next->prev = sp, sp->next->next = NULL, sp = sp->next;
	}
	while ((p = strchr(p, ',')) != NULL) {
		sp->next = (TimeSegment *) safe_malloc(sizeof(TimeSegment));
		sp->next->type = 1;
		if (parse_segment2(sp->next, ++p)) {
			free_time_segments();
			return 1;
		}
		sp->next->prev = sp, sp->next->next = NULL, sp = sp->next;
	}
	prev_end_meas = prev_end_beat = -1;
	for (sp = time_segments; sp != NULL; sp = sp->next) {
		if (sp->type != 1)
			continue;
		if (sp->begin.m.meas * 16 + sp->begin.m.beat
				<= prev_end_meas * 16 + prev_end_beat) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Segments must be ordered");
			free_time_segments();
			return 1;
		} else if (sp->end.m.meas != -1 && sp->end.m.beat != -1
				&& sp->begin.m.meas * 16 + sp->begin.m.beat
				>= sp->end.m.meas * 16 + sp->end.m.beat) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Segment time must be ordered");
			free_time_segments();
			return 1;
		}
		prev_end_meas = sp->end.m.meas, prev_end_beat = sp->end.m.beat;
	}
	return 0;
}

static int parse_segment(TimeSegment *seg, const char *p)
{
	const char *q;
	
	if (*p == '-')
		seg->begin.s = 0;
	else if (parse_time(&seg->begin.s, p))
		return 1;
	p = ((q = strchr(p, '-')) == NULL) ? p + strlen(p) : q + 1;
	if (*p == ',' || *p == '\0')
		seg->end.s = -1;
	else if (parse_time(&seg->end.s, p))
		return 1;
	return 0;
}

static int parse_segment2(TimeSegment *seg, const char *p)
{
	const char *q;
	
	if (*p == '-')
		seg->begin.m.meas = seg->begin.m.beat = 1;
	else if (parse_time2(&seg->begin.m, p))
		return 1;
	p = ((q = strchr(p, '-')) == NULL) ? p + strlen(p) : q + 1;
	if (*p == ',' || *p == 'm')
		seg->end.m.meas = seg->end.m.beat = -1;
	else if (parse_time2(&seg->end.m, p))
		return 1;
	return 0;
}

static int parse_time(FLOAT_T *param, const char *p)
{
	const char *p1, *p2, *p3;
	int min;
	FLOAT_T sec;
	
	p1 = ((p1 = strchr(p, ':')) == NULL) ? p + strlen(p) : p1;
	p2 = ((p2 = strchr(p, '-')) == NULL) ? p + strlen(p) : p2;
	p3 = ((p3 = strchr(p, ',')) == NULL) ? p + strlen(p) : p3;
	if ((p1 < p2 && p2 <= p3) || (p1 < p3 && p3 <= p2)) {
		if (set_value(&min, atoi(p), 0, 59, "Segment time (min part)"))
			return 1;
		if (parse_val_float_t(&sec, p1 + 1, 0, 59.999,
				"Segment time (sec+frac part)"))
			return 1;
		*param = min * 60 + sec;
	} else if (parse_val_float_t(param, p, 0, 3599.999, "Segment time"))
		return 1;
	return 0;
}

static int parse_time2(Measure *param, const char *p)
{
	const char *p1, *p2, *p3;
	
	if (set_value(&param->meas, atoi(p), 0, 999, "Segment time (measure)"))
		return 1;
	p1 = ((p1 = strchr(p, '.')) == NULL) ? p + strlen(p) : p1;
	p2 = ((p2 = strchr(p, '-')) == NULL) ? p + strlen(p) : p2;
	p3 = ((p3 = strchr(p, ',')) == NULL) ? p + strlen(p) : p3;
	if ((p1 < p2 && p2 <= p3) || (p1 < p3 && p3 <= p2)) {
		if (set_value(&param->beat, atoi(p1 + 1), 1, 15,
				"Segment time (beat)"))
			return 1;
	} else
		param->beat = 1;
	return 0;
}

static inline int parse_opt_g(const char *arg)
{
#ifdef SUPPORT_SOUNDSPEC
	if (!arg) return 0;
	spectrogram_update_sec = atof(arg);
	if (spectrogram_update_sec <= 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"Invalid -g argument: `%s'", arg);
		return 1;
	}
	view_soundspec_flag = 1;
	return 0;
#else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "-g option is not supported");
	return 1;
#endif	/* SUPPORT_SOUNDSPEC */
}

static inline int parse_opt_H(const char *arg)
{
	/* force keysig (number of sharp/flat) */
	int keysig;
	if (!arg) return 0;
	
	if (set_value(&keysig, atoi(arg), -7, 7,
			"Force keysig (number of sHarp(+)/flat(-))"))
		return 1;
	opt_force_keysig = keysig;
	return 0;
}

__attribute__((noreturn))
#ifndef __BORLANDC__
static inline int parse_opt_h(const char *arg)
#else
static int parse_opt_h(const char *arg)
#endif
{
	static char *help_list[] = {
"TiMidity++ %s (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>",
"The original version (C) 1995 Tuukka Toivonen <tt@cgs.fi>",
"TiMidity is free software and comes with ABSOLUTELY NO WARRANTY.",
"",
#ifdef __W32__
"Win32 version by Davide Moretti <dave@rimini.com>",
"              and Daisuke Aoki <dai@y7.net>",
"",
#endif /* __W32__ */
"Usage:",
"  %s [options] filename [...]",
"",
#ifndef __W32__		/*does not work in Windows */
"  Use \"-\" as filename to read a MIDI file from stdin",
"",
#endif
"Options:",
"  -A n,m     --volume=n, --drum-power=m",
"               Amplify volume by n percent (may cause clipping),",
"                 and amplify drum power by m percent",
"     (a)     --[no-]volume-compensation",
"               Toggle amplify compensation (disabled by default)",
"  -a         --[no-]anti-alias",
"               Enable the anti-aliasing filter",
"  -B n,m     --buffer-fragments=n,m",
"               Set number of buffer fragments(n), and buffer size(2^m)",
"  -C n       --control-ratio=n",
"               Set ratio of sampling and control frequencies (0...255)",
"  -c file    --config-file=file",
"               Read extra configuration file",
"  -D n       --drum-channel=n",
"               Play drums on channel n",
#ifdef IA_DYNAMIC
"  -d path    --interface-path=path",
"               Set dynamic interface module directory",
#endif /* IA_DYNAMIC */
"  -E mode    --ext=mode",
"               TiMidity sequencer extensional modes:",
"                 mode = w/W : Enable/Disable Modulation wheel",
"                        p/P : Enable/Disable Portamento",
"                        v/V : Enable/Disable NRPN Vibrato",
"                        s/S : Enable/Disable Channel pressure",
"                        e/E : Enable/Disable Modulation Envelope",
"                        t/T : Enable/Disable Trace Text Meta Event at playing",
"                        o/O : Enable/Disable Overlapped voice",
"                        z/Z : Enable/Disable Temperament control",
"                        j/J : Enable/Disable Insertion effect",
"                        q/Q : Enable/Disable EQ",
"                        d/D : Enable/Disable Drumpart effect",
"                        x/X : Enable/Disable TVA envelope control",
"                        c/C : Enable/Disable CC#94 delay(celeste) effect",
"                        m<HH>: Define default Manufacture ID <HH> in two hex",
"                        M<HH>: Define system Manufacture ID <HH> in two hex",
"                        b<n>: Use tone bank <n> as the default",
"                        B<n>: Always use tone bank <n>",
"                        i<n/m>: Use program <n> on channel <m> as the default",
"                        I<n/m>: Always use program <n> on channel <m>",
"                        F<args>: For effect.  See below for effect options",
"                   default: -E "
#ifdef MODULATION_WHEEL_ALLOW
"w"
#else
"W"
#endif /* MODULATION_WHEEL_ALLOW */
#ifdef PORTAMENTO_ALLOW
"p"
#else
"P"
#endif /* PORTAMENTO_ALLOW */
#ifdef NRPN_VIBRATO_ALLOW
"v"
#else
"V"
#endif /* NRPN_VIBRATO_ALLOW */
#ifdef GM_CHANNEL_PRESSURE_ALLOW
"s"
#else
"S"
#endif /* GM_CHANNEL_PRESSURE_ALLOW */
#ifdef MODULATION_ENVELOPE_ALLOW
"e"
#else
"E"
#endif /* MODULATION_ENVELOPE_ALLOW */
#ifdef ALWAYS_TRACE_TEXT_META_EVENT
"t"
#else
"T"
#endif /* ALWAYS_TRACE_TEXT_META_EVENT */
#ifdef OVERLAP_VOICE_ALLOW
"o"
#else
"O"
#endif /* OVERLAP_VOICE_ALLOW */
#ifdef TEMPER_CONTROL_ALLOW
"z"
#else
"Z"
#endif /* TEMPER_CONTROL_ALLOW */
"J"
"Q"
"D"
"X"
"C"
,
#ifdef __W32__
"  -e         --evil",
"               Increase thread priority (evil) - be careful!",
#endif
"  -F         --[no-]fast-panning",
"               Disable/Enable fast panning (toggle on/off, default is on)",
"  -f         --[no-]fast-decay",
"               "
#ifdef FAST_DECAY
"Disable "
#else
"Enable "
#endif
"fast decay mode (toggle)",
"               "
#ifdef FAST_DECAY
"(default is on)",
#else
"(default is off)",
#endif
#ifdef SUPPORT_SOUNDSPEC
"  -g sec     --spectrogram=sec",
"               Open Sound-Spectrogram Window",
#endif /* SUPPORT_SOUNDSPEC */
"  -H n       --force-keysig=n",
"               Force keysig number of sHarp(+)/flat(-) (-7..7)",
"  -h         --help",
"               Display this help message",
"  -i mode    --interface=mode",
"               Select user interface (see below for list)",
#ifdef IA_ALSASEQ
"             --realtime-priority=n (for alsaseq only)",
"               Set the realtime priority (0-100)",
"             --sequencer-ports=n (for alsaseq only)",
"               Set the number of opened sequencer ports (default is 4)",
#endif
#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) || defined(IA_NPSYN) || defined(IA_W32G_SYN)
"             --rtsyn-latency=sec (for rtsyn only)",
"               Set the realtime latency (sec)",
"                 (default is 0.2 sec, minimum is 0.04 sec)",
#endif
"  -j         --[no-]realtime-load",
"               Realtime load instrument (toggle on/off)",
"  -K n       --adjust-key=n",
"               Adjust key by n half tone (-24..24)",
"  -k msec    --voice-queue=msec",
"               Specify audio queue time limit to reduce voice",
"  -L path    --patch-path=path",
"               Append dir to search path",
"  -l n       --resample-filter=n",
"                 n=0  : Disable filtration",
#if !defined(DEFAULT_RESAMPLATION_FILTER) || (DEFAULT_RESAMPLATION_FILTER == 0)
"                        (default)",
#endif
"                   1  : LPFBW (Butterworth) x1",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 1)
"                        (default)",
#endif
"                   2  : LPFBW (Butterworth) x2",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 2)
"                        (default)",
#endif
"                   3  : LPFBW (Butterworth) x3",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 3)
"                        (default)",
#endif
"                   4  : LPFBW (Butterworth) x4",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 4)
"                        (default)",
#endif
"                   5  : LPFAM (24dB/oct)-2 x1",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 5)
"                        (default)",
#endif
"                   6  : LPFAM (24dB/oct)-2 x2",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 6)
"                        (default)",
#endif
"                   7  : LPFAU x1",
#if defined(DEFAULT_RESAMPLATION_FILTER) && (DEFAULT_RESAMPLATION_FILTER == 7)
"                        (default)",
#endif
"  -M name    --pcm-file=name",
"               Specify PCM filename (*.wav or *.aiff) to be played or:",
"                 \"auto\" : Play *.mid.wav or *.mid.aiff",
"                 \"none\" : Disable this feature (default)",
"  -m msec    --decay-time=msec",
"               Minimum time for a full volume sustained note to decay,",
"                 0 disables",
"  -N n       --interpolation=n",
"               Set the interpolation parameter (depends on -EFresamp option)",
"                 Linear interpolation is used if audio queue < 99%s",
"                 cspline, lagrange:",
"                   Toggle 4-point interpolation (default on)",
"                   (off: 0, on: 1)",
"                 newton:",
"                   n'th order Newton polynomial interpolation, n=1-45 odd",
"                   (default 11)",
"                 gauss:",
"                   n+1 point Gauss-like interpolation, n=2-32 (default 24)",
"                 sharp:",
"                   n+1 point Sharp interpolation, n=2-8 (default 6)",
"                 linearP:",
"                   n=0-100 (default 100)",
"  -O mode    --output-mode=mode",
"               Select output mode and format (see below for list)",
"             --wave-extensible (for RIFF WAVE file only)",
"               Enable WAVE_FORMAT_EXTENSIBLE tag (GUID)",
"             --wave-update-step=n (for RIFF WAVE file only)",
"               Update RIFF to n KBytes per. n=0-9999 (default is 512)",
#ifdef AU_FLAC
"             --flac-verify (for FLAC / OggFLAC only)",
"               Verify a correct encoding",
"             --flac-padding=n (for FLAC / OggFLAC only)",
"               Write a PADDING block of length n",
"             --flac-complevel=n (for FLAC / OggFLAC only)",
"               Set compression level n:[0..8]",
#ifdef AU_OGGFLAC
"             --oggflac (for Ogg FLAC only)",
"               Output OggFLAC stream (experimental)",
#endif /* AU_OGGFLAC */
#endif /* AU_FLAC */
#ifdef AU_OPUS
"             --opus-nframes=n (for Ogg Opus only)",
"               Number of frames per Opus packet",
"               n:[120, 240, 480, 960, 1920, 2880] (default is 960)",
"             --opus-bitrate=n (for Ogg Opus only)",
"               Encoding average bit-rate n:[5-512,513-512000] (default is 128)",
"             --opus-complexity=n (for Ogg Opus only)",
"               Set encoding complexity n:[0..10] (default is 10)",
"             --[no-]opus-vbr (for Ogg Opus only)",
"               Enable variable bit-rate (VBR) (default on)",
"             --[no-]opus-cvbr (for Ogg Opus only)",
"               Enable constrained variable bit-rate (CVBR) (default on)",
#endif /* AU_OPUS */
#ifdef AU_SPEEX
"             --speex-quality=n (for Ogg Speex only)",
"               Encoding quality n:[0..10]",
"             --speex-vbr (for Ogg Speex only)",
"               Enable variable bit-rate (VBR)",
"             --speex-abr=n (for Ogg Speex only)",
"               Enable average bit-rate (ABR) at rate bps",
"             --speex-vad (for Ogg Speex only)",
"               Enable voice activity detection (VAD)",
"             --speex-dtx (for Ogg Speex only)",
"               Enable file-based discontinuous transmission (DTX)",
"             --speex-complexity=n (for Ogg Speex only)",
"               Set encoding complexity n:[0-10]",
"             --speex-nframes=n (for Ogg Speex only)",
"               Number of frames per Ogg packet n:[0-10]",
#endif
"             --output-device-id=n",
#ifdef AU_W32
"             --wmme-device-id=n (for Windows only)",
"               Number of WMME device ID (-1: Default device, 0..19: other)",
"             --wave-format-ext=n , --wmme-format-ext=n (for Windows only)",
"               WMME Enable WAVE_FORMAT_EXTENSIBLE (default is 1)",
#endif
#ifdef AU_WDMKS
"             --wdmks-device-id=n (for Windows only)",
"               Number of WDMKS device ID (0..19:)",
"             --wdmks-latency=n (for Windows only)",
"               WDMKS Latency ms n=1-9999 depend device (default is 20)",
"             --wdmks-format-ext=n (for Windows only)",
"               WDMKS Enable WAVE_FORMAT_EXTENSIBLE (default is 1)",
"             --wdmks-realtime=n (for Windows only)",
"               WDMKS 0:WaveCyclic 1:WaveRT (default is 0)",
"               supported since Windows Vista",
"             --wdmks-polling=n (for Windows only)",
"               WDMKS Flags RT 0:Event 1:Polling (default is 0)",
"             --wdmks_priority=n (for Windows only)",
"               WDMKS ThreadPriority (default is 1)",
"               0:Low , 1:Normal , 2:High , 3:Exclusive",
"             --wdmks_priority-rt=n (for Windows only)",
"               WDMKS ThreadPriority RT (default is 0)",
"               0:Auto(Audio) , 1:Audio , 2:Capture , 3:Distribution , 4:Games , 5:playback , 6:ProAudio , 7:WindowManager",
#endif
#ifdef AU_WASAPI
"             --wasapi-device-id=n (for Windows only)",
"               Number of WASAPI device ID (-1: Default device, 0..19: other)",
"             --wasapi-latency=n (for Windows only)",
"               WASAPI Latency ms n=1-9999 depend device (default is 10)",
"             --wasapi-format-ext=n (for Windows only)",
"               WASAPI Enable WAVE_FORMAT_EXTENSIBLE (default is 1)",
"             --wasapi-exclusive=n (for Windows only)",
"               WASAPI 0:Shared mode 1:Exclusive mode (default is 0)",
"             --wasapi-polling=n (for Windows only)",
"               WASAPI Flags 0:Event 1:Polling (default is 0)",
"             --wasapi_priority=n (for Windows only)",
"               WASAPI ThreadPriority (default is 0)",
"               0:None , 1:Audio , 2:Capture , 3:Distribution , 4:Games , 5:playback , 6:ProAudio , 7:WindowManager",
"             --wasapi-stream-category=n (for Windows only)",
"               WASAPI StreamCategory (default is 0)",
"               0:Other , 1:None , 2:None , 3:Communications , 4:Alerts , 5:SoundEffects ,",
"               6:GameEffects , 7:GameMedia , 8:GameChat , 9:Speech , 10:Movie , 11:Media",
"               values 1,2 are deprecated on Windows 10 and not included into enumeration",
"             --wasapi-stream-option=n (for Windows only)",
"               WASAPI StreamOption 0:None 1:Raw 2:MatchFormat (default is 0)",
"               1:Raw bypass WASAPI Audio Engine DSP effects, supported since Windows 8.1",
#endif
#ifdef AU_PORTAUDIO
"             --pa-asio-device-id=n",
"               Number of PortAudio device ID (-2: Default device, 0..99: other)",
#ifdef __W32__
"             --pa-wmme-device-id=n (for Windows only)",
"               Number of PortAudio device ID (-2: Default device, 0..99: other)",
"             --pa-ds-device-id=n (for Windows only)",
"               Number of PortAudio device ID (-2: Default device, 0..99: other)",
#ifdef PORTAUDIO_V19
"             --pa-wdmks-device-id=n (for Windows only)",
"               Number of PortAudio device ID (-2: Default device, 0..99: other)",
"             --pa-wasapi-device-id=n (for Windows only)",
"               Number of PortAudio device ID (-2: Default device, 0..99: other)",
"             --pa-wasapi-flag=n",
#endif
#endif
#endif
"             --add-play-time=sec (default is 0.5)",
"             --add-silent-time=sec (default is 0.5)",
"             --emu-delay-time=sec (default is 0.1)",
#if defined(__W32__)
"             --process-priority=n (for Windows only)",
#if !defined(IA_W32G_SYN)
"             --player-thread-priority=n (for Windows only)",
#endif
#endif
"  -o file    --output-file=file",
"               Output to another file (or device/server) (Use \"-\" for stdout)",
#if defined(AU_PORTAUDIO) || defined(AU_WIN32)
"               Set the output device no. (-1 shows available device no. list)",
#endif
"  -P file    --patch-file=file",
"               Use patch file for all programs",
"  -p n       --polyphony=n",
"               Allow n-voice polyphony.  Optional auto polyphony reduction",
"     (a)     --[no-]polyphony-reduction",
"               Toggle automatic polyphony reduction.  Enabled by default",
"  -Q n[,...] --mute=n[,...]",
"               Ignore channel n (0: ignore all, -n: resume channel n)",
"     (t)     --temper-mute=n[,...]",
"               Quiet temperament type n (0..3: preset, 4..7: user-defined)",
"             --preserve-silence",
"               Do not drop initial silence.  Default: drop initial silence",
"  -q sec/n   --audio-buffer=sec/n",
"               Specify audio buffer in seconds",
"                 sec: Maxmum buffer, n: Filled to start (default is 5.0/100%s)",
"                 (size of 100%s equals device buffer size)",
"  -R msec      Pseudo reveb effect (set every instrument's release to msec)",
"                 if n=0, n is set to 800",
"  -r n       --resample-queue=n",
"               Specify audio queue time limit to reduce resample quality",
"                 n: size of 100 equals device buffer size",
"  -S n       --cache-size=n",
"               Cache size (0 means no cache)",
"  -s freq    --sampling-freq=freq",
"               Set sampling frequency to freq (Hz or kHz)",
"  -T n       --adjust-tempo=n",
"               Adjust tempo to n%s,",
"                 120=play MOD files with an NTSC Amiga's timing",
"  -t code    --output-charset=code",
"               Output text language code:",
"                 code=auto  : Auto conversion by `LANG' environment variable",
"                              (UNIX only)",
"                      ascii : Convert unreadable characters to '.' (0x2e)",
"                      nocnv : No conversion",
"                      1251  : Convert from windows-1251 to koi8-r",
#ifdef JAPANESE
"                      euc   : EUC-japan",
"                      jis   : JIS",
"                      sjis  : shift JIS",
#endif /* JAPANESE */
"  -U         --[no-]unload-instruments",
"               Unload instruments from memory between MIDI files",
#ifdef AU_VOLUME_CALC
"             --volume-calc-rms",
"               Soundfont Volume Calc output to `root mean square' format",
"             --volume-calc-trim",
"               Trim silence samples",
#endif /* AU_VOLUME_CALC */
"  -V power   --volume-curve=power",
"               Define the velocity/volume/expression curve",
"                 amp = vol^power (auto: 0, linear: 1, ideal: ~1.661, GS: ~2)",
"  -v         --version",
"               Display TiMidity version information",
"  -W mode    --wrd=mode",
"               Select WRD interface (see below for list)",
#ifdef __W32__
"  -w mode    --rcpcv-dll=mode",
"               Windows extensional modes:",
"                 mode=r/R : Enable/Disable rcpcv.dll",
#endif /* __W32__ */
"  -x str     --config-string=str",
"               Read configuration str from command line argument",
"  -Z file    --freq-table=file",
"               Load frequency table (Use \"pure\" for pure intonation)",
"  pure<n>(m) --pure-intonation=n(m)",
"               Initial keysig number <n> of sharp(+)/flat(-) (-7..7)",
"                 'm' stands for minor mode",
///r
"  --module=n",
"               Simulate behavior of specific synthesizer module by n",
"                 n=0       : TiMidity++ Default (default)",
"                   1-4     : Roland SC family",
"                   5-15    : GS family",
"                   16-23   : Yamaha MU family",
"                   24-31   : XG family",
"                   32-33   : SoundBlaster family",
"                   56-79   : LA family",
"                   80-84   : KORG family",
"                   96-98   : SD series",
"                   99-111  : other systhesizer modules",
"                   112-127 : TiMidity++ specification purposes",
" --mix-envelope=n",
" --cut-short-time=msec",
" --limiter=n (gain per)",
#ifdef SUPPORT_LOOPEVENT
"  --loop-repeat=n",
"               Set number of repeat count between CC#111 and EOT (CC#111)",
#endif /* SUPPORT_LOOPEVENT */
#ifdef ENABLE_THREAD
"  --compute-thread-num=n",
"               Set number of divide multi-threads (0..16)",
"                 (0..1 means single-thread, default is 0)",
#endif /* ENABLE_THREAD */
"  --od-level-gs=n",
"               Set GS overdrive-amplify-level (1..400:default=100)",
"  --od-drive-gs=n",
"               Set GS overdrive-drive-level (1..400:default=100)",
"  --od-level-xg=n",
"               Set XG overdrive-amplify-level (1..400:default=100)",
"  --od-drive-xg=n",
"               Set XG overdrive-drive-level (1..400:default=100)",
#if defined(PRE_RESAMPLATION) || defined(DEFAULT_PRE_RESAMPLATION)
"  --pre-resample=n",
#endif
		NULL
	};
	void show_ao_device_info(FILE *fp);
	FILE *fp;
	char version[32], *help_args[7], per_mark[2];
	int i, j;
	char *h;
	ControlMode *cmp, **cmpp;
	char mark[128];
	PlayMode *pmp, **pmpp;
	WRDTracer *wlp, **wlpp;
	
	fp = open_pager();
	strcpy(version, (!strstr(timidity_version, "current")) ? "version " : "");
	strcat(version, timidity_version);
	per_mark[0] = '%';
	per_mark[1] = '\0';
	help_args[0] = version;
	help_args[1] = program_name;
	help_args[2] = per_mark;
	help_args[3] = per_mark;
	help_args[4] = per_mark;
	help_args[5] = per_mark;
	help_args[6] = NULL;
	for (i = 0, j = 0; (h = help_list[i]) != NULL; i++) {
		if (strchr(h, '%')) {
			if (*(strchr(h, '%') + 1) != '%')
				fprintf(fp, h, help_args[j++]);
			else
				fprintf(fp, "%s", h);
		} else
			fputs(h, fp);
		fputs(NLS, fp);
	}
	fputs(NLS, fp);
	fputs("Effect options (-EF, --ext=F option):" NLS
"  -EFdelay=d   Disable delay effect (default)" NLS
"  -EFdelay=D   Enable delay effect" NLS
"  -EFchorus=d  Disable MIDI chorus effect control" NLS
"  -EFchorus=n  Enable Normal MIDI chorus effect control" NLS
"    [,level]     `level' is optional to specify chorus level [0..127]" NLS
"                 (default)" NLS
"  -EFchorus=s  Surround sound, chorus detuned to a lesser degree" NLS
"    [,level]     `level' is optional to specify chorus level [0..127]" NLS
"  -EFreverb=d  Disable MIDI reverb effect control" NLS
#if !defined(REVERB_CONTROL_ALLOW) && !defined(FREEVERB_CONTROL_ALLOW)
"                 (default)" NLS
#endif
"  -EFreverb=n  Enable Normal MIDI reverb effect control" NLS
"    [,level]     `level' is optional to specify reverb level [1..127]" NLS
#if defined(REVERB_CONTROL_ALLOW)
"                 (default)" NLS
#endif
"  -EFreverb=g  Global reverb effect" NLS
"    [,level]     `level' is optional to specify reverb level [1..127]" NLS
"  -EFreverb=f  Enable Freeverb MIDI reverb effect control" NLS
"    [,level]     `level' is optional to specify reverb level [1..127]" NLS
#if !defined(REVERB_CONTROL_ALLOW) && defined(FREEVERB_CONTROL_ALLOW)
"                 (default)" NLS
#endif
"  -EFreverb=G  Global Freeverb effect" NLS
"    [,level]     `level' is optional to specify reverb level [1..127]" NLS
"  -EFvlpf=d    Disable voice LPF" NLS
#if !defined(VOICE_MOOG_LPF_ALLOW) && !defined(VOICE_CHAMBERLIN_LPF_ALLOW)
"                 (default)" NLS
#endif
"  -EFvlpf=c    Enable Chamberlin resonant LPF (12dB/oct)" NLS
#if defined(VOICE_CHAMBERLIN_LPF_ALLOW)
"                 (default)" NLS
#endif
"  -EFvlpf=m    Enable Moog resonant lowpass VCF (24dB/oct)" NLS
#if defined(VOICE_MOOG_LPF_ALLOW) && !defined(VOICE_CHAMBERLIN_LPF_ALLOW)
"                 (default)" NLS
#endif
"  -EFvlpf=b    Enable ButterworthFilter resonant lowpass (butterworth)" NLS
"  -EFvlpf=i    Enable Resonant IIR lowpass VCF (12dB/oct)-2" NLS
"  -EFvlpf=a    Enable amSynth resonant lowpass VCF (24dB/oct)-2" NLS
"  -EFvlpf=o    Enable 1 pole 6db/oct resonant lowpass VCF (6dB/oct)" NLS
"  -EFvlpf=e    Enable resonant 3 pole lowpass VCF (18dB/oct)" NLS
"  -EFvlpf=t    Enable two first order lowpass VCF " NLS
"  -EFvlpf=h    Enable HPF ButterworthFilter VCF (butterworth)" NLS
"  -EFvlpf=B    Enable BPF ButterworthFilter VCF (butterworth)" NLS
"  -EFns=n      Enable the n'th degree (type) noise shaping filter" NLS
"                 n:[0..4] (for 8-bit linear encoding, default is 4)" NLS
"                 n:[0..4] (for 16-bit linear encoding, default is 4)" NLS
"                 n:[0] (for 24-bit linear encoding, default is 0)" NLS
"                 n:[0] (for 32-bit linear encoding, default is 0)" NLS
"                 n:[0] (for 64-bit linear encoding, default is 0)" NLS
"                 n:[0] (for float 32-bit linear encoding, default is 0)" NLS
"                 n:[0] (for float 64-bit linear encoding, default is 0)" NLS, fp);
#ifndef FIXED_RESAMPLATION
#ifdef HAVE_STRINGIZE
#define tim_str_internal(x) #x
#define tim_str(x) tim_str_internal(x)
#else
#define tim_str(x) "x"
#endif
	fputs("  -EFresamp=d  Disable resamplation", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_none"))
		fputs(" (default)", fp);
	fputs(NLS, fp);
	fputs("  -EFresamp=l  Enable Linear resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_linear"))
		fputs(" (default)", fp);
	fputs(NLS, fp);
	fputs("  -EFresamp=c  Enable C-spline resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_cspline"))
		fputs(" (default)", fp);
	fputs(NLS, fp);
	fputs("  -EFresamp=L  Enable Lagrange resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_lagrange"))
		fputs(" (default)", fp);
	fputs(NLS, fp);
	fputs("  -EFresamp=n  Enable Newton resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_newton"))
		fputs(" (default)", fp);
	fputs(NLS, fp);
	fputs("  -EFresamp=g  Enable Gauss-like resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_gauss"))
		fputs(" (default)", fp);
///r
	fputs(NLS, fp);
	fputs("  -EFresamp=s  Enable Sharp resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_sharp"))
		fputs(" (default)", fp);
	fputs(NLS, fp);
	fputs("  -EFresamp=p  Enable LinearP resample algorithm", fp);
	if (! strcmp(tim_str(DEFAULT_RESAMPLATION), "resample_linear_p"))
		fputs(" (default)", fp);
	fputs(NLS
"                 -EFresamp affects the behavior of -N option" NLS, fp);
#endif
	fputs(NLS, fp);
	fputs("Alternative TiMidity sequencer extensional mode long options:" NLS
"  --[no-]mod-wheel" NLS
"  --[no-]portamento" NLS
"  --[no-]vibrato" NLS
"  --[no-]ch-pressure" NLS
"  --[no-]mod-envelope" NLS
"  --[no-]trace-text-meta" NLS
"  --[no-]overlap-voice" NLS
///r
"  --overlap-voice-count=n" NLS
"  --[no-]temper-control" NLS
"  --default-mid=<HH>" NLS
"  --system-mid=<HH>" NLS
"  --default-bank=n" NLS
"  --force-bank=n" NLS
"  --default-program=n/m" NLS
"  --force-program=n/m" NLS
"  --delay=(d|D)" NLS
"  --chorus=(d|n|s|w|W|b|B|t|T|h|H|e|E)[,level]" NLS
"  --reverb=(d|n|g|f|G)[,level]" NLS
"  --reverb=(f|G)[,level[,scaleroom[,offsetroom[,predelay]]]]" NLS
"  --voice-lpf=(d|c|m|b|i|a|o|e|t|h|B)" NLS
"  --noise-shaping=n" NLS, fp);
#ifndef FIXED_RESAMPLATION
	fputs("  --resample=(d|l|c|L|n|g|s|p)" NLS, fp);
#endif
	fputs(NLS, fp);
	fputs("Available interfaces (-i, --interface option):" NLS, fp);
	for (cmpp = ctl_list; (cmp = *cmpp) != NULL; cmpp++)
		fprintf(fp, "  -i%c          %s" NLS,
				cmp->id_character, cmp->id_name);
#ifdef IA_DYNAMIC
	fprintf(fp, "Supported dynamic load interfaces (%s):" NLS,
			dynamic_lib_root);
	memset(mark, 0, sizeof(mark));
	for (cmpp = ctl_list; (cmp = *cmpp) != NULL; cmpp++)
		mark[(int) cmp->id_character] = 1;
	list_dyna_interface(fp, dynamic_lib_root, mark);
#endif	/* IA_DYNAMIC */
	fputs(NLS, fp);
	fputs("Interface options (append to -i? option):" NLS
"  `v'          more verbose (cumulative)" NLS
"  `q'          quieter (cumulative)" NLS
"  `t'          trace playing" NLS
"  `l'          loop playing (some interface ignore this option)" NLS
"  `r'          randomize file list arguments before playing" NLS
"  `s'          sorting file list arguments before playing" NLS, fp);
#ifdef IA_ALSASEQ
	fputs("  `D'          daemonize TiMidity++ in background "
			"(for alsaseq only)" NLS, fp);
#endif
	fputs(NLS, fp);
	fputs("Alternative interface long options:" NLS
"  --verbose=n" NLS
"  --quiet=n" NLS
"  --[no-]trace" NLS
"  --[no-]loop" NLS
"  --[no-]random" NLS
"  --[no-]sort" NLS, fp);
#ifdef IA_ALSASEQ
	fputs("  --[no-]background" NLS, fp);
#endif
	fputs(NLS, fp);
	fputs("Available output modes (-O, --output-mode option):" NLS, fp);
	for (pmpp = play_mode_list; (pmp = *pmpp) != NULL; pmpp++)
		fprintf(fp, "  -O%c          %s" NLS,
				pmp->id_character, pmp->id_name);
	fputs(NLS, fp);
	fputs("Output format options (append to -O? option):" NLS
"  `S'          stereo" NLS
"  `M'          monophonic" NLS
"  `s'          signed output" NLS
"  `u'          unsigned output" NLS
"  `1'          16-bit sample width" NLS
"  `2'          24-bit sample width" NLS
"  `3'          32-bit sample width" NLS
"  `6'          64-bit sample width" NLS
"  `8'          8-bit sample width" NLS
"  `f'          float 32-bit sample width" NLS
"  `D'          float 64-bit sample width" NLS
"  `l'          linear encoding" NLS
"  `U'          U-Law encoding" NLS
"  `A'          A-Law encoding" NLS
"  `x'          byte-swapped output" NLS, fp);
	fputs(NLS, fp);
	fputs("Alternative output format long options:" NLS
"  --output-stereo" NLS
"  --output-mono" NLS
"  --output-signed" NLS
"  --output-unsigned" NLS
"  --output-16bit" NLS
"  --output-24bit" NLS
"  --output-32bit" NLS
"  --output-64bit" NLS
"  --output-8bit" NLS
"  --output-f32bit" NLS
"  --output-float32bit" NLS
"  --output-f64bit" NLS
"  --output-float64bit" NLS
"  --output-linear" NLS
"  --output-ulaw" NLS
"  --output-alaw" NLS
"  --[no-]output-swab" NLS, fp);
	fputs(NLS, fp);
	fputs("Available WRD interfaces (-W, --wrd option):" NLS, fp);
	for (wlpp = wrdt_list; (wlp = *wlpp) != NULL; wlpp++)
		fprintf(fp, "  -W%c          %s" NLS, wlp->id, wlp->name);
	fputs(NLS, fp);
	close_pager(fp);
	exit(EXIT_SUCCESS);
	return 0; // dummy call
}


#ifdef IA_DYNAMIC
static inline void list_dyna_interface(FILE *fp, char *path, char *mark)
{
    URL dir;
    char fname[NAME_MAX];
    int cwd, dummy;
	if ((dir = url_dir_open(path)) == NULL)
		return;
#if defined(__W32__)
	cwd = -1;
#else
	cwd = open(".", 0);
#endif
	if(chdir(path) != 0)
		return;
	while (url_gets(dir, fname, sizeof(fname)) != NULL)
		if (strncmp(fname, "if_", 3) == 0) {
			void* handle = NULL;
			char path[NAME_MAX];
			snprintf(path, NAME_MAX, ".%c%s", PATH_SEP, fname);
			if((handle = dl_load_file(path))) {
				ControlMode *(* loader)(void) = NULL;
				char c;
				for (c = 'A'; c <= 'z'; c++) {
					char buf[20]; /* enough */
					if(mark[(int)c]) continue;
					sprintf(buf, "interface_%c_loader", c);
					if((loader = dl_find_symbol(handle, buf))) {
						fprintf(fp, "  -i%c          %s" NLS, c, loader()->id_name);
						mark[(int)c] = 1;
						break;
					}
				}
				dl_free(handle);
			}
		}
	if(cwd != -1) {
		dummy = fchdir(cwd);
		dummy += close(cwd);
	}
	url_close(dir);
}

ControlMode *dynamic_interface_module(int id_char)
{
	URL url;
	char fname[FILEPATH_MAX];
	ControlMode *ctl = NULL;
	int cwd, dummy;
	void *handle;
	ControlMode *(* inferface_loader)(void);

	if ((url = url_dir_open(dynamic_lib_root)) == NULL)
		return NULL;
	cwd = open(".", 0);
	if(chdir(dynamic_lib_root) != 0)
		return NULL;
	while (url_gets(url, fname, sizeof(fname)) != NULL) {
		if (strncmp(fname, "if_", 3) == 0) {
			char path[NAME_MAX], buff[20];
			snprintf(path, NAME_MAX-1, ".%c%s", PATH_SEP, fname);
			if((handle = dl_load_file(path)) == NULL)
				continue;
			sprintf(buff, "interface_%c_loader", id_char);
			if((inferface_loader = dl_find_symbol(handle, buff)) == NULL) {
				dl_free(handle);
				continue;
			}
			ctl = inferface_loader();
			if(ctl->id_character == id_char)
				break;
		}
	}
	dummy = fchdir(cwd);
	dummy += close(cwd);
	url_close(url);
	return ctl;
}
#endif	/* IA_DYNAMIC */

static inline int parse_opt_i(const char *arg)
{
	/* interface mode */
	ControlMode *cmp, **cmpp;
	int found = 0;
	if (!arg) arg = "";
	
	for (cmpp = ctl_list; (cmp = *cmpp) != NULL; cmpp++) {
		if (cmp->id_character == *arg) {
			found = 1;
			ctl = cmp;
#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
			cmp->verbosity = 1;
			cmp->trace_playing = 0;
			cmp->flags = 0;
#endif	/* IA_W32GUI */
			break;
		}
	}
#ifdef IA_DYNAMIC
	if (! found) {
		cmp = dynamic_interface_module(*arg);
		if(cmp) {
			ctl = cmp;
			found = 1;
		}
	}
#endif	/* IA_DYNAMIC */
	if (! found) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"Interface `%c' is not compiled in.", *arg);
		return 1;
	}
	while (*(++arg))
		switch (*arg) {
		case 'v':
			cmp->verbosity++;
			break;
		case 'q':
			cmp->verbosity--;
			break;
		case 't':	/* toggle */
			cmp->trace_playing = (cmp->trace_playing) ? 0 : 1;
			break;
		case 'l':
			cmp->flags ^= CTLF_LIST_LOOP;
			break;
		case 'r':
			cmp->flags ^= CTLF_LIST_RANDOM;
			break;
		case 's':
			cmp->flags ^= CTLF_LIST_SORT;
			break;
		case 'a':
			cmp->flags ^= CTLF_AUTOSTART;
			break;
		case 'x':
			cmp->flags ^= CTLF_AUTOEXIT;
			break;
		case 'd':
			cmp->flags ^= CTLF_DRAG_START;
			break;
		case 'u':
			cmp->flags ^= CTLF_AUTOUNIQ;
			break;
		case 'R':
			cmp->flags ^= CTLF_AUTOREFINE;
			break;
		case 'C':
			cmp->flags ^= CTLF_NOT_CONTINUE;
			break;
		case 'D':
			cmp->flags ^= CTLF_DAEMONIZE;
			break;
		default:
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"Unknown interface option `%c'", *arg);
			return 1;
		}
	return 0;
}

static inline int parse_opt_verbose(const char *arg)
{
	/* --verbose */
	ctl->verbosity += (arg) ? atoi(arg) : 1;
	return 0;
}

static inline int parse_opt_quiet(const char *arg)
{
	/* --quiet */
	ctl->verbosity -= (arg) ? atoi(arg) : 1;
	return 0;
}

static inline int parse_opt_trace(const char *arg)
{
	/* --[no-]trace */
	if (!arg) return 0;
	ctl->trace_playing = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_loop(const char *arg)
{
	/* --[no-]loop */
	if (!arg) return 0;
	return set_flag(&(ctl->flags), CTLF_LIST_LOOP, arg);
}

static inline int parse_opt_random(const char *arg)
{
	/* --[no-]random */
	if (!arg) return 0;
	return set_flag(&(ctl->flags), CTLF_LIST_RANDOM, arg);
}

static inline int parse_opt_sort(const char *arg)
{
	/* --[no-]sort */
	if (!arg) return 0;
	return set_flag(&(ctl->flags), CTLF_LIST_SORT, arg);
}

#ifdef IA_ALSASEQ
static inline int parse_opt_background(const char *arg)
{
	/* --[no-]background */
	if (!arg) return 0;
	return set_flag(&(ctl->flags), CTLF_DAEMONIZE, arg);
}

static inline int parse_opt_rt_prio(const char *arg)
{
	/* --realtime-priority */
	if (!arg) return 0;
	if (set_value(&opt_realtime_priority, atoi(arg), 0, 100,
			"Realtime priority"))
		return 1;
	return 0;
}

static inline int parse_opt_seq_ports(const char *arg)
{
	/* --sequencer-ports */
	if (!arg) return 0;
	if (set_value(&opt_sequencer_ports, atoi(arg), 1, 16,
			"Number of sequencer ports"))
		return 1;
	return 0;
}
#endif

#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) ||defined(IA_NPSYN) || defined(IA_W32G_SYN)
static inline int parse_opt_rtsyn_latency(const char *arg)
{
	/* --rtsyn-latency */
	double latency;
	if (!arg) arg = "";
	
	if (sscanf(arg, "%lf", &latency) == EOF)
		latency = RTSYN_LATENCY;
	rtsyn_set_latency(latency);
	return 0;
}

static inline int parse_opt_rtsyn_skip_aq(const char *arg)
{
	/* --rtsyn-skip-aq */
	if (!arg) return 0;
	rtsyn_set_skip_aq(atoi(arg));
	return 0;
}
#elif defined(IA_W32GUI)
static inline int parse_opt_rtsyn_latency(const char *arg)
{
	/* --rtsyn-latency */
	return 0;
}
static inline int parse_opt_rtsyn_skip_aq(const char *arg)
{
	/* --rtsyn-skip-aq */
	return 0;
}
#endif

static inline int parse_opt_j(const char *arg)
{
	if (!arg) return 0;
	opt_realtime_playing = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_K(const char *arg)
{
	/* key adjust */
	if (!arg) return 0;
	if (set_value(&key_adjust, atoi(arg), -24, 24, "Key adjust"))
		return 1;
	return 0;
}

static inline int parse_opt_k(const char *arg)
{
	safe_free(opt_reduce_voice_threshold);
	opt_reduce_voice_threshold = safe_strdup(arg ? arg : DEFAULT_REDUCE_VOICE_THRESHOLD);
//	reduce_voice_threshold = atoi(arg);
	return 0;
}

static inline int parse_opt_L(const char *arg)
{
	if (!arg) return 0;
	add_to_pathlist(arg);
	try_config_again = 1;
	return 0;
}
///r
static inline int parse_opt_l(const char *arg)
{
	if (!arg) return 0;
	opt_resample_filter = atoi(arg);
	return 0;
}

static inline int parse_opt_resample_over_sampling(const char *arg)
{
	if (!arg) return 0;
	opt_resample_over_sampling = atoi(arg);
	return 0;
}

static inline int parse_opt_pre_resample(const char *arg)
{
	if (!arg) return 0;
	opt_pre_resamplation = atoi(arg);
	return 0;
}

static inline int parse_opt_M(const char *arg)
{
	safe_free(pcm_alternate_file);
	if (!arg) arg = "";
	pcm_alternate_file = safe_strdup(arg);
	return 0;
}

static inline int parse_opt_m(const char *arg)
{
	if (!arg) return 0;
	min_sustain_time = atoi(arg);
	if (min_sustain_time < 0)
		min_sustain_time = 0;
	return 0;
}

static inline int parse_opt_N(const char *arg)
{
	if (!arg) return 0;
	opt_resample_param = atoi(arg);
	return 0;
}

static inline int parse_opt_n(const char *arg)
{
	safe_free(opt_reduce_polyphony_threshold);
	opt_reduce_polyphony_threshold = safe_strdup(arg ? arg : DEFAULT_REDUCE_POLYPHONY_THRESHOLD);
	return 0;
}

static inline int parse_opt_O(const char *arg)
{
	/* output mode */
	PlayMode *pmp, **pmpp, *prev_playmode;
	int found = 0;
	if (!arg) arg = "";

	prev_playmode = play_mode;
	for (pmpp = play_mode_list; (pmp = *pmpp) != NULL; pmpp++)
		if (pmp->id_character == *arg) {
			found = 1;
			play_mode = pmp;
			break;
		}
	if (!found) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"Playmode `%c' is not compiled in.", *arg);
		return 1;
	}
	if (prev_playmode) {
		if (prev_playmode->close_output) {
			prev_playmode->close_output();
		}
		safe_free(prev_playmode->name);
		prev_playmode->name = NULL;
	}
	while (*(++arg))
		switch (*arg) {
		case 'S':	/* stereo */
			pmp->encoding &= ~PE_MONO;
			break;
		case 'M':
			pmp->encoding |= PE_MONO;
			break;
		case 's':
			pmp->encoding |= PE_SIGNED;
			pmp->encoding &= ~(PE_ULAW | PE_ALAW);
			break;
///r
		case 'u':
			pmp->encoding &= ~PE_SIGNED;
//			pmp->encoding &= ~(PE_ULAW | PE_ALAW); // w32g can't use ULAW ALAW
			break;
		case 'D':	/* D for float 64-bit */
			pmp->encoding |= PE_F64BIT;
			pmp->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_ULAW | PE_ALAW);
			break;
		case '6':	/* 6 for 64-bit */
			pmp->encoding |= PE_64BIT;
			pmp->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
			break;
		case 'f':	/* f for float 32-bit */
			pmp->encoding |= PE_F32BIT;
			pmp->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
			break;
		case '3':	/* 3 for 32-bit */
			pmp->encoding |= PE_32BIT;
			pmp->encoding &= ~(PE_16BIT | PE_24BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
			break;
		case '2':	/* 2 for 24-bit */
			pmp->encoding |= PE_24BIT;
			pmp->encoding &= ~(PE_16BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
			break;
		case '1':	/* 1 for 16-bit */
			pmp->encoding |= PE_16BIT;
			pmp->encoding &= ~(PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
			break;
		case '8':
			pmp->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_BYTESWAP);
			break;
		case 'l':	/* linear */
			pmp->encoding &= ~(PE_ULAW | PE_ALAW);
			break;
		case 'U':	/* uLaw */
			pmp->encoding |= PE_ULAW;
			pmp->encoding &= ~(PE_SIGNED | PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ALAW | PE_BYTESWAP);
			break;
		case 'A':	/* aLaw */
			pmp->encoding |= PE_ALAW;
			pmp->encoding &= ~(PE_SIGNED | PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_BYTESWAP);
			break;
		case 'x':
			pmp->encoding ^= PE_BYTESWAP;	/* toggle */
			pmp->encoding &= ~(PE_ULAW | PE_ALAW);
			break;
		default:
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
					"Unknown format modifier `%c'", *arg);
			return 1;
		}
	return 0;
}

static inline int parse_opt_output_stereo(const char *arg)
{
	/* --output-stereo, --output-mono */
	if (!arg) return 0;
	if (y_or_n_p(arg))
		/* I first thought --mono should be the syntax sugar to
		 * --stereo=no, but the source said stereo should be !PE_MONO,
		 * not mono should be !PE_STEREO.  Perhaps I took a wrong
		 * choice? -- mput
		 */
		play_mode->encoding &= ~PE_MONO;
	else
		play_mode->encoding |= PE_MONO;
	return 0;
}

static inline int parse_opt_output_signed(const char *arg)
{
	/* --output-singed, --output-unsigned */
	if (!arg) return 0;
	if (set_flag(&(play_mode->encoding), PE_SIGNED, arg))
		return 1;
	play_mode->encoding &= ~(PE_ULAW | PE_ALAW);
	return 0;
}
///r
static inline int parse_opt_output_bitwidth(const char *arg)
{
	/* --output-16bit, --output-24bit, --output-32bit, --output-64bit, --output-8bit, --output-float32bit, --output-float64bit */
	if (!arg) return 0;
	switch (*arg) {
	case 'f':	/* float32bit */
		play_mode->encoding |= PE_F32BIT;
		play_mode->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
		return 0;
	case 'D':	/* float64bit */
		play_mode->encoding |= PE_F64BIT;
		play_mode->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_ULAW | PE_ALAW);
		return 0;
	case '1':	/* 16bit */
		play_mode->encoding |= PE_16BIT;
		play_mode->encoding &= ~(PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
		return 0;
	case '2':	/* 24bit */
		play_mode->encoding |= PE_24BIT;
		play_mode->encoding &= ~(PE_16BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
		return 0;
	case '3':	/* 32bit */
		play_mode->encoding |= PE_32BIT;
		play_mode->encoding &= ~(PE_16BIT | PE_24BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
		return 0;
	case '6':	/* 64bit */
		play_mode->encoding |= PE_64BIT;
		play_mode->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_ALAW);
		return 0;
	case '8':	/* 8bit */
		play_mode->encoding &= ~(PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT);
		return 0;
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid output bitwidth %s", arg);
		return 1;
	}
}

static inline int parse_opt_output_format(const char *arg)
{
	/* --output-linear, --output-ulaw, --output-alaw */
	if (!arg) return 0;
	switch (*arg) {
	case 'l':	/* linear */
		play_mode->encoding &= ~(PE_ULAW | PE_ALAW);
		return 0;
	case 'u':	/* uLaw */
		play_mode->encoding |= PE_ULAW;
		play_mode->encoding &=
				~(PE_SIGNED | PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ALAW | PE_BYTESWAP);
		return 0;
	case 'a':	/* aLaw */
		play_mode->encoding |= PE_ALAW;
		play_mode->encoding &=
				~(PE_SIGNED | PE_16BIT | PE_24BIT | PE_32BIT | PE_64BIT | PE_F32BIT | PE_F64BIT | PE_ULAW | PE_BYTESWAP);
		return 0;
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid output format %s", arg);
		return 1;
	}
}

static inline int parse_opt_output_swab(const char *arg)
{
	/* --[no-]output-swab */
	if (!arg) return 0;
	if (set_flag(&(play_mode->encoding), PE_BYTESWAP, arg))
		return 1;
	play_mode->encoding &= ~(PE_ULAW | PE_ALAW);
	return 0;
}
///r
static inline int parse_opt_output_device_id(const char *arg)
{
	/* --opt-output-device-id */
	if (!arg) return 0;
	opt_output_device_id = atoi(arg);
	return 0;
}
#ifdef AU_W32
static inline int parse_opt_wmme_device_id(const char *arg)
{
	/* --opt-wmme-device-id */
	if (!arg) return 0;
	opt_wmme_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_wave_format_ext(const char *arg)
{
	/* --opt-wave_format_ext */
	if (!arg) return 0;
	opt_wave_format_ext = atoi(arg);
	return 0;
}
static inline int parse_opt_wmme_buffer_bit(const char *arg)
{
	/* --opt_wmme_buffer_bit */
	if (!arg) return 0;
	opt_wmme_buffer_bits = atoi(arg);
	return 0;
}
static inline int parse_opt_wmme_buffer_num(const char *arg)
{
	/* --opt_wmme_buffer_num */
	if (!arg) return 0;
	opt_wmme_buffer_num = atoi(arg);
	return 0;
}
#endif
#ifdef AU_WDMKS
static inline int parse_opt_wdmks_device_id(const char *arg)
{
	/* --wdmks-device-id */
	if (!arg) return 0;
	opt_wdmks_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_wdmks_latency(const char *arg)
{
	/* --wdmks-latency */
	if (!arg) return 0;
	opt_wdmks_latency = atoi(arg);
	return 0;
}
static inline int parse_opt_wdmks_format_ext(const char *arg)
{
	/* --wdmks-format_ext */
	if (!arg) return 0;
	opt_wdmks_format_ext = atoi(arg);
	return 0;
}
static inline int parse_opt_wdmks_polling(const char *arg)
{
	/* --wdmks-polling */
	if (!arg) return 0;
	opt_wdmks_polling = atoi(arg);
	return 0;
}
static inline int parse_opt_wdmks_thread_priority(const char *arg)
{
	/* --wdmks-thread-priority */
	if (!arg) return 0;
	opt_wdmks_thread_priority = atoi(arg);
	return 0;
}
static inline int parse_opt_wdmks_pin_priority(const char *arg)
{
	/* --wdmks-pin-priority */
	if (!arg) return 0;
	opt_wdmks_pin_priority = atoi(arg);
	return 0;
}
static inline int parse_opt_wdmks_rt_priority(const char *arg)
{
	/* --wdmks-rt-priority */
	if (!arg) return 0;
	opt_wdmks_rt_priority = atoi(arg);
	return 0;
}
#endif
#ifdef AU_WASAPI
static inline int parse_opt_wasapi_device_id(const char *arg)
{
	/* --wasapi-device-id */
	if (!arg) return 0;
	opt_wasapi_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_latency(const char *arg)
{
	/* --wasapi-latency */
	if (!arg) return 0;
	opt_wasapi_latency = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_format_ext(const char *arg)
{
	/* --wasapi-format_ext */
	if (!arg) return 0;
	opt_wasapi_format_ext = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_exclusive(const char *arg)
{
	/* --wasapi-exclusive */
	if (!arg) return 0;
	opt_wasapi_exclusive = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_polling(const char *arg)
{
	/* --wasapi-polling */
	if (!arg) return 0;
	opt_wasapi_polling = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_priority(const char *arg)
{
	/* --wasapi-priority */
	if (!arg) return 0;
	opt_wasapi_priority = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_stream_category(const char *arg)
{
	/* --wasapi-stream-category */
	if (!arg) return 0;
	opt_wasapi_stream_category = atoi(arg);
	return 0;
}
static inline int parse_opt_wasapi_stream_option(const char *arg)
{
	/* --wasapi-stream-option */
	if (!arg) return 0;
	opt_wasapi_stream_option = atoi(arg);
	return 0;
}
#endif
///r
#ifdef AU_PORTAUDIO
static inline int parse_opt_pa_wmme_device_id(const char *arg)
{
	/* --pa-wmme-device-id */
	if (!arg) return 0;
	opt_pa_wmme_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_pa_ds_device_id(const char *arg)
{
	/* --pa-ds-device */
	if (!arg) return 0;
	opt_pa_ds_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_pa_asio_device_id(const char *arg)
{
	/* --pa-asio-device-id */
	if (!arg) return 0;
	opt_pa_asio_device_id = atoi(arg);
	return 0;
}
#ifdef PORTAUDIO_V19
static inline int parse_opt_pa_wdmks_device_id(const char *arg)
{
	/* --pa-wdmks-device */
	if (!arg) return 0;
	opt_pa_wdmks_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_pa_wasapi_device_id(const char *arg)
{
	/* --pa-wasapi-device-id */
	if (!arg) return 0;
	opt_pa_wasapi_device_id = atoi(arg);
	return 0;
}
static inline int parse_opt_pa_wasapi_flag(const char *arg)
{
	/* --pa-wasapi-flag */
	if (!arg) return 0;
	opt_pa_wasapi_flag = atoi(arg);
	return 0;
}
static inline int parse_opt_pa_wasapi_stream_category(const char *arg)
{
	/* --pa-wasapi-stream-category */
	if (!arg) return 0;
	opt_pa_wasapi_stream_category = atoi(arg);
	return 0;
}
static inline int parse_opt_pa_wasapi_stream_option(const char *arg)
{
	/* --pa-wasapi-stream-option */
	if (!arg) return 0;
	opt_pa_wasapi_stream_option = atoi(arg);
	return 0;
}
#endif
#endif


extern void wave_set_option_extensible(int);
extern void wave_set_option_update_step(int);

static inline int parse_opt_wave_extensible(const char *arg)
{
	wave_set_option_extensible(y_or_n_p(arg));
	return 0;
}

static inline int parse_opt_wave_update_step(const char *arg)
{
	wave_set_option_update_step(arg ? atoi(arg) : 0);
	return 0;
}


#ifdef AU_FLAC

static inline int parse_opt_flac_verify(const char *arg)
{
	flac_set_option_verify(1);
	return 0;
}

static inline int parse_opt_flac_padding(const char *arg)
{
	if (!arg) return 0;
	flac_set_option_padding(atoi(arg));
	return 0;
}

static inline int parse_opt_flac_complevel(const char *arg)
{
	if (!arg) return 0;
	flac_set_compression_level(atoi(arg));
	return 0;
}

#ifdef AU_OGGFLAC
extern void flac_set_option_oggflac(int);

static inline int parse_opt_flac_oggflac(const char *arg)
{
	flac_set_option_oggflac(1);
	return 0;
}
#endif /* AU_OGGFLAC */
#endif /* AU_FLAC */

#ifdef AU_OPUS
extern void opus_set_nframes(int);
extern void opus_set_bitrate(int);
extern void opus_set_complexity(int);
extern void opus_set_vbr(int);
extern void opus_set_cvbr(int);

static inline int parse_opt_opus_nframes(const char *arg)
{
	opus_set_nframes(arg ? atoi(arg) : 1920);
	return 0;
}

static inline int parse_opt_opus_bitrate(const char *arg)
{
	opus_set_bitrate(arg ? atoi(arg) : 128);
	return 0;
}

static inline int parse_opt_opus_complexity(const char *arg)
{
	opus_set_complexity(arg ? atoi(arg) : 10);
	return 0;
}

static inline int parse_opt_opus_vbr(const char *arg)
{
	opus_set_vbr(y_or_n_p(arg));
	return 0;
}

static inline int parse_opt_opus_cvbr(const char *arg)
{
	opus_set_cvbr(y_or_n_p(arg));
	return 0;
}
#endif /* AU_OPUS */

#ifdef AU_SPEEX
extern void speex_set_option_quality(int);
extern void speex_set_option_vbr(int);
extern void speex_set_option_abr(int);
extern void speex_set_option_vad(int);
extern void speex_set_option_dtx(int);
extern void speex_set_option_complexity(int);
extern void speex_set_option_nframes(int);

static inline int parse_opt_speex_quality(const char *arg)
{
	if (!arg) return 0;
	speex_set_option_quality(atoi(arg));
	return 0;
}

static inline int parse_opt_speex_vbr(const char *arg)
{
	speex_set_option_vbr(1);
	return 0;
}

static inline int parse_opt_speex_abr(const char *arg)
{
	if (!arg) return 0;
	speex_set_option_abr(atoi(arg));
	return 0;
}

static inline int parse_opt_speex_vad(const char *arg)
{
	speex_set_option_vad(1);
	return 0;
}

static inline int parse_opt_speex_dtx(const char *arg)
{
	speex_set_option_dtx(1);
	return 0;
}

static inline int parse_opt_speex_complexity(const char *arg)
{
	if (!arg) return 0;
	speex_set_option_complexity(atoi(arg));
	return 0;
}

static inline int parse_opt_speex_nframes(const char *arg)
{
	if (!arg) return 0;
	speex_set_option_nframes(atoi(arg));
	return 0;
}
#endif /* AU_SPEEX */

static inline int parse_opt_o(const char *arg)
{
	if (!arg) return 0;
	safe_free(opt_output_name);
	opt_output_name = safe_strdup(url_expand_home_dir(arg));
	return 0;
}

static inline int parse_opt_P(const char *arg)
{
	/* set overriding instrument */
	if (!arg) return 0;
	strncpy(def_instr_name, arg, sizeof(def_instr_name) - 1);
	def_instr_name[sizeof(def_instr_name) - 1] = '\0';
	return 0;
}

static inline int parse_opt_p(const char *arg)
{
	if (!arg) return 0;
#if 1 //def __W32__
	// safe_calloc() sizeof(Voice) < MAX_SAFE_MALLOC_SIZE であればいいはず (win以外は不明
	if (set_value(&voices, atoi(arg), 1, MAX_VOICES, "Polyphony"))
		return 1;
#else
	if (set_value(&voices, atoi(arg), 1,
			MAX_SAFE_MALLOC_SIZE / sizeof(Voice), "Polyphony"))
		return 1;
#endif
	max_voices = voices;
	return 0;
}

static inline int parse_opt_p1(const char *arg)
{
	/* --[no-]polyphony-reduction */
	if (!arg) arg = "n";
	auto_reduce_polyphony = y_or_n_p(arg);
	return 0;
}

static inline int parse_opt_Q(const char *arg)
{
	const char *p = arg;
	if (!arg) return 0;
	
	if (strchr(arg, 't'))
		/* backward compatibility */
		return parse_opt_Q1(arg);
	if (set_channel_flag(&quietchannels, atoi(arg), "Quiet channel"))
		return 1;
	while ((p = strchr(p, ',')) != NULL)
		if (set_channel_flag(&quietchannels, atoi(++p), "Quiet channel"))
			return 1;
	return 0;
}

static inline int parse_opt_Q1(const char *arg)
{
	/* --temper-mute */
	int prog;
	const char *p = arg;
	if (!arg) return 0;
	
	if (set_value(&prog, atoi(arg), 0, 7, "Temperament program number"))
		return 1;
	temper_type_mute |= 1 << prog;
	while ((p = strchr(p, ',')) != NULL) {
		if (set_value(&prog, atoi(++p), 0, 7, "Temperament program number"))
			return 1;
		temper_type_mute |= 1 << prog;
	}
	return 0;
}

static inline int parse_opt_preserve_silence(const char *arg)
{
	opt_preserve_silence = 1;
	return 0;
}

static inline int parse_opt_q(const char *arg)
{
	char *max_buff = NULL;
	char *fill_buff = NULL;
	if (!arg) return 0;
	max_buff = safe_strdup(arg);
	fill_buff = strchr(max_buff, '/');
	
	if (fill_buff != max_buff) {
		safe_free(opt_aq_max_buff);
		opt_aq_max_buff = max_buff;
	}
	if (fill_buff) {
		safe_free(opt_aq_fill_buff);
		*fill_buff = '\0';
		opt_aq_fill_buff = safe_strdup(++fill_buff);
	}
	return 0;
}

static inline int parse_opt_R(const char *arg)
{
	/* I think pseudo reverb can now be retired... Computers are
	 * enough fast to do a full reverb, don't they?
	 */
	if (!arg) arg = "-1";
	if (atoi(arg) == -1)	/* reset */
		modify_release = 0;
	else {
		if (set_val_i32(&modify_release, atoi(arg), 0, MAX_MREL,
				"Modify Release"))
			return 1;
		if (modify_release == 0)
			modify_release = DEFAULT_MREL;
	}
	return 0;
}
///r
static inline int parse_opt_r(const char *arg)
{
	safe_free(opt_reduce_quality_threshold);
	opt_reduce_quality_threshold = safe_strdup((arg) ? arg : DEFAULT_REDUCE_QUALITY_THRESHOLD);
//    reduce_quality_threshold = atoi(arg);
	return 0;
}

static inline int parse_opt_add_play_time(const char *arg)
{
	if (!arg) return 0;
	add_play_time = atoi(arg);
	if(add_play_time < 0)
		add_play_time = 0;
	return 0;
}

static inline int parse_opt_add_silent_time(const char *arg)
{
	if (!arg) return 0;
	add_silent_time = atoi(arg);
	if(add_silent_time < 0)
		add_silent_time = 0;
	return 0;
}

static inline int parse_opt_emu_delay_time(const char *arg)
{
	if (!arg) return 0;
	emu_delay_time = atoi(arg);
	if(emu_delay_time < 0)
		emu_delay_time = 0;
	return 0;
}
///r
static inline int parse_opt_mix_envelope(const char *arg)
{
	if (!arg) return 0;
	opt_mix_envelope = atoi(arg);
	if (opt_mix_envelope < 0)
		opt_mix_envelope = 0;
	return 0;
}

static inline int parse_opt_modulation_update(const char *arg)
{
	if (!arg) return 0;
	opt_modulation_update = atoi(arg);
	if (opt_modulation_update < 0)
		opt_modulation_update = 0;
	return 0;
}

static inline int parse_opt_cut_short_time(const char *arg)
{
	if (!arg) return 0;
	opt_cut_short_time = atoi(arg);
	if (opt_cut_short_time < 0)
		opt_cut_short_time = 0;
	return 0;
}

static inline int parse_opt_limiter(const char *arg)
{
	if (!arg) return 0;
	opt_limiter = atoi(arg);
	if (opt_limiter < 0)
		opt_limiter = 0;
	return 0;
}

static inline int parse_opt_compute_thread_num(const char *arg)
{
	if (!arg) return 0;
	compute_thread_num = atoi(arg);
	return 0;
}

static inline int parse_opt_trace_mode_update(const char *arg)
{
#ifdef USE_TRACE_TIMER	
	if (!arg) return 0;
	trace_mode_update_time = atoi(arg);
	set_trace_mode_update_time();
#endif
	return 0;
}

static inline int parse_opt_load_all_instrument(const char *arg)
{
	if (!arg) return 0;
	opt_load_all_instrument = atoi(arg);
	return 0;
}

static inline int parse_opt_int_synth_rate(const char *arg)
{
	if (!arg) return 0;
	opt_int_synth_rate = atoi(arg);
	return 0;
}
static inline int parse_opt_int_synth_sine(const char *arg)
{
	if (!arg) return 0;
	opt_int_synth_sine = atoi(arg);
	return 0;
}
static inline int parse_opt_int_synth_update(const char *arg)
{
	if (!arg) return 0;
	opt_int_synth_update = atoi(arg);
	return 0;
}

#ifdef SUPPORT_LOOPEVENT
static inline int parse_opt_midi_loop_repeat(const char *arg)
{
	opt_midi_loop_repeat = (arg) ? atoi(arg) : 0;
	opt_use_midi_loop_repeat = (opt_midi_loop_repeat >= 1) ? 1 : 0;
	return 0;
}
#endif /* SUPPORT_LOOPEVENT */

static inline int parse_opt_od_level_gs(const char *arg)
{
	int level;
	if (set_value(&level, arg ? atoi(arg) : 100, 1, 400, "GS OD Level adjust"))
		return 1;
	otd.gsefx_CustomODLv = level / 100.0;
	return 0;
}

static inline int parse_opt_od_drive_gs(const char *arg)
{
	int level;
	if (set_value(&level, arg ? atoi(arg) : 100, 1, 400, "GS OD Drive adjust"))
		return 1;
	otd.gsefx_CustomODDrive = level / 100.0;
	return 0;
}

static inline int parse_opt_od_level_xg(const char *arg)
{
	int level;
	if (set_value(&level, arg ? atoi(arg) : 100, 1, 400, "XG OD Level adjust"))
		return 1;
	otd.xgefx_CustomODLv = level / 100.0;
	return 0;
}

static inline int parse_opt_od_drive_xg(const char *arg)
{
	int level;
	if (set_value(&level, arg ? atoi(arg) : 100, 1, 400, "XG OD Drive adjust"))
		return 1;
	otd.xgefx_CustomODDrive = level / 100.0;
	return 0;
}

#ifdef __W32__
static inline int parse_opt_process_priority(const char *arg)
{
	processPriority = (arg) ? (DWORD)atoi(arg) : NORMAL_PRIORITY_CLASS;
	return 0;
}

#if !(defined(IA_W32G_SYN) || defined(WINDRV))
static inline int parse_opt_player_thread_priority(const char *arg)
{
	PlayerThreadPriority = (arg) ? (DWORD)atoi(arg) : THREAD_PRIORITY_NORMAL;
	return 0;
}

static inline int parse_opt_GUI_thread_priority(const char *arg)
{
	GUIThreadPriority = (arg) ? (DWORD)atoi(arg) : THREAD_PRIORITY_NORMAL;
	return 0;
}
#endif /* !IA_W32G_SYN */
#endif /* __W32__ */

static inline int parse_opt_S(const char *arg)
{
	int suffix;
	int32 figure;
	if (!arg) return 0;
	suffix = arg[strlen(arg) - 1];
	
	switch (suffix) {
	case 'M':
	case 'm':
		figure = 1 << 20;
		break;
	case 'K':
	case 'k':
		figure = 1 << 10;
		break;
	default:
		figure = 1;
		break;
	}
	allocate_cache_size = atof(arg) * figure;
	return 0;
}

static inline int parse_opt_s(const char *arg)
{
	/* sampling rate */
	int32 freq;

	if (!arg) arg = "44.1";
	if ((freq = atoi(arg)) < 1000)
		freq = atof(arg) * 1000 + 0.5;
	return set_val_i32(&opt_output_rate, freq, MIN_OUTPUT_RATE, MAX_OUTPUT_RATE, "Resampling frequency");
}

static inline int parse_opt_T(const char *arg)
{
	/* tempo adjust */
	int adjust;
	
	if (!arg) arg = "100";
	if (set_value(&adjust, atoi(arg), 10, 400, "Tempo adjust"))
		return 1;
	tempo_adjust = 100.0 / adjust;
	return 0;
}

static inline int parse_opt_t(const char *arg)
{
	if (!arg) arg = OUTPUT_TEXT_CODE;
	safe_free(output_text_code);
	output_text_code = safe_strdup(arg);
	return 0;
}

static inline int parse_opt_U(const char *arg)
{
	if (!arg) arg = "n";
	free_instruments_afterwards = y_or_n_p(arg);
	return 0;
}

#if defined(AU_VOLUME_CALC)
static inline int parse_opt_volume_calc_rms(const char *arg)
{
	opt_volume_calc_rms = 1;
	return 0;
}

static inline int parse_opt_volume_calc_trim(const char *arg)
{
	opt_volume_calc_trim = 1;
	return 0;
}
#endif /* AU_VOLUME_CALC */

///r
static inline int parse_opt_volume_curve(const char *arg)
{
	if (!arg) return 0;
	if (!(atof(arg) >= 0)) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Volume curve power must be >= 0", *arg);
		return 1;
	}
	if (atof(arg) != 0) {
		init_user_vol_table(atof(arg));
		opt_user_volume_curve = atof(arg);
	}else
		opt_user_volume_curve = 0;
	return 0;
}

__attribute__((noreturn))
static inline int parse_opt_v(const char *arg)
{
	const char *version_list[] = {
#if defined(__BORLANDC__) || defined(__MRC__) || defined(__WATCOMC__) ||  defined(__DMC__)
		"TiMidity++ ",
				"",
				NULL, NLS,
		NLS,
#else
		"TiMidity++ ",
				(strcmp(timidity_version, "current")) ? "version " : "",
				timidity_version, NLS,
		NLS,
#endif
		"Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>", NLS,
		"Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>", NLS,
		NLS,
#ifdef __W32__
		"Win32 version by Davide Moretti <dmoretti@iper.net>", NLS,
		"              and Daisuke Aoki <dai@y7.net>", NLS,
		NLS,
#endif	/* __W32__ */
		"This program is distributed in the hope that it will be useful,", NLS,
		"but WITHOUT ANY WARRANTY; without even the implied warranty of", NLS,
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the", NLS,
		"GNU General Public License for more details.", NLS,
	};
	FILE *fp = open_pager();
	int i;

#if defined(__BORLANDC__) || defined(__MRC__)
	if (strcmp(timidity_version, "current"))
		version_list[1] = "version ";
	version_list[2] = timidity_version;
#endif
	for (i = 0; i < sizeof(version_list) / sizeof(char *); i++)
		fputs(version_list[i], fp);
	close_pager(fp);
	exit(EXIT_SUCCESS);
}

static inline int parse_opt_W(const char *arg)
{
	WRDTracer *wlp, **wlpp;
	
	if (!arg) arg = "";
	if (*arg == 'R') {	/* for WRD reader options */
		put_string_table(&wrd_read_opts, arg + 1, strlen(arg + 1));
		return 0;
	}
	for (wlpp = wrdt_list; (wlp = *wlpp) != NULL; wlpp++)
		if (wlp->id == *arg) {
			wrdt = wlp;
			safe_free(wrdt_open_opts);
			wrdt_open_opts = safe_strdup(arg + 1);
			return 0;
		}
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			"WRD Tracer `%c' is not compiled in.", *arg);
	return 1;
}

#ifdef __W32__
static inline int parse_opt_w(const char *arg)
{
	if (!arg) arg = "";
	switch (*arg) {
#ifdef SMFCONV
	case 'r':
		opt_rcpcv_dll = 1;
		return 0;
	case 'R':
		opt_rcpcv_dll = 0;
		return 0;
#else
	case 'r':
	case 'R':
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"-w%c option is not supported", *arg);
		return 1;
#endif	/* SMFCONV */
	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "-w: Illegal mode `%c'", *arg);
		return 1;
	}
}
#endif	/* __W32__ */

static inline int parse_opt_x(const char *arg)
{
	StringTableNode *st;
	
	if (!arg) arg = "";
	if ((st = put_string_table(&opt_config_string,
			arg, strlen(arg))) != NULL)
		expand_escape_string(st->string);
	return 0;
}
///r
static inline int parse_opt_Y(const char *arg)
{
	/* --compute-buffer */
	if (!arg) return 0;
	if (set_value(&opt_compute_buffer_bits, atoi(arg), -5, AUDIO_BUFFER_BITS - 1,
			"Compute Buffer (bit)"))
		return 1;
	return 0;
}

static inline void expand_escape_string(char *s)
{
	char *t = s;
	
	if (s == NULL)
		return;
	for (t = s; *s; s++)
		if (*s == '\\') {
			switch (*++s) {
			case 'a':
				*t++ = '\a';
				break;
			case 'b':
				*t++ = '\b';
				break;
			case 't':
				*t++ = '\t';
				break;
			case 'n':
				*t++ = '\n';
				break;
			case 'f':
				*t++ = '\f';
				break;
			case 'v':
				*t++ = '\v';
				break;
			case 'r':
				*t++ = '\r';
				break;
			case '\\':
				*t++ = '\\';
				break;
			default:
				if (! (*t++ = *s))
					return;
				break;
			}
		} else
			*t++ = *s;
	*t = *s;
}

static inline int parse_opt_Z(const char *arg)
{
	/* load frequency table */
	return load_table(arg);
}

static inline int parse_opt_Z1(const char *arg)
{
	/* --pure-intonation */
	int keysig;
	
	opt_pure_intonation = 1;
	if (*arg) {
		if (set_value(&keysig, atoi(arg), -7, 7,
				"Initial keysig (number of #(+)/b(-)[m(minor)])"))
			return 1;
		opt_init_keysig = keysig;
		if (strchr(arg, 'm'))
			opt_init_keysig += 16;
	}
	return 0;
}

static inline int parse_opt_default_module(const char *arg)
{
	if (!arg) return 0;
	opt_default_module = atoi(arg);
	if (opt_default_module < 0)
		opt_default_module = 0;
	return 0;
}


__attribute__((noreturn))
static inline int parse_opt_fail(const char *arg)
{
	/* getopt_long failed to recognize any options */
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			"Could not understand option : try --help");
	exit(EXIT_FAILURE);
}

static inline int set_value(int *param, int i, int low, int high, const char *name)
{
	int32 val;
	
	if (set_val_i32(&val, i, low, high, name))
		return 1;
	*param = val;
	return 0;
}

static inline int set_val_i32(int32 *param,
		int32 i, int32 low, int32 high, const char *name)
{
	if (i < low || i > high) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s must be between %ld and %ld", name, low, high);
		return 1;
	}
	*param = i;
	return 0;
}

static int parse_val_float_t(FLOAT_T *param,
		const char *arg, FLOAT_T low, FLOAT_T high, const char *name)
{
	FLOAT_T value;
	char *errp;

	value = strtod(arg, &errp);
	if (arg == errp) {
		/* only when nothing was parsed */
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Invalid %s", name);
		return 1;
	}
	return set_val_float_t(param, value, low, high, name);
}

static inline int set_val_float_t(FLOAT_T *param,
		FLOAT_T i, FLOAT_T low, FLOAT_T high, const char *name)
{
	if (i < low || i > high) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s must be between %.1f and %.1f", name, low, high);
		return 1;
	}
	*param = i;
	return 0;
}

static inline int set_channel_flag(ChannelBitMask *flags, int32 i, const char *name)
{
	if (i == 0) {
		FILL_CHANNELMASK(*flags);
		return 0;
	} else if (abs(i) > MAX_CHANNELS) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s must be between (-)1 and (-)%d, or 0",
						name, MAX_CHANNELS);
		return 1;
	}
	if (i > 0)
		SET_CHANNELMASK(*flags, i - 1);
	else
		UNSET_CHANNELMASK(*flags, -i - 1);
	return 0;
}

static inline int y_or_n_p(const char *arg)
{
	if (arg) {
		switch (arg[0]) {
		case 'y':
		case 'Y':
		case 't':
		case 'T':
			return 1;
		case 'n':
		case 'N':
		case 'f':
		case 'F':
		default:
			return 0;
		}
	} else
		return 1;
}

static inline int set_flag(int32 *fields, int32 bitmask, const char *arg)
{
	if (y_or_n_p(arg))
		*fields |= bitmask;
	else
		*fields &= ~bitmask;
	return 0;
}

static inline FILE *open_pager(void)
{
#if ! defined(__MACOS__) && defined(HAVE_POPEN) && defined(HAVE_ISATTY) \
		&& ! defined(IA_W32GUI) && ! defined(IA_W32G_SYN)
	char *pager;
	
	if (isatty(1) && (pager = getenv("PAGER")) != NULL)
		return popen(pager, "w");
#endif
	return stdout;
}

static inline void close_pager(FILE *fp)
{
#if ! defined(__MACOS__) && defined(HAVE_POPEN) && defined(HAVE_ISATTY) \
		&& ! defined(IA_W32GUI) && ! defined(IA_W32G_SYN)
	if (fp != stdout)
		pclose(fp);
#endif
}

static void interesting_message(void)
{
	printf(
"TiMidity++ %s%s -- MIDI to WAVE converter and player" NLS
"Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>" NLS
"Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>" NLS
			NLS
#ifdef __W32__
"Win32 version by Davide Moretti <dmoretti@iper.net>" NLS
"              and Daisuke Aoki <dai@y7.net>" NLS
			NLS
#endif /* __W32__ */
"This program is free software; you can redistribute it and/or modify" NLS
"it under the terms of the GNU General Public License as published by" NLS
"the Free Software Foundation; either version 2 of the License, or" NLS
"(at your option) any later version." NLS
			NLS
"This program is distributed in the hope that it will be useful," NLS
"but WITHOUT ANY WARRANTY; without even the implied warranty of" NLS
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" NLS
"GNU General Public License for more details." NLS
			NLS
"You should have received a copy of the GNU General Public License" NLS
"along with this program; if not, write to the Free Software" NLS
"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA" NLS
			NLS, (strcmp(timidity_version, "current")) ? "version " : "",
			timidity_version);
}

/* -------- functions for getopt_long ends here --------- */

#ifdef HAVE_SIGNAL
static RETSIGTYPE sigterm_exit(int sig)
{
    char s[4];
    size_t dummy;

    /* NOTE: Here, fprintf is dangerous because it is not re-enterance
     * function.  It is possible coredump if the signal is called in printf's.
     */

    dummy = write(2, "Terminated sig=0x", 17);
    s[0] = "0123456789abcdef"[(sig >> 4) & 0xf];
    s[1] = "0123456789abcdef"[sig & 0xf];
    s[2] = '\n';
    dummy += write(2, s, 3);

    safe_exit(1);
	return 0;
}
#endif /* HAVE_SIGNAL */

static void timidity_arc_error_handler(char *error_message)
{
    extern int open_file_noise_mode;
    if(open_file_noise_mode)
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "%s", error_message);
}

static PlayMode null_play_mode = {
    0,                          /* rate */
    0,                          /* encoding */
    0,                          /* flag */
    -1,                         /* fd */
    {0,0,0,0,0},                /* extra_param */
    "Null output device",       /* id_name */
    '\0',                       /* id_character */
    NULL,                       /* open_output */
    NULL,                       /* close_output */
    NULL,                       /* output_data */
    NULL,                       /* acntl */
    NULL                        /* detect */
};

MAIN_INTERFACE void timidity_start_initialize(void)
{
    int i;
    static int drums[] = DEFAULT_DRUMCHANNELS;
    static int is_first = 1;
#if defined(__FreeBSD__) && !defined(__alpha__)
    fp_except_t fpexp;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
    fp_except fpexp;
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    fpexp = fpgetmask();
    fpsetmask(fpexp & ~(FP_X_INV|FP_X_DZ));
#endif

    if(!output_text_code)
	output_text_code = safe_strdup(OUTPUT_TEXT_CODE);
    if(!opt_aq_max_buff)
	opt_aq_max_buff = safe_strdup("20.0");
    if(!opt_aq_fill_buff)
	opt_aq_fill_buff = safe_strdup("200%");
///r	
    if(!opt_reduce_voice_threshold)
	opt_reduce_voice_threshold = safe_strdup("0");
    if(!opt_reduce_quality_threshold)
	opt_reduce_quality_threshold = safe_strdup("0");
    if(!opt_reduce_polyphony_threshold)
	opt_reduce_polyphony_threshold = safe_strdup("0");

    /* Check the byte order */
    i = 1;
#ifdef LITTLE_ENDIAN
    if(*(char *)&i != 1)
#else
    if(*(char *)&i == 1)
#endif
    {
	fprintf(stderr, "Byte order is miss configured.\n");
	exit(1);
    }

    for(i = 0; i < MAX_CHANNELS; i++)
    {
	memset(&(channel[i]), 0, sizeof(Channel));
    }

    CLEAR_CHANNELMASK(quietchannels);
    CLEAR_CHANNELMASK(default_drumchannels);

    for(i = 0; drums[i] > 0; i++)
	SET_CHANNELMASK(default_drumchannels, drums[i] - 1);
#if MAX_CHANNELS > 16
    for(i = 16; i < MAX_CHANNELS; i++)
	if(IS_SET_CHANNELMASK(default_drumchannels, i & 0xF))
	    SET_CHANNELMASK(default_drumchannels, i);
#endif

    if(program_name == NULL)
	program_name = "TiMidity";
    uudecode_unquote_html = 1;
///r
    for(i = 0; i < MAX_CHANNELS; i++)
    {
	default_program[i] = DEFAULT_PROGRAM;
	special_program[i] = -1;
	memset(channel[i].drums, 0, sizeof(channel[i].drums));
    }
    arc_error_handler = timidity_arc_error_handler;

    if(play_mode == NULL) play_mode = &null_play_mode;

    if(is_first) /* initialize once time */
    {
	got_a_configuration = 0;

#ifdef SUPPORT_SOCKET
	init_mail_addr();
	if(url_user_agent == NULL)
	{
	    url_user_agent =
		(char *)safe_malloc(10 + strlen(timidity_version));
	    strcpy(url_user_agent, "TiMidity-");
	    strcat(url_user_agent, timidity_version);
	}
#endif /* SUPPORT_SOCKET */

	for(i = 0; url_module_list[i]; i++)
	    url_add_module(url_module_list[i]);
	init_string_table(&opt_config_string);
	init_freq_table();
	init_freq_table_tuning();
	init_freq_table_pytha();
	init_freq_table_meantone();
	init_freq_table_pureint();
	init_freq_table_user();
	init_bend_fine();
	init_bend_coarse();
	init_tables();
	init_gm2_pan_table();
	init_attack_vol_table();
	init_sb_vol_table();
	init_modenv_vol_table();
	init_def_vol_table();
	init_gs_vol_table();
	init_perceived_vol_table();
	init_gm2_vol_table();
#ifdef SUPPORT_SOCKET
	url_news_connection_cache(URL_NEWS_CONN_CACHE);
#endif /* SUPPORT_SOCKET */
	for(i = 0; i < NSPECIAL_PATCH; i++)
	    special_patch[i] = NULL;
	init_midi_trace();
	int_rand(-1);	/* initialize random seed */
	int_rand(42);	/* the 1st number generated is not very random */
	ML_RegisterAllLoaders ();
    }

    is_first = 0;
}

MAIN_INTERFACE int timidity_pre_load_configuration(void)
{
#if defined(__W32__)
    /* Windows */
    char *strp;
    int check;
    char local[1024];

#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
    extern char *ConfigFile;
    if(!ConfigFile[0]) {
      GetWindowsDirectory(ConfigFile, 1023 - 13);
      strcat(ConfigFile, "\\TIMIDITY.CFG");
    }
    strncpy(local, ConfigFile, sizeof(local) - 1);
    if((check = open(local, 0)) >= 0)
    {
	close(check);
	if(!read_config_file(local, 0, 0)) {
	    got_a_configuration = 1;
		return 0;
	}
    }
#endif
	/* First, try read configuration file which is in the
     * TiMidity directory.
     */
    if(GetModuleFileName(NULL, local, 1023))
    {
        local[1023] = '\0';
	if(strp = strrchr(local, '\\'))
	{
	    *(++strp)='\0';
	    strncat(local,"TIMIDITY.CFG",sizeof(local)-strlen(local)-1);
	    if((check = open(local, 0)) >= 0)
	    {
		close(check);
		if(!read_config_file(local, 0, 0)) {
		    got_a_configuration = 1;
			return 0;
		}
	    }
	}
#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
    /* Next, try read system configuration file.
     * Default is C:\WINDOWS\TIMIDITY.CFG
     */
    GetWindowsDirectory(local, 1023 - 13);
    strcat(local, "\\TIMIDITY.CFG");
    if((check = open(local, 0)) >= 0)
    {
	close(check);
	if(!read_config_file(local, 0, 0)) {
	    got_a_configuration = 1;
		return 0;
	}
    }
#endif

    }

#else
    /* UNIX */
    if(!read_config_file(CONFIG_FILE, 0, 0))
		got_a_configuration = 1;
#endif

    /* Try read configuration file which is in the
     * $HOME (or %HOME% for DOS) directory.
     * Please setup each user preference in $HOME/.timidity.cfg
     * (or %HOME%/timidity.cfg for DOS)
     */
    if(read_user_config_file()) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Error: Syntax error in ~/.timidity.cfg");
	return 1;
    }

    return 0;
}

MAIN_INTERFACE int timidity_post_load_configuration(void)
{
    int i, cmderr = 0;

    /* If we're going to fork for daemon mode, we need to fork now, as
       certain output libraries (pulseaudio) become unhappy if initialized
       before forking and then being used from the child. */
#ifndef WIN32
    if (ctl->id_character == 'A' && (ctl->flags & CTLF_DAEMONIZE))
    {
	int pid = (-1);
	FILE *pidf;
	switch (pid)
	{
	    case 0:		// child is the daemon
		break;
	    case -1:		// error status return
		exit(7);
	    default:		// no error, doing well
		if ((pidf = fopen( "/var/run/timidity.pid", "w" )) != NULL )
		{
		    fprintf( pidf, "%d\n", pid );
		    fclose( pidf );
		}
		exit(0);
	}
    }
#endif
    if(play_mode == &null_play_mode)
    {
	char *output_id;

	output_id = getenv("TIMIDITY_OUTPUT_ID");
#ifdef TIMIDITY_OUTPUT_ID
	if(output_id == NULL)
	    output_id = TIMIDITY_OUTPUT_ID;
#endif /* TIMIDITY_OUTPUT_ID */
	if(output_id != NULL)
	{
	    for(i = 0; play_mode_list[i]; i++)
		if(play_mode_list[i]->id_character == *output_id)
		{
		    if (! play_mode_list[i]->detect ||
			play_mode_list[i]->detect()) {
			play_mode = play_mode_list[i];
			break;
		    }
		}
	}
    }

    if (play_mode == &null_play_mode) {
	/* try to detect the first available device */
	for(i = 0; play_mode_list[i]; i++) {
	    /* check only the devices with detect callback */
	    if (play_mode_list[i]->detect) {
		if (play_mode_list[i]->detect()) {
		    play_mode = play_mode_list[i];
		    break;
		}
	    }
	}
    }

    if (play_mode == &null_play_mode) {
	fprintf(stderr, "Couldn't open output device" NLS);
	exit(1);
    }
    else {
        /* apply changes made for null play mode to actual play mode */
        if(null_play_mode.encoding != 0) {
            play_mode->encoding = apply_encoding(play_mode->encoding, null_play_mode.encoding);
        }
        if(null_play_mode.rate != 0) {
            play_mode->rate = null_play_mode.rate;
        }
    }

    if(!got_a_configuration)
    {
	if(try_config_again && !read_config_file(CONFIG_FILE, 0, 0))
	    got_a_configuration = 1;
    }

    if(opt_config_string.nstring > 0)
    {
	char **config_string_list;

	config_string_list = make_string_array(&opt_config_string);
	if(config_string_list != NULL)
	{
	    for(i = 0; config_string_list[i]; i++)
	    {
		if(!read_config_file(config_string_list[i], 1, 0))
		    got_a_configuration = 1;
		else
		    cmderr++;
	    }
	    free(config_string_list[0]);
	    free(config_string_list);
	}
    }

    if(!got_a_configuration)
	cmderr++;
    return cmderr;
}


MAIN_INTERFACE void timidity_init_player(void)
{
    initialize_resampler_coeffs();

    /* Allocate voice[] */
    free_voices();
    safe_free(voice);
    voice = (Voice*) safe_calloc(max_voices, sizeof(Voice));

    /* Set play mode parameters */
    if(opt_output_rate != 0)
		play_mode->rate = opt_output_rate;
    else if(play_mode->rate == 0)
		play_mode->rate = DEFAULT_RATE;

    /* save defaults */
    COPY_CHANNELMASK(drumchannels, default_drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);

///r //tim13-
    if(opt_buffer_fragments != -1)
    {
		if(play_mode->flag & PF_BUFF_FRAGM_OPT)
			play_mode->extra_param[0] = opt_buffer_fragments;
		else
			ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
				  "%s: -B option is ignored", play_mode->id_name);
    }
	else
		opt_buffer_fragments = 64;

///r //tim13-
    if(opt_audio_buffer_bits != -1)
    {
        audio_buffer_bits = opt_audio_buffer_bits;
    }
    else
        opt_audio_buffer_bits = audio_buffer_bits;

    if(opt_compute_buffer_bits != -128)
    {
        compute_buffer_bits = opt_compute_buffer_bits;
    }
    else
        opt_compute_buffer_bits = compute_buffer_bits;

	init_output(); // div_playmode_rate
	init_playmidi();
	init_mix_c();	
#ifdef INT_SYNTH
	init_int_synth();
#endif // INT_SYNTH
#ifdef ENABLE_SFZ
	init_sfz();
#endif

#ifdef SUPPORT_SOUNDSPEC
    if(view_soundspec_flag)
    {
	open_soundspec();
	soundspec_setinterval(spectrogram_update_sec);
    }
#endif /* SOUNDSPEC */
}


///r
void timidity_init_reduce_queue(void)
{
    double time1,time2,time3,base;

	base = aq_get_dev_queuesize() / 2;
	if(base <= 0)
		base = AUDIO_BUFFER_SIZE;

#ifdef REDUCE_QUALITY_TIME_TUNING
	time1 = atof(opt_reduce_quality_threshold);
	if(time1 == 0)
		time1 = 0;
	else if(time1 < 0)
		time1 = REDUCE_QUALITY_TIME_TUNING;
	else if(!strchr(opt_reduce_quality_threshold, '%'))
		time1 = time1 * play_mode->rate / base * 100;
	reduce_quality_threshold = (int)time1;
#endif
#ifdef REDUCE_VOICE_TIME_TUNING
	time2 = atof(opt_reduce_voice_threshold);
	if(time2 == 0)
		time2 = 0;
	else if(time2 < 0)
		time2 = REDUCE_VOICE_TIME_TUNING;
	else if(!strchr(opt_reduce_voice_threshold, '%'))
		time2 = time2 * play_mode->rate / base * 100;
	reduce_voice_threshold = (int)time2;
#endif
#ifdef REDUCE_POLYPHONY_TIME_TUNING
	time3 = atof(opt_reduce_polyphony_threshold);
	if(time3 == 0)
		time3 = 0;
	else if(time3 < 0)
		time3 = REDUCE_POLYPHONY_TIME_TUNING;
	else if(!strchr(opt_reduce_polyphony_threshold, '%'))
		time3 = time3 * play_mode->rate / base * 100;
	reduce_polyphony_threshold = (int)time3;
#endif
}

void timidity_init_aq_buff(void)
{
    double time1, /* max buffer */
	   time2, /* init filled */
	   base;  /* buffer of device driver */

    if(!IS_STREAM_TRACE)
	return; /* Ignore */

    time1 = atof(opt_aq_max_buff);
    time2 = atof(opt_aq_fill_buff);
    base  = (double)aq_get_dev_queuesize() * div_playmode_rate;

    if(strchr(opt_aq_max_buff, '%'))
    {
		time1 = base * (time1 - 100) * DIV_100;
		if(time1 < 0) time1 = 0;
    }
    if(strchr(opt_aq_fill_buff, '%'))
		time2 = base * time2 * DIV_100;

    aq_set_soft_queue(time1, time2);
	timidity_init_reduce_queue();
}

MAIN_INTERFACE int timidity_play_main(int nfiles, char **files)
{
    int need_stdin = 0, need_stdout = 0;
    int i;
    int output_fail = 0;
    int retval;
#if (defined(__W32__) && !defined(__W32G__)) && defined(FORCE_TIME_PERIOD)
    TIMECAPS tcaps;
#endif /* (__W32__ && !__W32G__) && FORCE_TIME_PERIOD */

    if(nfiles == 0 && !strchr(INTERACTIVE_INTERFACE_IDS, ctl->id_character))
	return 0;

    if(opt_output_name)
    {
	play_mode->name = opt_output_name;
    if(!strcmp(opt_output_name, "-")){
	    need_stdout = 1;
#ifdef __W32__
    	setmode( fileno(stdout), O_BINARY );
#endif
    }
    }

    for(i = 0; i < nfiles; i++)
	if (!strcmp(files[i], "-")){
	    need_stdin = 1;
#ifdef __W32__
    	setmode( fileno(stdin), O_BINARY );
#endif
	}

    if(ctl->open(need_stdin, need_stdout))
    {
	fprintf(stderr, "Couldn't open %s (`%c')" NLS,
		ctl->id_name, ctl->id_character);
	if(play_mode->close_output)
	{
		play_mode->close_output();
	}
	safe_free(play_mode->name);
	play_mode->name = NULL;
	return 3;
    }

    if(wrdt->open(wrdt_open_opts))
    {
	fprintf(stderr, "Couldn't open WRD Tracer: %s (`%c')" NLS,
		wrdt->name, wrdt->id);
	if(play_mode->close_output)
	{
		play_mode->close_output();
	}
	safe_free(play_mode->name);
	play_mode->name = NULL;
	ctl->close();
	return 1;
    }

#ifdef BORLANDC_EXCEPTION
    __try
    {
#endif /* BORLANDC_EXCEPTION */
#ifdef __W32__

#ifdef HAVE_SIGNAL
	signal(SIGTERM, sigterm_exit);
#endif
	SetConsoleCtrlHandler(handler, TRUE);

	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Initialize for Critical Section");
	InitializeCriticalSection(&critSect);

	if(opt_evil_mode)
	    if(!SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL))
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Error raising thread priority");
///r
#if defined(__W32__)
#ifndef BELOW_NORMAL_PRIORITY_CLASS	/* VC6.0 doesn't support them. */
#define BELOW_NORMAL_PRIORITY_CLASS 0x4000
#define ABOVE_NORMAL_PRIORITY_CLASS 0x8000
#endif /* BELOW_NORMAL_PRIORITY_CLASS */
	if( processPriority == IDLE_PRIORITY_CLASS ||
		processPriority == BELOW_NORMAL_PRIORITY_CLASS ||
		processPriority == NORMAL_PRIORITY_CLASS ||
		processPriority == ABOVE_NORMAL_PRIORITY_CLASS ||
		processPriority == HIGH_PRIORITY_CLASS ||
		processPriority == REALTIME_PRIORITY_CLASS)
	    if(!SetPriorityClass(GetCurrentProcess(), processPriority))
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Error changing process priority");
#if !(defined(IA_W32G_SYN) || defined(WINDRV))
	if( PlayerThreadPriority == THREAD_PRIORITY_IDLE ||
		PlayerThreadPriority == THREAD_PRIORITY_LOWEST ||
		PlayerThreadPriority == THREAD_PRIORITY_BELOW_NORMAL ||
		PlayerThreadPriority == THREAD_PRIORITY_NORMAL ||
		PlayerThreadPriority == THREAD_PRIORITY_ABOVE_NORMAL ||
		PlayerThreadPriority == THREAD_PRIORITY_HIGHEST ||
		PlayerThreadPriority == THREAD_PRIORITY_TIME_CRITICAL)
	    if(!SetThreadPriority(GetCurrentThread(), PlayerThreadPriority))
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Error changing thread priority");
#endif
#endif
#else
	/* UNIX */
#ifdef HAVE_SIGNAL
	signal(SIGINT, sigterm_exit);
	signal(SIGTERM, sigterm_exit);
#ifdef SIGPIPE
	signal(SIGPIPE, sigterm_exit);    /* Handle broken pipe */
#endif /* SIGPIPE */
#endif /* HAVE_SIGNAL */

#endif

	/* Open output device */
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Open output: %c, %s",
		  play_mode->id_character,
		  play_mode->id_name);

	if (play_mode->flag & PF_PCM_STREAM) {
	    play_mode->extra_param[1] = aq_calc_fragsize();
	    ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		      "requesting fragment size: %d",
		      play_mode->extra_param[1]);
	}
#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
	if(play_mode->open_output() < 0)
	{
	    ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
		      "Couldn't open %s (`%c')",
		      play_mode->id_name, play_mode->id_character);
	    output_fail = 1;
	    ctl->close();
	    return 2;
	}
#endif /* IA_W32GUI */
	if(!control_ratio)
	{
	    control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	    if(control_ratio < 1)
		control_ratio = 1;
	    else if (control_ratio > MAX_CONTROL_RATIO)
		control_ratio = MAX_CONTROL_RATIO;
	}
	if(!opt_load_all_instrument)
		init_load_soundfont();
	if(!output_fail)
	{
	    aq_setup();
	    timidity_init_aq_buff();
	}
	if(allocate_cache_size > 0)
	    resamp_cache_reset();

	if (def_prog >= 0)
		set_default_program(def_prog);
	if (*def_instr_name)
		set_default_instrument(def_instr_name);

	if(ctl->flags & CTLF_LIST_RANDOM)
	    randomize_string_list(files, nfiles);
	else if(ctl->flags & CTLF_LIST_SORT)
	    sort_pathname(files, nfiles);

#if (defined(__W32__) && !defined(__W32G__)) && defined(FORCE_TIME_PERIOD)
	if (timeGetDevCaps(&tcaps, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	    tcaps.wPeriodMin = 10;
	timeBeginPeriod(tcaps.wPeriodMin);
#endif /* (__W32__ && !__W32G__) && FORCE_TIME_PERIOD */

	/* Return only when quitting */
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "pass_playing_list() nfiles=%d", nfiles);

	retval=ctl->pass_playing_list(nfiles, files);

#if (defined(__W32__) && !defined(__W32G__)) && defined(FORCE_TIME_PERIOD)
	timeEndPeriod(tcaps.wPeriodMin);
#endif /* (__W32__ && !__W32G__) && FORCE_TIME_PERIOD */

	if(intr)
	    aq_flush(1);

#ifdef XP_UNIX
	return 0;
#endif /* XP_UNIX */
	
	if (play_mode->close_output)
		play_mode->close_output();
	safe_free(play_mode->name);
	play_mode->name = NULL;
	ctl->close();
	wrdt->close();
#ifdef __W32__
	DeleteCriticalSection (&critSect);
#endif

#ifdef BORLANDC_EXCEPTION
    } __except(1) {
	fprintf(stderr, "\nError!!!\nUnexpected Exception Occured!\n");
	if(play_mode->fd != -1)
	{
		play_mode->purge_output();
		play_mode->close_output();
	}
	ctl->close();
	wrdt->close();
	DeleteCriticalSection (&critSect);
	exit(EXIT_FAILURE);
    }
#endif /* BORLANDC_EXCEPTION */

#ifdef SUPPORT_SOUNDSPEC
    if(view_soundspec_flag)
	close_soundspec();
#endif /* SUPPORT_SOUNDSPEC */

    free_archive_files();
#ifdef SUPPORT_SOCKET
    url_news_connection_cache(URL_NEWS_CLOSE_CACHE);
#endif /* SUPPORT_SOCKET */

    return retval;
}

#ifdef IA_W32GUI
int w32gSecondTiMidity(int opt, int argc, char **argv);
int w32gSecondTiMidityExit(void);
int w32gLoadDefaultPlaylist(void);
int w32gSaveDefaultPlaylist(void);
extern int volatile save_playlist_once_before_exit_flag;
#endif /* IA_W32GUI */


#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
static int CoInitializeOK = 0;
#endif


#if !defined(KBTIM_SETUP) && !defined(WINDRV_SETUP)

#if !defined(ANOTHER_MAIN) || defined(__W32__)
#ifdef __W32__ /* Windows */
#if defined(IA_W32GUI) && !defined(__CYGWIN32__) && !defined(__MINGW32__) \
		|| defined(IA_W32G_SYN) /* _MSC_VER, _BORLANDC_, __WATCOMC__ */
int win_main(int argc, char **argv)
#else /* Cygwin, MinGW or console */
int __cdecl main(int argc, char **argv)
#endif
#else /* UNIX */
int main(int argc, char **argv)
#endif
{
	int c, err = 0, i;
	int nfiles;
	char **files;
	char *files_nbuf = NULL;
	int main_ret;
	int longind;
#if defined(DANGEROUS_RENICE) && !defined(__W32__) && !defined(main)
	/*
	 * THIS CODES MUST EXECUTE BEGINNING OF MAIN FOR SECURITY.
	 * DON'T PUT ANY CODES ABOVE.
	 */
#include <sys/resource.h>
	int uid;
#ifdef sun
	extern int setpriority(int which, id_t who, int prio);
	extern int setreuid(int ruid, int euid);
#endif
	
	uid = getuid();
	if (setpriority(PRIO_PROCESS, 0, DANGEROUS_RENICE) < 0) {
		perror("setpriority");
		fprintf(stderr, "Couldn't set priority to %d.", DANGEROUS_RENICE);
	}
	setreuid(uid, uid);
#endif

#if defined(REDIRECT_STDOUT)
	memcpy(stdout, fopen(REDIRECT_STDOUT, "a+"), sizeof(FILE));
	printf("TiMidity++ start\n");
	fflush(stdout);
#endif
///r
#ifdef __W32__
#if defined(IA_W32GUI) && !defined(WIN32GCC) || defined(IA_W32G_SYN)
	/* WinMain */
#elif defined(TIMIDITY_LEAK_CHECK)
	_CrtSetDbgFlag(CRTDEBUGFLAGS);
#endif
	atexit(w32_exit);
#endif /* __W32__ */
#if !defined(KBTIM) && !defined(WINDRV)
	OverrideSFSettingLoad();
#endif /* KBTIM WINVSTI */
#if defined(VST_LOADER_ENABLE) && defined(TIM_CUI) && defined(TWSYNSRV)
	if (hVSTHost == NULL) {
#ifdef _WIN64
		hVSTHost = LoadLibrary("timvstwrap_x64.dll");
#else
		hVSTHost = LoadLibrary("timvstwrap.dll");
#endif
		if (hVSTHost != NULL) {
			((vst_open)GetProcAddress(hVSTHost, "vstOpen"))();
		}
	}
#endif

#if defined(__W32__) && !defined(WINDRV)
	(void)w32_reset_dll_directory();
#endif
#ifdef main
{
	static int maincnt = 0;
	
	if (maincnt++ > 0) {
		do {
			argc--, argv++;
		} while (argv[0][0] == '-');
		ctl->pass_playing_list(argc, argv);
		return 0;
	}
}
#endif
#ifdef IA_DYNAMIC
{
	dynamic_lib_root = safe_strdup(SHARED_LIB_PATH);
	dl_init(argc, argv);
}
#endif /* IA_DYNAMIC */
	if ((program_name = pathsep_strrchr(argv[0])))
		program_name++;
	else
		program_name = argv[0];
	if (strncmp(program_name, "timidity", 8) == 0)
		;
	else if (strncmp(program_name, "kmidi", 5) == 0)
		set_ctl("q");
	else if (strncmp(program_name, "tkmidi", 6) == 0)
		set_ctl("k");
	else if (strncmp(program_name, "gtkmidi", 6) == 0)
		set_ctl("g");
	else if (strncmp(program_name, "xmmidi", 6) == 0)
		set_ctl("m");
	else if (strncmp(program_name, "xawmidi", 7) == 0)
		set_ctl("a");
	else if (strncmp(program_name, "xskinmidi", 9) == 0)
		set_ctl("i");
	if (argc == 1 && !strchr(INTERACTIVE_INTERFACE_IDS, ctl->id_character)) {
		interesting_message();
		return 0;
	}
	timidity_start_initialize();
#if defined (IA_W32GUI) || defined (IA_W32G_SYN)
	if (CoInitialize(NULL) == S_OK)
		CoInitializeOK = 1;
	w32g_initialize();
#ifdef IA_W32GUI
	/* Secondary TiMidity Execute */
	/*	FirstLoadIniFile(); */
	if (w32gSecondTiMidity(SecondMode, argc, argv) == FALSE) {
		w32gSecondTiMidityExit();
		if (CoInitializeOK)
			CoUninitialize();
		w32g_free_playlist();
		w32g_uninitialize();
		w32g_free_doc();
		return 0;
	}
#endif
	for (c = 1; c < argc; c++)
		if (is_directory(argv[c])) {
			char *p;
			
			p = (char *) safe_malloc(strlen(argv[c]) + 2);
			strcpy(p, argv[c]);
			directory_form(p);
			argv[c] = p;
		}
#endif /* IA_W32GUI || IA_W32G_SYN */
#if defined(IA_WINSYN) || defined(IA_PORTMIDISYN) || defined(IA_NPSYN) || defined(IA_W32G_SYN)
	opt_sf_close_each_file = 0;
#endif 
	optind = longind = 0;
#ifdef __W32__
	while ((c = getopt_long(argc, argv, optcommands, longopts, &longind)) > 0)
		if ((err = set_tim_opt_long_cfg(c, optarg, longind)) != 0)
			break;
#endif
	if (got_a_configuration != 1){	
		if ((err = timidity_pre_load_configuration()) != 0)
			return err;
	}	
	optind = longind = 0;
	while ((c = getopt_long(argc, argv, optcommands, longopts, &longind)) > 0)
		if ((err = set_tim_opt_long(c, optarg, longind)) != 0)
			break;
	err += timidity_post_load_configuration();
	/* If there were problems, give up now */
	if (err || (optind >= argc && !strchr(INTERACTIVE_INTERFACE_IDS, ctl->id_character))) {
		if (!got_a_configuration) {
#ifdef __W32__
			char config1[1024], config2[1024];
			
			memset(config1, 0, sizeof(config1));
			memset(config2, 0, sizeof(config2));
#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
{
			extern char *ConfigFile;
			
			strncpy(config1, ConfigFile, sizeof(config1) - 1);
}
#else /* !IA_W32GUI && !IA_W32G_SYN */
			GetWindowsDirectory(config1, 1023 - 13);
			strcat(config1, "\\TIMIDITY.CFG");
#endif
			if (GetModuleFileName(NULL, config2, 1023)) {
				char *strp;
				
				config2[1023] = '\0';
				if (strp = strrchr(config2, '\\')) {
					*(++strp) = '\0';
					strncat(config2, "TIMIDITY.CFG",
							sizeof(config2) - strlen(config2) - 1);
				}
			}
			ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
					"%s: Can't read any configuration file.\n"
					"Please check %s or %s", program_name, config1, config2);
#else
			ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
					"%s: Can't read any configuration file.\n"
					"Please check " CONFIG_FILE, program_name);
#endif /* __W32__ */
		} else
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Try %s -h for help", program_name);
/* Try to continue if it is Windows version */
#if !defined(IA_W32GUI) && !defined(IA_W32G_SYN)
		return 1; /* problems with command line */
#endif
	}
	timidity_init_player();
///r
	load_all_instrument();

#ifdef MULTI_THREAD_COMPUTE	
#if defined(__W32__) && ( defined(IA_W32G_SYN) )
	set_compute_thread_priority(syn_ThreadPriority);
#endif
	begin_compute_thread();
#endif
	nfiles = argc - optind;
	files  = argv + optind;
	if (nfiles > 0
			&& ctl->id_character != 'r' && ctl->id_character != 'A'
			&& ctl->id_character != 'W' && ctl->id_character != 'N'
			&& ctl->id_character != 'P')
		files = expand_file_archives(files, &nfiles);
	if (nfiles > 0)
		files_nbuf = files[0];
#if !defined(IA_W32GUI) && !defined(IA_W32G_SYN)
	if (dumb_error_count)
		sleep(1);
#endif
#ifdef IA_W32GUI
	w32gLoadDefaultPlaylist();
	main_ret = timidity_play_main(nfiles, files);
	w32_atexit = 0;
	if (save_playlist_once_before_exit_flag) {
		save_playlist_once_before_exit_flag = 0;
		w32gSaveDefaultPlaylist();
	}
	w32gSecondTiMidityExit();
	if (CoInitializeOK)
		CoUninitialize();
	w32g_free_playlist();
	w32g_uninitialize();
	w32g_free_doc();
#elif defined(__W32__)
	/* CUI, SYN */
	main_ret = timidity_play_main(nfiles, files);
	w32_atexit = 0;
#ifdef IA_W32G_SYN
	if (CoInitializeOK)
		CoUninitialize();
	w32g_uninitialize();
#endif /* IA_W32G_SYN */
#else
	/* UNIX */
	main_ret = timidity_play_main(nfiles, files);
#endif /* IA_W32GUI */
///r
#ifdef MULTI_THREAD_COMPUTE
	terminate_compute_thread();
#endif
#ifdef SUPPORT_SOCKET
	safe_free(url_user_agent);
	url_user_agent = NULL;
	safe_free(url_http_proxy_host);
	url_http_proxy_host = NULL;
	safe_free(url_ftp_proxy_host);
	url_ftp_proxy_host = NULL;
	safe_free(user_mailaddr);
	user_mailaddr = NULL;
#endif
#ifdef IA_DYNAMIC
	safe_free(dynamic_lib_root);
	dynamic_lib_root = NULL;
#endif
	safe_free(pcm_alternate_file);
	pcm_alternate_file = NULL;
	safe_free(opt_output_name);
	opt_output_name = NULL;
	safe_free(opt_aq_max_buff);
	opt_aq_max_buff = NULL;
	safe_free(opt_aq_fill_buff);
	opt_aq_fill_buff = NULL;
	safe_free(opt_reduce_voice_threshold);
	opt_reduce_voice_threshold = NULL;
	safe_free(opt_reduce_quality_threshold);
	opt_reduce_quality_threshold = NULL;
	safe_free(opt_reduce_polyphony_threshold);
	opt_reduce_polyphony_threshold = NULL;
	safe_free(output_text_code);
	output_text_code = NULL;
	safe_free(wrdt_open_opts);
	wrdt_open_opts = NULL;
	if (nfiles > 0
			&& ctl->id_character != 'r' && ctl->id_character != 'A'
			&& ctl->id_character != 'W' && ctl->id_character != 'N'
			&& ctl->id_character != 'P') {
		safe_free(files_nbuf);
		files_nbuf = NULL;
		safe_free(files);
		files = NULL;
	}
	free_soft_queue();
	free_audio_bucket();
	free_instruments(0);
	free_soundfonts();
	free_cache_data();
	free_freq_data();
	free_wrd();
	free_readmidi();
///r
	free_playmidi();
	free_mix_c();
	free_global_mblock();
	tmdy_free_config();
	//free_reverb_buffer();
	free_effect_buffers();
///r
#ifdef ENABLE_SFZ
	free_sfz();
#endif
#ifdef INT_SYNTH
	free_int_synth();
#endif // INT_SYNTH
	free_voices();
	uninitialize_resampler_coeffs();
	for (i = 0; i < MAX_CHANNELS; i++)
		free_drum_effect(i);
#if defined(VST_LOADER_ENABLE) && defined(TIM_CUI) && defined(TWSYNSRV)
	if (hVSTHost) {
	// only load , no save
	//	(vst_close)GetProcAddress(hVSTHost, "vstClose")();
		FreeLibrary(hVSTHost);
		hVSTHost = NULL;
	}
#endif
	return main_ret;
}
#endif /* !ANOTHER_MAIN || __W32__ */

#ifdef __W32__
static void w32_exit(void)
{
	int i;
	if (!w32_atexit)
		return;
	
#if defined(IA_W32GUI) || defined(IA_W32G_SYN) || defined(TIM_CUI) || defined(TWSYNSRV)
#ifdef MULTI_THREAD_COMPUTE
	terminate_compute_thread();
#endif
#endif
#ifdef SUPPORT_SOCKET
	safe_free(url_user_agent);
	url_user_agent = NULL;
	safe_free(url_http_proxy_host);
	url_http_proxy_host = NULL;
	safe_free(url_ftp_proxy_host);
	url_ftp_proxy_host = NULL;
	safe_free(user_mailaddr);
	user_mailaddr = NULL;
#endif
#ifdef IA_DYNAMIC
	safe_free(dynamic_lib_root);
	dynamic_lib_root = NULL;
#endif
	safe_free(pcm_alternate_file);
	pcm_alternate_file = NULL;
	safe_free(opt_output_name);
	opt_output_name = NULL;
	safe_free(opt_aq_max_buff);
	opt_aq_max_buff = NULL;
	safe_free(opt_aq_fill_buff);
	opt_aq_fill_buff = NULL;
	safe_free(opt_reduce_voice_threshold);
	opt_reduce_voice_threshold = NULL;
	safe_free(opt_reduce_quality_threshold);
	opt_reduce_quality_threshold = NULL;
	safe_free(opt_reduce_polyphony_threshold);
	opt_reduce_polyphony_threshold = NULL;
	safe_free(output_text_code);
	output_text_code = NULL;
	safe_free(wrdt_open_opts);
	wrdt_open_opts = NULL;

	free_soft_queue();
	free_audio_bucket();
	free_instruments(0);
	free_soundfonts();
	free_cache_data();
	free_freq_data();
	free_wrd();
	free_readmidi();
	free_playmidi();
	free_mix_c();
	free_global_mblock();
	tmdy_free_config();
	//free_reverb_buffer();
	free_effect_buffers();
///r
#ifdef ENABLE_SFZ
	free_sfz();
#endif
#ifdef INT_SYNTH
	free_int_synth();
#endif // INT_SYNTH
	free_voices();
	uninitialize_resampler_coeffs();
	for (i = 0; i < MAX_CHANNELS; i++)
		free_drum_effect(i);
///r
#ifdef VST_LOADER_ENABLE
	if (hVSTHost) {
	// only load , no save
	//	((vst_close) GetProcAddress(hVSTHost, "vstClose"))();
		FreeLibrary(hVSTHost);
		hVSTHost = NULL;
	}
#endif
}
#endif /* __W32__ */


#endif /* KBTIM_SETUP */
