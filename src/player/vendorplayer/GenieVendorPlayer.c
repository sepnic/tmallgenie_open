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

#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "liteplayer_main.h"
#include "liteplayer_ttsplayer.h"
#include "source_httpclient_wrapper.h"
#include "source_file_wrapper.h"
#include "GenieVendorPlayer.h"

#define TAG "GenieVendorPlayer"

#define GENIE_TTS_PLAYER_RINGBUF_SIZE       (32*1024)
#define GENIE_PROMPT_PLAYER_RINGBUF_SIZE    (32*1024)
#define GENIE_MUSIC_PLAYER_RINGBUF_SIZE     (256*1024)

typedef struct {
    char *base;
    long length;
    long offset;
} GnVendorPlayer_Prebuilt_t;

typedef struct {
    GnPlayer_Stream_t stream;
    liteplayer_handle_t urlPlayer;
    ttsplayer_handle_t ttsPlayer;
    void (*upperListener)(GnPlayer_Stream_t stream, GnPlayer_State_t state);
    GnPlayer_State_t upperState;
    GnVendorPlayer_Prebuilt_t prebuilt;
    bool hasCompleted;
} GnVendorPlayer_Priv_t;

#include "prebuilt_prompt_WAKEUP_REMIND.c"
#include "prebuilt_prompt_RECORD_REMIND.c"
#include "prebuilt_prompt_NETWORK_DISCONNECTED.c"
#include "prebuilt_prompt_SERVER_DISCONNECTED.c"
#include "prebuilt_prompt_ACCOUNT_UNAUTHORIZED.c"

static GnPlayer_Adapter_t sGnVendorPlayer;
static GnVendor_PcmOut_t  sGnVendorPcmOut;
static bool               sGnInited = false;

static int GnVendorPlayer_StateListener(enum liteplayer_state state, int errcode, void *priv)
{
    GnVendorPlayer_Priv_t *handle = (GnVendorPlayer_Priv_t *)priv;
    bool needSync = true;

    switch (state) {
    case LITEPLAYER_IDLE:
        //OS_LOGD(TAG, "-->LITEPLAYER_IDLE");
        handle->upperState = GENIE_PLAYER_STATE_IDLE;
        handle->hasCompleted = false;
        break;
    case LITEPLAYER_INITED:
        //OS_LOGD(TAG, "-->LITEPLAYER_INITED");
        needSync = false;
        break;
    case LITEPLAYER_PREPARED:
        //OS_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        handle->upperState = GENIE_PLAYER_STATE_PREPARED;
        break;
    case LITEPLAYER_STARTED:
        //OS_LOGD(TAG, "-->LITEPLAYER_STARTED");
        handle->upperState = GENIE_PLAYER_STATE_STARTED;
        break;
    case LITEPLAYER_PAUSED:
        //OS_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        handle->upperState = GENIE_PLAYER_STATE_PAUSED;
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        //OS_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        handle->upperState = GENIE_PLAYER_STATE_NEARLYCOMPLETED;
        break;
    case LITEPLAYER_COMPLETED:
        //OS_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        handle->upperState = GENIE_PLAYER_STATE_COMPLETED;
        handle->hasCompleted = true;
        break;
    case LITEPLAYER_STOPPED:
        //OS_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        handle->upperState = GENIE_PLAYER_STATE_STOPPED;
        if (!handle->hasCompleted)
            needSync = false;
        break;
    case LITEPLAYER_ERROR:
        OS_LOGE(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        handle->upperState = GENIE_PLAYER_STATE_ERROR;
        handle->hasCompleted = true;
        break;
    default:
        needSync = false;
        break;
    }

    if (needSync && handle->upperListener != NULL)
        handle->upperListener(handle->stream, handle->upperState);
    return 0;
}

static const char *GnVendorPlayer_PrebuiltUrlProtocol()
{
    return "prebuilt";
}

static source_handle_t GnVendorPlayer_PrebuiltOpen(const char *url, long long content_pos, void *priv_data)
{
    OS_LOGD(TAG, "Opening prebuilt source, url: %s, content_pos: %d", url, (int)content_pos);
    GnVendorPlayer_Prebuilt_t *prebuilt = (GnVendorPlayer_Prebuilt_t *)priv_data;
    if (strcmp(url, GENIE_PREBUILT_WAKEUP_REMIND) == 0) {
        prebuilt->base = (char *)(&WAKEUP_REMIND[0]);
        prebuilt->length = sizeof(WAKEUP_REMIND);
    } else if (strcmp(url, GENIE_PREBUILT_RECORD_REMIND) == 0) {
        prebuilt->base = (char *)(&RECORD_REMIND[0]);
        prebuilt->length = sizeof(RECORD_REMIND);
    } else if (strcmp(url, GENIE_PREBUILT_NETWORK_DISCONNECTED) == 0) {
        prebuilt->base = (char *)(&NETWORK_DISCONNECTED[0]);
        prebuilt->length = sizeof(NETWORK_DISCONNECTED);
    } else if (strcmp(url, GENIE_PREBUILT_SERVER_DISCONNECTED) == 0) {
        prebuilt->base = (char *)(&SERVER_DISCONNECTED[0]);
        prebuilt->length = sizeof(SERVER_DISCONNECTED);
    } else if (strcmp(url, GENIE_PREBUILT_ACCOUNT_UNAUTHORIZED) == 0) {
        prebuilt->base = (char *)(&ACCOUNT_UNAUTHORIZED[0]);
        prebuilt->length = sizeof(ACCOUNT_UNAUTHORIZED);
    } else {
        OS_LOGE(TAG, "Invalid prebuilt source");
        return NULL;
    }
    prebuilt->offset = (long)content_pos;
    if (prebuilt->offset > prebuilt->length) {
        memset(prebuilt, 0x0, sizeof(GnVendorPlayer_Prebuilt_t));
        return NULL;
    }
    return prebuilt;
}

static int GnVendorPlayer_PrebuiltRead(source_handle_t handle, char *buffer, int size)
{
    GnVendorPlayer_Prebuilt_t *prebuilt = (GnVendorPlayer_Prebuilt_t *)handle;
    if (prebuilt->offset + size > prebuilt->length)
        size = prebuilt->length - prebuilt->offset;
    if (size > 0) {
        memcpy(buffer, prebuilt->base+prebuilt->offset, size);
        prebuilt->offset += size;
    }
    return size;
}

static long long GnVendorPlayer_PrebuiltContentPos(source_handle_t handle)
{
    GnVendorPlayer_Prebuilt_t *prebuilt = (GnVendorPlayer_Prebuilt_t *)handle;
    return prebuilt->offset;
}

static long long GnVendorPlayer_PrebuiltContentLen(source_handle_t handle)
{
    GnVendorPlayer_Prebuilt_t *prebuilt = (GnVendorPlayer_Prebuilt_t *)handle;
    return prebuilt->length;
}

static int GnVendorPlayer_PrebuiltSeek(source_handle_t handle, long offset)
{
    GnVendorPlayer_Prebuilt_t *prebuilt = (GnVendorPlayer_Prebuilt_t *)handle;
    if (offset > prebuilt->length)
        return -1;
    prebuilt->offset = offset;
    return 0;
}

static void GnVendorPlayer_PrebuiltClose(source_handle_t handle)
{
    OS_LOGD(TAG, "Closing prebuilt source");
    GnVendorPlayer_Prebuilt_t *prebuilt = (GnVendorPlayer_Prebuilt_t *)handle;
    memset(prebuilt, 0x0, sizeof(GnVendorPlayer_Prebuilt_t));
}

static const char *GnVendorPlayer_SinkName()
{
    return "GeniePcmOut";
}

static sink_handle_t GnVendorPlayer_SinkOpen(int samplerate, int channels, int bits, void *priv_data)
{
    void *handle = sGnVendorPcmOut.open(samplerate, channels, bits);
    return (sink_handle_t)handle;
}

static int GnVendorPlayer_SinkWrite(sink_handle_t handle, char *buffer, int size)
{
    return sGnVendorPcmOut.write((void *)handle, buffer, size);
}

static void GnVendorPlayer_SinkClose(sink_handle_t handle)
{
    sGnVendorPcmOut.close((void *)handle);
}

static void *GnVendorPlayer_Create(GnPlayer_Stream_t stream)
{
    GnVendorPlayer_Priv_t *priv = OS_CALLOC(1, sizeof(GnVendorPlayer_Priv_t));
    if (priv == NULL) return NULL;

    struct sink_wrapper sinkOps = {
        .priv_data = NULL,
        .name = GnVendorPlayer_SinkName,
        .open = GnVendorPlayer_SinkOpen,
        .write = GnVendorPlayer_SinkWrite,
        .close = GnVendorPlayer_SinkClose,
    };

    if (stream == GENIE_PLAYER_STREAM_TTS) {
        struct ttsplayer_cfg cfg = {
            .ringbuf_size = GENIE_TTS_PLAYER_RINGBUF_SIZE,
        };
        priv->ttsPlayer = ttsplayer_create(&cfg);
        if (priv->ttsPlayer == NULL) goto __error_create;
        ttsplayer_register_sink_wrapper(priv->ttsPlayer, &sinkOps);
    } else {
        priv->urlPlayer = liteplayer_create();
        if (priv->urlPlayer == NULL) goto __error_create;
        liteplayer_register_sink_wrapper(priv->urlPlayer, &sinkOps);

        struct source_wrapper prebuiltOps = {
            .async_mode = false,
            .buffer_size = 2048,
            .priv_data = &priv->prebuilt,
            .url_protocol = GnVendorPlayer_PrebuiltUrlProtocol,
            .open = GnVendorPlayer_PrebuiltOpen,
            .read = GnVendorPlayer_PrebuiltRead,
            .content_pos = GnVendorPlayer_PrebuiltContentPos,
            .content_len = GnVendorPlayer_PrebuiltContentLen,
            .seek = GnVendorPlayer_PrebuiltSeek,
            .close = GnVendorPlayer_PrebuiltClose,
        };
        liteplayer_register_source_wrapper(priv->urlPlayer, &prebuiltOps);

        struct source_wrapper fileOps = {
            .async_mode = false,
            .buffer_size = 2048,
            .priv_data = NULL,
            .url_protocol = file_wrapper_url_protocol,
            .open = file_wrapper_open,
            .read = file_wrapper_read,
            .content_pos = file_wrapper_content_pos,
            .content_len = file_wrapper_content_len,
            .seek = file_wrapper_seek,
            .close = file_wrapper_close,
        };
        liteplayer_register_source_wrapper(priv->urlPlayer, &fileOps);

        int httpRingbufSize = GENIE_PROMPT_PLAYER_RINGBUF_SIZE;
        if (stream == GENIE_PLAYER_STREAM_MUSIC)
            httpRingbufSize = GENIE_MUSIC_PLAYER_RINGBUF_SIZE;
        struct source_wrapper httpOps = {
            .async_mode = true,
            .buffer_size = httpRingbufSize,
            .priv_data = NULL,
            .url_protocol = httpclient_wrapper_url_protocol,
            .open = httpclient_wrapper_open,
            .read = httpclient_wrapper_read,
            .content_pos = httpclient_wrapper_content_pos,
            .content_len = httpclient_wrapper_content_len,
            .seek = httpclient_wrapper_seek,
            .close = httpclient_wrapper_close,
        };
        liteplayer_register_source_wrapper(priv->urlPlayer, &httpOps);
        // todo: add prebuilt source for WAKEUP_REMIND/RECORD_REMIND tones
    }

    priv->stream = stream;
    return priv;

__error_create:
    OS_FREE(priv);
    return NULL;
}

static bool GnVendorPlayer_RegisterStateListener(void *handle, void (*listener)(GnPlayer_Stream_t stream, GnPlayer_State_t state))
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || listener == NULL)
        return false;
    int ret = 0;
    priv->upperListener = listener;
    if (priv->stream == GENIE_PLAYER_STREAM_TTS)
        ret = ttsplayer_register_state_listener(priv->ttsPlayer, GnVendorPlayer_StateListener, priv);
    else
        ret = liteplayer_register_state_listener(priv->urlPlayer, GnVendorPlayer_StateListener, priv);
    return ret == 0;
}

static bool GnVendorPlayer_SetDataSource(void *handle, const char *url)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL)
        return false;
    int ret = 0;
    if (priv->stream != GENIE_PLAYER_STREAM_TTS)
        ret = liteplayer_set_data_source(priv->urlPlayer, url);
    return ret == 0;
}

static bool GnVendorPlayer_PrepareAsync(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL)
        return false;
    int ret = 0;
    if (priv->stream == GENIE_PLAYER_STREAM_TTS)
        ret = ttsplayer_prepare_async(priv->ttsPlayer);
    else
        ret = liteplayer_prepare_async(priv->urlPlayer);
    return ret == 0;
}

static bool GnVendorPlayer_Write(void *handle, char *buffer, int size, bool final)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || priv->stream != GENIE_PLAYER_STREAM_TTS)
        return false;
    int ret = ttsplayer_write(priv->ttsPlayer, buffer, size, final);
    return ret >= 0;
}

static bool GnVendorPlayer_Start(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL)
        return false;
    int ret = 0;
    if (priv->stream == GENIE_PLAYER_STREAM_TTS)
        ret = ttsplayer_start(priv->ttsPlayer);
    else
        ret = liteplayer_start(priv->urlPlayer);
    return ret == 0;
}

static bool GnVendorPlayer_Pause(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || priv->stream == GENIE_PLAYER_STREAM_TTS)
        return false;
    int ret = liteplayer_pause(priv->urlPlayer);
    return ret == 0;
}

static bool GnVendorPlayer_Resume(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || priv->stream == GENIE_PLAYER_STREAM_TTS)
        return false;
    int ret = liteplayer_resume(priv->urlPlayer);
    return ret == 0;
}

static bool GnVendorPlayer_Seek(void *handle, int positonMs)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || positonMs < 0 || priv->stream == GENIE_PLAYER_STREAM_TTS)
        return false;
    int ret = liteplayer_seek(priv->urlPlayer, positonMs);
    return ret == 0;
}

static bool GnVendorPlayer_Stop(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL)
        return false;
    int ret = 0;
    if (priv->stream == GENIE_PLAYER_STREAM_TTS)
        ret = ttsplayer_stop(priv->ttsPlayer);
    else
        ret = liteplayer_stop(priv->urlPlayer);
    return ret == 0;
}

static bool GnVendorPlayer_Reset(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL)
        return false;
    int ret = 0;
    if (priv->stream == GENIE_PLAYER_STREAM_TTS)
        ret = ttsplayer_reset(priv->ttsPlayer);
    else
        ret = liteplayer_reset(priv->urlPlayer);
    return ret == 0;
}

static bool GnVendorPlayer_GetPosition(void *handle, int *positonMs)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || positonMs == NULL || priv->stream == GENIE_PLAYER_STREAM_TTS)
        return false;
    int ret = liteplayer_get_position(priv->urlPlayer, positonMs);
    return ret == 0;
}

static bool GnVendorPlayer_GetDuration(void *handle, int *durationMs)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL || durationMs == NULL || priv->stream == GENIE_PLAYER_STREAM_TTS)
        return false;
    int ret = liteplayer_get_duration(priv->urlPlayer, durationMs);
    return ret == 0;
}

static void GnVendorPlayer_Destroy(void *handle)
{
    GnVendorPlayer_Priv_t *priv = (GnVendorPlayer_Priv_t *)handle;
    if (priv == NULL)
        return;
    if (priv->stream == GENIE_PLAYER_STREAM_TTS) {
        ttsplayer_reset(priv->ttsPlayer);
        ttsplayer_destroy(priv->ttsPlayer);
    } else {
        liteplayer_reset(priv->urlPlayer);
        liteplayer_destroy(priv->urlPlayer);
    }
    OS_FREE(priv);
}

GnPlayer_Adapter_t *GnVendorPlayer_GetInstance(GnVendor_PcmOut_t *pcmOut)
{
    if (sGnInited) return &sGnVendorPlayer;

    if (pcmOut == NULL || pcmOut->open == NULL || pcmOut->write == NULL || pcmOut->close == NULL)
        return NULL;
    sGnVendorPcmOut.open  = pcmOut->open;
    sGnVendorPcmOut.write = pcmOut->write;
    sGnVendorPcmOut.close = pcmOut->close;

    sGnVendorPlayer.create                  = GnVendorPlayer_Create;
    sGnVendorPlayer.registerStateListener   = GnVendorPlayer_RegisterStateListener;
    sGnVendorPlayer.setDataSource           = GnVendorPlayer_SetDataSource;
    sGnVendorPlayer.prepareAsync            = GnVendorPlayer_PrepareAsync;
    sGnVendorPlayer.write                   = GnVendorPlayer_Write;
    sGnVendorPlayer.start                   = GnVendorPlayer_Start;
    sGnVendorPlayer.pause                   = GnVendorPlayer_Pause;
    sGnVendorPlayer.resume                  = GnVendorPlayer_Resume;
    sGnVendorPlayer.seek                    = GnVendorPlayer_Seek;
    sGnVendorPlayer.stop                    = GnVendorPlayer_Stop;
    sGnVendorPlayer.reset                   = GnVendorPlayer_Reset;
    sGnVendorPlayer.getPosition             = GnVendorPlayer_GetPosition;
    sGnVendorPlayer.getDuration             = GnVendorPlayer_GetDuration;
    sGnVendorPlayer.destroy                 = GnVendorPlayer_Destroy;

    sGnInited = true;
    return &sGnVendorPlayer;
}
