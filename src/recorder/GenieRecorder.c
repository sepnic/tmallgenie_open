// Copyright (c) 2021-2022 Qinglong<sysu.zqlong@gmail.com>
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "osal/os_time.h"
#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "json/cJSON.h"

#include "GenieDefine.h"
#include "core/GenieService.h"
#include "GenieRecorder.h"

//#define GENIE_HAVE_SPEEXOGG_ENABLED
#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
#include "speex/speex.h"
#include "speex/speex_header.h"
#include "speex/speex_stereo.h"
#include "ogg/ogg.h"
#endif

#define TAG "GenieRecorder"

#define GENIE_RECORDER_THREAD_NAME      "GnRecorder"
#define GENIE_RECORDER_THREAD_PRIO      OS_THREAD_PRIO_HIGH
#define GENIE_RECORDER_THREAD_STACK     (4*1024)

#define GENIE_RECORDER_SAMPLE_RATE      16000
#define GENIE_RECORDER_CHANNEL_COUNT    1
#define GENIE_RECORDER_SAMPLE_BITS      16
#define GENIE_RECORDER_DURATION_MAX     15000 // 15s

#define GENIE_RECORDER_FRAME_MS         30
#define GENIE_RECORDER_FRAME_SIZE       (GENIE_RECORDER_SAMPLE_RATE/1000*GENIE_RECORDER_FRAME_MS)
#define GENIE_RECORDER_BUFFER_SIZE      (GENIE_RECORDER_FRAME_SIZE*GENIE_RECORDER_CHANNEL_COUNT*GENIE_RECORDER_SAMPLE_BITS/8)
#define GENIE_RECORDER_BYTES_PER_SECOND (GENIE_RECORDER_SAMPLE_RATE*GENIE_RECORDER_CHANNEL_COUNT*GENIE_RECORDER_SAMPLE_BITS/8)

#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
#define GENIE_RECORDER_SPEECH_FORMAT    GENIE_SPEECH_FORMAT_SPEEXOGG
#else
#define GENIE_RECORDER_SPEECH_FORMAT    GENIE_SPEECH_FORMAT_WAV
#endif

typedef struct {
    os_mutex stateLock;
    os_cond stateCond;
    os_thread thread;
    os_mutex threadLock;
    bool isThreadRunning;

    GnService_Callback_t *serviceCallback;
    GnVendor_PcmIn_t pcmIn;
    void *pcmHandle;
    char pcmBuffer[GENIE_RECORDER_BUFFER_SIZE];
    int pcmSize;
    unsigned long long recordTimestampMs;
    int recordDurationMs;
    bool isRecording;

    bool isNetworkConnected;
    bool isGatewayConnected;
    bool isAccountAuthorized;
    bool isSpeakerUnmuted;

#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
#define GENIE_SPEEX_QULITY      8
#define GENIE_SPEEX_COMPLEXITY  2
#define GENIE_SPEEX_MODEID      SPEEX_MODEID_NB
    void *speexEncodeHandle;
    SpeexBits speexBits;
    int speexFrameSize;
    int speexLookAhead;
    char speexEncodeBuffer[GENIE_RECORDER_BUFFER_SIZE];
    int speexEncodeSize;
    ogg_stream_state oggStream;
    ogg_page oggPage;
    ogg_packet oggPacket;
    char oggEncodeBuffer[GENIE_RECORDER_BUFFER_SIZE];
    int oggEncodeSize;
#endif
} GnRecorder_priv_t;

static GnRecorder_priv_t sGnRecorder;
static bool              sGnInited = false;

static void GnService_CommandListener(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    switch (command) {
    case GENIE_COMMAND_ExpectSpeechStart:
        if (sGnRecorder.isNetworkConnected && sGnRecorder.isAccountAuthorized &&
            sGnRecorder.isGatewayConnected && sGnRecorder.isSpeakerUnmuted) {
            os_mutex_lock(sGnRecorder.stateLock);
            sGnRecorder.isRecording = true;
            sGnRecorder.recordTimestampMs = os_monotonic_usec()/1000;
            sGnRecorder.recordDurationMs = 0;
            os_cond_signal(sGnRecorder.stateCond);
            os_mutex_unlock(sGnRecorder.stateLock);
        }
        break;
    case GENIE_COMMAND_ExpectSpeechStop:
    case GENIE_COMMAND_StopListen:
        sGnRecorder.isRecording = false;
        break;
    default:
        break;
    }
}

static void GnService_StatusListener(Genie_Status_t status)
{
    switch (status) {
    case GENIE_STATUS_NetworkDisconnected:
        sGnRecorder.isNetworkConnected = false;
        sGnRecorder.isRecording = false;
        break;
    case GENIE_STATUS_NetworkConnected:
        sGnRecorder.isNetworkConnected = true;
        break;
    case GENIE_STATUS_Unauthorized:
        sGnRecorder.isAccountAuthorized = false;
        sGnRecorder.isRecording = false;
        break;
    case GENIE_STATUS_Authorized:
        sGnRecorder.isAccountAuthorized = true;
        break;
    case GENIE_STATUS_GatewayDisconnected:
        sGnRecorder.isGatewayConnected = false;
        sGnRecorder.isRecording = false;
        break;
    case GENIE_STATUS_GatewayConnected:
        sGnRecorder.isGatewayConnected = true;
        break;
    case GENIE_STATUS_SpeakerUnmuted:
        sGnRecorder.isSpeakerUnmuted = true;
        break;
    case GENIE_STATUS_SpeakerMuted:
        sGnRecorder.isSpeakerUnmuted = false;
        sGnRecorder.isRecording = false;
        break;
    // don't response wakeup event, player will handle it and send GENIE_COMMAND_ExpectSpeech
    case GENIE_STATUS_MicphoneWakeup:
    case GENIE_STATUS_MicphoneStarted:
    case GENIE_STATUS_MicphoneStopped:
    default:
        break;
    }
}

#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
static bool GnRecorder_SpeexOgg_Init()
{
    spx_int32_t complexity = GENIE_SPEEX_COMPLEXITY;
    spx_int32_t quality = GENIE_SPEEX_QULITY;
    spx_int32_t rate = GENIE_RECORDER_SAMPLE_RATE;
    spx_int32_t channels = GENIE_RECORDER_CHANNEL_COUNT;
    const SpeexMode *mode = speex_lib_get_mode(GENIE_SPEEX_MODEID);

    if ((rate != 8000 && rate != 16000 && rate != 24000 && rate != 32000) ||
        (channels != 1 && channels != 2)) {
        OS_LOGE(TAG, "Unsupported sampling rate or channel count");
        return false;
    }

    if (sGnRecorder.speexEncodeHandle == NULL) {
        sGnRecorder.speexEncodeHandle = speex_encoder_init(mode);
        if (sGnRecorder.speexEncodeHandle == NULL) {
            OS_LOGE(TAG, "Failed to init speex encoder");
            return false;
        }
        speex_encoder_ctl(sGnRecorder.speexEncodeHandle, SPEEX_SET_COMPLEXITY, &complexity);
        speex_encoder_ctl(sGnRecorder.speexEncodeHandle, SPEEX_SET_QUALITY, &quality);
        speex_encoder_ctl(sGnRecorder.speexEncodeHandle, SPEEX_SET_SAMPLING_RATE, &rate);
        speex_encoder_ctl(sGnRecorder.speexEncodeHandle, SPEEX_GET_FRAME_SIZE, &sGnRecorder.speexFrameSize);
        speex_encoder_ctl(sGnRecorder.speexEncodeHandle, SPEEX_GET_LOOKAHEAD, &sGnRecorder.speexLookAhead);
        speex_bits_init(&sGnRecorder.speexBits);
    }

    if (ogg_stream_check(&sGnRecorder.oggStream) != 0) {
        ogg_stream_init(&sGnRecorder.oggStream, 0);
        if (ogg_stream_check(&sGnRecorder.oggStream) != 0) {
            OS_LOGE(TAG, "Failed to init ogg encoder");
            return false;
        }
    }
    ogg_stream_reset_serialno(&sGnRecorder.oggStream, os_monotonic_usec()/1000000);

    return true;
}

static void GnRecorder_SpeexOgg_Reset()
{
    ogg_stream_reset(&sGnRecorder.oggStream);
    speex_bits_reset(&sGnRecorder.speexBits);
}

static void GnRecorder_SpeexOgg_EncodeHeader()
{
    SpeexHeader header;
    const SpeexMode *mode = speex_lib_get_mode(GENIE_SPEEX_MODEID);
    speex_init_header(&header,
        GENIE_RECORDER_SAMPLE_RATE, GENIE_RECORDER_CHANNEL_COUNT, mode);
    int packetSize = 0;
    char *packet = speex_header_to_packet(&header, &packetSize);
    header.frames_per_packet = 1;
    header.vbr = 0;
    header.nb_channels = GENIE_RECORDER_CHANNEL_COUNT;
    sGnRecorder.oggPacket.packet = (unsigned char *)packet;
    sGnRecorder.oggPacket.bytes = packetSize;
    sGnRecorder.oggPacket.b_o_s = 1;
    sGnRecorder.oggPacket.e_o_s = 0;
    sGnRecorder.oggPacket.granulepos = 0;
    sGnRecorder.oggPacket.packetno = 0;
    ogg_stream_packetin(&sGnRecorder.oggStream, &sGnRecorder.oggPacket);
    speex_header_free(sGnRecorder.oggPacket.packet);

    sGnRecorder.oggEncodeSize = 0;
    while (ogg_stream_flush(&sGnRecorder.oggStream, &sGnRecorder.oggPage) != 0) {
        memcpy(&sGnRecorder.oggEncodeBuffer[sGnRecorder.oggEncodeSize],
                sGnRecorder.oggPage.header,
                sGnRecorder.oggPage.header_len);
        sGnRecorder.oggEncodeSize += sGnRecorder.oggPage.header_len;
        memcpy(&sGnRecorder.oggEncodeBuffer[sGnRecorder.oggEncodeSize],
                sGnRecorder.oggPage.body,
                sGnRecorder.oggPage.body_len);
        sGnRecorder.oggEncodeSize += sGnRecorder.oggPage.body_len;
    }
}

static void GnRecorder_SpeexOgg_EncodeStream(bool final)
{
    static int sId = 0;
    int totalBytes = sGnRecorder.pcmSize;
    int encodeBytes = 0;
    int frameBytes = sGnRecorder.speexFrameSize;

    frameBytes *= (GENIE_RECORDER_SAMPLE_BITS/8*GENIE_RECORDER_CHANNEL_COUNT);
    sGnRecorder.oggEncodeSize = 0;
    while (totalBytes >= frameBytes) {
        sId++;

        if (GENIE_RECORDER_CHANNEL_COUNT == 2)
            speex_encode_stereo_int((spx_int16_t *)&sGnRecorder.pcmBuffer[encodeBytes],
                                    sGnRecorder.speexFrameSize,
                                    &sGnRecorder.speexBits);
        speex_encode_int(sGnRecorder.speexEncodeHandle,
                         (spx_int16_t *)&sGnRecorder.pcmBuffer[encodeBytes],
                         &sGnRecorder.speexBits);
        speex_bits_insert_terminator(&sGnRecorder.speexBits);
        sGnRecorder.speexEncodeSize = speex_bits_write(&sGnRecorder.speexBits,
                                                       sGnRecorder.speexEncodeBuffer,
                                                       sizeof(sGnRecorder.speexEncodeBuffer));
        speex_bits_reset(&sGnRecorder.speexBits);

        sGnRecorder.oggPacket.packet = (unsigned char *)sGnRecorder.speexEncodeBuffer;
        sGnRecorder.oggPacket.bytes = sGnRecorder.speexEncodeSize;
        sGnRecorder.oggPacket.b_o_s = 0;
        sGnRecorder.oggPacket.e_o_s = final ? 1 : 0;
        sGnRecorder.oggPacket.granulepos = sId*sGnRecorder.speexFrameSize-sGnRecorder.speexLookAhead;
        sGnRecorder.oggPacket.packetno = sId;
        ogg_stream_packetin(&sGnRecorder.oggStream, &sGnRecorder.oggPacket);

        while (ogg_stream_flush(&sGnRecorder.oggStream, &sGnRecorder.oggPage) != 0) {
            memcpy(&sGnRecorder.oggEncodeBuffer[sGnRecorder.oggEncodeSize],
                    sGnRecorder.oggPage.header,
                    sGnRecorder.oggPage.header_len);
            sGnRecorder.oggEncodeSize += sGnRecorder.oggPage.header_len;
            memcpy(&sGnRecorder.oggEncodeBuffer[sGnRecorder.oggEncodeSize],
                    sGnRecorder.oggPage.body,
                    sGnRecorder.oggPage.body_len);
            sGnRecorder.oggEncodeSize += sGnRecorder.oggPage.body_len;
        }

        encodeBytes += frameBytes;
        totalBytes -= frameBytes;
    }

    if (totalBytes > 0)
        OS_LOGE(TAG, "Insufficent data to encode whole frame, discard %d bytes", totalBytes);

    if (final)
        sId = 0;
}
#endif

static void *GnRecorder_Thread_Entry(void *arg)
{
    OS_LOGD(TAG, "GenieRecorder thread enter");
    while (sGnRecorder.isThreadRunning) {
        os_mutex_lock(sGnRecorder.stateLock);
        while (sGnRecorder.isThreadRunning && !sGnRecorder.isRecording)
            os_cond_wait(sGnRecorder.stateCond, sGnRecorder.stateLock);
        os_mutex_unlock(sGnRecorder.stateLock);

        while (sGnRecorder.isThreadRunning && sGnRecorder.isRecording) {
            if (sGnRecorder.pcmHandle == NULL) {
#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
                if (GnRecorder_SpeexOgg_Init())
#endif
                /**/sGnRecorder.pcmHandle = sGnRecorder.pcmIn.open(
                            GENIE_RECORDER_SAMPLE_RATE,
                            GENIE_RECORDER_CHANNEL_COUNT,
                            GENIE_RECORDER_SAMPLE_BITS);

#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
                if (sGnRecorder.pcmHandle != NULL) {
                    GnRecorder_SpeexOgg_EncodeHeader();
                    sGnRecorder.serviceCallback->onMicphoneStreaming(
                            GENIE_RECORDER_SPEECH_FORMAT,
                            sGnRecorder.oggEncodeBuffer, sGnRecorder.oggEncodeSize,
                            false);
                }
#endif
            }
            if (sGnRecorder.pcmHandle != NULL) {
                sGnRecorder.pcmSize = sGnRecorder.pcmIn.read(sGnRecorder.pcmHandle,
                        sGnRecorder.pcmBuffer, sizeof(sGnRecorder.pcmBuffer));
                if (sGnRecorder.pcmSize > 0) {
#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
                    GnRecorder_SpeexOgg_EncodeStream(false);
                    sGnRecorder.serviceCallback->onMicphoneStreaming(
                            GENIE_RECORDER_SPEECH_FORMAT,
                            sGnRecorder.oggEncodeBuffer, sGnRecorder.oggEncodeSize,
                            false);
#else
                    sGnRecorder.serviceCallback->onMicphoneStreaming(
                        GENIE_RECORDER_SPEECH_FORMAT, sGnRecorder.pcmBuffer, sGnRecorder.pcmSize, false);
#endif
                    sGnRecorder.recordDurationMs += sGnRecorder.pcmSize*1000/GENIE_RECORDER_BYTES_PER_SECOND;
                } else {
                    OS_LOGE(TAG, "Failed to read PcmIn");
                    os_thread_sleep_msec(100);
                }
            } else {
                OS_LOGE(TAG, "Failed to open PcmIn");
                os_thread_sleep_msec(100);
            }

            if (os_monotonic_usec()/1000 - sGnRecorder.recordTimestampMs >= GENIE_RECORDER_DURATION_MAX ||
                sGnRecorder.recordDurationMs >= GENIE_RECORDER_DURATION_MAX) {
                OS_LOGW(TAG, "Reach max duration, stop recording forcely");
                sGnRecorder.isRecording = false;
            }
        }

        // final streaming
        if (sGnRecorder.pcmHandle != NULL) {
            sGnRecorder.pcmSize = sGnRecorder.pcmIn.read(
                sGnRecorder.pcmHandle, sGnRecorder.pcmBuffer, sizeof(sGnRecorder.pcmBuffer));
            if (sGnRecorder.pcmSize != sizeof(sGnRecorder.pcmBuffer)) {
                memset(sGnRecorder.pcmBuffer, 0x0, sizeof(sGnRecorder.pcmBuffer));
                sGnRecorder.pcmSize = sizeof(sGnRecorder.pcmBuffer);
            }
#if defined(GENIE_HAVE_SPEEXOGG_ENABLED)
            GnRecorder_SpeexOgg_EncodeStream(true);
            sGnRecorder.serviceCallback->onMicphoneStreaming(
                    GENIE_RECORDER_SPEECH_FORMAT,
                    sGnRecorder.oggEncodeBuffer, sGnRecorder.oggEncodeSize,
                    true);
            GnRecorder_SpeexOgg_Reset();
#else
            sGnRecorder.serviceCallback->onMicphoneStreaming(
                GENIE_RECORDER_SPEECH_FORMAT, sGnRecorder.pcmBuffer, sGnRecorder.pcmSize, true);
#endif
            sGnRecorder.pcmIn.close(sGnRecorder.pcmHandle);
            sGnRecorder.pcmHandle = NULL;
        }
    }
    OS_LOGD(TAG, "GenieRecorder thread leave");
    return NULL;
}

bool GnRecorder_Init(GnVendor_PcmIn_t *pcmIn)
{
    if (sGnInited) return true;
    if (pcmIn == NULL || pcmIn->open == NULL || pcmIn->read == NULL || pcmIn->close == NULL)
        return NULL;
    memset(&sGnRecorder, 0x0, sizeof(sGnRecorder));
    sGnRecorder.pcmIn.open = pcmIn->open;
    sGnRecorder.pcmIn.read = pcmIn->read;
    sGnRecorder.pcmIn.close = pcmIn->close;
    sGnRecorder.isNetworkConnected = true;
    sGnRecorder.isGatewayConnected = true;
    sGnRecorder.isAccountAuthorized = true;
    sGnRecorder.isSpeakerUnmuted = true;
    if ((sGnRecorder.threadLock = os_mutex_create()) == NULL)
        goto __error_init;
    if ((sGnRecorder.stateLock = os_mutex_create()) == NULL)
        goto __error_init;
    if ((sGnRecorder.stateCond = os_cond_create()) == NULL)
        goto __error_init;
    sGnInited = true;
    return true;
__error_init:
    if (sGnRecorder.stateCond != NULL)
        os_cond_destroy(sGnRecorder.stateCond);
    if (sGnRecorder.stateLock != NULL)
        os_mutex_destroy(sGnRecorder.stateLock);
    if (sGnRecorder.threadLock != NULL)
        os_mutex_destroy(sGnRecorder.threadLock);
    return false;
}

bool GnRecorder_IsInit()
{
    return sGnInited;
}

bool GnRecorder_Start()
{
    if (!GnRecorder_IsInit() || !GnService_IsInit()) {
        OS_LOGE(TAG, "Genie Service/Recorder is NOT inited");
        return false;
    }
    os_mutex_lock(sGnRecorder.threadLock);
    if (sGnRecorder.isThreadRunning && sGnRecorder.thread != NULL) {
        OS_LOGD(TAG, "Genie Recorder already started");
        os_mutex_unlock(sGnRecorder.threadLock);
        return true;
    }
    if (!GnService_Get_Callback(&sGnRecorder.serviceCallback))
        goto __error_start;
    if (!GnService_Register_CommandListener(GnService_CommandListener))
        goto __error_start;
    if (!GnService_Register_StatusListener(GnService_StatusListener))
        goto __error_start;
    struct os_thread_attr attr = {
        .name = GENIE_RECORDER_THREAD_NAME,
        .priority = GENIE_RECORDER_THREAD_PRIO,
        .stacksize = GENIE_RECORDER_THREAD_STACK,
        .joinable = true,
    };
    sGnRecorder.isThreadRunning = true;
    sGnRecorder.thread = os_thread_create(&attr, GnRecorder_Thread_Entry, NULL);
    if (sGnRecorder.thread == NULL)
        goto __error_start;
    os_mutex_unlock(sGnRecorder.threadLock);
    return true;

__error_start:
    GnService_Unregister_CommandListener(GnService_CommandListener);
    GnService_Unregister_StatusListener(GnService_StatusListener);
    sGnRecorder.isThreadRunning = false;
    os_mutex_unlock(sGnRecorder.threadLock);
    return false;
}

bool GnRecorder_IsActive()
{
    return sGnRecorder.isThreadRunning;
}

void GnRecorder_Stop()
{
    os_mutex_lock(sGnRecorder.threadLock);
    GnService_Unregister_CommandListener(GnService_CommandListener);
    GnService_Unregister_StatusListener(GnService_StatusListener);
    if (sGnRecorder.thread != NULL) {
        os_mutex_lock(sGnRecorder.stateLock);
        sGnRecorder.isThreadRunning = false;
        sGnRecorder.isRecording = false;
        os_cond_signal(sGnRecorder.stateCond);
        os_mutex_unlock(sGnRecorder.stateLock);

        os_thread_join(sGnRecorder.thread, NULL);
        sGnRecorder.thread = NULL;
    }
    os_mutex_unlock(sGnRecorder.threadLock);
}
