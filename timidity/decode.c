
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __POCC__
#include <sys/types.h>
#endif //for off_t
#include <stdio.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "instrum.h"
#include "decode.h"

#include "libarc/url.h"

static sample_t DummySampleData[128];

int get_sample_size_for_sample_type(int data_type)
{
	switch (data_type)
	{
	case SAMPLE_TYPE_INT32:
	case SAMPLE_TYPE_FLOAT:
		return 4;

	case SAMPLE_TYPE_DOUBLE:
		return 8;

	default:
		return 2;
	}
}

void clear_sample_decode_result(SampleDecodeResult *sdr)
{
	for (int i = 0; i < DECODE_MAX_CHANNELS; i++) {
		if (sdr->data_alloced[i]) {
			safe_free(sdr->data[i]);
			sdr->data_alloced[i] = 0;
		}

		sdr->data[i] = DummySampleData;
	}

	sdr->data_type = SAMPLE_TYPE_INT16;
	sdr->data_length = 0;
	sdr->channels = 0;
	sdr->sample_rate = 0;
}

#ifdef HAVE_LIBVORBIS

#include <vorbis/vorbisfile.h>

extern int load_vorbis_dll(void);	// w32g_vorbis_dll.c

#ifndef VORBIS_DLL_INCLUDE_VORBISFILE
extern int load_vorbisfile_dll(void);	// w32g_vorbisfile_dll.c
#endif

static size_t oggvorbis_read_callback(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    struct timidity_file *tf = (struct timidity_file *)datasource;
    return tf_read(ptr, size, nmemb, tf);
}

static int oggvorbis_seek_callback(void *datasource, ogg_int64_t offset, int whence)
{
    struct timidity_file *tf = (struct timidity_file *)datasource;
    return tf_seek(tf, offset, whence);
}

static long oggvorbis_tell_callback(void *datasource)
{
    struct timidity_file *tf = (struct timidity_file *)datasource;
    return tf_tell(tf);
}

SampleDecodeResult decode_oggvorbis(struct timidity_file *tf)
{
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "decoding ogg vorbis file...");

    SampleDecodeResult sdr = {.data[0] = DummySampleData, .data[1] = DummySampleData, .data_type = SAMPLE_TYPE_INT16};
    OggVorbis_File vf;

#ifdef AU_VORBIS_DLL
    if (load_vorbis_dll() != 0) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to load vorbis dll");
        return sdr;
    }
#ifndef VORBIS_DLL_INCLUDE_VORBISFILE
    if (load_vorbisfile_dll() != 0) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to load vorbisfile dll");
        return sdr;
    }
#endif
#endif

    int result = ov_open_callbacks(
        tf, &vf, 0, 0,
        (ov_callbacks){.read_func = &oggvorbis_read_callback, .seek_func = &oggvorbis_seek_callback, .tell_func = &oggvorbis_tell_callback}
    );

    if (result != 0) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to open ogg vorbis data; ov_open_callbacks() failed");
        return sdr;
    }

    vorbis_info *info = ov_info(&vf, -1);

    if (!info) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to read ogg vorbis info; ov_info() failed");
        goto cleanup;
    }

    sdr.channels = info->channels;

	if (sdr.channels < 1 || DECODE_MAX_CHANNELS < sdr.channels) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "samples with more than %d channels are not supported", DECODE_MAX_CHANNELS);
		goto cleanup;
	}

	sdr.sample_rate = info->rate;
    int64 total = ov_pcm_total(&vf, -1);

    if (total < 0) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to get data length; ov_pcm_total() failed");
        goto cleanup;
    }

    ptr_size_t data_length = info->channels * sizeof(sample_t) * total;
	data_length = (data_length > 0 ? data_length : 4096);
	ptr_size_t current_size = 0;
    sdr.data[0] = (sample_t *)safe_large_malloc(data_length);
	sdr.data_alloced[0] = 1;

    while (1) {
        int bitstream = 0;
        long ret = ov_read(&vf, (char *)(sdr.data[0]) + current_size, data_length - current_size, 0, 2, 1, &bitstream);

        if (ret < 0) {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to decode ogg vorbis data; ov_read() failed");
            goto cleanup;
        } else if (ret == 0) {
            break;
        }

		current_size += ret;

		if (data_length - current_size < 512) {
			ptr_size_t new_data_length = data_length + data_length / 2;
			sdr.data[0] = (sample_t *)safe_large_realloc(sdr.data[0], new_data_length);
			data_length = new_data_length;
		}
    }

    memset(((char *)sdr.data[0]) + current_size, 0, data_length - current_size);
	ov_clear(&vf);

	if (sdr.channels > 1) {
		// split data into multiple channels
		sample_t *single_data = sdr.data[0];

		for (int i = 0; i < sdr.channels; i++) {
			sdr.data[i] = (sample_t *)safe_large_calloc((current_size / sdr.channels) + 128, sizeof(sample_t));
			sdr.data_alloced[i] = 1;
		}

		for (int i = 0; i < current_size / sdr.channels; i++) {
			for (int j = 0; j < sdr.channels; j++) {
				sdr.data[j][i] = single_data[i * sdr.channels + j];
			}
		}

		safe_free(single_data);
	}

	sdr.data_length = (current_size / sdr.channels / sizeof(sample_t)) << FRACTION_BITS;
	return sdr;

cleanup:
    ov_clear(&vf);
	clear_sample_decode_result(&sdr);
	return sdr;
}

#else // HAVE_LIBVORBIS

SampleDecodeResult decode_oggvorbis(struct timidity_file *tf)
{
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ogg vorbis decoder support is disabled");
	return (SampleDecodeResult){.data[0] = DummySampleData, .data[1] = DummySampleData, .data_type = SAMPLE_TYPE_INT16};
}

#endif // HAVE_LIBVORBIS

#ifdef AU_FLAC

#include <FLAC/all.h>

#ifdef AU_FLAC_DLL

#include "w32_libFLAC_dll_g.h"

#endif // AU_FLAC_DLL

typedef struct {
	struct timidity_file *input;
	SampleDecodeResult *output;
	ptr_size_t current_size_in_samples;
	ptr_size_t buffer_size_in_samples;
} FLACDecodeContext;

static FLAC__StreamDecoderReadStatus flac_read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	struct timidity_file *tf = ((FLACDecodeContext *)client_data)->input;

	if (*bytes > 0) {
		*bytes = tf_read(buffer, 1, *bytes, tf);

		if (*bytes == 0) {
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		} else {
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
	} else {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
}

static FLAC__StreamDecoderSeekStatus flac_seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	struct timidity_file *tf = ((FLACDecodeContext *)client_data)->input;

	if (!IS_URL_SEEK_SAFE(tf->url)) {
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	}

	if (tf_seek_uint64(tf, absolute_byte_offset, SEEK_SET) == -1L) {
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	} else {
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
	}
}

static FLAC__StreamDecoderTellStatus flac_tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	struct timidity_file *tf = ((FLACDecodeContext *)client_data)->input;

	if (!IS_URL_SEEK_SAFE(tf->url)) {
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
	}

	*absolute_byte_offset = (FLAC__uint64)tf_tell(tf);
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus flac_length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	struct timidity_file *tf = ((FLACDecodeContext *)client_data)->input;

	if (!IS_URL_SEEK_SAFE(tf->url)) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	}

	off_size_t prevpos = tf_seek(tf, 0, SEEK_END);

	if (prevpos == -1L) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}

	*stream_length = (FLAC__uint64)tf_tell(tf);

	if (tf_seek(tf, prevpos, SEEK_SET) == -1L) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}

	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flac_eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
	struct timidity_file *tf = ((FLACDecodeContext *)client_data)->input;
	return !!url_eof(tf->url);
}

static FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	FLACDecodeContext *context = (FLACDecodeContext *)client_data;
	SampleDecodeResult *sdr = context->output;

	if (context->current_size_in_samples + frame->header.blocksize + 128 > context->buffer_size_in_samples) {
		context->buffer_size_in_samples += context->buffer_size_in_samples / 2 + frame->header.blocksize + 128;

		for (int i = 0; i < sdr->channels; i++) {
			sdr->data[i] = (sample_t *)safe_large_realloc(sdr->data[i], get_sample_size_for_sample_type(sdr->data_type) * context->buffer_size_in_samples);
		}
	}

	for (int i = 0; i < sdr->channels; i++) {
		switch (sdr->data_type) {
		case SAMPLE_TYPE_INT32:
			memcpy(((FLAC__int32 *)sdr->data[i]) + context->current_size_in_samples, buffer[i], frame->header.blocksize * sizeof(FLAC__int32));
			break;

		case SAMPLE_TYPE_INT16:
			for (unsigned int j = 0; j < frame->header.blocksize; j++) {
				sdr->data[i][context->current_size_in_samples + j] = (FLAC__int16)buffer[i][j];
			}
			break;
		}
	}

	context->current_size_in_samples += frame->header.blocksize;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

// FIXME: not safe if called multiple times
static void flac_metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	FLACDecodeContext *context = (FLACDecodeContext *)client_data;
	SampleDecodeResult *sdr = context->output;

	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		sdr->sample_rate = metadata->data.stream_info.sample_rate;
		sdr->data_type = (metadata->data.stream_info.bits_per_sample > 16 ? SAMPLE_TYPE_INT32 : SAMPLE_TYPE_INT16);

		context->buffer_size_in_samples = metadata->data.stream_info.total_samples + 128;
		sdr->channels = metadata->data.stream_info.channels;

		if (sdr->channels > DECODE_MAX_CHANNELS) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "samples with more than %d channels are not supported", DECODE_MAX_CHANNELS);
			sdr->channels = DECODE_MAX_CHANNELS;
		}

		for (int i = 0; i < sdr->channels; i++) {
			sdr->data[i] = (sample_t *)safe_large_malloc(get_sample_size_for_sample_type(sdr->data_type) * context->buffer_size_in_samples);
			sdr->data_alloced[i] = 1;
		}
	}
}

static void flac_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "an error has occurred while decoding FLAC stream [FLAC__StreamDecoderErrorStatus = %d]", status);
}

SampleDecodeResult decode_flac(struct timidity_file *tf)
{
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "decoding FLAC file...");
	SampleDecodeResult sdr = {.data[0] = DummySampleData, .data[1] = DummySampleData, .data_type = SAMPLE_TYPE_INT16};

#ifdef AU_FLAC_DLL
	if (g_load_libFLAC_dll() != 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to load FLAC dll");
		return sdr;
	}
#endif

	FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();

	if (!decoder) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC__stream_decoder_new() failed");
		return sdr;
	}

	FLAC__stream_decoder_set_md5_checking(decoder, TRUE);

	FLACDecodeContext context = {tf, &sdr};

	FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
		decoder,
		&flac_read_callback,
		&flac_seek_callback,
		&flac_tell_callback,
		&flac_length_callback,
		&flac_eof_callback,
		&flac_write_callback,
		&flac_metadata_callback,
		&flac_error_callback,
		&context
	);

	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC__stream_decoder_init_stream() failed [FLAC__StreamDecoderInitStatus = %d]", init_status);
		goto cleanup;
	}

	if (!FLAC__stream_decoder_process_until_end_of_stream(decoder)) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC__stream_decoder_process_until_end_of_stream() failed");
		goto cleanup;
	}

	for (int i = 0; i < sdr.channels; i++) {
		memset(
			(char *)sdr.data[i] + context.current_size_in_samples * get_sample_size_for_sample_type(sdr.data_type),
			0,
			(context.buffer_size_in_samples - context.current_size_in_samples) * get_sample_size_for_sample_type(sdr.data_type)
		);
	}

	FLAC__stream_decoder_delete(decoder);
	sdr.data_length = (splen_t)context.current_size_in_samples << FRACTION_BITS;
	return sdr;

cleanup:
	if (decoder) {
		FLAC__stream_decoder_delete(decoder);
	}

	clear_sample_decode_result(&sdr);
	return sdr;
}

#else // AU_FLAC

SampleDecodeResult decode_flac(struct timidity_file *tf)
{
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "FLAC decoder support is disabled");
	return (SampleDecodeResult){.data[0] = DummySampleData, .data[1] = DummySampleData, .data_type = SAMPLE_TYPE_INT16};
}

#endif // AU_FLAC

#ifdef HAVE_LIBMPG123

#include <mpg123.h>

int load_mpg123_dll(void);

static ssize_t mp3_read_callback(void *handle, void *buf, size_t count)
{
	struct timidity_file *tf = (struct timidity_file *)handle;
	return (ssize_t)tf_read(buf, 1, count, tf);
}

static off_t mp3_seek_callback(void *handle, off_t offset, int whence)
{
	struct timidity_file *tf = (struct timidity_file *)handle;
	return tf_seek(tf, offset, whence);
}

SampleDecodeResult decode_mp3(struct timidity_file *tf)
{
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "decoding mp3 file...");
	SampleDecodeResult sdr = {.data[0] = DummySampleData,.data[1] = DummySampleData,.data_type = SAMPLE_TYPE_INT16};

	if (load_mpg123_dll() != 0) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to load libmpg123.dll");
		return sdr;
	}

	if (mpg123_init() != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_init() failed");
		return sdr;
	}

	mpg123_handle *mh = mpg123_new(NULL, NULL);

	if (!mh) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_new() failed");
		return sdr;
	}

	if (mpg123_format_none(mh) != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_format_none() failed");
		goto cleanup;
	}

	// from libmpg123/format.c
	static const long supported_rates[9] = {
		8000, 11025, 12000,
		16000, 22050, 24000,
		32000, 44100, 48000
	};

	static const int supported_encodings[4] = {
		MPG123_ENC_SIGNED_16,
		MPG123_ENC_SIGNED_32,
		MPG123_ENC_FLOAT_32,
		MPG123_ENC_FLOAT_64
	};

	for (int i = 0; i < sizeof(supported_rates) / sizeof(supported_rates[0]); i++) {
		for (int j = 0; j < sizeof(supported_encodings) / sizeof(supported_encodings[0]); j++) {
			if (mpg123_format(mh, supported_rates[i], MPG123_STEREO | MPG123_MONO, supported_encodings[j]) != MPG123_OK) {
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_format() failed");
				goto cleanup;
			}
		}
	}

	if (mpg123_replace_reader_handle(mh, &mp3_read_callback, &mp3_seek_callback, NULL) != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_replace_reader_handle() failed");
		goto cleanup;
	}

	if (mpg123_open_handle(mh, tf) != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_open_handle() failed");
		goto cleanup;
	}

	long rate;
	int channels;
	int encoding;
	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_getformat() failed");
		goto cleanup;
	}

	if (channels < 1 || DECODE_MAX_CHANNELS < channels) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "samples with more than %d channels are not supported", DECODE_MAX_CHANNELS);
		goto cleanup;
	}

	sdr.channels = channels;
	sdr.sample_rate = rate;

	switch (encoding) {
	case MPG123_ENC_SIGNED_16:
		sdr.data_type = SAMPLE_TYPE_INT16;
		break;

	case MPG123_ENC_SIGNED_32:
		sdr.data_type = SAMPLE_TYPE_INT32;
		break;

	case MPG123_ENC_FLOAT_32:
		sdr.data_type = SAMPLE_TYPE_FLOAT;
		break;

	case MPG123_ENC_FLOAT_64:
		sdr.data_type = SAMPLE_TYPE_DOUBLE;
		break;

	default:
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unsupported mp3 format");
		goto cleanup;
	}

	// prevent format changes
	if (mpg123_format_none(mh) != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_format_none() failed");
		goto cleanup;
	}

	if (mpg123_format(mh, rate, channels, encoding) != MPG123_OK) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_format() failed");
		goto cleanup;
	}

	size_t current_length = 0;
	size_t buffer_size = 1024;
	sdr.data[0] = (sample_t *)safe_large_malloc(buffer_size);
	sdr.data_alloced[0] = 1;

	while (1) {
		buffer_size += buffer_size / 2 + 128;
		sdr.data[0] = (sample_t *)safe_large_realloc(sdr.data[0], buffer_size);

		size_t decoded;
		int err = mpg123_read(mh, (unsigned char *)sdr.data[0] + current_length, buffer_size - current_length - 128, &decoded);
		current_length += decoded;

		if (err == MPG123_DONE) {
			break;
		} else if (err != MPG123_OK) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mpg123_read() failed");
			goto cleanup;
		}
	}

	mpg123_close(mh);
	mpg123_delete(mh);

	memset((unsigned char *)sdr.data[0] + current_length, 0, buffer_size - current_length);

	if (sdr.channels > 1) {
		// split data into multiple channels
		sample_t *single_data = sdr.data[0];

		for (int i = 0; i < sdr.channels; i++) {
			sdr.data[i] = (sample_t *)safe_large_calloc(current_length / sdr.channels + 128, 1);
			sdr.data_alloced[i] = 1;
		}

		for (int i = 0; i < current_length / get_sample_size_for_sample_type(sdr.data_type) / sdr.channels; i++) {
			for (int j = 0; j < sdr.channels; j++) {
				memcpy(
					&sdr.data[j][i],
					(char *)single_data + (i * sdr.channels + j) * get_sample_size_for_sample_type(sdr.data_type),
					get_sample_size_for_sample_type(sdr.data_type)
				);
			}
		}

		safe_free(single_data);
	}

	sdr.data_length = (splen_t)(current_length / get_sample_size_for_sample_type(sdr.data_type) / sdr.channels) << FRACTION_BITS;
	return sdr;

cleanup:
	mpg123_close(mh);
	mpg123_delete(mh);
	// mpg123_exit();
	clear_sample_decode_result(&sdr);
	return (SampleDecodeResult){.data[0] = DummySampleData, .data[1] = DummySampleData, .data_type = SAMPLE_TYPE_INT16};
}

#else // HAVE_LIBMPG123

SampleDecodeResult decode_mp3(struct timidity_file *tf)
{
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "mp3 decoder support is disabled");
	return (SampleDecodeResult){.data[0] = DummySampleData, .data[1] = DummySampleData, .data_type = SAMPLE_TYPE_INT16};
}

#endif // HAVE_LIBMPG123
