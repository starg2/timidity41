/***************************************************************
 name: opus_dll  dll: opus.dll 
***************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "common.h"

#ifdef AU_OPUS_DLL

#include <windows.h>
#include <tchar.h>
#include <opus/opus.h>

extern int load_opus_dll(void);
extern void free_opus_dll(void);

typedef struct OpusEncoder OpusEncoder;
//typedef struct OpusDecoder OpusDecoder;
//typedef struct OpusRepacketizer OpusRepacketizer;

typedef int               (*type_opus_encoder_get_size)(int channels);
typedef OpusEncoder *     (*type_opus_encoder_create)(opus_int32 Fs, int channels, int application, int *error);
typedef int               (*type_opus_encoder_init)(OpusEncoder *st, opus_int32 Fs, int channels, int application);
typedef opus_int32        (*type_opus_encode)(OpusEncoder *st, const opus_int16 *pcm, int frame_size, unsigned char *data, opus_int32 max_data_bytes);
typedef opus_int32        (*type_opus_encode_float)(OpusEncoder *st, const float *pcm, int frame_size, unsigned char *data, opus_int32 max_data_bytes);
typedef void              (*type_opus_encoder_destroy)(OpusEncoder *st);
typedef int               (*type_opus_encoder_ctl)(OpusEncoder *st, int request, ...);
typedef int               (*type_opus_encoder_ctl_0)(OpusEncoder *st, int request);
typedef int               (*type_opus_encoder_ctl_1)(OpusEncoder *st, int request, int param1);
typedef int               (*type_opus_encoder_ctl_2)(OpusEncoder *st, int request, int param1, int param2);
typedef int               (*type_opus_encoder_ctl_3)(OpusEncoder *st, int request, int param1, int param2, int param3);
//typedef int               (*type_opus_decoder_get_size)(int channels);
//typedef OpusDecoder *     (*type_opus_decoder_create)(opus_int32 Fs, int channels, int *error);
//typedef int               (*type_opus_decoder_init)(OpusDecoder *st, opus_int32 Fs, int channels);
//typedef int               (*type_opus_decode)(OpusDecoder *st, const unsigned char *data, opus_int32 len, opus_int16 *pcm, int frame_size, int decode_fec);
//typedef int               (*type_opus_decode_float)(OpusDecoder *st, const unsigned char *data, opus_int32 len, float *pcm, int frame_size, int decode_fec);
//typedef int               (*type_opus_decoder_ctl)(OpusDecoder *st, int request, ...);
//typedef void              (*type_opus_decoder_destroy)(OpusDecoder *st);
//typedef int               (*type_opus_packet_parse)(const unsigned char *data, opus_int32 len, unsigned char *out_toc, const unsigned char *frames[48], opus_int16 size[48], int *payload_offset);
//typedef int               (*type_opus_packet_get_bandwidth)(const unsigned char *data);
//typedef int               (*type_opus_packet_get_samples_per_frame)(const unsigned char *data, opus_int32 Fs);
//typedef int               (*type_opus_packet_get_nb_channels)(const unsigned char *data);
//typedef int               (*type_opus_packet_get_nb_frames)(const unsigned char packet[], opus_int32 len);
//typedef int               (*type_opus_packet_get_nb_samples)(const unsigned char packet[], opus_int32 len, opus_int32 Fs);
//typedef int               (*type_opus_decoder_get_nb_samples)(const OpusDecoder *dec, const unsigned char packet[], opus_int32 len);
typedef void              (*type_opus_pcm_soft_clip)(float *pcm, int frame_size, int channels, float *softclip_mem);
//typedef OpusRepacketizer *(*type_opus_repacketizer_init)(OpusRepacketizer *rp);
//typedef OpusRepacketizer *(*type_opus_repacketizer_create)(void);
//typedef void              (*type_opus_repacketizer_destroy)(OpusRepacketizer *rp);
//typedef int               (*type_opus_repacketizer_cat)(OpusRepacketizer *rp, const unsigned char *data, opus_int32 len);
//typedef opus_int32        (*type_opus_repacketizer_out_range)(OpusRepacketizer *rp, int begin, int end, unsigned char *data, opus_int32 maxlen);
//typedef int               (*type_opus_repacketizer_get_nb_frames)(OpusRepacketizer *rp);
//typedef opus_int32        (*type_opus_repacketizer_out)(OpusRepacketizer *rp, unsigned char *data, opus_int32 maxlen);
//typedef int               (*type_opus_packet_pad)(unsigned char *data, opus_int32 len, opus_int32 new_len);
//typedef opus_int32        (*type_opus_packet_unpad)(unsigned char *data, opus_int32 len);
//typedef int               (*type_opus_multistream_packet_pad)(unsigned char *data, opus_int32 len, opus_int32 new_len, int nb_streams);
//typedef opus_int32        (*type_opus_multistream_packet_unpad)(unsigned char *data, opus_int32 len, int nb_streams);

static struct opus_dll_ {
	type_opus_encoder_get_size opus_encoder_get_size;
	type_opus_encoder_create opus_encoder_create;
	type_opus_encoder_init opus_encoder_init;
	type_opus_encode opus_encode;
	type_opus_encode_float opus_encode_float;
	type_opus_encoder_destroy opus_encoder_destroy;
	type_opus_encoder_ctl opus_encoder_ctl;
//	type_opus_decoder_get_size opus_decoder_get_size;
//	type_opus_decoder_create opus_decoder_create;
//	type_opus_decoder_init opus_decoder_init;
//	type_opus_decode opus_decode;
//	type_opus_decode_float opus_decode_float;
//	type_opus_decoder_ctl opus_decoder_ctl;
//	type_opus_decoder_destroy opus_decoder_destroy;
//	type_opus_packet_parse opus_packet_parse;
//	type_opus_packet_get_bandwidth opus_packet_get_bandwidth;
//	type_opus_packet_get_samples_per_frame opus_packet_get_samples_per_frame;
//	type_opus_packet_get_nb_channels opus_packet_get_nb_channels;
//	type_opus_packet_get_nb_frames opus_packet_get_nb_frames;
//	type_opus_packet_get_nb_samples opus_packet_get_nb_samples;
//	type_opus_decoder_get_nb_samples opus_decoder_get_nb_samples;
	type_opus_pcm_soft_clip opus_pcm_soft_clip;
//	type_opus_repacketizer_init opus_repacketizer_init;
//	type_opus_repacketizer_create opus_repacketizer_create;
//	type_opus_repacketizer_destroy opus_repacketizer_destroy;
//	type_opus_repacketizer_cat opus_repacketizer_cat;
//	type_opus_repacketizer_out_range opus_repacketizer_out_range;
//	type_opus_repacketizer_get_nb_frames opus_repacketizer_get_nb_frames;
//	type_opus_repacketizer_out opus_repacketizer_out;
//	type_opus_packet_pad opus_packet_pad;
//	type_opus_packet_unpad opus_packet_unpad;
//	type_opus_multistream_packet_pad opus_multistream_packet_pad;
//	type_opus_multistream_packet_unpad opus_multistream_packet_unpad;
} opus_dll;

static volatile HANDLE h_opus_dll = NULL;

void free_opus_dll(void)
{
	if(h_opus_dll){
		FreeLibrary(h_opus_dll);
		h_opus_dll = NULL;
	}
}

int load_opus_dll(void)
{
	if(!h_opus_dll){
		w32_reset_dll_directory();
		h_opus_dll = LoadLibrary(_T("opus.dll"));
		if (!h_opus_dll) h_opus_dll = LoadLibrary(_T("libopus.dll"));
		if (!h_opus_dll) h_opus_dll = LoadLibrary(_T("libopus-0.dll"));
		if(!h_opus_dll) return -1;
	}
	opus_dll.opus_encoder_get_size = (type_opus_encoder_get_size)GetProcAddress(h_opus_dll,"opus_encoder_get_size");
	if(!opus_dll.opus_encoder_get_size){ free_opus_dll(); return -1; }
	opus_dll.opus_encoder_create = (type_opus_encoder_create)GetProcAddress(h_opus_dll,"opus_encoder_create");
	if(!opus_dll.opus_encoder_create){ free_opus_dll(); return -1; }
	opus_dll.opus_encoder_init = (type_opus_encoder_init)GetProcAddress(h_opus_dll,"opus_encoder_init");
	if(!opus_dll.opus_encoder_init){ free_opus_dll(); return -1; }
	opus_dll.opus_encode = (type_opus_encode)GetProcAddress(h_opus_dll,"opus_encode");
	if(!opus_dll.opus_encode){ free_opus_dll(); return -1; }
	opus_dll.opus_encode_float = (type_opus_encode_float)GetProcAddress(h_opus_dll,"opus_encode_float");
	if(!opus_dll.opus_encode_float){ free_opus_dll(); return -1; }
	opus_dll.opus_encoder_destroy = (type_opus_encoder_destroy)GetProcAddress(h_opus_dll,"opus_encoder_destroy");
	if(!opus_dll.opus_encoder_destroy){ free_opus_dll(); return -1; }
	opus_dll.opus_encoder_ctl = (type_opus_encoder_ctl)GetProcAddress(h_opus_dll,"opus_encoder_ctl");
	if(!opus_dll.opus_encoder_ctl){ free_opus_dll(); return -1; }
//	opus_dll.opus_decoder_get_size = (type_opus_decoder_get_size)GetProcAddress(h_opus_dll,"opus_decoder_get_size");
//	if(!opus_dll.opus_decoder_get_size){ free_opus_dll(); return -1; }
//	opus_dll.opus_decoder_create = (type_opus_decoder_create)GetProcAddress(h_opus_dll,"opus_decoder_create");
//	if(!opus_dll.opus_decoder_create){ free_opus_dll(); return -1; }
//	opus_dll.opus_decoder_init = (type_opus_decoder_init)GetProcAddress(h_opus_dll,"opus_decoder_init");
//	if(!opus_dll.opus_decoder_init){ free_opus_dll(); return -1; }
//	opus_dll.opus_decode = (type_opus_decode)GetProcAddress(h_opus_dll,"opus_decode");
//	if(!opus_dll.opus_decode){ free_opus_dll(); return -1; }
//	opus_dll.opus_decode_float = (type_opus_decode_float)GetProcAddress(h_opus_dll,"opus_decode_float");
//	if(!opus_dll.opus_decode_float){ free_opus_dll(); return -1; }
//	opus_dll.opus_decoder_ctl = (type_opus_decoder_ctl)GetProcAddress(h_opus_dll,"opus_decoder_ctl");
//	if(!opus_dll.opus_decoder_ctl){ free_opus_dll(); return -1; }
//	opus_dll.opus_decoder_destroy = (type_opus_decoder_destroy)GetProcAddress(h_opus_dll,"opus_decoder_destroy");
//	if(!opus_dll.opus_decoder_destroy){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_parse = (type_opus_packet_parse)GetProcAddress(h_opus_dll,"opus_packet_parse");
//	if(!opus_dll.opus_packet_parse){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_get_bandwidth = (type_opus_packet_get_bandwidth)GetProcAddress(h_opus_dll,"opus_packet_get_bandwidth");
//	if(!opus_dll.opus_packet_get_bandwidth){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_get_samples_per_frame = (type_opus_packet_get_samples_per_frame)GetProcAddress(h_opus_dll,"opus_packet_get_samples_per_frame");
//	if(!opus_dll.opus_packet_get_samples_per_frame){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_get_nb_channels = (type_opus_packet_get_nb_channels)GetProcAddress(h_opus_dll,"opus_packet_get_nb_channels");
//	if(!opus_dll.opus_packet_get_nb_channels){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_get_nb_frames = (type_opus_packet_get_nb_frames)GetProcAddress(h_opus_dll,"opus_packet_get_nb_frames");
//	if(!opus_dll.opus_packet_get_nb_frames){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_get_nb_samples = (type_opus_packet_get_nb_samples)GetProcAddress(h_opus_dll,"opus_packet_get_nb_samples");
//	if(!opus_dll.opus_packet_get_nb_samples){ free_opus_dll(); return -1; }
//	opus_dll.opus_decoder_get_nb_samples = (type_opus_decoder_get_nb_samples)GetProcAddress(h_opus_dll,"opus_decoder_get_nb_samples");
//	if(!opus_dll.opus_decoder_get_nb_samples){ free_opus_dll(); return -1; }
	opus_dll.opus_pcm_soft_clip = (type_opus_pcm_soft_clip)GetProcAddress(h_opus_dll,"opus_pcm_soft_clip");
	if(!opus_dll.opus_pcm_soft_clip){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_init = (type_opus_repacketizer_init)GetProcAddress(h_opus_dll,"opus_repacketizer_init");
//	if(!opus_dll.opus_repacketizer_init){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_create = (type_opus_repacketizer_create)GetProcAddress(h_opus_dll,"opus_repacketizer_create");
//	if(!opus_dll.opus_repacketizer_create){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_destroy = (type_opus_repacketizer_destroy)GetProcAddress(h_opus_dll,"opus_repacketizer_destroy");
//	if(!opus_dll.opus_repacketizer_destroy){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_cat = (type_opus_repacketizer_cat)GetProcAddress(h_opus_dll,"opus_repacketizer_cat");
//	if(!opus_dll.opus_repacketizer_cat){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_out_range = (type_opus_repacketizer_out_range)GetProcAddress(h_opus_dll,"opus_repacketizer_out_range");
//	if(!opus_dll.opus_repacketizer_out_range){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_get_nb_frames = (type_opus_repacketizer_get_nb_frames)GetProcAddress(h_opus_dll,"opus_repacketizer_get_nb_frames");
//	if(!opus_dll.opus_repacketizer_get_nb_frames){ free_opus_dll(); return -1; }
//	opus_dll.opus_repacketizer_out = (type_opus_repacketizer_out)GetProcAddress(h_opus_dll,"opus_repacketizer_out");
//	if(!opus_dll.opus_repacketizer_out){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_pad = (type_opus_packet_pad)GetProcAddress(h_opus_dll,"opus_packet_pad");
//	if(!opus_dll.opus_packet_pad){ free_opus_dll(); return -1; }
//	opus_dll.opus_packet_unpad = (type_opus_packet_unpad)GetProcAddress(h_opus_dll,"opus_packet_unpad");
//	if(!opus_dll.opus_packet_unpad){ free_opus_dll(); return -1; }
//	opus_dll.opus_multistream_packet_pad = (type_opus_multistream_packet_pad)GetProcAddress(h_opus_dll,"opus_multistream_packet_pad");
//	if(!opus_dll.opus_multistream_packet_pad){ free_opus_dll(); return -1; }
//	opus_dll.opus_multistream_packet_unpad = (type_opus_multistream_packet_unpad)GetProcAddress(h_opus_dll,"opus_multistream_packet_unpad");
//	if(!opus_dll.opus_multistream_packet_unpad){ free_opus_dll(); return -1; }
	return 0;
}

int opus_encoder_get_size(int channels)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_get_size(channels);
	}
	return 0;
}

OpusEncoder *opus_encoder_create(opus_int32 Fs, int channels, int application, int *error)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_create(Fs,channels,application,error);
	}
	return 0;
}

int opus_encoder_init(OpusEncoder *st, opus_int32 Fs, int channels, int application)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_init(st,Fs,channels,application);
	}
	return 0;
}

opus_int32 opus_encode(OpusEncoder *st, const opus_int16 *pcm, int frame_size, unsigned char *data, opus_int32 max_data_bytes)
{
	if(h_opus_dll){
		return opus_dll.opus_encode(st,pcm,frame_size,data,max_data_bytes);
	}
	return 0;
}

opus_int32 opus_encode_float(OpusEncoder *st, const float *pcm, int frame_size, unsigned char *data, opus_int32 max_data_bytes)
{
	if(h_opus_dll){
		return opus_dll.opus_encode_float(st,pcm,frame_size,data,max_data_bytes);
	}
	return 0;
}

void opus_encoder_destroy(OpusEncoder *st)
{
	if(h_opus_dll){
		opus_dll.opus_encoder_destroy(st);
	}
	return;
}

#if 0
int opus_encoder_ctl(OpusEncoder *st, int request, ...)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_ctl(st,request,...);
	}
	return 0;
}
#endif

int opus_encoder_ctl_0(OpusEncoder *st, int request)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_ctl(st,request);
	}
	return 0;
}

int opus_encoder_ctl_1(OpusEncoder *st, int request, int param1)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_ctl(st,request,param1);
	}
	return 0;
}

int opus_encoder_ctl_2(OpusEncoder *st, int request, int param1, int param2)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_ctl(st,request,param1,param2);
	}
	return 0;
}

int opus_encoder_ctl_3(OpusEncoder *st, int request, int param1, int param2, int param3)
{
	if(h_opus_dll){
		return opus_dll.opus_encoder_ctl(st,request,param1,param2,param3);
	}
	return 0;
}

#if 0
int opus_decoder_get_size(int channels)
{
	if(h_opus_dll){
		return opus_dll.opus_decoder_get_size(channels);
	}
	return 0;
}

OpusDecoder *opus_decoder_create(opus_int32 Fs, int channels, int *error)
{
	if(h_opus_dll){
		return opus_dll.opus_decoder_create(Fs,channels,error);
	}
	return 0;
}

int opus_decoder_init(OpusDecoder *st, opus_int32 Fs, int channels)
{
	if(h_opus_dll){
		return opus_dll.opus_decoder_init(st,Fs,channels);
	}
	return 0;
}

int opus_decode(OpusDecoder *st, const unsigned char *data, opus_int32 len, opus_int16 *pcm, int frame_size, int decode_fec)
{
	if(h_opus_dll){
		return opus_dll.opus_decode(st,data,len,pcm,frame_size,decode_fec);
	}
	return 0;
}

int opus_decode_float(OpusDecoder *st, const unsigned char *data, opus_int32 len, float *pcm, int frame_size, int decode_fec)
{
	if(h_opus_dll){
		return opus_dll.opus_decode_float(st,data,len,pcm,frame_size,decode_fec);
	}
	return 0;
}

int opus_decoder_ctl(OpusDecoder *st, int request, ...)
{
	if(h_opus_dll){
		return opus_dll.opus_decoder_ctl(st,request,...);
	}
	return 0;
}

void opus_decoder_destroy(OpusDecoder *st)
{
	if(h_opus_dll){
		opus_dll.opus_decoder_destroy(st);
	}
	return;
}

int opus_packet_parse(const unsigned char *data, opus_int32 len, unsigned char *out_toc, const unsigned char *frames[48], opus_int16 size[48], int *payload_offset)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_parse(data,len,out_toc,frames,size,payload_offset);
	}
	return 0;
}

int opus_packet_get_bandwidth(const unsigned char *data)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_get_bandwidth(data);
	}
	return 0;
}

int opus_packet_get_samples_per_frame(const unsigned char *data, opus_int32 Fs)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_get_samples_per_frame(data,Fs);
	}
	return 0;
}

int opus_packet_get_nb_channels(const unsigned char *data)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_get_nb_channels(data);
	}
	return 0;
}

int opus_packet_get_nb_frames(const unsigned char packet[], opus_int32 len)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_get_nb_frames(packet,len);
	}
	return 0;
}

int opus_packet_get_nb_samples(const unsigned char packet[], opus_int32 len, opus_int32 Fs)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_get_nb_samples(packet,len,Fs);
	}
	return 0;
}

int opus_decoder_get_nb_samples(const OpusDecoder *dec, const unsigned char packet[], opus_int32 len)
{
	if(h_opus_dll){
		return opus_dll.opus_decoder_get_nb_samples(dec,packet,len);
	}
	return 0;
}
#endif

void opus_pcm_soft_clip(float *pcm, int frame_size, int channels, float *softclip_mem)
{
	if(h_opus_dll){
		opus_dll.opus_pcm_soft_clip(pcm,frame_size,channels,softclip_mem);
	}
	return;
}

#if 0
OpusRepacketizer *opus_repacketizer_init(OpusRepacketizer *rp)
{
	if(h_opus_dll){
		return opus_dll.opus_repacketizer_init(rp);
	}
	return 0;
}

OpusRepacketizer *opus_repacketizer_create(void)
{
	if(h_opus_dll){
		return opus_dll.opus_repacketizer_create();
	}
	return 0;
}

void opus_repacketizer_destroy(OpusRepacketizer *rp)
{
	if(h_opus_dll){
		opus_dll.opus_repacketizer_destroy(rp);
	}
	return;
}

int opus_repacketizer_cat(OpusRepacketizer *rp, const unsigned char *data, opus_int32 len)
{
	if(h_opus_dll){
		return opus_dll.opus_repacketizer_cat(rp,data,len);
	}
	return 0;
}

opus_int32 opus_repacketizer_out_range(OpusRepacketizer *rp, int begin, int end, unsigned char *data, opus_int32 maxlen)
{
	if(h_opus_dll){
		return opus_dll.opus_repacketizer_out_range(rp,begin,end,data,maxlen);
	}
	return 0;
}

int opus_repacketizer_get_nb_frames(OpusRepacketizer *rp)
{
	if(h_opus_dll){
		return opus_dll.opus_repacketizer_get_nb_frames(rp);
	}
	return 0;
}

opus_int32 opus_repacketizer_out(OpusRepacketizer *rp, unsigned char *data, opus_int32 maxlen)
{
	if(h_opus_dll){
		return opus_dll.opus_repacketizer_out(rp,data,maxlen);
	}
	return 0;
}

int opus_packet_pad(unsigned char *data, opus_int32 len, opus_int32 new_len)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_pad(data,len,new_len);
	}
	return 0;
}

opus_int32 opus_packet_unpad(unsigned char *data, opus_int32 len)
{
	if(h_opus_dll){
		return opus_dll.opus_packet_unpad(data,len);
	}
	return 0;
}

int opus_multistream_packet_pad(unsigned char *data, opus_int32 len, opus_int32 new_len, int nb_streams)
{
	if(h_opus_dll){
		return opus_dll.opus_multistream_packet_pad(data,len,new_len,nb_streams);
	}
	return 0;
}

opus_int32 opus_multistream_packet_unpad(unsigned char *data, opus_int32 len, int nb_streams)
{
	if(h_opus_dll){
		return opus_dll.opus_multistream_packet_unpad(data,len,nb_streams);
	}
	return 0;
}
#endif

/***************************************************************/
#endif /* AU_OPUS_DLL */


