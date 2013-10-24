/*
 * audio_thread.h
 *
 *  Created on: 2013-10-20
 *      Author: hemiao
 */

#ifndef AUDIO_THREAD_H
#define AUDIO_THREAD_H

#include <libavformat/avformat.h>
#include <SDL/SDL.h>

void audio_callback(void *userdata, Uint8 *stream, int len);
int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size);

#endif
