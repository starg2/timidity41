
#pragma once

#define DECODE_MAX_CHANNELS 2

typedef struct SampleDecodeResult {
    sample_t *data[DECODE_MAX_CHANNELS];
	uint8 data_alloced[DECODE_MAX_CHANNELS];
	int data_type;
    splen_t data_length;
    int channels;
	int sample_rate;
} SampleDecodeResult;

int get_sample_size_for_sample_type(int data_type);

void clear_sample_decode_result(SampleDecodeResult *sdr);

SampleDecodeResult decode_oggvorbis(struct timidity_file *tf);
SampleDecodeResult decode_flac(struct timidity_file *tf);
