// Copyright (c) 2019-2023 Qinglong<sysu.zqlong@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <alsa/asoundlib.h>
#include "litevad.h"

#if 0
#define ALSA_DEVICE             "default"
#else
#define ALSA_DEVICE             "sysdefault"
#endif

#define RECORD_SAMPLE_RATE      16000
#define RECORD_SAMPLE_BIT       16
#define RECORD_CHANNEL_COUNT    1
#define RECORD_BUFFER_TIME      30 // in ms

int main()
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *hwparams = NULL;
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    uint32_t sample_rate = RECORD_SAMPLE_RATE;
    uint32_t channel_count = RECORD_CHANNEL_COUNT;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    uint32_t buffer_time, period_time;
    snd_pcm_uframes_t chunk_size, buffer_size;

    if (snd_pcm_open(&pcm, ALSA_DEVICE, stream, 0) < 0) {
        fprintf(stderr, "snd_pcm_open failed\n");
        goto __out;
    }

    if (snd_pcm_nonblock(pcm, 0) < 0) {
        fprintf(stderr, "snd_pcm_nonblock failed\n");
        goto __out;
    }

    snd_pcm_hw_params_alloca(&hwparams);
    if (snd_pcm_hw_params_any(pcm, hwparams) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_any failed\n");
        goto __out;
    }
    if (snd_pcm_hw_params_set_access(pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_access failed\n");
        goto __out;
    }
    if (snd_pcm_hw_params_set_format(pcm, hwparams, format) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_format failed\n");
        goto __out;
    }
    if (snd_pcm_hw_params_set_channels(pcm, hwparams, channel_count) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_channels failed\n");
        goto __out;
    }
    if (snd_pcm_hw_params_set_rate_near(pcm, hwparams, &sample_rate, 0) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_rate_near failed\n");
        goto __out;
    }
    if (sample_rate != RECORD_SAMPLE_RATE) {
        fprintf(stderr, "%d Hz is not supported by your hardware\n", RECORD_SAMPLE_RATE);
        goto __out;
    }
    if (snd_pcm_hw_params_get_buffer_time_max(hwparams, &buffer_time, 0) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_get_buffer_time_max failed\n");
        goto __out;
    }
    if (buffer_time > 500000)
        buffer_time = 500000;
    if (snd_pcm_hw_params_set_buffer_time_near(pcm, hwparams, &buffer_time, 0) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_buffer_time_near failed\n");
        goto __out;
    }
    period_time = buffer_time / 4;
    if (snd_pcm_hw_params_set_period_time_near(pcm, hwparams, &period_time, 0) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_period_time_near failed\n");
        goto __out;
    }
    if (snd_pcm_hw_params(pcm, hwparams) < 0) {
        fprintf(stderr, "snd_pcm_hw_params failed\n");
        goto __out;
    }

    snd_pcm_hw_params_get_period_size(hwparams, &chunk_size, 0);    
    snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
    if (chunk_size == buffer_size) {        
        fprintf(stderr, "Can't use period equal to buffer size\n");
        goto __out;
    }

    size_t bits_per_frame = snd_pcm_format_physical_width(format) * channel_count;
    int bytes_per_ms = (RECORD_SAMPLE_RATE/1000)*(RECORD_SAMPLE_BIT/8)*RECORD_CHANNEL_COUNT;
    int read_size = bytes_per_ms * RECORD_BUFFER_TIME;
    char *read_buffer = (char *)malloc(read_size);
    if (read_buffer == NULL)
        goto __out;

    litevad_handle_t vad_handle =
        litevad_create(RECORD_SAMPLE_RATE, RECORD_CHANNEL_COUNT, RECORD_SAMPLE_BIT);
    if (vad_handle == NULL) {
        fprintf(stderr, "litevad_create failed\n");
        free(read_buffer);
        goto __out;
    }

    while (1) {
        size_t frame_count = read_size*8/bits_per_frame;
        unsigned char *data = (unsigned char *)read_buffer;
        while (frame_count > 0) {
            ssize_t ret = snd_pcm_readi(pcm, data, frame_count);
            if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < frame_count)) {
                fprintf(stderr, "snd_pcm_wait\n");
                snd_pcm_wait(pcm, 1000);
            } else if (ret == -EPIPE) {
                fprintf(stderr, "Overrun\n");
                snd_pcm_prepare(pcm);
            } else if (ret == -ESTRPIPE) {
                fprintf(stderr, "Need suspend\n");
            } else if (ret < 0) {
                fprintf(stderr, "snd_pcm_readi failed: %s\n", snd_strerror(ret));
                continue;
            }
            if (ret > 0) {
                frame_count -= ret;
                data += (ret*bits_per_frame/8);
            }
        }

        litevad_result_t vad_state = litevad_process(vad_handle, read_buffer, read_size);
        switch (vad_state) {
        case LITEVAD_RESULT_SPEECH_BEGIN:
            fprintf(stdout, "speech begin\n");
            break;
        case LITEVAD_RESULT_SPEECH_END:
            fprintf(stdout, "speech end\n");
            break;
        case LITEVAD_RESULT_SPEECH_BEGIN_AND_END:
            fprintf(stdout, "speech begin and end\n");
            break;
        case LITEVAD_RESULT_FRAME_SILENCE:
            //fprintf(stdout, "frame silence\n");
            break;
        case LITEVAD_RESULT_FRAME_ACTIVE:
            //fprintf(stdout, "frame active\n");
            break;
        case LITEVAD_RESULT_ERROR:
            fprintf(stdout, "vad error\n");
            break;
        default:
            break;
        }
    }

    litevad_destroy(vad_handle);
    free(read_buffer);

__out:
    if (pcm != NULL)
        snd_pcm_close(pcm);
    return 0;
}
