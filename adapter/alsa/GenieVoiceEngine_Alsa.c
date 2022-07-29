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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "cutils/ringbuf.h"
#include "litevad.h"
#include "GenieSdk.h"
#include "GenieVoiceEngine_Alsa.h"
#if defined(GENIE_HAVE_SNOWBOY_KEYWORD_DETECT_ENABLED)
#include "snowboy-detect-c-wrapper.h"
static SnowboyDetect *sGnSnowboyDetect = NULL;
#endif

#define TAG "GenieVoiceEngnieAlsa"

#if 0
#define GENIE_LINUX_ALSA_DEVICE     "default"
#else
#define GENIE_LINUX_ALSA_DEVICE     "sysdefault"
#endif

// DO NOT MODIFY RECORD SETTINGS
#define GENIE_RECORD_SAMPLE_RATE        16000
#define GENIE_RECORD_SAMPLE_BIT         16
#define GENIE_RECORD_CHANNEL_COUNT      1
#define GENIE_RECORD_RINGBUF_SIZE       8192
#define GENIE_RECORD_READ_SIZE          1920
#define GENIE_RECORD_READ_TIMEOUT       3000 // ms

//resources/models/snowboy.umdl:
//    Universal model for the hotword "Snowboy".
//    Set SetSensitivity to "0.5" and ApplyFrontend to false.
//resources/models/alexa.umdl:
//    Universal model for the hotword "Alexa".
//    Set SetSensitivity to "0.6" and set ApplyFrontend to true.
//resources/models/jarvis.umdl:
//    Universal model for the hotword "Jarvis".
//    It has two different models for the hotword Jarvis,
//    so you have to use two sensitivites.
//    Set SetSensitivity to "0.8,0.8" and ApplyFrontend to true.
//resources/models/computer.umdl:
//    Universal model for the hotword "Computer".
//    Set SetSensitivity to "0.6" and ApplyFrontend to true.
// See https://github.com/Kitt-AI/snowboy#pretrained-universal-models
#define GENIE_SNOWBOY_RESOURCE_FILE     "common.res"
#define GENIE_SNOWBOY_MODEL_FILE        "jarvis.umdl"
#define GENIE_SNOWBOY_SENSIVIVITY       "0.8,0.8"
#define GENIE_SNOWBOY_AUDIO_GAIN        1.0
#define GENIE_SNOWBOY_APPLY_FRONTEND    true

static GnLinux_Alsa_t *sGnAlsa = NULL;
static ringbuf_handle sGnRingbuf = NULL;
static bool sGnIsRecording = false;
static char sGnRecordBuf[GENIE_RECORD_READ_SIZE];
static litevad_handle_t sGnVadHandle = NULL;
static bool sGnVadActive = false;

GnLinux_Alsa_t *GnLinux_alsaOpen(snd_pcm_stream_t stream, int sampleRate, int channelCount, int bitsPerSample)
{
    GnLinux_Alsa_t *alsa = (GnLinux_Alsa_t *)OS_CALLOC(1, sizeof(GnLinux_Alsa_t));
    if (alsa == NULL)
        return NULL;

    snd_pcm_hw_params_t *hwparams = NULL;
    uint32_t exact_rate = (uint32_t)sampleRate;
    uint32_t buffer_time, period_time;
    switch (bitsPerSample) {
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
        OS_LOGE(TAG, "Unsupported bitsPerSample: %d", bitsPerSample);
        goto fail_open;
        break;
    }

    if (snd_pcm_open(&alsa->pcm, GENIE_LINUX_ALSA_DEVICE, stream, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_open failed");
        goto fail_open;
    }

    if (snd_pcm_nonblock(alsa->pcm, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_nonblock failed");
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
    if (snd_pcm_hw_params_set_channels(alsa->pcm, hwparams, channelCount) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_channels failed");
        goto fail_open;
    }
    if (snd_pcm_hw_params_set_rate_near(alsa->pcm, hwparams, &exact_rate, 0) < 0) {
        OS_LOGE(TAG, "snd_pcm_hw_params_set_rate_near failed");
        goto fail_open;
    }
    if (sampleRate != exact_rate) {
        OS_LOGI(TAG, "%d Hz is not supported by your hardware, using %d Hz instead", 
            sampleRate, exact_rate);
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
    alsa->bits_per_frame = alsa->bits_per_sample * channelCount;
    alsa->sample_rate = sampleRate;

    return alsa;

fail_open:
    if (alsa->pcm != NULL)
        snd_pcm_close(alsa->pcm);
    OS_FREE(alsa);
    return NULL;
}

static void *GnVoiceEngine_Thread(void *arg)
{
    static GenieSdk_Callback_t *sdkCallback = NULL;
    while (1) {
        size_t frame_count = sizeof(sGnRecordBuf)*8/sGnAlsa->bits_per_frame;
        unsigned char *data = (unsigned char *)sGnRecordBuf;
        while (frame_count > 0) {
            ssize_t ret = snd_pcm_readi(sGnAlsa->pcm, data, frame_count);
            if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < frame_count)) {
                OS_LOGW(TAG, "snd_pcm_wait");
                snd_pcm_wait(sGnAlsa->pcm, 1000);
            } else if (ret == -EPIPE) {
                OS_LOGW(TAG, "Overrun");
                snd_pcm_prepare(sGnAlsa->pcm);
            } else if (ret == -ESTRPIPE) {
                OS_LOGW(TAG, "Need suspend");
            } else if (ret < 0) {
                OS_LOGE(TAG, "snd_pcm_readi failed: %s", snd_strerror(ret));
                continue;
            }
            if (ret > 0) {
                frame_count -= ret;
                data += ret * sGnAlsa->bits_per_frame / 8;
            }
        }

        if (sGnIsRecording) {
            litevad_result_t vad_state = litevad_process(sGnVadHandle, sGnRecordBuf, sizeof(sGnRecordBuf));
            if (sGnVadActive && vad_state == LITEVAD_RESULT_SPEECH_END) {
                OS_LOGI(TAG, "VAD state changed: speech >> silence, onMicphoneSilence");
                if (sdkCallback == NULL)
                    GenieSdk_Get_Callback(&sdkCallback);
                if (sdkCallback != NULL)
                    sdkCallback->onMicphoneSilence();
            }
            if (!sGnVadActive && vad_state == LITEVAD_RESULT_SPEECH_BEGIN)
                sGnVadActive = true;

            rb_write(sGnRingbuf, sGnRecordBuf, sizeof(sGnRecordBuf), GENIE_RECORD_READ_TIMEOUT);
        }
#if defined(GENIE_HAVE_SNOWBOY_KEYWORD_DETECT_ENABLED)
        else {
            if (sGnSnowboyDetect != NULL &&
                SnowboyDetectRunDetection(sGnSnowboyDetect,
                    (const int16_t* const)sGnRecordBuf, sizeof(sGnRecordBuf)/sizeof(short),
                    false) > 0) {
                OS_LOGI(TAG, "Hotword detect, onMicphoneWakeup");
                if (sdkCallback == NULL)
                    GenieSdk_Get_Callback(&sdkCallback);
                if (sdkCallback != NULL)
                    sdkCallback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);
            }
        }
#endif
    }
    return NULL;
}

bool GnVoiceEngine_init()
{
    sGnAlsa = GnLinux_alsaOpen(SND_PCM_STREAM_CAPTURE,
        GENIE_RECORD_SAMPLE_RATE, GENIE_RECORD_CHANNEL_COUNT, GENIE_RECORD_SAMPLE_BIT);
    if (sGnAlsa == NULL) {
        OS_LOGE(TAG, "GnLinux_alsaOpen failed");
        return false;
    }
    sGnRingbuf = rb_create(GENIE_RECORD_RINGBUF_SIZE);
    if (sGnRingbuf == NULL) {
        OS_LOGE(TAG, "rb_create failed");
        return false;
    }
    sGnVadHandle = litevad_create(GENIE_RECORD_SAMPLE_RATE, GENIE_RECORD_CHANNEL_COUNT);
    if (sGnVadHandle == NULL) {
        OS_LOGE(TAG, "litevad_create failed");
        return false;
    }
    pthread_t tid;
    if (pthread_create(&tid, NULL, GnVoiceEngine_Thread, NULL) != 0) {
        OS_LOGE(TAG, "pthread_create failed");
        return false;
    }

#if defined(GENIE_HAVE_SNOWBOY_KEYWORD_DETECT_ENABLED)
    // Initializes Snowboy detector.
    sGnSnowboyDetect = SnowboyDetectConstructor(GENIE_SNOWBOY_RESOURCE_FILE, GENIE_SNOWBOY_MODEL_FILE);
    SnowboyDetectSetSensitivity(sGnSnowboyDetect, GENIE_SNOWBOY_SENSIVIVITY);
    SnowboyDetectSetAudioGain(sGnSnowboyDetect, GENIE_SNOWBOY_AUDIO_GAIN);
    SnowboyDetectApplyFrontend(sGnSnowboyDetect, GENIE_SNOWBOY_APPLY_FRONTEND);
    if (SnowboyDetectSampleRate(sGnSnowboyDetect) != GENIE_RECORD_SAMPLE_RATE ||
        SnowboyDetectNumChannels(sGnSnowboyDetect) != GENIE_RECORD_CHANNEL_COUNT ||
        SnowboyDetectBitsPerSample(sGnSnowboyDetect) != GENIE_RECORD_SAMPLE_BIT) {
        OS_LOGE(TAG, "Record parameters not matched, abort voice trigger");
        SnowboyDetectDestructor(sGnSnowboyDetect);
        sGnSnowboyDetect = NULL;
    }
#endif
    return true;
}

bool GnVoiceEngine_recorderStart(int sampleRate, int channelCount, int bitsPerSample)
{
    if (sampleRate != GENIE_RECORD_SAMPLE_RATE ||
        channelCount != GENIE_RECORD_CHANNEL_COUNT ||
        bitsPerSample != GENIE_RECORD_SAMPLE_BIT) {
        OS_LOGE(TAG, "Invalid record parameters");
        return false;
    }
    litevad_reset(sGnVadHandle);
    rb_reset(sGnRingbuf);
    sGnVadActive = false;
    sGnIsRecording = true;
    return true;
}

int GnVoiceEngine_recorderRead(void *buffer, unsigned int size)
{
    return rb_read(sGnRingbuf, buffer, size, GENIE_RECORD_READ_TIMEOUT);
}

void GnVoiceEngine_recorderStop()
{
    sGnIsRecording = false;
    rb_reset(sGnRingbuf);
}
