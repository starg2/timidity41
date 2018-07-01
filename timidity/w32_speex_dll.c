
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef AU_SPEEX_DLL

#include <string.h>

#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>

#include <windows.h>

#ifdef speex_lib_get_mode
#undef speex_lib_get_mode
#endif

int load_speex_dll(void);
void free_speex_dll(void);

static HANDLE h_speex_dll;

typedef void (*type_speex_bits_init)(SpeexBits *bits);
typedef void (*type_speex_bits_reset)(SpeexBits *bits);
typedef int (*type_speex_bits_write)(SpeexBits *bits, char *bytes, int max_len);
typedef void (*type_speex_bits_pack)(SpeexBits *bits, int data, int nbBits);
typedef void (*type_speex_bits_destroy)(SpeexBits *bits);
typedef void *(*type_speex_encoder_init)(const SpeexMode *mode);
typedef void (*type_speex_encoder_destroy)(void *state);
typedef int (*type_speex_encode)(void *state, float *in, SpeexBits *bits);
typedef int (*type_speex_encoder_ctl)(void *state, int request, void *ptr);
typedef const SpeexMode *(*type_speex_lib_get_mode)(int mode);
typedef void (*type_speex_init_header)(SpeexHeader *header, int rate, int nb_channels, const struct SpeexMode *m);
typedef char *(*type_speex_header_to_packet)(SpeexHeader *header, int *size);
typedef void (*type_speex_encode_stereo)(float *data, int frame_size, SpeexBits *bits);
typedef void (*type_speex_bits_insert_terminator)(SpeexBits *bits);

static struct {
	type_speex_bits_init speex_bits_init;
	type_speex_bits_reset speex_bits_reset;
	type_speex_bits_write speex_bits_write;
	type_speex_bits_pack speex_bits_pack;
	type_speex_bits_destroy speex_bits_destroy;
	type_speex_encoder_init speex_encoder_init;
	type_speex_encoder_destroy speex_encoder_destroy;
	type_speex_encode speex_encode;
	type_speex_encoder_ctl speex_encoder_ctl;
	type_speex_lib_get_mode speex_lib_get_mode;
	type_speex_init_header speex_init_header;
	type_speex_header_to_packet speex_header_to_packet;
	type_speex_encode_stereo speex_encode_stereo;
	type_speex_bits_insert_terminator speex_bits_insert_terminator;
} speex_dll;

int load_speex_dll(void)
{
	if (!h_speex_dll) {
		h_speex_dll = LoadLibrary(TEXT("speex.dll"));

		if (!h_speex_dll) {
			h_speex_dll = LoadLibrary(TEXT("libspeex.dll"));
		}

		if (!h_speex_dll) {
			return -1;
		}

		speex_dll.speex_bits_init = (type_speex_bits_init)GetProcAddress(h_speex_dll, "speex_bits_init");
		if (!speex_dll.speex_bits_init) { free_speex_dll(); return -1; }
		speex_dll.speex_bits_reset = (type_speex_bits_reset)GetProcAddress(h_speex_dll, "speex_bits_reset");
		if (!speex_dll.speex_bits_reset) { free_speex_dll(); return -1; }
		speex_dll.speex_bits_write = (type_speex_bits_write)GetProcAddress(h_speex_dll, "speex_bits_write");
		if (!speex_dll.speex_bits_write) { free_speex_dll(); return -1; }
		speex_dll.speex_bits_pack = (type_speex_bits_pack)GetProcAddress(h_speex_dll, "speex_bits_pack");
		if (!speex_dll.speex_bits_pack) { free_speex_dll(); return -1; }
		speex_dll.speex_bits_destroy = (type_speex_bits_destroy)GetProcAddress(h_speex_dll, "speex_bits_destroy");
		if (!speex_dll.speex_bits_destroy) { free_speex_dll(); return -1; }
		speex_dll.speex_encoder_init = (type_speex_encoder_init)GetProcAddress(h_speex_dll, "speex_encoder_init");
		if (!speex_dll.speex_encoder_init) { free_speex_dll(); return -1; }
		speex_dll.speex_encoder_destroy = (type_speex_encoder_destroy)GetProcAddress(h_speex_dll, "speex_encoder_destroy");
		if (!speex_dll.speex_encoder_destroy) { free_speex_dll(); return -1; }
		speex_dll.speex_encode = (type_speex_encode)GetProcAddress(h_speex_dll, "speex_encode");
		if (!speex_dll.speex_encode) { free_speex_dll(); return -1; }
		speex_dll.speex_encoder_ctl = (type_speex_encoder_ctl)GetProcAddress(h_speex_dll, "speex_encoder_ctl");
		if (!speex_dll.speex_encoder_ctl) { free_speex_dll(); return -1; }
		speex_dll.speex_lib_get_mode = (type_speex_lib_get_mode)GetProcAddress(h_speex_dll, "speex_lib_get_mode");
		if (!speex_dll.speex_lib_get_mode) { free_speex_dll(); return -1; }
		speex_dll.speex_init_header = (type_speex_init_header)GetProcAddress(h_speex_dll, "speex_init_header");
		if (!speex_dll.speex_init_header) { free_speex_dll(); return -1; }
		speex_dll.speex_header_to_packet = (type_speex_header_to_packet)GetProcAddress(h_speex_dll, "speex_header_to_packet");
		if (!speex_dll.speex_header_to_packet) { free_speex_dll(); return -1; }
		speex_dll.speex_encode_stereo = (type_speex_encode_stereo)GetProcAddress(h_speex_dll, "speex_encode_stereo");
		if (!speex_dll.speex_encode_stereo) { free_speex_dll(); return -1; }
		speex_dll.speex_bits_insert_terminator = (type_speex_bits_insert_terminator)GetProcAddress(h_speex_dll, "speex_bits_insert_terminator");
		if (!speex_dll.speex_bits_insert_terminator) { free_speex_dll(); return -1; }
	}

	return 0;
}

void free_speex_dll(void)
{
	memset(&speex_dll, 0, sizeof(speex_dll));

	if (h_speex_dll) {
		FreeLibrary(h_speex_dll);
		h_speex_dll = NULL;
	}
}

void speex_bits_init(SpeexBits *bits)
{
	if (speex_dll.speex_bits_init) {
		(*speex_dll.speex_bits_init)(bits);
	}
}

void speex_bits_reset(SpeexBits *bits)
{
	if (speex_dll.speex_bits_reset) {
		(*speex_dll.speex_bits_reset)(bits);
	}
}

int speex_bits_write(SpeexBits *bits, char *bytes, int max_len)
{
	if (speex_dll.speex_bits_write) {
		return (*speex_dll.speex_bits_write)(bits, bytes, max_len);
	} else {
		return 0;
	}
}

void speex_bits_pack(SpeexBits *bits, int data, int nbBits)
{
	if (speex_dll.speex_bits_pack) {
		(*speex_dll.speex_bits_pack)(bits, data, nbBits);
	}
}

void speex_bits_destroy(SpeexBits *bits)
{
	if (speex_dll.speex_bits_destroy) {
		(*speex_dll.speex_bits_destroy)(bits);
	}
}

void *speex_encoder_init(const SpeexMode *mode)
{
	if (speex_dll.speex_encoder_init) {
		return (*speex_dll.speex_encoder_init)(mode);
	} else {
		return NULL;
	}
}

void speex_encoder_destroy(void *state)
{
	if (speex_dll.speex_encoder_destroy) {
		(*speex_dll.speex_encoder_destroy)(state);
	}
}

int speex_encode(void *state, float *in, SpeexBits *bits)
{
	if (speex_dll.speex_encode) {
		return (*speex_dll.speex_encode)(state, in, bits);
	} else {
		return 0;
	}
}

int speex_encoder_ctl(void *state, int request, void *ptr)
{
	if (speex_dll.speex_encoder_ctl) {
		return (*speex_dll.speex_encoder_ctl)(state, request, ptr);
	} else {
		return -1;
	}
}

const SpeexMode *speex_lib_get_mode(int mode)
{
	if (speex_dll.speex_lib_get_mode) {
		return (*speex_dll.speex_lib_get_mode)(mode);
	} else {
		return NULL;
	}
}

void speex_init_header(SpeexHeader *header, int rate, int nb_channels, const struct SpeexMode *m)
{
	if (speex_dll.speex_init_header) {
		(*speex_dll.speex_init_header)(header, rate, nb_channels, m);
	}
}

char *speex_header_to_packet(SpeexHeader *header, int *size)
{
	if (speex_dll.speex_header_to_packet) {
		return (*speex_dll.speex_header_to_packet)(header, size);
	} else {
		return NULL;
	}
}

void speex_encode_stereo(float *data, int frame_size, SpeexBits *bits)
{
	if (speex_dll.speex_encode_stereo) {
		(*speex_dll.speex_encode_stereo)(data, frame_size, bits);
	}
}

void speex_bits_insert_terminator(SpeexBits *bits)
{
	if (speex_dll.speex_bits_insert_terminator) {
		(*speex_dll.speex_bits_insert_terminator)(bits);
	}
}

#endif
