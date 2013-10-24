/*
 * audio_thread.c
 *
 *  Created on: 2013-10-20
 *      Author: hemiao
 */

#include "audio_thread.h"
#include "packet_queue.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

extern int quit;
extern packet_queue_t audio_queue;

void audio_callback(void *userdata, Uint8 *stream, int len)
{
	AVCodecContext * pCodecCtx = (AVCodecContext *) userdata;
	int len1, decode_size;

	static uint8_t audio_buf[(CODEC_CAP_VARIABLE_FRAME_SIZE * 3) / 2];
	static unsigned int audio_buf_size = 0;
	static unsigned int audio_buf_index = 0;

	while (len > 0)
	{
		if (audio_buf_index >= audio_buf_size)
		{
			decode_size = audio_decode_frame(pCodecCtx, audio_buf, sizeof(audio_buf));
			if (decode_size < 0)
			{
				memset(audio_buf, 0, 1024);
				audio_buf_size = 1024;
			}
			else
			{
				audio_buf_size = decode_size;
			}
			audio_buf_index = 0;
		}
		len1 = audio_buf_size - audio_buf_index;
		if (len < len1)
			len1 = len;
		memcpy(stream, audio_buf, len1);
		len -= len1;
		audio_buf_index += len1;
		stream += len1;
	}
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf,
		int buf_size) {

	static AVPacket pkt;
	static uint8_t *audio_pkt_data = NULL;
	static int audio_pkt_size = 0;
	//AVFrame  frame;
	AVFrame * frame = NULL;

	int len1, got_data, data_size;

	frame = av_malloc(sizeof(AVFrame));

	for (;;) {
		while (audio_pkt_size > 0) {
			data_size = buf_size;
			//len1 = avcodec_decode_audio4(aCodecCtx, (int16_t *) audio_buf, &data_size, &pkt);
			len1 = avcodec_decode_audio4(aCodecCtx, frame, &got_data, &pkt);
			if (len1 < 0) {
				/* if error, skip frame */
				audio_pkt_size = 0;
				break;
			}
			if (got_data <= 0)
			{
				/* No data yet, get more frames */
				continue;
			}
			audio_pkt_data += len1;
			audio_pkt_size -= len1;

			/* We have data, return it and come back for more later */
			//memcpy(audio_buf, frame->basedata[0], frame.linesize[0]);
			data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(frame), frame->nb_samples, frame->format, 1);
			memcpy(audio_buf, frame->data[0], data_size);
			return data_size;
		}
		if (pkt.data)
			av_free_packet(&pkt);

		if (quit) {
			return -1;
		}

		if (packet_queue_get(&audio_queue, &pkt, 1) < 0) {
			return -1;
		}
		audio_pkt_data = pkt.data;
		audio_pkt_size = pkt.size;
	}

	return -1;
}

