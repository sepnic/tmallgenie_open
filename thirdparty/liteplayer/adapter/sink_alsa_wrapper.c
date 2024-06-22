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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "sink_alsa_wrapper.h"

#define TAG "[liteplayer]alsa"

struct alsa_wrapper {
    snd_pcm_t *pcm;
    snd_output_t *log;
    snd_pcm_uframes_t chunk_size;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_format_t format;
    size_t bits_per_sample;
    size_t bits_per_frame;
};

const char *alsa_wrapper_name()
{
    return "alsa";
}

sink_handle_t alsa_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening alsa: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    struct alsa_wrapper *alsa =
        (struct alsa_wrapper *)OS_CALLOC(1, sizeof(struct alsa_wrapper));
    if (alsa == NULL)
        return NULL;

    snd_pcm_hw_params_t *hwparams = NULL;
    uint32_t exact_rate = (uint32_t)samplerate;
    uint32_t buffer_time, period_time;
    switch (bits) {
    case 16:
        alsa->format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        alsa->format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        alsa->format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        OS_LOGE(TAG, "Unsupported sample bits: %d", bits);
        goto fail_open;
        break;
    }

    if (snd_output_stdio_attach(&alsa->log, stdout, 0) < 0) {
        OS_LOGE(TAG, "snd_output_stdio_attach failed");
        goto fail_open;
	}

    if (snd_pcm_open(&alsa->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_open failed");
        goto fail_open;
    }

    snd_pcm_hw_params_alloca(&hwparams);
    if (snd_pcm_hw_params_any(alsa->pcm, hwparams) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_any failed");
        goto fail_open;
    }
    if (snd_pcm_hw_params_set_access(alsa->pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_access failed");
        goto fail_open;
    }
    if (snd_pcm_hw_params_set_format(alsa->pcm, hwparams, alsa->format) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_format failed");
        goto fail_open;
    }
    if (snd_pcm_hw_params_set_channels(alsa->pcm, hwparams, channels) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_channels failed");
        goto fail_open;
    }
    if (snd_pcm_hw_params_set_rate_near(alsa->pcm, hwparams, &exact_rate, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_rate_near failed");
        goto fail_open;
    }
    if (samplerate != exact_rate) {
        OS_LOGI(TAG, "%d Hz is not supported by your hardware, using %d Hz instead", 
            samplerate, exact_rate);
    }
    if (snd_pcm_hw_params_get_buffer_time_max(hwparams, &buffer_time, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_get_buffer_time_max failed");
        goto fail_open;
    }
    if (buffer_time > 500000)
        buffer_time = 500000;
    if (snd_pcm_hw_params_set_buffer_time_near(alsa->pcm, hwparams, &buffer_time, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_buffer_time_near failed");
        goto fail_open;
    }
    period_time = buffer_time / 4;
    if (snd_pcm_hw_params_set_period_time_near(alsa->pcm, hwparams, &period_time, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_period_time_near failed");
        goto fail_open;
    }
    if (snd_pcm_hw_params(alsa->pcm, hwparams) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params failed");
        goto fail_open;
    }

    snd_pcm_hw_params_get_period_size(hwparams, &alsa->chunk_size, 0);    
    snd_pcm_hw_params_get_buffer_size(hwparams, &alsa->buffer_size);
    if (alsa->chunk_size == alsa->buffer_size) {        
        OS_LOGE(TAG, "Can't use period equal to buffer size");
        goto fail_open;
    }
    alsa->bits_per_sample = snd_pcm_format_physical_width(alsa->format);
    alsa->bits_per_frame = alsa->bits_per_sample * channels;

    snd_pcm_dump(alsa->pcm, alsa->log);
    return alsa;

fail_open:
    if (alsa->pcm != NULL)
        snd_pcm_close(alsa->pcm);
    if (alsa->log != NULL)
        snd_output_close(alsa->log);

    OS_FREE(alsa);
    return NULL;
}

int alsa_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    struct alsa_wrapper *alsa = (struct alsa_wrapper *)handle;

    size_t frame_count = (size_t)(size * 8 / alsa->bits_per_frame);
    unsigned char *data = (unsigned char *)buffer;
    ssize_t ret;
    while (frame_count > 0) {
        ret = snd_pcm_writei(alsa->pcm, data, frame_count);
        if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < frame_count)) {
            OS_LOGW(TAG, "snd_pcm_wait");
            snd_pcm_wait(alsa->pcm, 1000);
        } else if (ret == -EPIPE) {
            OS_LOGW(TAG, "Underrun");
            snd_pcm_prepare(alsa->pcm);
        } else if (ret == -ESTRPIPE) {
            OS_LOGW(TAG, "Need suspend");
        } else if (ret < 0) {
            OS_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(ret));
            return -1;
        }
        if (ret > 0) {
            frame_count -= ret;
            data += ret * alsa->bits_per_frame / 8;
        }
    }
    return size;
}

void alsa_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing alsa");
    struct alsa_wrapper *alsa = (struct alsa_wrapper *)handle;

    snd_output_close(alsa->log);
    snd_pcm_drain(alsa->pcm);
    snd_pcm_close(alsa->pcm);

    OS_FREE(alsa);
}
