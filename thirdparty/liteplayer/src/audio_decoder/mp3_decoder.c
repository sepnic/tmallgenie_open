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

#include "cutils/log_helper.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"
#include "audio_decoder/mp3_decoder.h"

#define TAG "[liteplayer]mp3_decoder"

#define MP3_DECODER_INPUT_TIMEOUT_MAX  200

static esp_err_t mp3_decoder_destroy(audio_element_handle_t self)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)audio_element_getdata(self);
    OS_LOGV(TAG, "Destroy mp3 decoder");
    if (decoder->handle != NULL)
        mp3_wrapper_deinit(decoder);
    audio_free(decoder);
    return ESP_OK;
}

static esp_err_t mp3_decoder_open(audio_element_handle_t self)
{
    esp_err_t status = ESP_OK;
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)audio_element_getdata(self);

    if (decoder->handle != NULL) {
        OS_LOGD(TAG, "MP3 decoder already opened");
        return ESP_OK;
    }

    OS_LOGV(TAG, "Open mp3 decoder");
    if (mp3_wrapper_init(decoder) != 0) {
        OS_LOGE(TAG, "Failed to init mp3 wrapper");
        status = ESP_FAIL;
    }
    return status;
}

static esp_err_t mp3_decoder_close(audio_element_handle_t self)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)audio_element_getdata(self);

    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        OS_LOGV(TAG, "Close mp3 decoder");
        mp3_wrapper_deinit(decoder);

        memset(&decoder->buf_in, 0x0, sizeof(decoder->buf_in));
        memset(&decoder->buf_out, 0x0, sizeof(decoder->buf_out));
        decoder->handle = NULL;
        decoder->parsed_header = false;

        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        info.total_bytes = 0;
        audio_element_setinfo(self, &info);
    }

    return ESP_OK;
}

static int mp3_decoder_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int byte_write = 0;
    int ret = AEL_IO_OK;
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)audio_element_getdata(self);

    if (decoder->buf_out.bytes_remain > 0) {
        /* Output buffer have remain data */
        byte_write = audio_element_output(self,
                        decoder->buf_out.data+decoder->buf_out.bytes_written,
                        decoder->buf_out.bytes_remain);
    } else {
        /* More data need to be wrote */
        ret = mp3_wrapper_run(decoder);
        if (ret < 0) {
            if (ret == AEL_IO_TIMEOUT) {
                OS_LOGW(TAG, "mp3_wrapper_run AEL_IO_TIMEOUT");
            } else if (ret != AEL_IO_DONE) {
                OS_LOGE(TAG, "mp3_wrapper_run failed:%d", ret);
            }
            return ret;
        }

        //OS_LOGV(TAG, "ret=%d, bytes_remain=%d", ret, decoder->buf_out.bytes_remain);
        decoder->buf_out.bytes_written = 0;
        byte_write = audio_element_output(self,
                        decoder->buf_out.data,
                        decoder->buf_out.bytes_remain);
    }

    if (byte_write > 0) {
        decoder->buf_out.bytes_remain -= byte_write;
        decoder->buf_out.bytes_written += byte_write;

        audio_element_info_t audio_info = {0};
        audio_element_getinfo(self, &audio_info);
        audio_info.byte_pos += byte_write;
        audio_element_setinfo(self, &audio_info);
    }

    return byte_write;
}

static int mp3_decoder_seek(audio_element_handle_t self, long long offset)
{
    mp3_decoder_handle_t decoder = (mp3_decoder_handle_t)audio_element_getdata(self);

    mp3_wrapper_deinit(decoder);
    if (mp3_wrapper_init(decoder) != 0) {
        OS_LOGE(TAG, "Failed to init mp3 wrapper");
        return ESP_FAIL;
    }

    memset(&decoder->buf_in, 0x0, sizeof(decoder->buf_in));
    memset(&decoder->buf_out, 0x0, sizeof(decoder->buf_out));
    decoder->seek_mode = true;
    return ESP_OK;
}

audio_element_handle_t mp3_decoder_init(struct mp3_decoder_cfg *config)
{
    OS_LOGV(TAG, "Init mp3 decoder");

    mp3_decoder_handle_t decoder = audio_calloc(1, sizeof(struct mp3_decoder));
    AUDIO_MEM_CHECK(TAG, decoder, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = mp3_decoder_destroy;
    cfg.open    = mp3_decoder_open;
    cfg.close   = mp3_decoder_close;
    cfg.process = mp3_decoder_process;
    cfg.seek    = mp3_decoder_seek;
    cfg.buffer_len = 0;
    cfg.task_stack  = config->task_stack;
    cfg.task_prio   = config->task_prio;
    if (cfg.task_stack == 0)
        cfg.task_stack = MP3_DECODER_TASK_STACK;
    cfg.tag = "mp3_decoder";

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto mp3_init_error);
    decoder->mp3_info = config->mp3_info;
    decoder->el = el;
    audio_element_setdata(el, decoder);
    
    audio_element_info_t info = {0};
    audio_element_setinfo(el, &info);

    audio_element_set_input_timeout(el, MP3_DECODER_INPUT_TIMEOUT_MAX);
    return el;

mp3_init_error:
    audio_free(decoder);
    return NULL;
}
