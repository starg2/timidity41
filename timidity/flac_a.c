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

    flac_a.c
	Written by Iwata <b6330015@kit.jp>
    Functions to output FLAC / OggFLAC  (*.flac, *.ogg).
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "interface.h"
#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <fcntl.h>

#ifdef __W32__
#include <io.h>
#include <time.h>
#endif

#if defined(AU_FLAC) || defined(AU_OGGFLAC)


#include <FLAC/export.h> /* need export.h to figure out API version from FLAC_API_VERSION_CURRENT */
/* by LEGACY_FLAC we mean before FLAC 1.1.3 */
/* in FLAC 1.1.3, libOggFLAC is merged into libFLAC and all encoding layers are merged into the stream encoder */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 8
#define LEGACY_FLAC 1
#else
#undef LEGACY_FLAC
#endif

#include <FLAC/all.h>
#if defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
#include <OggFLAC/stream_encoder.h>
#endif

#if defined(AU_FLAC_DLL) || defined(AU_OGGFLAC_DLL)
#include <windows.h>
#undef FLAC_API
#undef OggFLAC_API
#define FLAC_API
#define OggFLAC_API
#endif

#ifdef AU_FLAC_DLL
#include "w32_libFLAC_dll_g.h"
#endif
#ifdef AU_OGGFLAC_DLL
#include "w32_libOGGFLAC_dll_g.h"
#endif

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "miditrace.h"

#ifdef __W32G__
#include "w32g.h"
#endif /* __W32G__ */

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int32 output_data(const uint8 *buf, size_t nbytes);
static int acntl(int request, void *arg);

/* export the playback mode */

#define dpm flac_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_SIGNED | PE_16BIT, PF_PCM_STREAM | PF_FILE_OUTPUT,
  -1,
  { 0 }, /* default: get all the buffer fragments you can */
#ifndef AU_OGGFLAC
  "FLAC", 'F',
#else
  "FLAC / OggFLAC", 'F',
#endif /* AU_OGGFLAC */
  NULL,
  open_output,
  close_output,
  output_data,
  acntl
};

typedef struct {
  off_size_t in_bytes;
  off_size_t out_bytes;
  union {
    FLAC__StreamEncoderState flac;
#ifdef LEGACY_FLAC
    FLAC__SeekableStreamEncoderState s_flac;
#ifdef AU_OGGFLAC
    OggFLAC__StreamEncoderState ogg;
#endif
#endif
  } state;
  union {
    union {
      FLAC__StreamEncoder *stream;
#ifdef LEGACY_FLAC
      FLAC__SeekableStreamEncoder *s_stream;
#endif
    } flac;
#if defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
    union {
      OggFLAC__StreamEncoder *stream;
    } ogg;
#endif
  } encoder;
  FLAC__int32 *oggbuf;
  int32 oggbuf_length;
} FLAC_ctx;

typedef struct {
#ifdef AU_OGGFLAC
  int isogg;
#endif
  int verify;
  int padding;
  int blocksize;
  int mid_side;
  int adaptive_mid_side;
  int exhaustive_model_search;
  int max_lpc_order;
  int qlp_coeff_precision_search;
  int qlp_coeff_precision;
  int min_residual_partition_order;
  int max_residual_partition_order;
	int seekable;
#ifndef LEGACY_FLAC
  int compression_level;
#endif
  int bits;
} FLAC_options;

/* default compress level is 5 */
FLAC_options flac_options = {
#ifdef AU_OGGFLAC
  0,    /* isogg */
#endif
  0,    /* verify */
  4096, /* padding */
  4608, /* blocksize */
  1,    /* mid_side */
  0,    /* adaptive_mid_side */
  0,    /* exhaustive-model-search */
  8,    /* max_lpc_order */
  0,    /* qlp_coeff_precision_search */
  0,    /* qlp_coeff_precision */
  3,    /* min_residual_partition_order */
  3,    /* max_residual_partition_order */
  0,    /* seekable */
#ifndef LEGACY_FLAC
  5,    /* compression_level */
#endif
  16,   /* bits */
};

#ifdef AU_OGGFLAC
static long serial_number = 0;
#endif
FLAC_ctx *flac_ctx = NULL;

#if defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
static FLAC__StreamEncoderWriteStatus
ogg_stream_encoder_write_callback(const OggFLAC__StreamEncoder *encoder,
				  const FLAC__byte buffer[],
				  unsigned bytes, unsigned samples,
				  unsigned current_frame, void *client_data);
#endif
static FLAC__StreamEncoderWriteStatus
flac_stream_encoder_write_callback(const FLAC__StreamEncoder *encoder,
				   const FLAC__byte buffer[],
#ifdef LEGACY_FLAC
				   unsigned bytes, unsigned samples,
#else
				   size_t bytes, unsigned samples,
#endif
				   unsigned current_frame, void *client_data);
#if !defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
static FLAC__StreamEncoderReadStatus
flac_stream_encoder_read_callback(const FLAC__StreamEncoder *encoder,
				  FLAC__byte buffer[],
				  size_t *bytes,
				  void *client_data);
#endif
#ifdef LEGACY_FLAC
static void flac_stream_encoder_metadata_callback(const FLAC__StreamEncoder *encoder,
						  const FLAC__StreamMetadata *metadata,
						  void *client_data);
static FLAC__StreamEncoderWriteStatus
flac_seekable_stream_encoder_write_callback(const FLAC__SeekableStreamEncoder *encoder,
				   const FLAC__byte buffer[],
				   unsigned bytes, unsigned samples,
				   unsigned current_frame, void *client_data);
static void flac_seekable_stream_encoder_metadata_callback(const FLAC__SeekableStreamEncoder *encoder,
						  const FLAC__StreamMetadata *metadata,
						  void *client_data);
#endif

/* preset */
void flac_set_compression_level(int compression_level)
{
  switch (compression_level) {
  case 0:
    flac_options.max_lpc_order = 0;
    flac_options.blocksize = 1152;
    flac_options.mid_side = 0;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 2;
    flac_options.max_residual_partition_order = 2;
    flac_options.exhaustive_model_search = 0;
    break;
  case 1:
    flac_options.max_lpc_order = 0;
    flac_options.blocksize = 1152;
    flac_options.mid_side = 0;
    flac_options.adaptive_mid_side = 1;
    flac_options.min_residual_partition_order = 2;
    flac_options.max_residual_partition_order = 2;
    flac_options.exhaustive_model_search = 0;
    break;
  case 2:
    flac_options.max_lpc_order = 0;
    flac_options.blocksize = 1152;
    flac_options.mid_side = 1;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 0;
    flac_options.max_residual_partition_order = 3;
    flac_options.exhaustive_model_search = 0;
    break;
  case 3:
    flac_options.max_lpc_order = 6;
    flac_options.blocksize = 4608;
    flac_options.mid_side = 0;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 3;
    flac_options.max_residual_partition_order = 3;
    flac_options.exhaustive_model_search = 0;
    break;
  case 4:
    flac_options.max_lpc_order = 8;
    flac_options.blocksize = 4608;
    flac_options.mid_side = 0;
    flac_options.adaptive_mid_side = 1;
    flac_options.min_residual_partition_order = 3;
    flac_options.max_residual_partition_order = 3;
    flac_options.exhaustive_model_search = 0;
    break;
  case 6:
    flac_options.max_lpc_order = 8;
    flac_options.blocksize = 4608;
    flac_options.mid_side = 1;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 0;
    flac_options.max_residual_partition_order = 4;
    flac_options.exhaustive_model_search = 0;
    break;
  case 7:
    flac_options.max_lpc_order = 8;
    flac_options.blocksize = 4608;
    flac_options.mid_side = 1;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 0;
    flac_options.max_residual_partition_order = 6;
    flac_options.exhaustive_model_search = 1;
    break;
  case 8:
    flac_options.max_lpc_order = 12;
    flac_options.blocksize = 4608;
    flac_options.mid_side = 1;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 0;
    flac_options.max_residual_partition_order = 6;
    flac_options.exhaustive_model_search = 1;
    break;
  case 5:
  default:
    flac_options.max_lpc_order = 8;
    flac_options.blocksize = 4608;
    flac_options.mid_side = 1;
    flac_options.adaptive_mid_side = 0;
    flac_options.min_residual_partition_order = 3;
    flac_options.max_residual_partition_order = 3;
    flac_options.exhaustive_model_search = 0;
  }

#ifndef LEGACY_FLAC
  flac_options.compression_level = compression_level;
#endif
}

void flac_set_option_padding(int padding)
{
  flac_options.padding = padding;
}
void flac_set_option_verify(int verify)
{
  flac_options.verify = verify;
}
#ifdef AU_OGGFLAC
void flac_set_option_oggflac(int isogg)
{
  flac_options.isogg = isogg;
}
#endif

/* packing function */
static void (*packing_func)(FLAC__int32 *dst, const uint8 *src, int32 length) = NULL;

static void packing_func_i16(FLAC__int32 *dst, const uint8 *src, int32 length)
{
  /*
    packing 16 -> 32 bit sample
  */
  int32 i;
  const FLAC__int16 *s = (const FLAC__int16*)src;
  for (i = 0; i < length; i++) {
    dst[i] = *s++;
  }
}

static void packing_func_i24(FLAC__int32 *dst, const uint8 *src, int32 length)
{
  /*
    packing 24 -> 32 bit sample
  */
  int32 i;
  const FLAC__byte *s = (const FLAC__byte*)src;
  for (i = 0; i < length; i++) {
    dst[i] = s[0] | (s[1] << 8u) | (s[2] << 16u) | (((s[2] & 0x80) ? 0xFF : 0x00) << 24u);
    s += 3;
  }
}

static void packing_func_i32(FLAC__int32 *dst, const uint8 *src, int32 length)
{
///r
  //test cnv
  /*
    packing 32 -> 24 bit range sample
  */
  int32 i;
  const FLAC__int32 *s = (const FLAC__int32*)src;
  for (i = 0; i < length; i++) {
    dst[i] = (*s++) >> 8;
  }
}

static void packing_func_f32(FLAC__int32 *dst, const uint8 *src, int32 length)
{
///r
  //test cnv
  /*
    packing floating-point -> 24 bit range sample
  */
  int32 i;
  const float *s = (const float*)src;
  const FLAC__int32 range = M_23BIT - 1;
  for (i = 0; i < length; i++) {
    dst[i] = (*s++) * range;
  }
}

static void packing_func_f64(FLAC__int32 *dst, const uint8 *src, int32 length)
{
  /*
    packing floating-point -> 24 bit range sample
  */
  int32 i;
  const double *s = (const double*)src;
  const FLAC__int32 range = M_23BIT - 1;
  for (i = 0; i < length; i++) {
    dst[i] = (*s++) * range;
  }
}

static void flac_session_close()
{
  FLAC_ctx *ctx = flac_ctx;

#if 1
  /*
  ¡Œã flac_output_open ŠÖ”“à•”‚Å‘Ä¶ŽžŠÔ‚ð’²‚×‚é•û–@‚ªì‚ç‚ê‚½‚ç
  FLAC__stream_encoder_set_total_samples_estimate ŠÖ”‚ðŽg‚Á‚Ä‚­‚¾‚³‚¢
   */
#ifdef AU_OGGFLAC
  if (flac_options.isogg) {
  }	/* ToDo */
  else
#endif
  if (0 < dpm.fd && ctx && 0 < ctx->in_bytes && (4 + 4 + 13) < ctx->out_bytes) {
    char buf[5];
    uint64 samples;
    int step;
    int nch;
    nch = (dpm.encoding & PE_MONO) ? 1 : 2;
    step = divi_8(flac_options.bits);
    samples = ctx->in_bytes / nch / step;

    if (-1 != lseek(dpm.fd, (4 + 4 + 13), SEEK_SET)) { /* "fLaC" + HEADER Block + STREAMINFO Block */
      /* calculate 'total_samples_estimate' */

      buf[4] = (samples >>  0) & 0xFF;
      buf[3] = (samples >>  8) & 0xFF;
      buf[2] = (samples >> 16) & 0xFF;
      buf[1] = (samples >> 24) & 0xFF;
      buf[0] = (((flac_options.bits - 1) << 4) & 0xF0) | ((samples >> 32) & 0x0F);
      std_write(dpm.fd, &buf[0], 5);
      ctl->cmsg(CMSG_INFO, VERB_DEBUG,
              "%s: Update FLAC header (samples=%d)", dpm.name, samples);
      lseek(dpm.fd, 0, SEEK_END);
    }
  }
#endif

  if (ctx) {
    safe_free(ctx->oggbuf);
#ifdef LEGACY_FLAC
#ifdef AU_OGGFLAC
    if (flac_options.isogg) {
      if (ctx->encoder.ogg.stream) {
	OggFLAC__stream_encoder_finish(ctx->encoder.ogg.stream);
	OggFLAC__stream_encoder_delete(ctx->encoder.ogg.stream);
      }
    }
    else
#endif /* AU_OGGFLAC */
    if (flac_options.seekable) {
      if (ctx->encoder.flac.s_stream) {
	FLAC__seekable_stream_encoder_finish(ctx->encoder.flac.s_stream);
	FLAC__seekable_stream_encoder_delete(ctx->encoder.flac.s_stream);
      }
    }
    else
    {
      if (ctx->encoder.flac.stream) {
	FLAC__stream_encoder_finish(ctx->encoder.flac.stream);
	FLAC__stream_encoder_delete(ctx->encoder.flac.stream);
      }
    }
#else
    if (ctx->encoder.flac.stream) {
      FLAC__stream_encoder_finish(ctx->encoder.flac.stream);
      FLAC__stream_encoder_delete(ctx->encoder.flac.stream);
    }
#endif
    safe_free(ctx);
    flac_ctx = ctx = NULL;
  }

  if (dpm.fd != STDOUT_FILENO && /* We don't close stdout */
    dpm.fd != -1) {
    close(dpm.fd);
  }
  dpm.fd = -1;
}

static int flac_output_open(const char *fname, const char *comment)
{
  int fd;
  int nch;
  FLAC__StreamMetadata padding;
  FLAC__StreamMetadata *metadata[4];
  int num_metadata = 0;
#ifndef LEGACY_FLAC
  FLAC__StreamEncoderInitStatus init_status;
#endif
  FLAC__StreamMetadata_VorbisComment_Entry vendentry;
  FLAC__StreamMetadata_VorbisComment_Entry commentry;

  FLAC_ctx *ctx;

  if (flac_ctx)
    flac_session_close();

  if (!(flac_ctx = (FLAC_ctx*) calloc(1, sizeof(FLAC_ctx)))) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s", strerror(errno));
    return -1;
  }

  ctx = flac_ctx;

  ctx->in_bytes = ctx->out_bytes = 0;
  ctx->oggbuf = NULL;
  ctx->oggbuf_length = 0;

  if (!strcmp(fname, "-")) {
    fd = STDOUT_FILENO; /* data to stdout */
    if (!comment)
      comment = "(stdout)";
  }
  else {
    /* Open the audio file */
    fd = open(fname, FILE_OUTPUT_MODE);
    if (fd < 0) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		fname, strerror(errno));
      return -1;
    }
    if (!comment)
      comment = fname;
  }

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
  flac_ConfigDialogInfoApply();
#endif

  dpm.fd = fd;
  nch = (dpm.encoding & PE_MONO) ? 1 : 2;
  if (dpm.encoding & PE_24BIT) {
    flac_options.bits = 24;
    packing_func = packing_func_i24;
  }
  else if (dpm.encoding & PE_32BIT) {
    flac_options.bits = 24;
    packing_func = packing_func_i32;
  }
  else if (dpm.encoding & PE_F32BIT) {
    flac_options.bits = 24;
    packing_func = packing_func_f32;
  }
  else if (dpm.encoding & PE_F64BIT) {
    flac_options.bits = 24;
    packing_func = packing_func_f64;
  }
  else {
    flac_options.bits = 16;
    packing_func = packing_func_i16;
  }

  metadata[num_metadata] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
  if (metadata[num_metadata]) {
    const char *vendor_string = "Encoded with Timidity++-" VERSION "(compiled " __DATE__ ")";
    /* Location=output_name */
    memset(&commentry, 0, sizeof(commentry));
    FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&commentry, "LOCATION", comment);
    FLAC__metadata_object_vorbiscomment_append_comment(metadata[num_metadata], commentry, false);
    /* Comment=vendor_string */
    memset(&vendentry, 0, sizeof(vendentry));
    FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&vendentry, "COMMENT", vendor_string);
    FLAC__metadata_object_vorbiscomment_append_comment(metadata[num_metadata], vendentry, false);
    num_metadata++;
  }

  if (0 < flac_options.padding) {
    metadata[num_metadata] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
    if (metadata[num_metadata]) {
      metadata[num_metadata]->length = flac_options.padding;
      num_metadata++;
    }
  }

#ifdef LEGACY_FLAC
#ifdef AU_OGGFLAC
  if (flac_options.isogg) {
    if ((ctx->encoder.ogg.stream = OggFLAC__stream_encoder_new()) == NULL) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create OggFLAC stream");
      flac_session_close();
      return -1;
    }

    OggFLAC__stream_encoder_set_channels(ctx->encoder.ogg.stream, nch);
    OggFLAC__stream_encoder_set_bits_per_sample(ctx->encoder.ogg.stream, flac_options.bits);

    /* set sequential number for serial */
    serial_number++;
    if (serial_number == 1) {
      srand(time(NULL));
      serial_number = rand();
    }
    OggFLAC__stream_encoder_set_serial_number(ctx->encoder.ogg.stream, serial_number);

    OggFLAC__stream_encoder_set_verify(ctx->encoder.ogg.stream, flac_options.verify);

    if (!FLAC__format_sample_rate_is_valid(dpm.rate)) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "invalid sampling rate %d",
		dpm.rate);
      flac_session_close();
      return -1;
    }
    OggFLAC__stream_encoder_set_sample_rate(ctx->encoder.ogg.stream, dpm.rate);

    OggFLAC__stream_encoder_set_qlp_coeff_precision(ctx->encoder.ogg.stream, flac_options.qlp_coeff_precision);
    /* expensive! */
    OggFLAC__stream_encoder_set_do_qlp_coeff_prec_search(ctx->encoder.ogg.stream, flac_options.qlp_coeff_precision_search);

    if (nch == 2) {
      OggFLAC__stream_encoder_set_do_mid_side_stereo(ctx->encoder.ogg.stream, flac_options.mid_side);
      OggFLAC__stream_encoder_set_loose_mid_side_stereo(ctx->encoder.ogg.stream, flac_options.adaptive_mid_side);
    }

    OggFLAC__stream_encoder_set_max_lpc_order(ctx->encoder.ogg.stream, flac_options.max_lpc_order);
    OggFLAC__stream_encoder_set_min_residual_partition_order(ctx->encoder.ogg.stream, flac_options.min_residual_partition_order);
    OggFLAC__stream_encoder_set_max_residual_partition_order(ctx->encoder.ogg.stream, flac_options.max_residual_partition_order);

    OggFLAC__stream_encoder_set_blocksize(ctx->encoder.ogg.stream, flac_options.blocksize);

    OggFLAC__stream_encoder_set_client_data(ctx->encoder.ogg.stream, ctx);

    if (0 < num_metadata)
      OggFLAC__stream_encoder_set_metadata(ctx->encoder.ogg.stream, metadata, num_metadata);

    /* set callback */
    OggFLAC__stream_encoder_set_write_callback(ctx->encoder.ogg.stream, ogg_stream_encoder_write_callback);

    ctx->state.ogg = OggFLAC__stream_encoder_init(ctx->encoder.ogg.stream);
    if (ctx->state.ogg != OggFLAC__STREAM_ENCODER_OK) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create OggFLAC state (%s)",
		OggFLAC__StreamEncoderStateString[ctx->state.ogg]);
      flac_session_close();
      return -1;
    }
  }
  else
#endif /* AU_OGGFLAC */
  if (flac_options.seekable) {
    if ((ctx->encoder.flac.s_stream = FLAC__seekable_stream_encoder_new()) == NULL) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC stream");
      flac_session_close();
      return -1;
    }

    FLAC__seekable_stream_encoder_set_channels(ctx->encoder.flac.s_stream, nch);
    FLAC__seekable_stream_encoder_set_bits_per_sample(ctx->encoder.flac.s_stream, flac_options.bits);

    FLAC__seekable_stream_encoder_set_verify(ctx->encoder.flac.s_stream, flac_options.verify);

    if (!FLAC__format_sample_rate_is_valid(dpm.rate)) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "invalid sampling rate %d",
		dpm.rate);
      flac_session_close();
      return -1;
    }
    FLAC__seekable_stream_encoder_set_sample_rate(ctx->encoder.flac.s_stream, dpm.rate);

    FLAC__seekable_stream_encoder_set_qlp_coeff_precision(ctx->encoder.flac.s_stream, flac_options.qlp_coeff_precision);
    /* expensive! */
    FLAC__seekable_stream_encoder_set_do_qlp_coeff_prec_search(ctx->encoder.flac.s_stream, flac_options.qlp_coeff_precision_search);

    if (nch == 2) {
      FLAC__seekable_stream_encoder_set_do_mid_side_stereo(ctx->encoder.flac.s_stream, flac_options.mid_side);
      FLAC__seekable_stream_encoder_set_loose_mid_side_stereo(ctx->encoder.flac.s_stream, flac_options.adaptive_mid_side);
    }

    FLAC__seekable_stream_encoder_set_max_lpc_order(ctx->encoder.flac.s_stream, flac_options.max_lpc_order);
    FLAC__seekable_stream_encoder_set_min_residual_partition_order(ctx->encoder.flac.s_stream, flac_options.min_residual_partition_order);
    FLAC__seekable_stream_encoder_set_max_residual_partition_order(ctx->encoder.flac.s_stream, flac_options.max_residual_partition_order);

    FLAC__seekable_stream_encoder_set_blocksize(ctx->encoder.flac.s_stream, flac_options.blocksize);
    FLAC__seekable_stream_encoder_set_client_data(ctx->encoder.flac.s_stream, ctx);

    if (0 < num_metadata)
      FLAC__seekable_stream_encoder_set_metadata(ctx->encoder.flac.s_stream, metadata, num_metadata);

    /* set callback */
/*  FLAC__seekable_stream_encoder_set_metadata_callback(ctx->encoder.flac.s_stream, flac_seekable_stream_encoder_metadata_callback); */
#if (!defined(__BORLANDC__) && !defined(__POCC__))
    FLAC__stream_encoder_set_metadata_callback(ctx->encoder.flac.s_stream, flac_seekable_stream_encoder_metadata_callback); /* */
#endif
    FLAC__seekable_stream_encoder_set_write_callback(ctx->encoder.flac.s_stream, flac_seekable_stream_encoder_write_callback);

    ctx->state.s_flac = FLAC__seekable_stream_encoder_init(ctx->encoder.flac.s_stream);
    if (ctx->state.s_flac != FLAC__SEEKABLE_STREAM_ENCODER_OK) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC state (%s)",
		FLAC__SeekableStreamEncoderStateString[ctx->state.s_flac]);
      flac_session_close();
      return -1;
    }
	}
	else
  {
    if ((ctx->encoder.flac.stream = FLAC__stream_encoder_new()) == NULL) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC stream");
      flac_session_close();
      return -1;
    }

    FLAC__stream_encoder_set_channels(ctx->encoder.flac.stream, nch);
    FLAC__stream_encoder_set_bits_per_sample(ctx->encoder.flac.stream, flac_options.bits);

    FLAC__stream_encoder_set_verify(ctx->encoder.flac.stream, flac_options.verify);

    if (!FLAC__format_sample_rate_is_valid(dpm.rate)) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "invalid sampling rate %d",
		dpm.rate);
      flac_session_close();
      return -1;
    }
    FLAC__stream_encoder_set_sample_rate(ctx->encoder.flac.stream, dpm.rate);

    FLAC__stream_encoder_set_qlp_coeff_precision(ctx->encoder.flac.stream, flac_options.qlp_coeff_precision);
    /* expensive! */
    FLAC__stream_encoder_set_do_qlp_coeff_prec_search(ctx->encoder.flac.stream, flac_options.qlp_coeff_precision_search);

    if (nch == 2) {
      FLAC__stream_encoder_set_do_mid_side_stereo(ctx->encoder.flac.stream, flac_options.mid_side);
      FLAC__stream_encoder_set_loose_mid_side_stereo(ctx->encoder.flac.stream, flac_options.adaptive_mid_side);
    }

    FLAC__stream_encoder_set_max_lpc_order(ctx->encoder.flac.stream, flac_options.max_lpc_order);
    FLAC__stream_encoder_set_min_residual_partition_order(ctx->encoder.flac.stream, flac_options.min_residual_partition_order);
    FLAC__stream_encoder_set_max_residual_partition_order(ctx->encoder.flac.stream, flac_options.max_residual_partition_order);

    FLAC__stream_encoder_set_blocksize(ctx->encoder.flac.stream, flac_options.blocksize);
    FLAC__stream_encoder_set_client_data(ctx->encoder.flac.stream, ctx);

    if (0 < num_metadata)
      FLAC__stream_encoder_set_metadata(ctx->encoder.flac.stream, metadata, num_metadata);

    /* set callback */
    FLAC__stream_encoder_set_metadata_callback(ctx->encoder.flac.stream, flac_stream_encoder_metadata_callback);
    FLAC__stream_encoder_set_write_callback(ctx->encoder.flac.stream, flac_stream_encoder_write_callback);

    ctx->state.flac = FLAC__stream_encoder_init(ctx->encoder.flac.stream);
    if (ctx->state.flac != FLAC__STREAM_ENCODER_OK) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC state (%s)",
		FLAC__StreamEncoderStateString[ctx->state.flac]);
      flac_session_close();
      return -1;
    }
  }
#else /* !LEGACY_FLAC */
	if ((ctx->encoder.flac.stream = FLAC__stream_encoder_new()) == NULL) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC stream");
		flac_session_close();
		return -1;
	}
	if (!FLAC__stream_encoder_set_streamable_subset(ctx->encoder.flac.stream, true)) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC stream");
		flac_session_close();
		return -1;
	}

	FLAC__stream_encoder_set_channels(ctx->encoder.flac.stream, nch);
	FLAC__stream_encoder_set_bits_per_sample(ctx->encoder.flac.stream, flac_options.bits);

	FLAC__stream_encoder_set_verify(ctx->encoder.flac.stream, flac_options.verify);

	if (!FLAC__format_sample_rate_is_valid(dpm.rate)) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "invalid sampling rate %d", dpm.rate);
		flac_session_close();
		return -1;
	}
	FLAC__stream_encoder_set_sample_rate(ctx->encoder.flac.stream, dpm.rate);

	FLAC__stream_encoder_set_compression_level(ctx->encoder.flac.stream, flac_options.compression_level);

	FLAC__stream_encoder_set_qlp_coeff_precision(ctx->encoder.flac.stream, flac_options.qlp_coeff_precision);
	/* expensive! */
	FLAC__stream_encoder_set_do_qlp_coeff_prec_search(ctx->encoder.flac.stream, flac_options.qlp_coeff_precision_search);

	if (nch == 2) {
		FLAC__stream_encoder_set_do_mid_side_stereo(ctx->encoder.flac.stream, flac_options.mid_side);
		FLAC__stream_encoder_set_loose_mid_side_stereo(ctx->encoder.flac.stream, flac_options.adaptive_mid_side);
	}

	FLAC__stream_encoder_set_max_lpc_order(ctx->encoder.flac.stream, flac_options.max_lpc_order);
	FLAC__stream_encoder_set_min_residual_partition_order(ctx->encoder.flac.stream, flac_options.min_residual_partition_order);
	FLAC__stream_encoder_set_max_residual_partition_order(ctx->encoder.flac.stream, flac_options.max_residual_partition_order);

	FLAC__stream_encoder_set_blocksize(ctx->encoder.flac.stream, flac_options.blocksize);

	/* ToDo */
	FLAC__stream_encoder_set_total_samples_estimate(ctx->encoder.flac.stream, /*(time_sec * dpm.rate)*/0);

	if (0 < num_metadata)
		FLAC__stream_encoder_set_metadata(ctx->encoder.flac.stream, metadata, num_metadata);

#ifdef AU_OGGFLAC
	if (flac_options.isogg)
		init_status = FLAC__stream_encoder_init_ogg_stream(ctx->encoder.flac.stream, flac_stream_encoder_read_callback, flac_stream_encoder_write_callback, NULL, NULL, NULL, ctx);
	else
		init_status = FLAC__stream_encoder_init_stream(ctx->encoder.flac.stream, flac_stream_encoder_write_callback, NULL, NULL, NULL, ctx);
#else
	init_status = FLAC__stream_encoder_init_stream(ctx->encoder.flac.stream, flac_stream_encoder_write_callback, NULL, NULL, NULL, ctx);
#endif
	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
//		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC encoder (init status: %s)", FLAC__StreamEncoderInitStatusString[init_status]);
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot create FLAC encoder (init status: %d)", init_status);
		flac_session_close();
		return -1;
	}
#endif

  if (0 < num_metadata) {
    int i;
    for (i = num_metadata - 1; i > 0; -- i) {
      FLAC__metadata_object_delete(metadata[i]);
      metadata[i] = 0;
    }
  }

  return 0;
}

static int auto_flac_output_open(const char *input_filename, const char *title)
{
  char *output_filename;

#ifdef AU_OGGFLAC
  if (flac_options.isogg) {
    output_filename = create_auto_output_name(input_filename, "oga", NULL, 0);
  }
  else
#endif /* AU_OGGFLAC */
  {
    output_filename = create_auto_output_name(input_filename, "flac", NULL, 0);
  }
  if (!output_filename) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unknown output file name");
	  return -1;
  }
  if ((flac_output_open(output_filename, input_filename)) == -1) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "files open failed %s->%s", output_filename, input_filename);
    safe_free(output_filename);
    return -1;
  }
  if (dpm.name)
    safe_free(dpm.name);
  dpm.name = output_filename;
  ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Output %s", dpm.name);
  return 0;
}

static int open_output(void)
{
  int include_enc, exclude_enc;

#ifdef AU_FLAC_DLL
	if (g_load_libFLAC_dll("libFLAC.dll")) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "DLL load failed: %s", "libFLAC.dll, ogg.dll");
		return -1;
	}
#endif
#ifdef AU_OGGFLAC_DLL
	if (g_load_libOggFLAC_dll("libOggFLAC.dll")) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "DLL load failed: %s", "libOggFLAC.dll, ogg.dll");
#ifdef AU_FLAC_DLL
		g_free_libFLAC_dll();
#endif
		return -1;
	}
#endif
///r
  include_enc = exclude_enc = 0;
  /* 16bit,24bit,32bit,F32bit,F64bit supported */
  include_enc |= PE_SIGNED | PE_16BIT;
  exclude_enc |= PE_BYTESWAP | PE_ULAW | PE_ALAW | PE_64BIT;
  if (dpm.encoding & (PE_64BIT)) {
    include_enc |= PE_32BIT;
  }
#if 0
  if (dpm.encoding & (PE_24BIT | PE_32BIT | PE_F32BIT | PE_F64BIT)) {
    include_enc |= PE_SIGNED;
    exclude_enc |= PE_BYTESWAP | PE_ULAW | PE_ALAW | PE_16BIT | PE_64BIT;
  } else {
    include_enc |= PE_SIGNED | PE_16BIT;
    exclude_enc |= PE_BYTESWAP | PE_ULAW | PE_ALAW | PE_64BIT;
  }
#endif
  dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

#if defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
  if (flac_options.isogg) {
    ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "*** cannot write back seekpoints when encoding to Ogg yet ***");
    ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "*** and stream end will not be written.                   ***");
  }
#endif


  if (!dpm.name) {
    dpm.flag |= PF_AUTO_SPLIT_FILE;
  } else {
    dpm.flag &= ~PF_AUTO_SPLIT_FILE;
    if (flac_output_open(dpm.name, NULL) == -1)
      return -1;
  }

  return 0;
}

#if defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
static FLAC__StreamEncoderWriteStatus
ogg_stream_encoder_write_callback(const OggFLAC__StreamEncoder *encoder,
				  const FLAC__byte buffer[],
				  unsigned bytes, unsigned samples,
				  unsigned current_frame, void *client_data)
{
  FLAC_ctx *ctx = (FLAC_ctx*)client_data;

  ctx->out_bytes += bytes;

  if (std_write(dpm.fd, buffer, bytes) != -1)
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
  else
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}
#endif
static FLAC__StreamEncoderWriteStatus
flac_stream_encoder_write_callback(const FLAC__StreamEncoder *encoder,
				   const FLAC__byte buffer[],
#ifdef LEGACY_FLAC
				   unsigned bytes, unsigned samples,
#else
				   size_t bytes, unsigned samples,
#endif
				   unsigned current_frame, void *client_data)
{
  FLAC_ctx *ctx = (FLAC_ctx*)client_data;

  ctx->out_bytes += bytes;

  if (std_write(dpm.fd, buffer, bytes) == bytes)
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
  else
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}
#if !defined(LEGACY_FLAC) && defined(AU_OGGFLAC)
static FLAC__StreamEncoderReadStatus
flac_stream_encoder_read_callback(const FLAC__StreamEncoder *encoder,
				  FLAC__byte buffer[],
				  size_t *bytes,
				  void *client_data)
{
  if (*bytes > 0 && dpm.fd != -1 && dpm.fd != STDOUT_FILENO) {
    *bytes = read(dpm.fd, buffer, *bytes);
    if (*bytes == -1)
      return FLAC__STREAM_ENCODER_READ_STATUS_ABORT;
    else if (*bytes == 0)
      return FLAC__STREAM_ENCODER_READ_STATUS_END_OF_STREAM;
    else
      return FLAC__STREAM_ENCODER_READ_STATUS_CONTINUE;
  }
  else
    return FLAC__STREAM_ENCODER_READ_STATUS_ABORT;
}
#endif
#ifdef LEGACY_FLAC
static void flac_stream_encoder_metadata_callback(const FLAC__StreamEncoder *encoder,
						  const FLAC__StreamMetadata *metadata,
						  void *client_data)
{
}
static FLAC__StreamEncoderWriteStatus
flac_seekable_stream_encoder_write_callback(const FLAC__SeekableStreamEncoder *encoder,
				   const FLAC__byte buffer[],
				   unsigned bytes, unsigned samples,
				   unsigned current_frame, void *client_data)
{
  FLAC_ctx *ctx = (FLAC_ctx*)client_data;

  ctx->out_bytes += bytes;

  if (std_write(dpm.fd, buffer, bytes) == bytes)
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
  else
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}
static void flac_seekable_stream_encoder_metadata_callback(const FLAC__SeekableStreamEncoder *encoder,
						  const FLAC__StreamMetadata *metadata,
						  void *client_data)
{
}
#endif

static int32 output_data(const uint8 *buf, size_t nbytes)
{
  const int nch = (dpm.encoding & PE_MONO) ? 1 : 2;
  const int step = divi_8(flac_options.bits);
  const int nsamples = nbytes / nch / step;
  const int nlength = nbytes / step;
  FLAC__int32 *newoggbuf;
  FLAC_ctx *ctx = flac_ctx;

  if (dpm.fd < 0)
    return 0;

  if (!ctx) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream is not initialized");
    return -1;
  }

  if (!ctx->oggbuf || ctx->oggbuf_length < nlength) {
    newoggbuf = (FLAC__int32*) realloc(ctx->oggbuf, nlength * sizeof(FLAC__int32));
    if (!newoggbuf) {
      safe_free(ctx->oggbuf);
      ctx->oggbuf = NULL;
      ctx->oggbuf_length = 0;
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Sorry. Couldn't allocate memory space.");
      return -1;
    }
    ctx->oggbuf = newoggbuf;
    ctx->oggbuf_length = nlength;
  }

  (*packing_func)(ctx->oggbuf, buf, nlength);

#ifdef LEGACY_FLAC
#ifdef AU_OGGFLAC
  if (flac_options.isogg) {
    ctx->state.ogg = OggFLAC__stream_encoder_get_state(ctx->encoder.ogg.stream);
    if (ctx->state.ogg != OggFLAC__STREAM_ENCODER_OK) {
      if (ctx->state.ogg == OggFLAC__STREAM_ENCODER_FLAC_STREAM_ENCODER_ERROR) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream verify error (%s)",
		  FLAC__StreamDecoderStateString[OggFLAC__stream_encoder_get_verify_decoder_state(ctx->encoder.ogg.stream)]);
      }
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode OggFLAC stream (%s)",
		OggFLAC__StreamEncoderStateString[ctx->state.ogg]);
      flac_session_close();
      return -1;
    }

    if (!OggFLAC__stream_encoder_process_interleaved(ctx->encoder.ogg.stream, ctx->oggbuf,
						     nsamples)) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode OggFLAC stream");
      flac_session_close();
      return -1;
    }
  }
  else
#endif /* AU_OGGFLAC */
	if (flac_options.seekable) {
    ctx->state.s_flac = FLAC__seekable_stream_encoder_get_state(ctx->encoder.flac.s_stream);
    if (ctx->state.s_flac != FLAC__STREAM_ENCODER_OK) {
      if (ctx->state.s_flac == FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR ||
	  ctx->state.s_flac == FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream verify error (%s)",
		  FLAC__SeekableStreamDecoderStateString[FLAC__seekable_stream_encoder_get_verify_decoder_state(ctx->encoder.flac.s_stream)]);
      }
      else {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode FLAC stream (%s)",
		  FLAC__SeekableStreamEncoderStateString[ctx->state.s_flac]);
      }
      flac_session_close();
      return -1;
    }

    if (!FLAC__seekable_stream_encoder_process_interleaved(ctx->encoder.flac.s_stream, ctx->oggbuf,
						  nsamples)) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode FLAC stream");
      flac_session_close();
      return -1;
    }
	}
  else
	{
    ctx->state.flac = FLAC__stream_encoder_get_state(ctx->encoder.flac.stream);
    if (ctx->state.flac != FLAC__STREAM_ENCODER_OK) {
      if (ctx->state.flac == FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR ||
	  ctx->state.flac == FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream verify error (%s)",
		  FLAC__StreamDecoderStateString[FLAC__stream_encoder_get_verify_decoder_state(ctx->encoder.flac.stream)]);
      }
      else {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode FLAC stream (%s)",
		  FLAC__StreamEncoderStateString[ctx->state.flac]);
      }
      flac_session_close();
      return -1;
    }

    if (!FLAC__stream_encoder_process_interleaved(ctx->encoder.flac.stream, ctx->oggbuf,
						  nsamples)) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode FLAC stream");
      flac_session_close();
      return -1;
    }
  }
#else /* !LEGACY_FLAC */
  ctx->state.flac = FLAC__stream_encoder_get_state(ctx->encoder.flac.stream);
  if (ctx->state.flac != FLAC__STREAM_ENCODER_OK) {
    if (ctx->state.flac == FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR) {
//     ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream verify error (%s)",
//		FLAC__StreamDecoderStateString[FLAC__stream_encoder_get_verify_decoder_state(ctx->encoder.flac.stream)]);
    }
    else {
//      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode FLAC stream (%s)",
//		FLAC__StreamEncoderStateString[ctx->state.flac]);
    }
    flac_session_close();
    return -1;
  }

  if (!FLAC__stream_encoder_process_interleaved(ctx->encoder.flac.stream, ctx->oggbuf,
						nsamples)) {
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "cannot encode FLAC stream");
    flac_session_close();
    return -1;
  }
#endif
  ctx->in_bytes += nbytes;

  return 0;
}

static void close_output(void)
{
  FLAC_ctx *ctx;

  ctx = flac_ctx;

  if (!ctx)
    return;

  if (dpm.fd < 0) {
    flac_session_close();
    return;
  }

#ifdef LEGACY_FLAC
#ifdef AU_OGGFLAC
  if (flac_options.isogg) {
    if ((ctx->state.ogg = OggFLAC__stream_encoder_get_state(ctx->encoder.ogg.stream)) != OggFLAC__STREAM_ENCODER_OK) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "OggFLAC stream encoder is invalid (%s)",
		OggFLAC__StreamEncoderStateString[ctx->state.ogg]);
      /* fall through */
    }
  }
  else
#endif /* AU_OGGFLAC */
  if (flac_options.seekable) {
    if ((ctx->state.s_flac = FLAC__seekable_stream_encoder_get_state(ctx->encoder.flac.s_stream)) != FLAC__SEEKABLE_STREAM_ENCODER_OK) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream encoder is invalid (%s)",
		FLAC__SeekableStreamEncoderStateString[ctx->state.s_flac]);
      /* fall through */
    }
  }
  else
  {
    if ((ctx->state.flac = FLAC__stream_encoder_get_state(ctx->encoder.flac.stream)) != FLAC__STREAM_ENCODER_OK) {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream encoder is invalid (%s)",
		FLAC__StreamEncoderStateString[ctx->state.flac]);
      /* fall through */
    }
  }
#else /* !LEGACY_FLAC */
  if ((ctx->state.flac = FLAC__stream_encoder_get_state(ctx->encoder.flac.stream)) != FLAC__STREAM_ENCODER_OK) {
//    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC stream encoder is invalid (%s)",
//	      FLAC__StreamEncoderStateString[ctx->state.flac]);
    /* fall through */
  }
#endif

  ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Wrote %lu/%lu bytes(%g%% compressed)",
            ctx->out_bytes, ctx->in_bytes, ((double)ctx->out_bytes / (double)ctx->in_bytes) * 100.);

  flac_session_close();

#ifdef AU_FLAC_DLL
	g_free_libFLAC_dll();
#endif
#ifdef AU_OGGFLAC_DLL
	g_free_libOggFLAC_dll();
#endif

}

static int acntl(int request, void *arg)
{
  switch (request) {
  case PM_REQ_PLAY_START:
    if (dpm.flag & PF_AUTO_SPLIT_FILE) {
      const char *filename = (current_file_info && current_file_info->filename) ?
			     current_file_info->filename : "Output.mid";
      const char *seq_name = (current_file_info && current_file_info->seq_name) ?
			     current_file_info->seq_name : NULL;
      return auto_flac_output_open(filename, seq_name);
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

#endif
