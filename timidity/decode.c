
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

#ifdef HAVE_LIBVORBIS

#include <vorbis/vorbisfile.h>

extern int load_vorbis_dll(void);	// w32g_vorbis_dll.c

#ifndef VORBIS_DLL_INCLUDE_VORBISFILE
extern int load_vorbisfile_dll(void);	// w32g_vorbisfile_dll.c
#endif

static sample_t DummySampleData[128];

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
	int result;
    SampleDecodeResult sdr = {0};
    OggVorbis_File vf;
	vorbis_info *info = NULL;
	int64 total;
	ptr_size_t data_length;
	ptr_size_t current_size;

	sdr.data = DummySampleData;
	sdr.data_type = SAMPLE_TYPE_INT16;

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
	vf.callbacks.read_func = &oggvorbis_read_callback;
	vf.callbacks.seek_func = &oggvorbis_seek_callback;
	vf.callbacks.tell_func = &oggvorbis_tell_callback;
    result = ov_open_callbacks(tf, &vf, 0, 0, vf.callbacks);

    if (result != 0) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to open ogg vorbis data; ov_open_callbacks() failed");
        return sdr;
    }

    info = ov_info(&vf, -1);

    if (!info) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to read ogg vorbis info; ov_info() failed");
        goto cleanup;
    }

    sdr.channels = info->channels;
    total = ov_pcm_total(&vf, -1);

    if (total < 0) {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to get data length; ov_pcm_total() failed");
        goto cleanup;
    }

    data_length = info->channels * sizeof(sample_t) * total;
	data_length = (data_length > 0 ? data_length : 4096);
	current_size = 0;
    sdr.data = (sample_t *)safe_large_malloc(data_length);
	sdr.data_alloced = 1;

    while (1) {
        int bitstream = 0;
        long ret = ov_read(&vf, (char *)(sdr.data) + current_size, data_length - current_size, 0, 2, 1, &bitstream);

        if (ret < 0) {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "unable to decode ogg vorbis data; ov_read() failed");
            goto cleanup;
        } else if (ret == 0) {
			sdr.data_length = (splen_t)(current_size / 2) << FRACTION_BITS;
            break;
        }

		current_size += ret;

		if (data_length - current_size < 512) {
			ptr_size_t new_data_length = data_length + data_length / 2;
			sdr.data = (sample_t *)safe_large_realloc(sdr.data, new_data_length);
			data_length = new_data_length;
		}
    }

    memset(((char *)sdr.data) + current_size, 0, data_length - current_size);
	ov_clear(&vf);
	return sdr;

cleanup:
    ov_clear(&vf);
	
    if (sdr.data_alloced) {
        safe_free(sdr.data);
    }

    sdr.data = DummySampleData;
	sdr.data_alloced = 0;

    return sdr;
}

#else // HAVE_LIBVORBIS

SampleDecodeResult decode_oggvorbis(struct timidity_file *tf)
{
    SampleDecodeResult sdr = {0};

	sdr.data = DummySampleData;
	sdr.data_type = SAMPLE_TYPE_INT16;
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "ogg vorbis decoder support is disabled");

    return sdr;
}

#endif // HAVE_LIBVORBIS
