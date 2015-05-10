// https://libav.org/doxygen/release/0.8/libavcodec_2api-example_8c-example.html
// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decoding_encoding.c

#include <stdio.h>
#include "ffmpeg/ffmpeg.h"
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <emscripten.h>

typedef struct {
	AVPacket avpkt;
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *decoded_frame;
    int len;
    int data_size;
    int got_frame;
} ME_DecodeState;

void me_init() {
	EM_ASM(
		FS.mkdir('/working');
		FS.mount(NODEFS, { root: '.' }, '/working');
	);
	avcodec_register_all();
	av_register_all();
	
	//FILE* at3 = fopen("/working/bgm_001_64.at3", "rb");
	//printf("AT3FILE: %p\n", at3);
	{
		char *filename = "/working/bgm_001_64.at3";
	    int ret;
	    unsigned int i;
		AVFormatContext *ifmt_ctx;
        AVCodecContext *codec_ctx;

	    ifmt_ctx = NULL;
	    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
	        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
	        return;
	    }
	
	    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
	        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
	        return;
	    }
		
	    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
	        AVStream *stream;
	        stream = ifmt_ctx->streams[i];
	        codec_ctx = stream->codec;
	        /* Reencode video & audio and remux subtitles etc. */
	        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
	                || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
	            /* Open decoder */
	            ret = avcodec_open2(codec_ctx,
	                    avcodec_find_decoder(codec_ctx->codec_id), NULL);
	            if (ret < 0) {
	                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
	                return;
	            }
	        }
	    }
	
	    av_dump_format(ifmt_ctx, 0, filename, 0);
		
		{
			AVPacket pkt;
			AVFrame *frame;
			int gotFrame;
			frame = av_frame_alloc();
			av_read_frame(ifmt_ctx, &pkt);
			avcodec_decode_audio4(codec_ctx, frame, &gotFrame, &pkt);
			printf("%d\n", pkt.size);
			printf("frame: %d\n", frame->nb_samples);
			printf("channels: %d\n", codec_ctx->channels);
			printf("format: %s\n", av_get_sample_fmt_name(codec_ctx->sample_fmt));
			printf("bytes per sample: %d\n", av_get_bytes_per_sample(codec_ctx->sample_fmt));

			{
				AVFrame *oframe = av_frame_alloc();

				int64_t src_ch_layout = codec_ctx->channel_layout;
				int src_rate = codec_ctx->sample_rate;
				int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO;
				//int64_t dst_ch_layout = AV_CH_LAYOU;
				int dst_rate = 44100;
				enum AVSampleFormat src_sample_fmt = codec_ctx->sample_fmt;
				enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;

				SwrContext *swr_ctx = swr_alloc_set_opts(
						NULL, dst_ch_layout, dst_sample_fmt, dst_rate, src_ch_layout, src_sample_fmt, src_rate, 0, NULL
				);
				/*
				SwrContext *swr_ctx = swr_alloc();
				av_dict_set_int(swr_ctx, "in_channel_layout",    (int)src_ch_layout, 0);
				av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
				//av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);
				av_opt_set_int(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);
				av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
				av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
				//av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
				av_opt_set_int(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
				*/

				swr_init(swr_ctx);
				swr_convert_frame(swr_ctx, oframe, frame);

				printf("frame: %d\n", oframe->nb_samples);
				//swr_ctx->;
			}
		}
		
	}	
}

void me_show_codec_ids() {
	int n;
	printf("MP3: 0x%08X\n", CODEC_ID_MP3);
	printf("ATRAC3: 0x%08X\n", CODEC_ID_ATRAC3);
	printf("ATRAC3P: 0x%08X\n", CODEC_ID_ATRAC3P);
}

ME_DecodeState *me_audio_decode_alloc(int format) {
	ME_DecodeState *state = malloc(sizeof(ME_DecodeState));
	av_init_packet(&state->avpkt);
	state->codec = avcodec_find_decoder(format);
    state->c = avcodec_alloc_context3(state->codec);
	avcodec_open2(state->c, state->codec, NULL);
    state->decoded_frame = av_frame_alloc();
	return state;
}

void me_audio_decode_set_data(ME_DecodeState* state, void* dataPtr, int dataSize) {
    state->avpkt.data = dataPtr;
    state->avpkt.size = dataSize;
    //state->len = avcodec_decode_audio4(state->c, state->decoded_frame, &state->got_frame, &state->avpkt);
    //state->data_size = av_get_bytes_per_sample(state->c->sample_fmt);
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
	int channels = state->c->channels;
	int data_size = state->data_size;
	int len;
	
    len = avcodec_decode_audio4(state->c, state->decoded_frame, &state->got_frame, &state->avpkt);
    state->data_size = av_get_bytes_per_sample(state->c->sample_fmt);

	//uint8_t **data = state->decoded_frame->data;
	for (n = 0; n < samples; n++) {
		for (ch = 0; ch < channels; ch++) {
			*ptr2 = *(short*)(state->decoded_frame->data[ch] + data_size * n);
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
