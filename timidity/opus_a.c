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

    opus_a.c - Functions to output Opus (*.opus).
				Written by yta
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>

#ifdef AU_OPUS

#ifdef AU_OPUS_DLL
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <io.h>
#include <ctype.h>
extern int load_ogg_dll(void);
extern void free_ogg_dll(void);
extern int load_opus_dll(void);
extern void free_opus_dll(void);
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef __W32__
#include <windows.h>
#endif

#include <opus/opus.h>
#include <ogg/ogg.h>

#ifdef AU_OPUS_DLL
extern int opus_encoder_ctl_0(OpusEncoder *st, int request);
extern int opus_encoder_ctl_1(OpusEncoder *st, int request, int param1);
extern int opus_encoder_ctl_2(OpusEncoder *st, int request, int param1, int param2);
extern int opus_encoder_ctl_3(OpusEncoder *st, int request, int param1, int param2, int param3);
#else
#define opus_encoder_ctl_0 opus_encoder_ctl
#define opus_encoder_ctl_1 opus_encoder_ctl
#define opus_encoder_ctl_2 opus_encoder_ctl
#define opus_encoder_ctl_3 opus_encoder_ctl
#endif

/* #define DEBUG_WRITE_RAWDATA */


#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"

#if defined(IA_W32GUI) || defined(IA_W32G_SYN)
#include "w32g.h"
#endif /* IA_W32GUI || IA_W32G_SYN */


static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(const uint8 *buf, size_t bytes);
static int acntl(int request, void *arg);

/* export the playback mode */
#define dpm opus_play_mode

PlayMode dpm = {
	48000, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_FILE_OUTPUT,
	-1,
	{0,0,0,0,0},
	"Ogg Opus", 'U',
	NULL,
	open_output,
	close_output,
	output_data,
	acntl,
	NULL
};


typedef struct {
	uint8 version;
	uint8 channels;
	int16 preskip;
	int32 input_sample_rate;
	int16 gain;
	uint8 channel_mapping;
	int8 nb_streams;
	int8 nb_coupled;
	unsigned char stream_map[255];
} OpusHeader;

typedef struct {
	OpusHeader *opus;
	ogg_stream_state *ogg;
	OpusEncoder *enc;
	uint8 *pcm_buffer;
	size_t pcm_offset;
	size_t pcm_bandwidth;
	uint8 *enc_store;
	size_t enc_size;
	ogg_int64_t packetno;
	ogg_int64_t granulepos;
	ogg_int64_t mul_smp;
	enum {
		FT_INT16,
		FT_FLOAT
	} format_type;
} Opus_ctx;

typedef struct {
	int nframes;
	int bitrate;
	int complexity;
	int vbr;
	int cvbr;
	int reserved;
} Opus_options;

Opus_ctx *opus_ctx = NULL;

Opus_options opus_options = {
	0,                      /* nframes (min 10ms) (default is 2 * ch * rate * 0.01 = 20ms) */
	128 * 1000,             /* bitrate */
	10,                     /* complexity */
	1,                      /* vbr */
	1,                      /* cvbr */
};


static const int Es = 3000;

#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
//extern int opus_ConfigDialogInfoApply(void);
#endif

#ifdef DEBUG_WRITE_RAWDATA
static int rawfd = -1;
#endif

static int opus_output_open(const char *fname, const char *comment);
static int auto_opus_output_open(const char *input_filename, const char *title);
static void opus_header_write(const OpusHeader *h);
static void encode_output_data(size_t nbytes);
static size_t encode_func_int16(Opus_ctx *ctx, const uint8 *buffer, size_t samples);
static size_t encode_func_float(Opus_ctx *ctx, const uint8 *buffer, size_t samples);
static void pagein_output_data(Opus_ctx *ctx, size_t packet_len);
static void flush_output_data(Opus_ctx *ctx);
static void opus_session_close(void);

static size_t (*encode_func)(Opus_ctx *ctx, const uint8 *buffer, size_t samples) = encode_func_int16;

void opus_set_nframes(int nframes);
void opus_set_bitrate(int bitrate);
void opus_set_complexity(int complexity);
void opus_set_vbr(int vbr);
void opus_set_cvbr(int cvbr);

/*************************************************************************/

/* open */

static int open_output(void)
{
	int32 include_enc, exclude_enc;

#ifdef AU_OPUS_DLL
	{
		int flag = 0;
		if(!load_opus_dll())
			if(!load_ogg_dll())
				flag = 1;
		if(!flag){
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "DLL load failed: %s", "opus.dll, ogg.dll");
			free_opus_dll();
			free_ogg_dll();
			return -1;
		}
	}
#endif

	if(!(dpm.rate == 8000 || dpm.rate == 12000 || dpm.rate == 16000 ||
		dpm.rate == 24000 || dpm.rate == 48000))
	{
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s (%d Hz)",
			dpm.name, "invalid sample rate", dpm.rate);
#ifdef AU_OPUS_DLL
		free_opus_dll();
		free_ogg_dll();
#endif
		return -1;
	}

	include_enc = 0;
	exclude_enc = ~(PE_MONO | PE_SIGNED);

	if(dpm.encoding & (PE_24BIT | PE_32BIT | PE_F32BIT |
		PE_64BIT | PE_F64BIT))
	{
		/*  32-bit floating point (-1..1) */
		include_enc |= PE_F32BIT;
		exclude_enc &= ~PE_F32BIT;
	}
	else {
		/* Signed 16-bit fixed point (-32768..32767) */
		include_enc |= PE_16BIT | PE_SIGNED;
		exclude_enc &= ~PE_16BIT;
	}

	exclude_enc |= PE_BYTESWAP | PE_ULAW | PE_ALAW;
	dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

#if !defined ( IA_W32GUI ) && !defined ( IA_W32G_SYN )
	if(dpm.name == NULL) {
		dpm.flag |= PF_AUTO_SPLIT_FILE;
	}
	else {
		dpm.flag &= ~PF_AUTO_SPLIT_FILE;
		if((dpm.fd = opus_output_open(dpm.name, NULL)) == -1) {
#ifdef AU_OPUS_DLL
			free_opus_dll();
			free_ogg_dll();
#endif
			return -1;
		}
	}
#else
	if(w32g_auto_output_mode > 0) {
			dpm.flag |= PF_AUTO_SPLIT_FILE;
			dpm.name = NULL;
		}
		else {
			dpm.flag &= ~PF_AUTO_SPLIT_FILE;
			if((dpm.fd = opus_output_open(dpm.name,NULL)) == -1) {
#ifdef AU_OPUS_DLL
				free_opus_dll();
				free_ogg_dll();
#endif
				return -1;
			}
		}
#endif

	return 0;
}

static int opus_output_open(const char *fname, const char *comment)
{
	int fd;
	int nch = (dpm.encoding & PE_MONO) ? 1 : 2;
	int err;
	OpusHeader h = { 0 };
	Opus_ctx *ctx = NULL;

	opus_session_close();

	if(strcmp(fname, "-") == 0) {
		fd = STDOUT_FILENO; /* data to stdout */
	}
	else {
		/* Open the audio file */
#ifdef __W32__
		TCHAR *t = char_to_tchar(fname);
		fd = _topen(t, FILE_OUTPUT_MODE);
		safe_free(t);
#else
		fd = open(fname, FILE_OUTPUT_MODE);
#endif
		if(fd < 0) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
				fname, strerror(errno));
			return -1;
		}
	}
	dpm.fd = fd;

#ifdef DEBUG_WRITE_RAWDATA
	rawfd = open("rawoutput.dat", FILE_OUTPUT_MODE);
#endif

#if defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
	//opus_ConfigDialogInfoApply();
#endif

	if(!opus_options.nframes) {
		opus_set_nframes(2 * nch * dpm.rate * 0.01);
	}

	if(opus_options.nframes < nch * dpm.rate * 0.01) {
		ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small --opus-nframes option: %d", opus_options.nframes);
		opus_set_nframes(nch * dpm.rate * 0.01);
	}

	if(!(opus_ctx = (Opus_ctx *)calloc(sizeof(Opus_ctx), 1))) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s", strerror(errno));
		return -1;
	}

	ctx = opus_ctx;
	ctx->opus = (OpusHeader *)safe_malloc(sizeof(OpusHeader));
	memset(ctx->opus, 0, sizeof(OpusHeader));

	ctx->ogg = (ogg_stream_state *)safe_malloc(sizeof(ogg_stream_state));
	memset(ctx->ogg, 0, sizeof(ogg_stream_state));

	srand(time(NULL));
	ogg_stream_init(ctx->ogg, rand());

	err = 0;
	ctx->enc = opus_encoder_create(dpm.rate, nch, OPUS_APPLICATION_AUDIO, &err);
	if(err != OPUS_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
			fname, "fail opus_encoder_create");
		return -1;
	}

	opus_encoder_ctl_1(ctx->enc, OPUS_SET_SIGNAL((opus_options.nframes < 2 * nch * dpm.rate * 0.01) ?
		OPUS_AUTO : OPUS_SIGNAL_MUSIC));
	opus_encoder_ctl_1(ctx->enc, OPUS_SET_BITRATE(opus_options.bitrate));
	opus_encoder_ctl_1(ctx->enc, OPUS_SET_COMPLEXITY(opus_options.complexity));
	opus_encoder_ctl_1(ctx->enc, OPUS_SET_VBR(opus_options.vbr));
	opus_encoder_ctl_1(ctx->enc, OPUS_SET_VBR_CONSTRAINT(opus_options.cvbr));

	ctx->pcm_buffer = 0;
	ctx->pcm_offset = 0;

	ctx->enc_size = Es;
	ctx->enc_store = (uint8 *)safe_malloc(ctx->enc_size);

	ctx->packetno = 0;
	ctx->granulepos = 0;
	ctx->mul_smp = opus_options.nframes * (48000 / (float)dpm.rate);

	if(dpm.encoding & PE_F32BIT) {
		ctx->format_type = FT_FLOAT;
		ctx->pcm_bandwidth = sizeof(float);
		encode_func = encode_func_float;
	}
	else {
		ctx->format_type = FT_INT16;
		ctx->pcm_bandwidth = sizeof(opus_int16);
		encode_func = encode_func_int16;
	}

	memset(ctx->opus, 0, sizeof(OpusHeader));
	ctx->opus->version = 1;
	ctx->opus->channels = nch;
	ctx->opus->preskip = 0;
	ctx->opus->input_sample_rate = dpm.rate;

	opus_header_write(ctx->opus);

	return fd;
}

static int auto_opus_output_open(const char *input_filename, const char *title)
{
	char *output_filename;

	output_filename = create_auto_output_name(input_filename, "opus", NULL, 0);
	if(output_filename == NULL) {
		return -1;
	}
	if((dpm.fd = opus_output_open(output_filename, input_filename)) == -1) {
		safe_free(output_filename);
		return -1;
	}
	safe_free(dpm.name);
	dpm.name = output_filename;
	ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Output %s", dpm.name);
	return 0;
}

/* close */

static void close_output(void)
{
	if(dpm.fd < 0) {
		return;
	}

	opus_session_close();

#ifdef AU_OPUS_DLL
	free_opus_dll();
	free_ogg_dll();
#endif
}

static void opus_session_close(void)
{
	Opus_ctx *ctx = opus_ctx;

	if(ctx && ctx->ogg && ctx->enc) {
		flush_output_data(ctx);
	}

	if(ctx) {
		safe_free(ctx->opus);
		ctx->opus = NULL;

		ogg_stream_clear(ctx->ogg);
		safe_free(ctx->ogg);
		ctx->ogg = NULL;

		safe_free(ctx->pcm_buffer);
		ctx->pcm_buffer = NULL;
		ctx->pcm_offset = 0;

		safe_free(ctx->enc_store);
		ctx->enc_store = NULL;
		ctx->enc_size = 0;

		if(ctx->enc) {
			opus_encoder_destroy(ctx->enc);
			ctx->enc = NULL;
		}

		safe_free(ctx);
		opus_ctx = ctx = NULL;
	}

#ifdef DEBUG_WRITE_RAWDATA
	if(rawfd != -1) {
		close(rawfd);
		rawfd = -1;
	}
#endif

	if(dpm.fd != STDOUT_FILENO && /* We don't close stdout */
		dpm.fd != -1)
	{
		close(dpm.fd);
	}

	dpm.fd = -1;
}

/* output */

static int output_data(const uint8 *readbuffer, size_t nbytes)
{
	int i;
	Opus_ctx *ctx = opus_ctx;

	if(dpm.fd < 0)
		return 0;

	if(!ctx->pcm_buffer) {
		const size_t chshift = (dpm.encoding & PE_MONO) ? 0 : 1;
		ctx->pcm_buffer = safe_malloc(nbytes +
			((opus_options.nframes << chshift) * ctx->pcm_bandwidth));
	}

	/* data to encode */

	memcpy(ctx->pcm_buffer + ctx->pcm_offset, readbuffer, nbytes);

	encode_output_data(nbytes + ctx->pcm_offset);

	return 0;
}

static void opus_header_write(const OpusHeader *h)
{
	ogg_packet header = { 0 };
	ogg_page og = { 0 };
	Opus_ctx *ctx = opus_ctx;
	char OpusHeader[19] = { 'O', 'p', 'u', 's', 'H', 'e', 'a', 'd' };
	char OpusTags[764] = { 'O', 'p', 'u', 's', 'T', 'a', 'g', 's' };

	*(OpusHeader + 8) = h->version;
	*((uint8 *)(OpusHeader + 9)) = h->channels;
	*((uint16 *)(OpusHeader + 10)) = LE_SHORT(h->preskip);
	*((uint32 *)(OpusHeader + 12)) = LE_LONG(h->input_sample_rate);
	*((uint16 *)(OpusHeader + 16)) = LE_SHORT(h->gain);
	*((uint8 *)(OpusHeader + 18)) = h->channel_mapping;

	header.packet = OpusHeader;
	header.bytes = sizeof(OpusHeader);
	header.b_o_s = 1;
	header.packetno = ctx->packetno ++;
	ogg_stream_packetin(ctx->ogg, &header);

	while(ogg_stream_pageout(ctx->ogg, &og) != 0) {
		std_write(dpm.fd, og.header, og.header_len);
		std_write(dpm.fd, og.body, og.body_len);
	}

	header.packet = OpusTags;
	header.bytes = sizeof(OpusTags);
	header.b_o_s = 0;
	header.packetno = ctx->packetno ++;
	ogg_stream_packetin(ctx->ogg, &header);

	while(ogg_stream_flush(ctx->ogg, &og) != 0) {
		std_write(dpm.fd, og.header, og.header_len);
		std_write(dpm.fd, og.body, og.body_len);
	}
}

static void encode_output_data(size_t nbytes)
{
	opus_int32 packet_len;
	int32 len;
	Opus_ctx *ctx = opus_ctx;
	const size_t chshift = (dpm.encoding & PE_MONO) ? 0 : 1;
	const size_t frame_size = opus_options.nframes;
	const size_t frame_byte = (frame_size << chshift) * ctx->pcm_bandwidth;
	const ogg_int64_t mul_smp = ctx->mul_smp;
	const uint8 *s;

	len = nbytes;
	s = (const uint8 *)(ctx->pcm_buffer);

	while(len >= frame_byte) {
		packet_len = (*encode_func)(ctx, s, frame_size);
		if(packet_len <= 0) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s=%d",
				"opus_encode", "fail code", packet_len);
			break;
		}

		pagein_output_data(ctx, packet_len);

#ifdef DEBUG_WRITE_RAWDATA
		std_write(rawfd, s, frame_byte);
#endif

		ctx->granulepos += mul_smp;

		s += frame_byte;
		len -= frame_byte;
	}

	if(len > 0 && len != nbytes) {
		memmove(ctx->pcm_buffer, s, len);
	}
	ctx->pcm_offset = len;
}

static size_t encode_func_int16(Opus_ctx *ctx, const uint8 *buffer, size_t samples)
{
	return opus_encode(ctx->enc, (const opus_int16 *)buffer, samples, ctx->enc_store, ctx->enc_size);
}

static size_t encode_func_float(Opus_ctx *ctx, const uint8 *buffer, size_t samples)
{
	return opus_encode_float(ctx->enc, (const float *)buffer, samples, ctx->enc_store, ctx->enc_size);
}

static void pagein_output_data(Opus_ctx *ctx, size_t packet_len)
{
	ogg_packet op = { 0 };
	ogg_page og;

	op.packet = ctx->enc_store;
	op.bytes = packet_len;
	op.packetno = ctx->packetno ++;
	op.granulepos = ctx->granulepos;
	ogg_stream_packetin(ctx->ogg, &op);

	while(ogg_stream_pageout(ctx->ogg, &og) != 0) {
		std_write(dpm.fd, og.header, og.header_len);
		std_write(dpm.fd, og.body, og.body_len);
	}
}

static void flush_output_data(Opus_ctx *ctx)
{
	ogg_page og = { 0 };
	size_t i;
	const size_t chshift = (dpm.encoding & PE_MONO) ? 0 : 1;
	const size_t bandwidth = ctx->pcm_bandwidth;
	const size_t frame_size = opus_options.nframes;

	while(ctx->pcm_offset > 0) {
		if(ctx->format_type == FT_FLOAT) {
			float *d;
			d = (float *)(ctx->pcm_buffer);
			for(i = (ctx->pcm_offset / bandwidth) >> chshift; i < frame_size; i ++) {
				d[i] = 0.0f;
			}
		}
		else {
			opus_int16 *d;
			d = (opus_int16 *)(ctx->pcm_buffer);
			for(i = (ctx->pcm_offset / bandwidth) >> chshift; i < frame_size; i ++) {
				d[i] = 0;
			}
		}
		encode_output_data((frame_size * bandwidth) << chshift);
	}

	while(ogg_stream_flush(ctx->ogg, &og) != 0) {
		std_write(dpm.fd, og.header, og.header_len);
		std_write(dpm.fd, og.body, og.body_len);
	}
}

/* acntl */

static int acntl(int request, void *arg)
{
	switch(request) {
	case PM_REQ_PLAY_START:
		if (dpm.flag & PF_AUTO_SPLIT_FILE) {
			const char *filename = (current_file_info && current_file_info->filename) ?
						current_file_info->filename : "Output.mid";
			const char *seq_name = (current_file_info && current_file_info->seq_name) ?
						current_file_info->seq_name : NULL;
			return auto_opus_output_open(filename, seq_name);
		}
		return 0;
	case PM_REQ_PLAY_END:
		if(dpm.flag & PF_AUTO_SPLIT_FILE) {
			close_output();
		}
		return 0;
	case PM_REQ_DISCARD:
		return 0;
	}
	return -1;
}

/* set */

void opus_set_nframes(int nframes)
{
	const int maximum_nframes = 2880;

	if(nframes > maximum_nframes) {
		nframes = maximum_nframes;
	}
	else {
		int i = 0;
		const int target[] = { 120, 240, 480, 960, 1920, 2880, maximum_nframes };

		while(target[i] < nframes) {
			i ++;
		}
		nframes = target[i];
	}

	opus_options.nframes = nframes;
}

void opus_set_bitrate(int bitrate)
{
	if(bitrate <= 512) {
		bitrate *= 1000;
	}

	if(bitrate < 500) {
		bitrate = 500;
	}
	if(bitrate > 512000) {
		bitrate = 512000;
	}
	opus_options.bitrate = bitrate;
}

void opus_set_complexity(int complexity)
{
	if(complexity < 0) {
		complexity = 0;
	}
	if(complexity > 10) {
		complexity = 10;
	}
	opus_options.complexity = complexity;
}

void opus_set_vbr(int vbr)
{
	opus_options.vbr = !(!vbr);
}

void opus_set_cvbr(int cvbr)
{
	opus_options.cvbr = !(!cvbr);
}


#endif


