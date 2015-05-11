// https://libav.org/doxygen/release/0.8/libavcodec_2api-example_8c-example.html
// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c

#include <stdio.h>
#include "ffmpeg/ffmpeg.h"
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <emscripten.h>

typedef struct {
	AVFormatContext *format;
	AVStream *stream_audio;
	AVStream *stream_video;
	/*
	AVPacket avpkt;
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *decoded_frame;
    int len;
    int data_size;
    int got_frame;
    */
} ME_DecodeState;

typedef struct {
	int size;
	union {
		void *data;
		char *data8;
		short *data16;
		int *data32;
		float *dataf;
	};
} ME_BufferData;

void me_init() {
	avcodec_register_all();
	av_register_all();
}

ME_DecodeState *me_open(char* filename) {
	ME_DecodeState *state = av_malloc(sizeof(ME_DecodeState));
	int i = 0;

	if ((avformat_open_input(&state->format, filename, NULL, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return NULL;
	}

	if ((avformat_find_stream_info(state->format, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return NULL;
	}

	for (i = 0; i < state->format->nb_streams; i++) {
		AVStream *stream = state->format->streams[i];
		switch (stream->codec->codec_type) {
			case AVMEDIA_TYPE_VIDEO:
				state->stream_video = stream;
				break;
			case AVMEDIA_TYPE_AUDIO:
				state->stream_audio = stream;
				break;
			default:
				break;
		}
		if (avcodec_open2(stream->codec, avcodec_find_decoder(stream->codec->codec_id), NULL) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
			return NULL;
		}
	}

	return state;
}

void me_close(ME_DecodeState *state) {
	avformat_close_input(&state->format);
	av_free(state);
}

AVPacket *me_packet_read(ME_DecodeState *state) {
	AVPacket *packet = av_malloc(sizeof(AVPacket));
	if (av_read_frame(state->format, packet) < 0) {
		av_free(packet);
		return NULL;
	} else {
		return packet;		
	}
}

void me_packet_free(AVPacket *packet) {
	av_free(packet);
}

enum AVMediaType me_packet_get_type(ME_DecodeState *state, AVPacket *packet) {
	return state->format->streams[packet->stream_index]->codec->codec_type;
}

int me_buffer_get_size(ME_BufferData *ad) { return ad->size; }
void *me_buffer_get_data(ME_BufferData *ad) { return ad->data;  }
ME_BufferData *me_buffer_alloc(int size) {
	ME_BufferData *buffer = av_malloc(sizeof(ME_BufferData));
	buffer->data8 = av_malloc(size);
	buffer->size = size;
	memset(buffer->data, 0, size);
	return buffer;
}
ME_BufferData *me_buffer_alloc_copy_data(unsigned char* data, int size) {
	ME_BufferData *buffer = me_buffer_alloc(size);
	memcpy(buffer->data8, data, size);
	return buffer;
}
void me_buffer_free(ME_BufferData *ad) {
	av_free(ad->data8);
	ad->size = 0;
	ad->data8 = NULL;
	av_free(ad);
}

int min(int a, int b) { return (a < b) ? a : b; }

ME_BufferData *me_packet_decode_audio(ME_DecodeState *state, AVPacket *packet, int channels, int orate) {
	AVFrame *frame = av_frame_alloc();
	ME_BufferData *buffer;
	int gotFrame = 0;
	int i = 0, ch = 0;
	short *ptr = NULL;
	int samples = 0;
	int irate = 0;
	void* ichannels[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	int consumed = 0;
	
	if (packet->size <= 0) return NULL;
	
	consumed = avcodec_decode_audio4(state->stream_audio->codec, frame, &gotFrame, packet);
	
	//if (gotFrame == 0 || consumed <= 0 && (packet->pos + consumed) >= packet->size) {
	if (gotFrame == 0 || consumed <= 0) {
		packet->data = NULL;
		packet->size = 0;
		//consumed = avcodec_decode_audio4(state->stream_audio->codec, frame, &gotFrame, packet);

		av_frame_free(&frame);
		return NULL;
	} else {
		packet->size -= consumed;
		packet->data += consumed;

		irate = frame->sample_rate;
		if (orate <= 0) orate = irate;
		samples = (frame->nb_samples * orate) / irate;
		buffer = me_buffer_alloc(channels * samples * sizeof(short));
		ptr = buffer->data16;
		for (i = 0; i < channels; i++) ichannels[i] = frame->data[min(i, frame->channels - 1)];
		
		//printf("\nSample rate: %d\n", frame->sample_rate);
	
		switch (frame->format) {
			case AV_SAMPLE_FMT_FLTP:
				for (i = 0; i < samples; i++) for (ch = 0; ch < channels; ch++) *ptr++ = ((float *)ichannels[ch])[(i * irate) / orate] * 32767;
				break;
			case AV_SAMPLE_FMT_S16P:
				for (i = 0; i < samples; i++) for (ch = 0; ch < channels; ch++) *ptr++ = ((short *)ichannels[ch])[(i * irate) / orate];
				break;
			default:
				av_log(NULL, AV_LOG_ERROR, "Invalid sample format #%u\n", frame->format);
				break;
		}
		av_frame_free(&frame);
		return buffer;
	}
}
