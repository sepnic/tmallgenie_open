// Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __TMALLGENIE_VOICE_ENGNIE_ALSA_H__
#define __TMALLGENIE_VOICE_ENGNIE_ALSA_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    snd_pcm_t *pcm;
    snd_pcm_uframes_t chunk_size;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_format_t format;
    int sample_rate;
    size_t bits_per_sample;
    size_t bits_per_frame;
} GnLinux_Alsa_t;

GnLinux_Alsa_t *GnLinux_alsaOpen(snd_pcm_stream_t stream, int sampleRate, int channelCount, int bitsPerSample);

bool GnVoiceEngine_init();

bool GnVoiceEngine_recorderStart(int sampleRate, int channelCount, int bitsPerSample);

int GnVoiceEngine_recorderRead(void *buffer, unsigned int size);

void GnVoiceEngine_recorderStop();

#ifdef __cplusplus
}
#endif

#endif // __TMALLGENIE_VOICE_ENGNIE_ALSA_H__
