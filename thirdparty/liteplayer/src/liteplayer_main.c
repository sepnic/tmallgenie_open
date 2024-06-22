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
#include <string.h>
#include <stdint.h>

#include "osal/os_thread.h"
#include "cutils/ringbuf.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_event_iface.h"
#include "esp_adf/audio_common.h"
#include "audio_decoder/mp3_decoder.h"
#include "audio_decoder/aac_decoder.h"
#include "audio_decoder/m4a_decoder.h"
#include "audio_decoder/wav_decoder.h"

#include "liteplayer_adapter_internal.h"
#include "liteplayer_adapter.h"
#include "liteplayer_config.h"
#include "liteplayer_source.h"
#include "liteplayer_parser.h"
#include "liteplayer_main.h"

#define TAG "[liteplayer]core"

struct liteplayer {
    const char             *url; // TTS   : tts.mp3
                                 // HTTP  : http://..., https://...
                                 // FILE  : others
    os_mutex                io_lock;

    enum liteplayer_state   state;
    os_mutex                state_lock;
    liteplayer_state_cb     state_listener;
    void                   *state_userdata;
    bool                    state_error;

    liteplayer_adapter_handle_t  adapter_handle;
    struct source_wrapper       *source_ops;
    struct sink_wrapper         *sink_ops;

    media_parser_handle_t   media_parser_handle;
    struct media_codec_info media_codec_info;

    audio_element_handle_t  ael_decoder;

    struct media_source_info media_source_info;
    media_source_handle_t    media_source_handle;
    int                      source_buffer_size; // for source synchronous mode
    char                    *source_buffer_addr; // for source synchronous mode

    sink_handle_t           sink_handle;
    int                     sink_samplerate;
    int                     sink_channels;
    int                     sink_bits;
    long long               sink_position;
    bool                    sink_inited;

    int                     seek_time;
    long long               seek_offset;
};

static int audio_source_open(audio_element_handle_t self, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (handle->media_source_info.source_handle == NULL) {
        OS_LOGI(TAG, "Opening source: url: %s", handle->url);
        handle->media_source_info.source_handle = handle->source_ops->open(handle->url,
            handle->media_codec_info.content_pos + handle->seek_offset, handle->source_ops->priv_data);
        if (handle->media_source_info.source_handle == NULL) {
            OS_LOGE(TAG, "Failed to open source");
            return AEL_IO_FAIL;
        }
        rb_reset(handle->media_source_info.out_ringbuf);
    }
    return AEL_IO_OK;
}

static int audio_source_read(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    int bytes_remain = rb_bytes_filled(handle->media_source_info.out_ringbuf);
    if (bytes_remain >= len) {
        rb_read_chunk(handle->media_source_info.out_ringbuf, buffer, len, 0);
        return len;
    }

    if (bytes_remain > 0)
        rb_read_chunk(handle->media_source_info.out_ringbuf, buffer, bytes_remain, 0);

    int bytes_want = len - bytes_remain;
    int bytes_read = 0;
    if (bytes_want < handle->source_buffer_size/2) {
        bytes_read = handle->source_ops->read(handle->media_source_info.source_handle,
                                              handle->source_buffer_addr,
                                              handle->source_buffer_size);
        if (bytes_read < 0 || bytes_read > handle->source_buffer_size) {
            OS_LOGE(TAG, "Failed to read source, ret:%d", bytes_read);
            return AEL_IO_FAIL;
        } else if (bytes_read == 0) {
            return AEL_IO_DONE;
        } else if (bytes_read > bytes_want) {
            memcpy(buffer + bytes_remain, handle->source_buffer_addr, bytes_want);
            rb_write_chunk(handle->media_source_info.out_ringbuf,
                           handle->source_buffer_addr + bytes_want,
                           bytes_read - bytes_want,
                           0);
            return len;
        } else {
            memcpy(buffer + bytes_remain, handle->source_buffer_addr, bytes_read);
            return bytes_read + bytes_remain;
        }
    } else {
        bytes_read = handle->source_ops->read(handle->media_source_info.source_handle,
                buffer + bytes_remain, bytes_want);
        if (bytes_read < 0 || bytes_read > bytes_want) {
            OS_LOGE(TAG, "Failed to read source, ret:%d", bytes_read);
            return AEL_IO_FAIL;
        } else if (bytes_read == 0) {
            return AEL_IO_DONE;
        } else {
            return bytes_read + bytes_remain;
        }
    }
}

static void audio_source_close(audio_element_handle_t self, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (audio_element_get_state(self) != AEL_STATE_PAUSED &&
        handle->media_source_info.source_handle != NULL) {
        OS_LOGI(TAG, "Closing source");
        handle->source_ops->close(handle->media_source_info.source_handle);
        handle->media_source_info.source_handle = NULL;
    }
}

static int audio_sink_open(audio_element_handle_t self, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (!handle->sink_inited) {
        OS_LOGV(TAG, "Sink not inited, abort opening");
        return AEL_IO_OK;
    }
    OS_LOGI(TAG, "Opening sink: rate:%d, channels:%d, bits:%d",
            handle->sink_samplerate, handle->sink_channels, handle->sink_bits);
    if (handle->sink_handle == NULL) {
        handle->sink_handle = handle->sink_ops->open(handle->sink_samplerate,
                                                     handle->sink_channels,
                                                     handle->sink_bits,
                                                     handle->sink_ops->priv_data);
        if (handle->sink_handle == NULL) {
            OS_LOGE(TAG, "Failed to open sink");
            return AEL_IO_FAIL;
        }
    }
    return AEL_IO_OK;
}

static int audio_sink_write(audio_element_handle_t self, char *buffer, int len, int timeout_ms, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (!handle->sink_inited) {
        handle->sink_inited = true;
        if (audio_sink_open(self, ctx) != 0)
            return AEL_IO_FAIL;
    }

    int bytes_written = handle->sink_ops->write(handle->sink_handle, buffer, len);
    if (bytes_written >= 0 && bytes_written <= len) {
        handle->sink_position += bytes_written;
    } else {
        OS_LOGE(TAG, "Failed to write pcm, ret:%d", bytes_written);
        bytes_written = AEL_IO_FAIL;
    }
    return bytes_written;
}

static void audio_sink_close(audio_element_handle_t self, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;
    if (handle->sink_handle != NULL) {
        OS_LOGI(TAG, "Closing sink");
        handle->sink_ops->close(handle->sink_handle);
        handle->sink_handle = NULL;
    }
    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        handle->sink_position = 0;
        handle->sink_inited = false;
    }
}

static void media_player_state_callback(liteplayer_handle_t handle, enum liteplayer_state state, int errcode)
{
    if (state == LITEPLAYER_ERROR) {
        if (!handle->state_error) {
            handle->state_error = true;
            if (handle->state_listener)
                handle->state_listener(LITEPLAYER_ERROR, errcode, handle->state_userdata);
        }
    } else {
        if (!handle->state_error || state == LITEPLAYER_IDLE || state == LITEPLAYER_STOPPED) {
            if (handle->state_listener)
                handle->state_listener(state, 0, handle->state_userdata);
        }
    }
}

static int audio_element_state_callback(audio_element_handle_t el, audio_event_iface_msg_t *msg, void *ctx)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)ctx;

    os_mutex_lock(handle->state_lock);

    if (msg->source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
        audio_element_status_t el_status = (audio_element_status_t)msg->data;

        if (msg->cmd == AEL_MSG_CMD_REPORT_STATUS) {
            switch (el_status) {
            case AEL_STATUS_ERROR_INPUT:
            case AEL_STATUS_ERROR_PROCESS:
            case AEL_STATUS_ERROR_OUTPUT:
            case AEL_STATUS_ERROR_UNKNOWN:
                OS_LOGE(TAG, "[ %s-%s ] Receive error[%d]",
                        handle->source_ops->url_protocol(), audio_element_get_tag(el), el_status);
                handle->state = LITEPLAYER_ERROR;
                media_player_state_callback(handle, LITEPLAYER_ERROR, el_status);
                break;

            case AEL_STATUS_ERROR_TIMEOUT:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGW(TAG, "[ %s-%s ] Receive inputtimeout event, filled/total: %d/%d",
                            handle->source_ops->url_protocol(), audio_element_get_tag(el),
                            rb_bytes_filled(handle->media_source_info.out_ringbuf),
                            rb_get_size(handle->media_source_info.out_ringbuf));
                }
                break;

            case AEL_STATUS_STATE_RUNNING:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive started event",
                            handle->source_ops->url_protocol(), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_STARTED;
                    //media_player_state_callback(handle, LITEPLAYER_STARTED, 0);
                }
                break;

            case AEL_STATUS_STATE_PAUSED:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive paused event",
                            handle->source_ops->url_protocol(), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_PAUSED;
                    //media_player_state_callback(handle, LITEPLAYER_PAUSED, 0);
                }
                break;

            case AEL_STATUS_STATE_FINISHED:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive finished event",
                            handle->source_ops->url_protocol(), audio_element_get_tag(el));
                    if (handle->state < LITEPLAYER_STARTED) {
                        OS_LOGE(TAG, "Receive finished event before starting player, it should not happen");
                        handle->state = LITEPLAYER_ERROR;
                        media_player_state_callback(handle, LITEPLAYER_ERROR, ESP_FAIL);
                    } else if (handle->state != LITEPLAYER_ERROR && handle->state != LITEPLAYER_STOPPED) {
                        handle->state = LITEPLAYER_COMPLETED;
                        media_player_state_callback(handle, LITEPLAYER_COMPLETED, 0);
                    }
                }
                break;

            case AEL_STATUS_STATE_STOPPED:
                if (msg->source == (void *)handle->ael_decoder) {
                    OS_LOGD(TAG, "[ %s-%s ] Receive stopped event",
                            handle->source_ops->url_protocol(), audio_element_get_tag(el));
                    //handle->state = LITEPLAYER_STOPPED;
                    //media_player_state_callback(handle, LITEPLAYER_STOPPED, 0);
                }
                break;

            default:
                break;
            }
        } else if (msg->cmd == AEL_MSG_CMD_REPORT_INFO) {
            if (msg->source == (void *)handle->ael_decoder) {
                audio_element_info_t info = {0};
                audio_element_getinfo(handle->ael_decoder, &info);
                OS_LOGI(TAG, "[ %s-%s ] Receive codec info: samplerate=%d, ch=%d, bits=%d",
                        handle->source_ops->url_protocol(), audio_element_get_tag(el),
                        info.samplerate, info.channels, info.bits);
                handle->sink_samplerate = info.samplerate;
                handle->sink_channels = info.channels;
                handle->sink_bits = info.bits;
            }
        }
    }

    os_mutex_unlock(handle->state_lock);
    return ESP_OK;
}

static void media_source_state_callback(enum media_source_state state, void *priv)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)priv;

    os_mutex_lock(handle->state_lock);

    switch (state) {
    case MEDIA_SOURCE_READ_FAILED:
    case MEDIA_SOURCE_WRITE_FAILED:
        OS_LOGE(TAG, "[ %s-source ] Receive error[%d]", handle->source_ops->url_protocol(), state);
        handle->state = LITEPLAYER_ERROR;
        media_player_state_callback(handle, LITEPLAYER_ERROR, state);
        break;
    case MEDIA_SOURCE_READ_DONE:
        OS_LOGD(TAG, "[ %s-source ] Receive inputdone event", handle->source_ops->url_protocol());
        media_player_state_callback(handle, LITEPLAYER_NEARLYCOMPLETED, 0);
        break;
    default:
        break;
    }

    os_mutex_unlock(handle->state_lock);
}

static void media_parser_state_callback(enum media_parser_state state, struct media_codec_info *info, void *priv)
{
    liteplayer_handle_t handle = (liteplayer_handle_t)priv;

    os_mutex_lock(handle->state_lock);

    switch (state) {
    case MEDIA_PARSER_FAILED:
        OS_LOGE(TAG, "[ %s-PARSER ] Receive error[%d]", handle->source_ops->url_protocol(), state);
        handle->state = LITEPLAYER_ERROR;
        media_player_state_callback(handle, LITEPLAYER_ERROR, MEDIA_PARSER_FAILED);
        break;
    case MEDIA_PARSER_SUCCEED:
        OS_LOGD(TAG, "[ %s-PARSER ] Receive prepared event", handle->source_ops->url_protocol());
        memcpy(&handle->media_codec_info, info, sizeof(struct media_codec_info));
        handle->state = LITEPLAYER_PREPARED;
        media_player_state_callback(handle, LITEPLAYER_PREPARED, 0);
        break;
    default:
        break;
    }

    os_mutex_unlock(handle->state_lock);
}

static void main_pipeline_deinit(liteplayer_handle_t handle)
{
    if (handle->ael_decoder != NULL) {
        OS_LOGD(TAG, "Destroy audio decoder");
        audio_element_deinit(handle->ael_decoder);
        handle->ael_decoder = NULL;
    }

    if (handle->media_parser_handle != NULL) {
        media_parser_stop(handle->media_parser_handle);
        handle->media_parser_handle = NULL;
    }

    if (handle->media_source_handle != NULL) {
        media_source_stop(handle->media_source_handle);
        handle->media_source_handle = NULL;
    } else if (handle->media_source_info.source_handle != NULL) {
        handle->source_ops->close(handle->media_source_info.source_handle);
        handle->media_source_info.source_handle = NULL;
    }

    if (handle->media_source_info.out_ringbuf != NULL) {
        rb_destroy(handle->media_source_info.out_ringbuf);
        handle->media_source_info.out_ringbuf = NULL;
    }

    if (handle->source_buffer_addr != NULL) {
        audio_free(handle->source_buffer_addr);
        handle->source_buffer_addr = NULL;
    }
}

static int main_pipeline_init(liteplayer_handle_t handle)
{
    {
        OS_LOGD(TAG, "[1.0] Create decoder element");
        switch (handle->media_codec_info.codec_type) {
        case AUDIO_CODEC_MP3: {
            struct mp3_decoder_cfg mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
            mp3_cfg.task_prio            = DEFAULT_MEDIA_DECODER_TASK_PRIO;
            mp3_cfg.task_stack           = DEFAULT_MEDIA_DECODER_TASK_STACKSIZE;
            mp3_cfg.mp3_info             = &(handle->media_codec_info.detail.mp3_info);
            handle->ael_decoder = mp3_decoder_init(&mp3_cfg);
            break;
        }
        case AUDIO_CODEC_AAC: {
            struct aac_decoder_cfg aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
            aac_cfg.task_prio            = DEFAULT_MEDIA_DECODER_TASK_PRIO;
            aac_cfg.task_stack           = DEFAULT_MEDIA_DECODER_TASK_STACKSIZE;
            aac_cfg.aac_info             = &(handle->media_codec_info.detail.aac_info);
            handle->ael_decoder = aac_decoder_init(&aac_cfg);
            break;
        }
        case AUDIO_CODEC_M4A: {
            struct m4a_decoder_cfg m4a_cfg = DEFAULT_M4A_DECODER_CONFIG();
            m4a_cfg.task_prio            = DEFAULT_MEDIA_DECODER_TASK_PRIO;
            m4a_cfg.task_stack           = DEFAULT_MEDIA_DECODER_TASK_STACKSIZE;
            m4a_cfg.m4a_info             = &(handle->media_codec_info.detail.m4a_info);
            handle->ael_decoder = m4a_decoder_init(&m4a_cfg);
            break;
        }
        case AUDIO_CODEC_WAV: {
            struct wav_decoder_cfg wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
            wav_cfg.task_prio            = DEFAULT_MEDIA_DECODER_TASK_PRIO;
            wav_cfg.task_stack           = DEFAULT_MEDIA_DECODER_TASK_STACKSIZE;
            wav_cfg.wav_info             = &(handle->media_codec_info.detail.wav_info);
            handle->ael_decoder = wav_decoder_init(&wav_cfg);
            break;
        }
        case AUDIO_CODEC_OPUS: {
            //struct opus_decoder_cfg opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
            //opus_cfg.task_prio            = DEFAULT_MEDIA_DECODER_TASK_PRIO;
            //opus_cfg.task_stack           = DEFAULT_MEDIA_DECODER_TASK_STACKSIZE;
            //opus_cfg.opus_info            = &(handle->media_codec_info.detail.opus_info);
            //handle->ael_decoder = opus_decoder_init(&opus_cfg);
            break;
        }
        case AUDIO_CODEC_FLAC: {
            //struct flac_decoder_cfg flac_cfg = DEFAULT_FLAC_DECODER_CONFIG();
            //flac_cfg.task_prio            = DEFAULT_MEDIA_DECODER_TASK_PRIO;
            //flac_cfg.task_stack           = DEFAULT_MEDIA_DECODER_TASK_STACKSIZE;
            //flac_cfg.flac_info            = &(handle->media_codec_info.detail.flac_info);
            //handle->ael_decoder = flac_decoder_init(&flac_cfg);
            break;
        }
        default:
            break;
        }
        AUDIO_MEM_CHECK(TAG, handle->ael_decoder, return ESP_FAIL);
    }

    {
        OS_LOGD(TAG, "[1.1] Create sink element");
        handle->sink_position = 0;
        handle->sink_samplerate = handle->media_codec_info.codec_samplerate;
        handle->sink_channels = handle->media_codec_info.codec_channels;
        handle->sink_bits = handle->media_codec_info.codec_bits;
        stream_callback_t audio_sink = {
            .open = audio_sink_open,
            .write = audio_sink_write,
            .close = audio_sink_close,
            .ctx = handle,
        };
        audio_element_set_write_cb(handle->ael_decoder, &audio_sink);
    }

    if (handle->source_ops->async_mode) {
        OS_LOGD(TAG, "[1.2] Create source element, async mode, ringbuf size: %d", handle->source_ops->buffer_size);
        audio_element_set_input_ringbuf(handle->ael_decoder, handle->media_source_info.out_ringbuf);
        handle->media_source_info.content_pos = handle->media_codec_info.content_pos + handle->seek_offset;
        handle->media_source_handle =
            media_source_start_async(&handle->media_source_info, media_source_state_callback, handle);
        AUDIO_MEM_CHECK(TAG, handle->media_source_handle, return ESP_FAIL);
    } else {
        OS_LOGD(TAG, "[1.2] Create source element, sync mode, ringbuf size: %d", handle->source_ops->buffer_size);
        handle->source_buffer_size = handle->source_ops->buffer_size;
        handle->source_buffer_addr = audio_malloc(handle->source_buffer_size);
        AUDIO_MEM_CHECK(TAG, handle->source_buffer_addr, return ESP_FAIL);
        stream_callback_t audio_source = {
            .open = audio_source_open,
            .read = audio_source_read,
            .close = audio_source_close,
            .ctx = handle,
        };
        audio_element_set_read_cb(handle->ael_decoder, &audio_source);
    }

    {
        OS_LOGD(TAG, "[2.0] Register event callback of decoder elements");
        audio_element_set_event_callback(handle->ael_decoder, audio_element_state_callback, handle);
    }

    {
        OS_LOGD(TAG, "[3.0] Run decoder element");
        if (audio_element_run(handle->ael_decoder) != 0)
            return ESP_FAIL;
    }

    return ESP_OK;
}

liteplayer_handle_t liteplayer_create()
{
    liteplayer_handle_t handle = audio_calloc(1, sizeof(struct liteplayer));
    if (handle != NULL) {
        handle->state = LITEPLAYER_IDLE;
        handle->io_lock = os_mutex_create();
        handle->state_lock = os_mutex_create();
        handle->adapter_handle = liteplayer_adapter_init();
        if (handle->io_lock == NULL || handle->state_lock == NULL || handle->adapter_handle == NULL) {
            goto create_fail;
        }
    }
    return handle;

create_fail:
    if (handle->io_lock != NULL)
        os_mutex_destroy(handle->io_lock);
    if (handle->state_lock != NULL)
        os_mutex_destroy(handle->state_lock);
    if (handle->adapter_handle != NULL)
        handle->adapter_handle->destory(handle->adapter_handle);
    audio_free(handle);
    return NULL;
}

int liteplayer_register_source_wrapper(liteplayer_handle_t handle, struct source_wrapper *wrapper)
{
    if (handle == NULL || wrapper == NULL)
        return ESP_FAIL;

    os_mutex_lock(handle->io_lock);
    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't register source wrapper in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }
    int ret = handle->adapter_handle->add_source_wrapper(handle->adapter_handle, wrapper);
    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_register_sink_wrapper(liteplayer_handle_t handle, struct sink_wrapper *wrapper)
{
    if (handle == NULL || wrapper == NULL)
        return ESP_FAIL;

    os_mutex_lock(handle->io_lock);
    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't register sink wrapper in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }
    int ret = handle->adapter_handle->add_sink_wrapper(handle->adapter_handle, wrapper);
    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_register_state_listener(liteplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv)
{
    if (handle == NULL || listener == NULL)
        return ESP_FAIL;

    os_mutex_lock(handle->io_lock);
    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't set listener in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }
    handle->state_listener = listener;
    handle->state_userdata = listener_priv;
    os_mutex_unlock(handle->io_lock);
    return ESP_OK;
}

int liteplayer_set_data_source(liteplayer_handle_t handle, const char *url)
{
    if (handle == NULL || url == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Set player source: %s", url);

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't set source in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    handle->source_ops = handle->adapter_handle->find_source_wrapper(handle->adapter_handle, url);
    if (handle->source_ops == NULL) {
        OS_LOGE(TAG, "Can't find source wrapper for this url");
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }
    handle->sink_ops = handle->adapter_handle->find_sink_wrapper(handle->adapter_handle, "default");
    if (handle->sink_ops == NULL) {
        OS_LOGE(TAG, "Can't find sink wrapper");
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }
    OS_LOGD(TAG, "Using source_wrapper: (%s), sink_wrapper: (%s)",
            handle->source_ops->url_protocol(), handle->sink_ops->name());

    handle->state_error = false;
    handle->url = audio_strdup(url);
    AUDIO_MEM_CHECK(TAG, handle->url, goto set_fail);

    handle->media_source_info.url = handle->url;
    handle->media_source_info.source_ops = handle->source_ops;
    handle->media_source_info.out_ringbuf = rb_create(handle->source_ops->buffer_size);
    AUDIO_MEM_CHECK(TAG, handle->media_source_info.out_ringbuf, goto set_fail);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = LITEPLAYER_INITED;
        media_player_state_callback(handle, LITEPLAYER_INITED, 0);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ESP_OK;

set_fail:
    if (handle->media_source_info.out_ringbuf != NULL) {
        rb_destroy(handle->media_source_info.out_ringbuf);
        handle->media_source_info.out_ringbuf = NULL;
    }
    if (handle->url != NULL) {
        audio_free(handle->url);
        handle->url = NULL;
    }
    os_mutex_unlock(handle->io_lock);
    return ESP_FAIL;
}

int liteplayer_prepare(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Preparing player[%s]", handle->source_ops->url_protocol());

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_INITED) {
        OS_LOGE(TAG, "Can't prepare in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = media_parser_get_codec_info(&handle->media_source_info, &handle->media_codec_info);
    if (ret == ESP_OK)
        ret = main_pipeline_init(handle);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PREPARED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_prepare_async(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Async preparing player[%s]", handle->source_ops->url_protocol());

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_INITED) {
        OS_LOGE(TAG, "Can't prepare in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = ESP_OK;
    if (handle->source_ops->async_mode) {
        handle->media_parser_handle = media_parser_start_async(&handle->media_source_info,
                                                               media_parser_state_callback,
                                                               handle);
        if (handle->media_parser_handle == NULL) {
            ret = ESP_FAIL;
            os_mutex_lock(handle->state_lock);
            handle->state = LITEPLAYER_ERROR;
            media_player_state_callback(handle, LITEPLAYER_ERROR, ret);
            os_mutex_unlock(handle->state_lock);
        }
    } else {
        ret = media_parser_get_codec_info(&handle->media_source_info, &handle->media_codec_info);
        if (ret == ESP_OK)
            ret = main_pipeline_init(handle);
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PREPARED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_start(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Starting player[%s]", handle->source_ops->url_protocol());

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_PREPARED &&
        handle->state != LITEPLAYER_PAUSED &&
        handle->state != LITEPLAYER_SEEKCOMPLETED) {
        OS_LOGE(TAG, "Can't start in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    if (handle->media_parser_handle != NULL) {
        media_parser_stop(handle->media_parser_handle);
        handle->media_parser_handle = NULL;
    }

    int ret = ESP_OK;

    if (handle->state == LITEPLAYER_PREPARED) {
        if (handle->ael_decoder == NULL)
            ret = main_pipeline_init(handle);
    } else {
        if (handle->ael_decoder == NULL)
            ret = ESP_FAIL;
    }
    if (ret == ESP_OK)
        ret = audio_element_resume(handle->ael_decoder, 0, 0);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_STARTED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_pause(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Pausing player[%s]", handle->source_ops->url_protocol());

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_STARTED) {
        OS_LOGE(TAG, "Can't pause in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = audio_element_pause(handle->ael_decoder);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_PAUSED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_resume(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    OS_LOGI(TAG, "Resuming player[%s]", handle->source_ops->url_protocol());

    os_mutex_lock(handle->io_lock);

    if (handle->state != LITEPLAYER_PAUSED && handle->state != LITEPLAYER_SEEKCOMPLETED) {
        OS_LOGE(TAG, "Can't resume in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    int ret = audio_element_resume(handle->ael_decoder, 0, 0);

    {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_STARTED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_seek(liteplayer_handle_t handle, int msec)
{
    if (handle == NULL || msec < 0)
        return ESP_FAIL;

    OS_LOGI(TAG, "Seeking player[%s], offset=%d(s)", handle->source_ops->url_protocol(), (int)(msec/1000));

    int ret = ESP_FAIL;
    bool state_sync = false;

    os_mutex_lock(handle->io_lock);

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_NEARLYCOMPLETED) {
        OS_LOGE(TAG, "Can't seek in state=[%d]", handle->state);
        ret = ESP_OK;
        goto seek_out;
    }

    if (msec >= handle->media_codec_info.duration_ms) {
        OS_LOGE(TAG, "Invalid seek time");
        ret = ESP_OK;
        goto seek_out;
    }

    long long offset = media_parser_get_seek_offset(&handle->media_codec_info, msec);
    if (offset < 0) {
        ret = ESP_OK;
        goto seek_out;
    }

    handle->seek_time = (msec/1000)*1000;
    handle->seek_offset = offset;
    handle->sink_position = 0;

    state_sync = true;

    if (handle->media_parser_handle != NULL) {
        media_parser_stop(handle->media_parser_handle);
        handle->media_parser_handle = NULL;
    }

    if (handle->ael_decoder == NULL) {
        ret = main_pipeline_init(handle);
        if (ret != ESP_OK)
            goto seek_out;
    } else {
        ret = audio_element_pause(handle->ael_decoder);
        if (ret != ESP_OK)
            goto seek_out;

        if (handle->media_source_handle != NULL) {
            media_source_stop(handle->media_source_handle);
            handle->media_source_handle = NULL;
        } else if (handle->media_source_info.source_handle != NULL) {
            OS_LOGI(TAG, "Closing source");
            handle->source_ops->close(handle->media_source_info.source_handle);
            handle->media_source_info.source_handle = NULL;
        }

        rb_reset(handle->media_source_info.out_ringbuf);

        if (handle->source_ops->async_mode) {
            handle->media_source_info.source_handle = NULL;
            handle->media_source_info.content_pos = handle->media_codec_info.content_pos + handle->seek_offset;
            handle->media_source_handle =
                media_source_start_async(&handle->media_source_info, media_source_state_callback, handle);
            AUDIO_MEM_CHECK(TAG, handle->media_source_handle, goto seek_out);
        } else {
            stream_callback_t audio_source = {
                .open = audio_source_open,
                .read = audio_source_read,
                .close = audio_source_close,
                .ctx = handle,
            };
            audio_element_set_read_cb(handle->ael_decoder, &audio_source);
        }
    }

    ret = audio_element_seek(handle->ael_decoder, handle->seek_offset);
    if (ret != ESP_OK)
        goto seek_out;

seek_out:
    if (state_sync) {
        os_mutex_lock(handle->state_lock);
        handle->state = (ret == ESP_OK) ? LITEPLAYER_SEEKCOMPLETED : LITEPLAYER_ERROR;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_stop(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    int ret = ESP_OK;

    OS_LOGI(TAG, "Stopping player[%s]", handle->source_ops->url_protocol());

    os_mutex_lock(handle->io_lock);

    if (handle->state == LITEPLAYER_ERROR)
        goto stop_out;

    if (handle->state < LITEPLAYER_PREPARED || handle->state > LITEPLAYER_COMPLETED) {
        OS_LOGE(TAG, "Can't stop in state=[%d]", handle->state);
        os_mutex_unlock(handle->io_lock);
        return ESP_FAIL;
    }

    ret = audio_element_stop(handle->ael_decoder);
    ret |= audio_element_wait_for_stop_ms(handle->ael_decoder, AUDIO_MAX_DELAY);
    audio_element_reset_state(handle->ael_decoder);
    audio_element_reset_input_ringbuf(handle->ael_decoder);
    audio_element_reset_output_ringbuf(handle->ael_decoder);

stop_out:
    {
        os_mutex_lock(handle->state_lock);
        handle->state = LITEPLAYER_STOPPED;
        media_player_state_callback(handle, handle->state, ret);
        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ret;
}

int liteplayer_reset(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return ESP_FAIL;

    os_mutex_lock(handle->io_lock);

    if (handle->state == LITEPLAYER_IDLE) {
        os_mutex_unlock(handle->io_lock);
        return ESP_OK;
    }

    OS_LOGI(TAG, "Resetting player[%s]", handle->source_ops->url_protocol());

    main_pipeline_deinit(handle);

    if (handle->url != NULL) {
        audio_free(handle->url);
        handle->url = NULL;
    }

    if (handle->media_codec_info.codec_type == AUDIO_CODEC_M4A) {
        if (handle->media_codec_info.detail.m4a_info.stsz_samplesize != NULL)
            audio_free(handle->media_codec_info.detail.m4a_info.stsz_samplesize);
        if (handle->media_codec_info.detail.m4a_info.stts_time2sample != NULL)
            audio_free(handle->media_codec_info.detail.m4a_info.stts_time2sample);
        if (handle->media_codec_info.detail.m4a_info.stsc_sample2chunk != NULL)
            audio_free(handle->media_codec_info.detail.m4a_info.stsc_sample2chunk);
        if (handle->media_codec_info.detail.m4a_info.stco_chunk2offset != NULL)
            audio_free(handle->media_codec_info.detail.m4a_info.stco_chunk2offset);
    } else if (handle->media_codec_info.codec_type == AUDIO_CODEC_WAV) {
        if (handle->media_codec_info.detail.wav_info.header_buff != NULL)
            audio_free(handle->media_codec_info.detail.wav_info.header_buff);
    }

    memset(&handle->media_source_info, 0x0, sizeof(handle->media_source_info));
    memset(&handle->media_codec_info, 0x0, sizeof(handle->media_codec_info));

    handle->state_error = false;
    handle->source_ops = NULL;
    handle->sink_ops = NULL;
    handle->sink_samplerate = 0;
    handle->sink_channels = 0;
    handle->sink_bits = 0;
    handle->sink_position = 0;
    handle->sink_inited = false;
    handle->seek_time = 0;
    handle->seek_offset = 0;

    {
        os_mutex_lock(handle->state_lock);

        if (handle->state != LITEPLAYER_STOPPED) {
            handle->state = LITEPLAYER_STOPPED;
            media_player_state_callback(handle, LITEPLAYER_STOPPED, 0);
        }

        handle->state = LITEPLAYER_IDLE;
        media_player_state_callback(handle, LITEPLAYER_IDLE, 0);

        os_mutex_unlock(handle->state_lock);
    }

    os_mutex_unlock(handle->io_lock);
    return ESP_OK;
}

int liteplayer_get_position(liteplayer_handle_t handle, int *msec)
{
    if (handle == NULL || msec == NULL)
        return ESP_FAIL;

    int samplerate = handle->sink_samplerate;
    int channels = handle->sink_channels;
    int bits = handle->sink_bits;
    long long position = handle->sink_position;
    int seek_time = handle->seek_time;

    if (samplerate == 0 || channels == 0 || bits == 0) {
        *msec = 0;
        return ESP_OK;
    }

    int bytes_per_sample = channels * bits / 8;
    long long out_samples = position / bytes_per_sample;
    *msec = (int)(out_samples/(samplerate/1000) + seek_time);
    return ESP_OK;
}

int liteplayer_get_duration(liteplayer_handle_t handle, int *msec)
{
    if (handle == NULL || msec == NULL)
        return ESP_FAIL;

    if (handle->state < LITEPLAYER_PREPARED)
        return ESP_FAIL;

    *msec = handle->media_codec_info.duration_ms;
    return ESP_OK;
}

void liteplayer_destroy(liteplayer_handle_t handle)
{
    if (handle == NULL)
        return;

    if (handle->state != LITEPLAYER_IDLE)
        liteplayer_reset(handle);

    handle->adapter_handle->destory(handle->adapter_handle);
    os_mutex_destroy(handle->state_lock);
    os_mutex_destroy(handle->io_lock);
    audio_free(handle);
}
