
#pragma once

typedef struct SampleDecodeResult {
    sample_t *data;
	uint8 data_alloced;
	int data_type;
    splen_t data_length;
    int channels;
} SampleDecodeResult;

SampleDecodeResult decode_oggvorbis(struct timidity_file *tf);
