// https://libav.org/doxygen/release/0.8/libavcodec_2api-example_8c-example.html
// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c
    #include <stdio.h>
#include "ffmpeg/ffmpeg.h"

typedef struct {
	AVPacket avpkt;
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *decoded_frame;
    int len;
    int data_size;
    int got_frame;
} ME_DecodeState;

ME_DecodeState *me_audio_decode_alloc(int format) {
	ME_DecodeState *state = malloc(sizeof(ME_DecodeState));
	av_init_packet(&state->avpkt);
	state->codec = avcodec_find_decoder(CODEC_ID_MP3);
    state->c = avcodec_alloc_context3(state->codec);
	avcodec_open2(state->c, state->codec, NULL);
    state->decoded_frame = av_frame_alloc();
	return state;
}

void me_audio_decode_set_data(ME_DecodeState* state, void* dataPtr, int dataSize) {
    state->avpkt.data = dataPtr;
    state->avpkt.size = dataSize;
    state->len = avcodec_decode_audio4(state->c, state->decoded_frame, &state->got_frame, &state->avpkt);
    state->data_size = av_get_bytes_per_sample(state->c->sample_fmt);
}

int me_audio_decode_get_numsamples(ME_DecodeState* state) {
	return state->decoded_frame->nb_samples;
}

int me_audio_decode_get_numchannels(ME_DecodeState* state) {
	return state->c->channels;
}

int me_audio_decode_get_data_size(ME_DecodeState* state) {
	return state->data_size * state->decoded_frame->nb_samples * state->c->channels;
	
}
void me_audio_decode_get_data(ME_DecodeState* state, void* ptr) {
	short *ptr2 = ptr;
	int n, ch;
	int samples = state->decoded_frame->nb_samples;
	int channels = state->decoded_frame->nb_samples;
	int data_size = state->data_size;
	uint8_t **data = state->decoded_frame->data;
	for (n = 0; n < samples; n++) {
		for (ch = 0; ch < channels; ch++) {
			*ptr2 = *(short*)(data[ch] + data_size * n);
			ptr2++;
		}
	}
}

int me_audio_decode_get_sample(ME_DecodeState* state, int channel, int sample) {
	if (state->data_size == 2) {
		return *(short*)(state->decoded_frame->data[channel] + (state->data_size) * sample);
	}
	return -1;
}

void me_audio_decode_free(ME_DecodeState* state) {
	free(state);
}
