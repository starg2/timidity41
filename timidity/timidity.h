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
*/

/*
 * Historical issues: This file once was a huge header file, but now is
 * devided into some smaller ones.  Please do not add things to this
 * header, but consider put them on other files.
 */

#ifndef TIMIDITY_H_INCLUDED
#define TIMIDITY_H_INCLUDED 1

/* 
   Table of contents:
   (1) Flags and definitions to customize timidity
   (3) inportant definitions not to customize
   (2) #includes -- include other headers
 */

/*****************************************************************************\
 section 1: some customize issues
\*****************************************************************************/

#define CONFIG_FILE_NAME "timidity.cfg"
#ifdef __W32__
#define CONFIG_FILE_NAME_P "\\timidity.cfg"
#else
#define CONFIG_FILE_NAME_P "/timidity.cfg"
#endif

/* You could specify a complete path, e.g. "/etc/timidity.cfg", and
   then specify the library directory in the configuration file. */
/* #define CONFIG_FILE "/etc/timidity.cfg" */
#ifndef CONFIG_FILE
#  ifdef DEFAULT_PATH
#    define CONFIG_FILE DEFAULT_PATH "/timidity.cfg"
#  else
#    define CONFIG_FILE PKGDATADIR "/timidity.cfg"
#  endif /* DEFAULT_PATH */
#endif /* CONFIG_FILE */


/* Filename extension, followed by command to run decompressor so that
   output is written to stdout. Terminate the list with a 0.

   Any file with a name ending in one of these strings will be run
   through the corresponding decompressor. If you don't like this
   behavior, you can undefine DECOMPRESSOR_LIST to disable automatic
   decompression entirely. */

#define DECOMPRESSOR_LIST { \
			      ".gz",  "gunzip -c %s", \
			      ".xz",  "xzcat %s", \
			      ".lzma", "lzcat %s", \
			      ".bz2", "bunzip2 -c %s", \
			      ".Z",   "zcat %s", \
			      ".zip", "unzip -p %s", \
			      ".lha", "lha -pq %s", \
			      ".lzh", "lha -pq %s", \
			      ".shn", "shorten -x %s -", \
			     0 }


/* Define GUS/patch converter. */
#define PATCH_CONVERTERS { \
			     ".wav", "wav2pat %s", \
			     0 }

/* When a patch file can't be opened, one of these extensions is
   appended to the filename and the open is tried again.

   This is ignored for Windows, which uses only ".pat" (see the bottom
   of this file if you need to change this.) */
#define PATCH_EXT_LIST { \
			   ".pat", \
			   ".shn", ".pat.shn", \
			   ".gz", ".pat.gz", \
			   ".bz2", ".pat.bz2", \
			   0 }


/* Acoustic Grand Piano seems to be the usual default instrument. */
#define DEFAULT_PROGRAM 0


/* Specify drum channels (terminated with -1).
   10 is the standard percussion channel.
   Some files (notably C:\WINDOWS\CANYON.MID) think that 16 is one too.
   On the other hand, some files know that 16 is not a drum channel and
   try to play music on it. This is now a runtime option, so this isn't
   a critical choice anymore. */
#define DEFAULT_DRUMCHANNELS {10, -1}
/* #define DEFAULT_DRUMCHANNELS {10, 16, -1} */

/* type of audio data */
//#define DATA_T_INT32 1
//#define DATA_T_FLOAT
#define DATA_T_DOUBLE

/* effect level type float/double */
//#define EFFECT_LEVEL_FLOAT // not yet

/* type of floating point number */
#define FLOAT_T_DOUBLE

/* A somewhat arbitrary frequency range. The low end of this will
   sound terrible as no lowpass filtering is performed on most
   instruments before resampling. */
#define MIN_OUTPUT_RATE 	4000
#define MAX_OUTPUT_RATE 	400000


/* Input volume in percent. */
#define DEFAULT_AMPLIFICATION 	70
/* Output volume in percent. */
#define DEFAULT_OUTPUT_AMPLIFICATION 	100


/* Default sampling rate, default polyphony, and maximum polyphony.
   All but the last can be overridden from the command line. */
#ifndef DEFAULT_RATE
#define DEFAULT_RATE	44100
#endif /* DEFAULT_RATE */

#define DEFAULT_VOICES	256
///r
#define MAX_VOICES	1024


/* The size of the internal buffer is 2^AUDIO_BUFFER_BITS samples.
   This determines maximum number of samples ever computed in a row.

   For Linux and FreeBSD users:

   This also specifies the size of the buffer fragment.  A smaller
   fragment gives a faster response in interactive mode -- 10 or 11 is
   probably a good number. Unfortunately some sound cards emit a click
   when switching DMA buffers. If this happens to you, try increasing
   this number to reduce the frequency of the clicks.

   For other systems:

   You should probably use a larger number for improved performance.

*/

#define AUDIO_BUFFER_BITS 12	/* Maxmum audio buffer size (2^bits) */


/* 1000 here will give a control ratio of 22:1 with 22 kHz output.
   Higher CONTROLS_PER_SECOND values allow more accurate rendering
   of envelopes and tremolo. The cost is CPU time. */
#define CONTROLS_PER_SECOND 1000

///r
/* Default resamplation algorighm. The following algorighms are available:
	num, name, order
    0, resample_none, 0
	1, resample_linear, 0
    2, resample_cspline, 0
    3, resample_lagrange, 0
	4, resample_newton, 11
    5, resample_gauss, 24
    6, resample_sharp, 2
    7, resample_linear_p, 2
	8, resample_sine, 0
	9, resample_square, 0
	10, resample_lanczos, 8
   check new resamplation in resample.h resample.c    
*/
#ifndef DEFAULT_RESAMPLATION
#define DEFAULT_RESAMPLATION_NUM 3
#define DEFAULT_RESAMPLATION_ORDER 0
#endif

#define DEFAULT_PRE_RESAMPLATION 0 // 0:off 1:on

/* Don't allow users to choose the resamplation algorithm. */
/* #define FIXED_RESAMPLATION */



/* This is an experimental kludge that needs to be done right, but if
   you've got an 8-bit sound card, or cheap multimedia speakers hooked
   to your 16-bit output device, you should definitely give it a try.

   Defining LOOKUP_HACK causes table lookups to be used in mixing
   instead of multiplication. We convert the sample data to 8 bits at
   load time and volumes to logarithmic 7-bit values before looking up
   the product, which degrades sound quality noticeably.

   Defining LOOKUP_HACK should save ~20% of CPU on an Intel machine.
   LOOKUP_INTERPOLATION might give another ~5% */
/* #define LOOKUP_HACK */
/* #define LOOKUP_INTERPOLATION */


/* Greatly reduces popping due to large volume/pan changes.
 * This is definitely worth the slight increase in CPU usage. */
//#define SMOOTH_MIXING // delete
#define CH_VOL_ENV_TIME  40 // default time (ms) for channel mixer XG
#define VOL_ENV_TIME  4 // default time (ms) for voice mixer other XG

///r
/* time (ms) to use for release a cut short note. Affects click removal. */
#define CUT_SHORT_TIME 40 // default time (ms) // altassign,mono 
#define CUT_SHORT_TIME2 200 // default time (ms) // overlaped voice

///r
/* time (ms) to use for ramping out a dying note. Affects click removal. */
#define MAX_DIE_TIME 40 // default time (0.1ms)

/* Make envelopes twice as fast. Saves ~20% CPU time (notes decay
   faster) and sounds more like a GUS. There is now a command line
   option to toggle this as well. */
/* #define FAST_DECAY */


/* How many bits to use for the fractional part of sample positions.
   This affects tonal accuracy. The entire position counter must fit
   in 32 bits, so with FRACTION_BITS equal to 12, the maximum size of
   a sample is 1048576 samples (2 megabytes in memory). The GUS gets
   by with just 9 bits and a little help from its friends...
   "The GUS does not SUCK!!!" -- a happy user :) */
/* if DATA_T_INT32 then FRACTION_BITS=12 */
#define FRACTION_BITS 14


/* For some reason the sample volume is always set to maximum in all
   patch files. Define this for a crude adjustment that may help
   equalize instrument volumes. */
#define ADJUST_SAMPLE_VOLUMES


/* If you have root access, you can define DANGEROUS_RENICE and chmod
   timidity setuid root to have it automatically raise its priority
   when run -- this may make it possible to play MIDI files in the
   background while running other CPU-intensive jobs. Of course no
   amount of renicing will help if the CPU time simply isn't there.

   The root privileges are used and dropped at the beginning of main()
   in timidity.c -- please check the code to your satisfaction before
   using this option. (And please check sections 11 and 12 in the
   GNU General Public License (under GNU Emacs, hit ^H^W) ;) */
/* #define DANGEROUS_RENICE -15 */


/* On some machines (especially PCs without math coprocessors),
   looking up sine values in a table will be significantly faster than
   computing them on the fly. Uncomment this to use lookups. */
#define LOOKUP_SINE


/* Shawn McHorse's resampling optimizations. These may not in fact be
   faster on your particular machine and compiler. You'll have to run
   a benchmark to find out. */
#define PRECALC_LOOPS


/* If calling ldexp() is faster than a floating point multiplication
   on your machine/compiler/libm, uncomment this. It doesn't make much
   difference either way, but hey -- it was on the TODO list, so it
   got done. */
/* #define USE_LDEXP */


/* Define the pre-resampling cache size.
 * This value is default. You can change the cache saze with
 * command line option.
 */
#define DEFAULT_CACHE_DATA_SIZE 0 // (2*1024*1024)


#ifdef SUPPORT_SOCKET
/* Please define your mail domain address. */
#ifndef MAIL_DOMAIN
#define MAIL_DOMAIN "@localhost"
#endif /* MAIL_DOMAIN */

/* Please define your mail name if you are at Windows.
 * Otherwise (maybe unix), undefine this macro
 */
/* #define MAIL_NAME "somebody" */
#endif /* SUPPORT_SOCKET */


/* Where do you want to put temporary file into ?
 * If you are in UNIX, you can undefine this macro. If TMPDIR macro is
 * undefined, the value is used in environment variable `TMPDIR'.
 * If both macro and environment variable is not set, the directory is
 * set to /tmp.
 */
/* #define TMPDIR "/var/tmp" */


/* To use GS drumpart setting. */
#define GS_DRUMPART

/**** Japanese section ****/
/* To use Japanese kanji code. */
#define JAPANESE

/* Select output text code:
 * "AUTO"	- Auto conversion by `LANG' environment variable (UNIX only)
 * "ASCII"	- Convert unreadable characters to '.'(0x2e)
 * "NOCNV"	- No conversion
 * "EUC"	- EUC
 * "JIS"	- JIS
 * "SJIS"	- shift JIS
 */

#ifndef JAPANESE
/* Not japanese (Select "ASCII" or "NOCNV") */
#define OUTPUT_TEXT_CODE "ASCII"
#else
/* Japanese */
#ifndef __W32__
/* UNIX (Select "AUTO" or "ASCII" or "NOCNV" or "EUC" or "JIS" or "SJIS") */
#define OUTPUT_TEXT_CODE "AUTO"
#else

#ifdef UNICODE
/* Windows (Unicode) */
#define OUTPUT_TEXT_CODE "UTF-8"
#else
/* Windows (Select "ASCII" or "NOCNV" or "SJIS") */
#define OUTPUT_TEXT_CODE "SJIS"
#endif
#endif
#endif


/* Undefine if you don't use modulation wheel MIDI controls.
 * There is a command line option to enable/disable this mode.
 */
#define MODULATION_WHEEL_ALLOW 1


/* Undefine if you don't use portamento MIDI controls.
 * There is a command line option to enable/disable this mode.
 */
#define PORTAMENTO_ALLOW 1


/* Undefine if you don't use NRPN vibrato MIDI controls
 * There is a command line option to enable/disable this mode.
 */
#define NRPN_VIBRATO_ALLOW 1


/* Define if you want to use reverb / freeverb controls in defaults.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
/* #define REVERB_CONTROL_ALLOW 1 */
#define FREEVERB_CONTROL_ALLOW 1


/* Define if you want to use chorus controls in defaults.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
#define CHORUS_CONTROL_ALLOW 1


/* Define if you want to use surround chorus in defaults.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
/* #define SURROUND_CHORUS_ALLOW 1 */


/* Define if you want to use channel pressure.
 * Channel pressure effect is different in sequencers.
 * There is a command line option to enable/disable this mode.
 */
#define GM_CHANNEL_PRESSURE_ALLOW 1


/* Define if you want to use voice chamberlin / moog LPF.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
/* #define VOICE_CHAMBERLIN_LPF_ALLOW 1 */
/* #define VOICE_MOOG_LPF_ALLOW 1 */
/* #define VOICE_BW_LPF_ALLOW 1 */
#define VOICE_LPF12_2_ALLOW 1

/* Define if you want to use modulation envelope.
 * This mode needs high CPU power.
 * There is a command line option to enable/disable this mode.
 */
#define MODULATION_ENVELOPE_ALLOW 1


/* Define if you want to trace text meta event at playing.
 * There is a command line option to enable/disable this mode.
 */
#define ALWAYS_TRACE_TEXT_META_EVENT


/* Define if you want to allow overlapped voice.
 * There is a command line option to enable/disable this mode.
 */
#define OVERLAP_VOICE_ALLOW 1
///r
#define OVERLAP_VOICE_COUNT 4

#define MAX_CHANNEL_VOICES 64

/* Define if you want to allow temperament control.
 * There is a command line option to enable/disable this mode.
 */
#define TEMPER_CONTROL_ALLOW 1


///r
#define DEFALT_REVERB_SEND 127
#define DEFALT_CHORUS_SEND 127
#define DEFALT_DELAY_SEND 127

///r
#if !defined(KBTIM) || !defined(WINVST) /*added by Kobarin*/
//#define REDUCE_VOICE_TIME_TUNING	(play_mode->rate/5) /* 0.2 sec */
//#define REDUCE_VOICE_TIME_TUNING	(play_mode->rate/200) /* 0.005 sec */
#define REDUCE_QUALITY_TIME_TUNING	99
#define REDUCE_VOICE_TIME_TUNING	75
#define REDUCE_POLYPHONY_TIME_TUNING 85
#endif
#define DEFAULT_REDUCE_QUALITY_THRESHOLD    "99.0%"
#define DEFAULT_REDUCE_VOICE_THRESHOLD      "75.0%"
#define DEFAULT_REDUCE_POLYPHONY_THRESHOLD  "85.0%"

///r
#define VOICE_EFFECT

#ifndef CFG_FOR_SF
#define INT_SYNTH
#endif

//#define ENABLE_SFZ
//#define ENABLE_DLS


/*****************************************************************************\
 section 2: some important definitions
\*****************************************************************************/
/*
  Anything below this shouldn't need to be changed unless you're porting
   to a new machine with other than 32-bit, big-endian words.
 */
/* change FRACTION_BITS above, not these */

/* This is enforced by some computations that must fit in an int */
#define MAX_CONTROL_RATIO 255

/* Audio buffer size has to be a power of two to allow DMA buffer
   fragments under the VoxWare (Linux & FreeBSD) audio driver */
#define AUDIO_BUFFER_SIZE (1<<AUDIO_BUFFER_BITS)


/* These affect general volume */
///r
#define MAX_BITS 32 // int32 buf ?
#define SAMPLE_BITS 16 // wave 16bit ?
#define GUARD_BITS 1 // guard over flow ?
#define OTHER_BITS 1 // mix, effect, ?
#define AMP_BITS (MAX_BITS - SAMPLE_BITS - GUARD_BITS - OTHER_BITS)

#define MAX_AMPLIFICATION 800
#define MAX_CHANNELS 64
/*#define MAX_CHANNELS 256*/
/* safe MAX_CHANNELS < 0x7F (SysEx broadcast, disable channel“™ */
#define MAXMIDIPORT 16

/* you cannot but use safe_malloc(). */
#define HAVE_SAFE_MALLOC 1

/* malloc's limit */
#define MAX_SAFE_MALLOC_SIZE (1<<23) /* 8M */

#define DEFAULT_SOUNDFONT_ORDER 0



/*****************************************************************************\
 section 3: include other headers
\*****************************************************************************/


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h> /* for __byte_swap_*() */
#endif /* HAVE_MACHINE_ENDIAN_H */

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#include "sysdep.h"
#include "support.h"
#include "optcode.h"

//#define VST_LOADER_ENABLE // config.h

#ifdef VST_LOADER_ENABLE
#include <windows.h>
extern HMODULE hVSTHost;
#include "vstwrapper.h"
#endif

#endif /* TIMIDITY_H_INCLUDED */
