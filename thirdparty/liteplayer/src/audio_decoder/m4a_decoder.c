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
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/m4a_decoder.h"

#define TAG "[liteplayer]m4a_decoder"

#define M4A_DECODER_INPUT_TIMEOUT_MAX  200

static esp_err_t m4a_decoder_destroy(audio_element_handle_t self)
{
    m4a_decoder_handle_t decoder = (m4a_decoder_handle_t)audio_element_getdata(self);
    OS_LOGV(TAG, "Destroy m4a decoder");
    if (decoder->handle != NULL)
        m4a_wrapper_deinit(decoder);
    audio_free(decoder);
    return ESP_OK;
}

static esp_err_t m4a_decoder_open(audio_element_handle_t self)
{
    esp_err_t err = ESP_OK;
    m4a_decoder_handle_t decoder = (m4a_decoder_handle_t)audio_element_getdata(self);

    if (decoder->handle != NULL) {
        OS_LOGD(TAG, "M4A decoder already opened");
        return ESP_OK;
    }

    OS_LOGV(TAG, "Open m4a decoder");
    if (m4a_wrapper_init(decoder) != 0) {
        OS_LOGE(TAG, "Failed to init m4a wrapper");
        err = ESP_FAIL;
    }
    return err;
}

static esp_err_t m4a_decoder_close(audio_element_handle_t self)
{
    m4a_decoder_handle_t decoder = (m4a_decoder_handle_t)audio_element_getdata(self);

    if (audio_element_get_state(self) != AEL_STATE_PAUSED) {
        OS_LOGV(TAG, "Close m4a decoder");
        m4a_wrapper_deinit(decoder);

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

static int m4a_decoder_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int byte_write = 0;
    int ret = AEL_IO_FAIL;
    m4a_decoder_handle_t decoder = (m4a_decoder_handle_t)audio_element_getdata(self);

    if (decoder->buf_out.bytes_remain > 0) {
        /* Output buffer have remain data */
        byte_write = audio_element_output(self,
                        decoder->buf_out.data+decoder->buf_out.bytes_written,
                        decoder->buf_out.bytes_remain);
    } else {
        /* More data need to be wrote */
        ret = m4a_wrapper_run(decoder);
        if (ret < 0) {
            if (ret == AEL_IO_TIMEOUT) {
                OS_LOGW(TAG, "m4a_wrapper_run AEL_IO_TIMEOUT");
            } else if (ret != AEL_IO_DONE) {
                OS_LOGE(TAG, "m4a_wrapper_run failed:%d", ret);
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

static esp_err_t m4a_decoder_seek(audio_element_handle_t self, long long offset)
{
    m4a_decoder_handle_t decoder = (m4a_decoder_handle_t)audio_element_getdata(self);

    m4a_wrapper_deinit(decoder);
    if (m4a_wrapper_init(decoder) != 0) {
        OS_LOGE(TAG, "Failed to init m4a wrapper");
        return ESP_FAIL;
    }

    memset(&decoder->buf_in, 0x0, sizeof(decoder->buf_in));
    memset(&decoder->buf_out, 0x0, sizeof(decoder->buf_out));
    return ESP_OK;
}

audio_element_handle_t m4a_decoder_init(struct m4a_decoder_cfg *config)
{
    OS_LOGV(TAG, "Init m4a decoder");

    m4a_decoder_handle_t decoder = audio_calloc(1, sizeof(struct m4a_decoder));
    AUDIO_MEM_CHECK(TAG, decoder, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = m4a_decoder_destroy;
    cfg.open    = m4a_decoder_open;
    cfg.close   = m4a_decoder_close;
    cfg.process = m4a_decoder_process;
    cfg.seek    = m4a_decoder_seek;
    cfg.buffer_len = 0;
    cfg.task_stack  = config->task_stack;
    cfg.task_prio   = config->task_prio;
    if (cfg.task_stack == 0)
        cfg.task_stack = AAC_DECODER_TASK_STACK;
    cfg.tag = "m4a_decoder";

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto m4a_init_error);
    decoder->m4a_info = config->m4a_info;
    decoder->el = el;
    audio_element_setdata(el, decoder);

    audio_element_set_input_timeout(el, M4A_DECODER_INPUT_TIMEOUT_MAX);
    return el;

m4a_init_error:
    audio_free(decoder);
    return NULL;
}
