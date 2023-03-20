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
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "cutils/ringbuf.h"
#include "litevad.h"
#include "GenieSdk.h"
#include "portaudio.h"
#include "GenieVoiceEngine_PortAudio.h"
#if defined(GENIE_HAVE_SNOWBOY_KEYWORD_DETECT_ENABLED)
#include "snowboy-detect-c-wrapper.h"
static SnowboyDetect *sGnSnowboyDetect = NULL;
#endif

#define TAG "GenieVoiceEngniePortAudio"

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

static ringbuf_handle sGnRingbuf = NULL;
static bool sGnIsRecording = false;
static litevad_handle_t sGnVadHandle = NULL;
static bool sGnVadActive = false;

static int GnVoiceEngine_inStreamCallback(const void *input, void *output,
    unsigned long frame_count, const PaStreamCallbackTimeInfo *time_info,
    PaStreamCallbackFlags status_flags, void *user_data)
{
    static GenieSdk_Callback_t *sdkCallback = NULL;
    unsigned long nbytes = frame_count*GENIE_RECORD_CHANNEL_COUNT*GENIE_RECORD_SAMPLE_BIT/8;
    if (sGnIsRecording) {
        litevad_result_t vad_state = litevad_process(sGnVadHandle, input, nbytes);
        if (sGnVadActive && vad_state == LITEVAD_RESULT_SPEECH_END) {
            OS_LOGI(TAG, "VAD state changed: speech >> silence, onMicphoneSilence");
            if (sdkCallback == NULL)
                GenieSdk_Get_Callback(&sdkCallback);
            if (sdkCallback != NULL)
                sdkCallback->onMicphoneSilence();
        }
        if (!sGnVadActive && vad_state == LITEVAD_RESULT_SPEECH_BEGIN)
            sGnVadActive = true;

        if (rb_bytes_available(sGnRingbuf) >= nbytes)
            rb_write(sGnRingbuf, (char *)input, nbytes, GENIE_RECORD_READ_TIMEOUT);
        else
            OS_LOGW(TAG, "Insufficient available space in ringbuf, discard current frame");
    }
#if defined(GENIE_HAVE_SNOWBOY_KEYWORD_DETECT_ENABLED)
    else {
        if (sGnSnowboyDetect != NULL &&
            SnowboyDetectRunDetection(sGnSnowboyDetect,
                (const int16_t* const)input, frame_count, false) > 0) {
            OS_LOGI(TAG, "Hotword detect, onMicphoneWakeup");
            if (sdkCallback == NULL)
                GenieSdk_Get_Callback(&sdkCallback);
            if (sdkCallback != NULL)
                sdkCallback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);
        }
    }
#endif
    return paContinue;
}

bool GnVoiceEngine_init()
{
    sGnRingbuf = rb_create(GENIE_RECORD_RINGBUF_SIZE);
    if (sGnRingbuf == NULL) {
        OS_LOGE(TAG, "rb_create failed");
        return false;
    }
    sGnVadHandle = litevad_create(GENIE_RECORD_SAMPLE_RATE, GENIE_RECORD_CHANNEL_COUNT, GENIE_RECORD_SAMPLE_BIT);
    if (sGnVadHandle == NULL) {
        OS_LOGE(TAG, "litevad_create failed");
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

    if (Pa_Initialize() != paNoError) {
        OS_LOGE(TAG, "Pa_Initialize failed");
        return false;
    }
    unsigned long framesNeed =
        GENIE_RECORD_READ_SIZE/(GENIE_RECORD_CHANNEL_COUNT*GENIE_RECORD_SAMPLE_BIT/8);
    PaStream *inStream = NULL;
    PaStreamParameters inParameters;
    inParameters.device = Pa_GetDefaultInputDevice();
    if (inParameters.device == paNoDevice)
        return false;
    inParameters.channelCount = GENIE_RECORD_CHANNEL_COUNT;
    switch (GENIE_RECORD_SAMPLE_BIT) {
    case 32:
        inParameters.sampleFormat = paInt32;
        break;
    case 24:
        inParameters.sampleFormat = paInt24;
        break;
    case 16:
    default:
        inParameters.sampleFormat = paInt16;
        break;
    }
    inParameters.suggestedLatency =
            Pa_GetDeviceInfo(inParameters.device)->defaultLowInputLatency;
    inParameters.hostApiSpecificStreamInfo = NULL;
    if (Pa_OpenStream(&inStream, &inParameters, NULL,
            GENIE_RECORD_SAMPLE_RATE, framesNeed, paFramesPerBufferUnspecified,
            GnVoiceEngine_inStreamCallback, NULL) != paNoError)
        return false;
    if (Pa_StartStream(inStream) != paNoError)
        return false;
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
