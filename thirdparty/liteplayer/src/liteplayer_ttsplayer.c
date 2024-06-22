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
#include <string.h>

#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/ringbuf.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_main.h"
#include "liteplayer_ttsplayer.h"

#define TAG "[liteplayer]ttsplayer"

#define DEFAULT_TTS_URL_PREFIX    "tts"
#define DEFAULT_TTS_HEADER_SIZE   2048
#define DEFAULT_TTS_WRITE_TIMEOUT 1000 // ms
#define DEFAULT_TTS_RINGBUF_SIZE  (1024*16)

struct ttsplayer {
    struct ttsplayer_cfg   cfg;
    liteplayer_handle_t    player;
    ringbuf_handle         ringbuf;
    bool                   force_stop;
    bool                   waiting_data;
    bool                   has_prepared;
    long                   tts_offset;
};

static const char *tts_source_url_protocol();
static source_handle_t tts_source_open(const char *url, long long content_pos, void *priv_data);
static int tts_source_read(source_handle_t handle, char *buffer, int size);
static long long tts_source_content_pos(source_handle_t handle);
static long long tts_source_content_len(source_handle_t handle);
static int tts_source_seek(source_handle_t handle, long offset);
static void tts_source_close(source_handle_t handle);

ttsplayer_handle_t ttsplayer_create(struct ttsplayer_cfg *cfg)
{
    ttsplayer_handle_t handle = audio_calloc(1, sizeof(struct ttsplayer));
    if (handle != NULL) {
        if (cfg != NULL)
            handle->cfg.ringbuf_size = cfg->ringbuf_size;
        if (handle->cfg.ringbuf_size < DEFAULT_TTS_RINGBUF_SIZE)
            handle->cfg.ringbuf_size = DEFAULT_TTS_RINGBUF_SIZE;

        handle->ringbuf = rb_create(handle->cfg.ringbuf_size);
        if (handle->ringbuf == NULL)
            goto create_fail;

        handle->player = liteplayer_create();
        if (handle->player == NULL)
            goto create_fail;

        struct source_wrapper tts_ops = {
            .async_mode = false,
            .buffer_size = 2048,
            .priv_data = handle,
            .url_protocol = tts_source_url_protocol,
            .open = tts_source_open,
            .read = tts_source_read,
            .content_pos = tts_source_content_pos,
            .content_len = tts_source_content_len,
            .seek = tts_source_seek,
            .close = tts_source_close,
        };
        if (liteplayer_register_source_wrapper(handle->player, &tts_ops) != 0)
            goto create_fail;
    }
    return handle;

create_fail:
    ttsplayer_destroy(handle);
    return NULL;
}

int ttsplayer_register_sink_wrapper(ttsplayer_handle_t handle, struct sink_wrapper *wrapper)
{
    if (handle == NULL || wrapper == NULL)
        return -1;
    return liteplayer_register_sink_wrapper(handle->player, wrapper);
}

int ttsplayer_register_state_listener(ttsplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv)
{
    if (handle == NULL || listener == NULL)
        return -1;
    return liteplayer_register_state_listener(handle->player, listener, listener_priv);
}

int ttsplayer_prepare_async(ttsplayer_handle_t handle)
{
#define TTS_SOURCE_URL_NAME DEFAULT_TTS_URL_PREFIX ".rawdata"
    if (handle == NULL)
        return -1;
    rb_reset(handle->ringbuf);
    handle->force_stop = false;
    handle->waiting_data = true;
    handle->has_prepared = false;
    handle->tts_offset = 0;
    return liteplayer_set_data_source(handle->player, TTS_SOURCE_URL_NAME);
}

int ttsplayer_write(ttsplayer_handle_t handle, char *buffer, int size, bool final)
{
    if (handle == NULL || !handle->waiting_data) {
        OS_LOGE(TAG, "Can't write before player is ready");
        return -1;
    }

    int bytes_written = 0;
    int ret = 0;
    while (!handle->force_stop && size > 0) {
        int tmp = rb_write(handle->ringbuf, &buffer[bytes_written], size, DEFAULT_TTS_WRITE_TIMEOUT);
        if (tmp > 0) {
            size -= tmp;
            bytes_written += tmp;
        } else {
            if (tmp == RB_TIMEOUT) {
                OS_LOGW(TAG, "Timeout to write tts to RB");
                continue;
            } else if (tmp != RB_DONE) {
                OS_LOGE(TAG, "Failed to write tts to RB with error[%d]", tmp);
                ret = -1;
            }
            break;
        }
    }

    if (handle->force_stop || final) {
        rb_done_write(handle->ringbuf);
        ret = 0;
    }

    if (!handle->has_prepared && !handle->force_stop) {
        if (rb_bytes_filled(handle->ringbuf) >= DEFAULT_TTS_HEADER_SIZE || final) {
            ret = liteplayer_prepare_async(handle->player);
            handle->has_prepared = true;
        }
    }
    return ret;
}

int ttsplayer_start(ttsplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;
    return liteplayer_start(handle->player);
}

int ttsplayer_stop(ttsplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;
    handle->force_stop = true;
    handle->waiting_data = false;
    rb_done_write(handle->ringbuf);
    return liteplayer_stop(handle->player);
}

int ttsplayer_reset(ttsplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;
    handle->force_stop = true;
    handle->waiting_data = false;
    rb_done_write(handle->ringbuf);
    return liteplayer_reset(handle->player);
}

void ttsplayer_destroy(ttsplayer_handle_t handle)
{
    if (handle == NULL)
        return;
    if (handle->player != NULL)
        liteplayer_destroy(handle->player);
    if (handle->ringbuf != NULL)
        rb_destroy(handle->ringbuf);
    audio_free(handle);
}

static const char *tts_source_url_protocol()
{
    return DEFAULT_TTS_URL_PREFIX;
}

static source_handle_t tts_source_open(const char *url, long long content_pos, void *priv_data)
{
    ttsplayer_handle_t priv = (ttsplayer_handle_t)priv_data;
    OS_LOGD(TAG, "Opening tts source, url: %s, content_pos: %d", url, (int)content_pos);
    if (content_pos != priv->tts_offset) {
        if (tts_source_seek(priv, content_pos) != 0) {
            return NULL;
        }
    }
    return priv;
}

static int tts_source_read(source_handle_t handle, char *buffer, int size)
{
    ttsplayer_handle_t priv = (ttsplayer_handle_t)handle;
    if (!priv->has_prepared) {
        if (size > DEFAULT_TTS_HEADER_SIZE)
            size = DEFAULT_TTS_HEADER_SIZE;
        if (size > rb_bytes_filled(priv->ringbuf)) {
            OS_LOGE(TAG, "Insufficient data to prepare player, recommend mp3/aac without id3v2 for tts source");
            return -1;
        }
    }
    int ret = rb_read(priv->ringbuf, buffer, size, AUDIO_MAX_DELAY);
    if (ret > 0)
        priv->tts_offset += ret;
    else if (ret == RB_OK || ret == RB_DONE)
        ret = 0;
    return ret;
}

static long long tts_source_content_pos(source_handle_t handle)
{
    ttsplayer_handle_t priv = (ttsplayer_handle_t)handle;
    return priv->tts_offset;
}

static long long tts_source_content_len(source_handle_t handle)
{
    OS_LOGD(TAG, "Unsupported content len for tts source");
    return 0;
}

static int tts_source_seek(source_handle_t handle, long offset)
{
    ttsplayer_handle_t priv = (ttsplayer_handle_t)handle;
    OS_LOGD(TAG, "Seeking tts source: %ld>>%ld", priv->tts_offset, offset);
    if (offset < priv->tts_offset) {
        OS_LOGE(TAG, "Unsupported seek backward for tts source");
        return -1;
    }
    if (!priv->has_prepared && (offset - priv->tts_offset) > rb_bytes_filled(priv->ringbuf)) {
        OS_LOGE(TAG, "Insufficient data to prepare player, recommend mp3/aac without id3v2 for tts source");
        return -1;
    }
    int bytes_discard = (int)(offset - priv->tts_offset);
    char buffer[1024];
    while (bytes_discard > 0) {
        int read_size = sizeof(buffer);
        if (read_size > bytes_discard)
            read_size = bytes_discard;
        read_size = rb_read(priv->ringbuf, buffer, read_size, AUDIO_MAX_DELAY);
        if (read_size > 0) {
            bytes_discard -= read_size;
        } else {
            OS_LOGE(TAG, "Failed to seek tts source, rb_read: %d", read_size);
            return -1;
        }
    }
    priv->tts_offset = offset;
    return 0;
}

static void tts_source_close(source_handle_t handle)
{
    OS_LOGD(TAG, "Closing tts source");
}
