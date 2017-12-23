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

    wave_audio.c

    Functions to output RIFF WAVE format data to a file or stdout.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __POCC__
#include <sys/types.h>
#endif // for off_t
#include <stdio.h>
#include <math.h>

#ifdef __W32__
#include <io.h>
#endif /* __W32__ */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#elif defined(HAVE_STRINGS_H)
#include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#ifdef __FreeBSD__
#include <stdio.h>
#endif
#include <ctype.h>

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "timer.h"

#ifdef _MSC_VER
#pragma warning(disable:4005)
#undef __cplusplus
//#include <audiodefs.h> // DirectX SDK
//#include <Ks.h> // Windows SDK
//#include <KsGuid.h> // Windows SDK
//#include <KsMedia.h> // Windows SDK
#endif /* _MSC_VER */

///r
/* Windows WAVE File Encoding Tags */
#ifndef WAVE_FORMAT_UNKNOWN
#define  WAVE_FORMAT_UNKNOWN                    0x0000 /* Microsoft Corporation */
#define  WAVE_FORMAT_PCM                        0x0001 /* Microsoft Corporation */
#define  WAVE_FORMAT_ADPCM                      0x0002 /* Microsoft Corporation */
#define  WAVE_FORMAT_IEEE_FLOAT                 0x0003 /* Microsoft Corporation */
#define  WAVE_FORMAT_ALAW                       0x0006 /* Microsoft Corporation */
#define  WAVE_FORMAT_MULAW                      0x0007 /* Microsoft Corporation */
#define  WAVE_FORMAT_EXTENSIBLE                 0xFFFE /* Microsoft */
#endif

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int32 output_data(const uint8 *buf, size_t bytes);
static int acntl(int request, void *arg);

/* export the playback mode */

#define dpm wave_play_mode

PlayMode dpm = {
    DEFAULT_RATE,
#ifdef LITTLE_ENDIAN
    PE_16BIT | PE_SIGNED,
#else
    PE_16BIT | PE_SIGNED | PE_BYTESWAP,
#endif
    PF_PCM_STREAM | PF_FILE_OUTPUT,
    -1,
    { 0, 0, 0, 0, 0 },
    "RIFF WAVE file", 'w',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

#ifndef UPDATE_STEP
#define UPDATE_STEP ((DEFAULT_RATE * 2 * 2) * 2) /* (44k * 16bit * 2ch * 2sec) */
#endif

typedef struct {
    uint8 *buffer;
    off_size_t total_output_bytes, buffer_offset_size,
	header_bytes, datachunk_offset,
	factchunk_offset,
	peakl_point, peakr_point, peakchunk_offset;
    float current_peakl, current_peakr;
    int bytes_per_sample;
    int already_warning_lseek;
} WAVE_ctx;

WAVE_ctx *wave_ctx = NULL;

typedef struct {
    int extensible;             /* --wave-format-extensible */
    int update_step;            /* --wave-update-step */
} WAVE_options;

WAVE_options wave_options = {
    0,              /* extensible */
    UPDATE_STEP,    /* update_step */
};

static double counter = 0.0;

/*************************************************************************/

static const char *orig_RIFFheader =
  "RIFF" "\377\377\377\377"
  "WAVE" "fmt " "\050\000\000\000" "\001\000"
  /* 22: channels */ "\001\000"
  /* 24: frequency */ "xxxx"
  /* 28: bytes/second */ "xxxx"
  /* 32: bytes/sample */ "\004\000"
  /* 34: bits/sample */ "\020\000"
  /* 36: cbSize */ "\026\000"
  /* 38: valid bits */ "\000\000"
  /* 40: channnelmask */ "\000\000\000\000"
  /* 44: sub format */ "\377\377" "\x00\x00\x00\x00\x10\x00"
                       "\x80\x00\x00\xAA\x00\x38\x9B\x71"
;

static const char *orig_DATAchunk =
  "data" "\377\377\377\377";

static const char *orig_FACTchunk =
  "fact" "\004\000\000\000"
  /* 8: length in sample */ "\000\000\000\000"
;

static const char *orig_PEAKchunk =
  "PEAK" "\030\000\000\000"
  /* 8: version */ "\001\000\000\000"
  /* 12: unix timestamp */ "\000\000\000\000"
  /* 16: peak(32-bit LE float) */ "\000\000\200\077"
  /* 20: peak point(sample) */ "\000\000\000\000"
  /* 24: peak(32-bit LE float) */ "\000\000\200\077"
  /* 28: peak point(sample) */ "\000\000\000\000"
;


/* We support follows WAVE format:
 * 8 bit unsigned pcm
 * 16 bit signed pcm (little endian)
 * A-law
 * U-law
 */



void wave_set_option_extensible(int extensible)
{
    wave_options.extensible = extensible;
}

void wave_set_option_update_step(int update_step)
{
    if (update_step >= 10000)
	wave_options.update_step = update_step;
    else if (update_step > 0)
	wave_options.update_step = update_step * 1024;
    else
	wave_options.update_step = UPDATE_STEP;
}

static int wav_output_open(const char *fname)
{
///r
    int ch, bytes;
//  int t;
    uint8 RIFFheader[112];
    int extensible, non_pcm;
    int writes;
    int fd;

    if (!strcmp(fname, "-"))
	fd = STDOUT_FILENO; /* data to stdout */
    else {
	/* Open the audio file */
	fd = open(fname, FILE_OUTPUT_MODE);
	if (fd < 0) {
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		      fname, strerror(errno));
	    return -1;
	}
    }

    if (!(wave_ctx = (WAVE_ctx*) calloc(1, sizeof(WAVE_ctx)))) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s", strerror(errno));
	return -1;
    }

    wave_ctx->datachunk_offset = 0;
    wave_ctx->factchunk_offset = 0;
    wave_ctx->peakchunk_offset = 0;

    /* Generate a (rather non-standard) RIFF header. We don't know yet
       what the block lengths will be. We'll fix that at close if this
       is a seekable file. */

    memcpy(RIFFheader, orig_RIFFheader, 60);
    writes = 60;
///r
    if (dpm.encoding & PE_16BIT) {
	if (wave_options.extensible) {
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_EXTENSIBLE);
	    *((uint16*)(RIFFheader + 44)) = LE_SHORT(WAVE_FORMAT_PCM);
	}
	else
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_PCM);
	bytes = 2;
    } else if (dpm.encoding & PE_24BIT) {
	if (wave_options.extensible) {
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_EXTENSIBLE);
	    *((uint16*)(RIFFheader + 44)) = LE_SHORT(WAVE_FORMAT_PCM);
	}
	else
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_PCM);
	bytes = 3;
    } else if (dpm.encoding & PE_32BIT) {
	if (wave_options.extensible) {
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_EXTENSIBLE);
	    *((uint16*)(RIFFheader + 44)) = LE_SHORT(WAVE_FORMAT_PCM);
	}
	else
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_PCM);
	bytes = 4;
    } else if (dpm.encoding & PE_F32BIT) {
	if (wave_options.extensible) {
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_EXTENSIBLE);
	    *((uint16*)(RIFFheader + 44)) = LE_SHORT(WAVE_FORMAT_IEEE_FLOAT);
	}
	else
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_IEEE_FLOAT);
	bytes = 4;
    } else if (dpm.encoding & PE_64BIT) {
	if (wave_options.extensible) {
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_EXTENSIBLE);
	    *((uint16*)(RIFFheader + 44)) = LE_SHORT(WAVE_FORMAT_PCM);
	}
	else
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_PCM);
	bytes = 8;
    } else if (dpm.encoding & PE_F64BIT) {
	if (wave_options.extensible) {
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_EXTENSIBLE);
	    *((uint16*)(RIFFheader + 44)) = LE_SHORT(WAVE_FORMAT_IEEE_FLOAT);
	}
	else
	    *((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_IEEE_FLOAT);
	bytes = 8;
    } else if (dpm.encoding & PE_ALAW) {
	*((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_ALAW);
	bytes = 1;
    } else if (dpm.encoding & PE_ULAW) {
	*((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_MULAW);
	bytes = 1;
    } else {
	*((uint16*)(RIFFheader + 20)) = LE_SHORT(WAVE_FORMAT_PCM);
	bytes = 1;
    }
    ch = (dpm.encoding & PE_MONO) ? 1 : 2;
    *((uint16*)(RIFFheader + 22)) = LE_SHORT(ch);
    *((uint32*)(RIFFheader + 24)) = LE_LONG(dpm.rate);
    *((uint32*)(RIFFheader + 28)) = LE_LONG(dpm.rate * bytes * ch);
    *((uint16*)(RIFFheader + 32)) = LE_SHORT(bytes * ch);
    *((uint16*)(RIFFheader + 34)) = LE_SHORT(bytes * 8);
    wave_ctx->bytes_per_sample = bytes * ch;

    extensible = *((uint16*)(RIFFheader + 20)) == LE_SHORT(WAVE_FORMAT_EXTENSIBLE) ? 1 : 0;
    non_pcm = *((uint16*)(RIFFheader + 20)) != LE_SHORT(WAVE_FORMAT_PCM) ? 1 : 0;

    if (extensible) {
	*((uint16*)(RIFFheader + 38)) = LE_SHORT(bytes * 8); /* dwValidBitsPerSample */
    }
    else if (non_pcm) {
	*((uint16*)(RIFFheader + 16)) = LE_SHORT(18); /* ckSize */
	*((uint16*)(RIFFheader + 36)) = LE_SHORT(0); /* cbSize */
	writes -= 22;
    }
    else {
	*((uint16*)(RIFFheader + 16)) = LE_SHORT(16); /* ckSize */
	writes -= 24;
    }

    if (non_pcm) {
	memcpy(RIFFheader + writes, orig_FACTchunk, 12);
	wave_ctx->factchunk_offset = writes;
	writes += 12;
    }

    if (dpm.encoding & PE_F32BIT || dpm.encoding & PE_F64BIT) {
	memcpy(RIFFheader + writes, orig_PEAKchunk, 32);
	wave_ctx->peakchunk_offset = writes;
	writes += 32;
	if (dpm.encoding & PE_MONO) {
	    *((uint16*)(RIFFheader + wave_ctx->peakchunk_offset + 4)) = LE_SHORT(16); /* cbSize */
	    writes -= 8; /* del right ch */
	}
    }

    memcpy(RIFFheader + writes, orig_DATAchunk, 8);
    wave_ctx->datachunk_offset = writes;
    writes += 8;
    wave_ctx->header_bytes = writes;

    if (std_write(fd, RIFFheader, writes) == -1) {
	int32 n;
	while (((n = std_write(fd, RIFFheader, writes)) == -1) && errno == EINTR);
	if (n == -1) {
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: write: %s",
		      dpm.name, strerror(errno));
	    close_output();
	    return -1;
	}
    }

    /* Reset the length counter */
    wave_ctx->buffer = NULL;
    wave_ctx->total_output_bytes = 0;
    wave_ctx->already_warning_lseek = 0;
    wave_ctx->peakl_point = 0;
    wave_ctx->current_peakl = 0;
    wave_ctx->peakr_point = 0;
    wave_ctx->current_peakr = 0;

    counter = get_current_calender_time();

    return fd;
}

static int auto_wav_output_open(const char *input_filename)
{
    char *output_filename;

    output_filename = create_auto_output_name(input_filename, "wav", NULL, 0);
    if (!output_filename) {
	return -1;
    }
    if ((dpm.fd = wav_output_open(output_filename)) == -1) {
	safe_free(output_filename);
	return -1;
    }
    safe_free(dpm.name);
    dpm.name = output_filename;
    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Output %s", dpm.name);
    return 0;
}

static int open_output(void)
{
///r
#ifdef LITTLE_ENDIAN
    dpm.encoding &= ~(PE_BYTESWAP);
#else
    dpm.encoding |= PE_BYTESWAP;
#endif
    if (dpm.encoding & (PE_16BIT | PE_24BIT | PE_32BIT | PE_F32BIT | PE_64BIT | PE_F64BIT))
	dpm.encoding |= PE_SIGNED;
    else // 8bit PE_ULAW PE_ALAW only unsigned
	dpm.encoding &= ~PE_SIGNED;

#if 0
    int32 include_enc, exclude_enc;
    include_enc = exclude_enc = 0;
    if (dpm.encoding & (PE_F64BIT | PE_64BIT | PE_F32BIT | PE_32BIT | PE_24BIT | PE_16BIT)) {
#ifdef LITTLE_ENDIAN
	exclude_enc = PE_BYTESWAP;
#else
	include_enc = PE_BYTESWAP;
#endif /* LITTLE_ENDIAN */
	include_enc |= PE_SIGNED;
    }
    else if (!(dpm.encoding & (PE_ULAW | PE_ALAW))) {
	exclude_enc = PE_SIGNED;
    }
    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);
#endif

    if (!dpm.name)
	dpm.flag |= PF_AUTO_SPLIT_FILE;
    else {
	dpm.flag &= ~PF_AUTO_SPLIT_FILE;
	if ((dpm.fd = wav_output_open(dpm.name)) == -1)
	    return -1;
    }

    return 0;
}

static int update_file(void)
{
    WAVE_ctx *ctx = wave_ctx;
    int32 n;

    while (((n = std_write(dpm.fd, ctx->buffer, ctx->buffer_offset_size)) == -1) && errno == EINTR);
    if (n == -1) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		  dpm.name, strerror(errno));
	return -1;
    }

    ctx->buffer_offset_size = 0;

    return 0;
}

static int update_header(void)
{
    WAVE_ctx *ctx = wave_ctx;
    off_size_t save_point;
    int32 tmp;

    if (ctx->already_warning_lseek)
	return 0;

    save_point = lseek(dpm.fd, 0, SEEK_CUR);
    if (save_point == -1 || lseek(dpm.fd, 4, SEEK_SET) == -1) {
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
	          "Warning: %s: %s: Can't make valid header",
	          dpm.name, strerror(errno));
	ctx->already_warning_lseek = 1;
	return 0;
    }

    tmp = LE_LONG(ctx->total_output_bytes + ctx->header_bytes - 8);
    if (std_write(dpm.fd, (const char*) &tmp, 4) == -1) {
	lseek(dpm.fd, save_point, SEEK_SET);
	return -1;
    }
    lseek(dpm.fd, ctx->datachunk_offset + 4, SEEK_SET);
    tmp = LE_LONG(ctx->total_output_bytes);
    std_write(dpm.fd, (const char*) &tmp, 4);

    if (ctx->factchunk_offset) {
	lseek(dpm.fd, ctx->factchunk_offset + 8, SEEK_SET);
	/* peak point */
	tmp = LE_LONG(ctx->total_output_bytes / ctx->bytes_per_sample);
	std_write(dpm.fd, (const char*) &tmp, 4);
    }

    if (ctx->peakchunk_offset) {
	lseek(dpm.fd, ctx->peakchunk_offset + 12, SEEK_SET);
	/* unix timestamp */
	tmp = LE_LONG(0);
	std_write(dpm.fd, (const char*) &tmp, 4);
	/* current left peak */
	memcpy(&tmp, &(ctx->current_peakl), sizeof(float));
	tmp = LE_LONG(tmp);
	std_write(dpm.fd, (const char*) &tmp, 4);
	/* left peak point */
	tmp = LE_LONG(ctx->peakl_point);
	std_write(dpm.fd, (const char*) &tmp, 4);
	if (!(dpm.encoding & PE_MONO)) {
	    /* current right peak */
	    memcpy(&tmp, &(ctx->current_peakr), sizeof(float));
	    tmp = LE_LONG(tmp);
	    std_write(dpm.fd, (const char*) &tmp, 4);
	    /* right peak point */
	    tmp = LE_LONG(ctx->peakr_point);
	    std_write(dpm.fd, (const char*) &tmp, 4);
	}
    }

    lseek(dpm.fd, save_point, SEEK_SET);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "%s: Update RIFF WAVE header (size=%lu)", dpm.name, (unsigned long)(ctx->total_output_bytes));

    return 0;
}

static int32 output_data(const uint8 *buf, size_t bytes)
{
    WAVE_ctx *ctx = wave_ctx;

    if (dpm.fd == -1)
	return -1;

    if (dpm.encoding & PE_F32BIT) {
	const float *float_in = (const float*) buf;
	int32 count;
	if (dpm.encoding & PE_MONO) {
	    const int32 samples = divi_4(bytes);
	    count = samples;
	    while (count--) {
		const float in = fabs(*float_in);
		if (ctx->current_peakl < in) {
		    ctx->current_peakl = in;
		    ctx->peakl_point = divi_4(ctx->total_output_bytes)
			+ (samples - count);
		}
		float_in++;
	    }
	}
	else {
	    const int32 samples = divi_8(bytes);
	    count = samples;
	    while (count--) {
		float in = fabs(*float_in);
		if (ctx->current_peakl < in) {
		    ctx->current_peakl = in;
		    ctx->peakl_point = divi_8(ctx->total_output_bytes)
			+ (samples - count);
		}
		float_in++;
		in = fabs(*float_in);
		if (ctx->current_peakr < in) {
		    ctx->current_peakr = in;
		    ctx->peakr_point = divi_8(ctx->total_output_bytes)
			+ (samples - count);
		}
		float_in++;
	    }
	}
    }
    else if (dpm.encoding & PE_F64BIT) {
	const double *float_in = (const double*) buf;
	int32 count;
	if (dpm.encoding & PE_MONO) {
	    const int32 samples = divi_8(bytes);
	    count = samples;
	    while (count--) {
		const double in = fabs(*float_in);
		if (ctx->current_peakl < in) {
		    ctx->current_peakl = in;
		    ctx->peakl_point = divi_8(ctx->total_output_bytes)
			+ (samples - count);
		}
		float_in++;
	    }
	}
	else {
	    const int32 samples = divi_16(bytes);
	    count = samples;
	    while (count--) {
		double in = fabs(*float_in);
		if (ctx->current_peakl < in) {
		    ctx->current_peakl = in;
		    ctx->peakl_point = divi_16(ctx->total_output_bytes)
			+ (samples - count);
		}
		float_in++;
		in = fabs(*float_in);
		if (ctx->current_peakr < in) {
		    ctx->current_peakr = in;
		    ctx->peakr_point = divi_16(ctx->total_output_bytes)
			+ (samples - count);
		}
		float_in++;
	    }
	}
    }

    if (!ctx->buffer) {
	ctx->buffer = (uint8*) safe_large_malloc(wave_options.update_step + bytes);
	ctx->buffer_offset_size = 0;
    }

    memcpy(ctx->buffer + ctx->buffer_offset_size, buf, bytes);
    ctx->buffer_offset_size += bytes;

    if (ctx->buffer_offset_size >= wave_options.update_step)
    {
	if (update_file() == -1 || update_header() == -1)
	    return -1;
    }

    ctx->total_output_bytes += bytes;

    return bytes;
}

static void close_output(void)
{
    if (wave_ctx && dpm.fd != -1)
    {
	if (wave_ctx->total_output_bytes % 2 && wave_ctx->buffer) { /* Padding byte if `total_output_bytes' is odd */
	    memset(wave_ctx->buffer + wave_ctx->buffer_offset_size, 0, 1);
	    wave_ctx->buffer_offset_size++;
	    wave_ctx->total_output_bytes++;
	}

	update_file();
	update_header();

	safe_free(wave_ctx->buffer);
	wave_ctx->buffer = NULL;

	ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		  "%s: Finished output (real time=%.2f)", dpm.id_name,
		  (double)(get_current_calender_time() - counter));
    }

    safe_free(wave_ctx);
    wave_ctx = NULL;

    if (dpm.fd != STDOUT_FILENO && /* We don't close stdout */
        dpm.fd != -1)
    {
	close(dpm.fd);
    }
    dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
    switch (request) {
    case PM_REQ_PLAY_START:
	if (dpm.flag & PF_AUTO_SPLIT_FILE) {
	    const char *filename = (current_file_info && current_file_info->filename) ?
				   current_file_info->filename : "Output.mid";
	    return auto_wav_output_open(filename);
	}
	return 0;
    case PM_REQ_PLAY_END:
	if (dpm.flag & PF_AUTO_SPLIT_FILE)
	    close_output();
	return 0;
    case PM_REQ_DISCARD:
	return 0;
    }
    return -1;
}

