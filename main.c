/*
 * main.c
 *
 *  Created on: 2013-10-24
 *      Author: hemiao
 */

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include "packet_queue.h"
#include "audio_thread.h"


#define SDL_AUDIO_BUFFER_SIZE 1024
packet_queue_t audio_queue;
int quit = 0;

int img_convert(AVPicture * dst, int dst_pix_fmt, const AVPicture * src,
		int src_pix_fmt, int src_width, int src_height)
{
	int w;
	int h;
	struct SwsContext *pSwsCtx;
	w = src_width;
	h = src_height;
	pSwsCtx = sws_getContext(w, h, src_pix_fmt, w, h, dst_pix_fmt, SWS_BICUBIC,
			NULL, NULL, NULL);
	sws_scale(pSwsCtx, src->data, src->linesize, 0, h, dst->data,
			dst->linesize);

	return 0;
}


int main(int argc, char * argv[])
{
	AVFormatContext		* ptr_format_ctx;
	AVCodecContext 		* video_codec_ctx;
	AVCodecContext 		* audio_codec_ctx;
	AVCodec				* video_codec;
	AVCodec				* audio_codec;
	AVFrame				* frame;
	AVPacket			packet;
	int 				i, audio_stream, video_stream;
	int 				got_frame;

	SDL_AudioSpec 		wanted_spec, spec;
	SDL_Overlay			* bmp;
	SDL_Surface 		* screen;
	SDL_Event			event;
	SDL_Rect			rect;


	if (argc < 2)
	{
		av_log(NULL, AV_LOG_PANIC, "Usage: test <file>\n");
		exit(1);
	}

	av_register_all();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		av_log(NULL, AV_LOG_PANIC, "Can't init SDL.\n");
		exit(1);
	}

	ptr_format_ctx = avformat_alloc_context();
	if (avformat_open_input(&ptr_format_ctx, argv[1], NULL, NULL))
	{
		av_log(NULL, AV_LOG_PANIC, "Can't open input file.\n");
		exit(1);
	}

	if (avformat_find_stream_info(ptr_format_ctx, NULL) < 0)
		return -1;

	av_dump_format(ptr_format_ctx, 0, argv[1], 0);


	audio_stream = -1;
	video_stream = -1;

	for (i = 0; i < ptr_format_ctx->nb_streams; i++)
	{
		if (ptr_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audio_stream = i;
		}
		if (ptr_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream = i;
		}
	}

	if (audio_stream == -1)
		return -1;

	if (video_stream == -1)
		return -1;

	audio_codec_ctx = ptr_format_ctx->streams[audio_stream]->codec;
	video_codec_ctx = ptr_format_ctx->streams[video_stream]->codec;

	wanted_spec.callback	= audio_callback;
	wanted_spec.channels	= audio_codec_ctx->channels;
	wanted_spec.format   	= AUDIO_S16SYS;
	wanted_spec.freq		= audio_codec_ctx->sample_rate;
	wanted_spec.samples		= SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.silence		= 0;
	wanted_spec.userdata	= audio_codec_ctx;

	if (SDL_OpenAudio(&wanted_spec, &spec) < 0)
	{
		av_log(NULL, AV_LOG_FATAL, "Can't open audio device.\n");
		return -1;
	}

	if (!(audio_codec = avcodec_find_decoder(audio_codec_ctx->codec_id)))
	{
		av_log(NULL, AV_LOG_FATAL, "Can't find audio decoder.\n");
		return -1;
	}

	if (!(video_codec = avcodec_find_decoder(video_codec_ctx->codec_id)))
	{
		av_log(NULL, AV_LOG_FATAL, "Can't find video decoder.\n");
		return -1;
	}

	if (avcodec_open2(audio_codec_ctx, audio_codec, NULL) < 0)
	{
		av_log(NULL, AV_LOG_FATAL, "Can't open audio stream.\n");
		return -1;
	}

	if (avcodec_open2(video_codec_ctx, video_codec, NULL) < 0)
	{
		av_log(NULL, AV_LOG_FATAL, "Can't open video stream.\n");
		return -1;
	}

	packet_queue_init(audio_queue);
	SDL_PauseAudio(0);

	frame = avcodec_alloc_frame();

#ifndef __DARWIN__
	screen = SDL_SetVideoMode(video_codec_ctx->width, video_codec_ctx->height,
			0, 0);
#else
	screen = SDL_SetVideoMode(video_codec_ctx->width, video_codec_ctx->height, 24, 0);
#endif

	bmp = SDL_CreateYUVOverlay(video_codec_ctx->width, video_codec_ctx->height, SDL_YV12_OVERLAY, screen);

	got_frame = 0;
	while (av_read_frame(ptr_format_ctx, &packet) >= 0)
	{
		if (packet.stream_index == video_stream)
		{
			avcodec_decode_video2(video_codec_ctx, frame, &got_frame, &packet);

			if (got_frame)
			{
				SDL_LockYUVOverlay(bmp);

				AVPicture pict;
				pict.data[0] = bmp->pixels[0];
				pict.data[1] = bmp->pixels[2];
				pict.data[2] = bmp->pixels[1];

				pict.linesize[0] = bmp->pitches[0];
				pict.linesize[1] = bmp->pitches[2];
				pict.linesize[2] = bmp->pitches[1];

				// Convert the image into YUV format that SDL uses
				img_convert(&pict, AV_PIX_FMT_YUV420P, (AVPicture *) frame,
						video_codec_ctx->pix_fmt, video_codec_ctx->width,
						video_codec_ctx->height);

				SDL_UnlockYUVOverlay(bmp);

				rect.x = 0;
				rect.y = 0;
				rect.w = video_codec_ctx->width;
				rect.h = video_codec_ctx->height;
				SDL_DisplayYUVOverlay(bmp, &rect);
				av_free_packet(&packet);
			}
		}
		else if (packet.stream_index == audio_stream)
		{
			packet_queue_put(&audio_queue, &packet);
		}
		else
		{
			av_free_packet(&packet);
		}

		SDL_PollEvent(&event);
		switch (event.type) {
		case SDL_QUIT:
			quit = 1;
			SDL_Quit();
			exit(0);
			break;
		default:
			break;
		}
	}

	avcodec_free_frame(&frame);

	// Close the codec
	avcodec_close(audio_codec_ctx);
	avcodec_close(video_codec_ctx);

	// Close the video file
	avformat_close_input(&ptr_format_ctx);



	return 0;
}
