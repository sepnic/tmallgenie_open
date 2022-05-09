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
#include "litevad.h"
#include "GenieSdk.h"
#include "GenieVoiceEngine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "i2s_stream.h"
#include "ringbuf.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"

#define TAG "GenieVoiceEngine"

#define GENIE_RECORD_SAMPLE_RATE    16000
#define GENIE_RECORD_SAMPLE_BIT     16
#define GENIE_RECORD_CHANNEL_COUNT  1
#define GENIE_RECORD_RINGBUF_SIZE   8192
#define GENIE_RECORD_READ_SIZE      960

static audio_element_handle_t   sGnI2sElement = NULL;
static audio_element_handle_t   sGnRspElement = NULL;
static audio_element_handle_t   sGnRawElement = NULL;
static ringbuf_handle_t         sGnRingbuf = NULL;
static bool                     sGnIsRecording = false;
static char                     sGnRecordBuf[GENIE_RECORD_READ_SIZE];
static litevad_handle_t         sGnVadHandle = NULL;
static bool                     sGnVadActive = false;

// see rec_eng_helper.h for more configs
extern const esp_wn_iface_t esp_sr_wakenet5_quantized;
extern const model_coeff_getter_t get_coeff_hilexin_wn5;
static esp_wn_iface_t          *sGnWakenet = (esp_wn_iface_t *)&esp_sr_wakenet5_quantized;
static model_coeff_getter_t    *sGnModelCoeffGetter = (model_coeff_getter_t *)&get_coeff_hilexin_wn5;
static model_iface_data_t      *sGnModelData = NULL;

static void GnVoiceEngine_recordTask(void *arg)
{
    static GenieSdk_Callback_t *sdkCallback = NULL;
    while (1) {
        int bytes_read = raw_stream_read(sGnRawElement, sGnRecordBuf, sizeof(sGnRecordBuf));
        //ESP_LOGW(TAG, "raw_stream_read: %d", bytes_read);
        if (bytes_read <= 0) {
            ESP_LOGE(TAG, "raw_stream_read failed");
            continue;
        }

        if (sGnIsRecording) {
            litevad_result_t vad_state = litevad_process(sGnVadHandle, sGnRecordBuf, bytes_read);
            if (sGnVadActive && vad_state == LITEVAD_RESULT_SPEECH_END) {
                ESP_LOGI(TAG, "VAD state changed: speech >> silence, onMicphoneSilence");
                if (sdkCallback == NULL)
                    GenieSdk_Get_Callback(&sdkCallback);
                if (sdkCallback != NULL)
                    sdkCallback->onMicphoneSilence();
            }
            if (!sGnVadActive && vad_state == LITEVAD_RESULT_SPEECH_BEGIN)
                sGnVadActive = true;

            rb_write(sGnRingbuf, sGnRecordBuf, bytes_read, portMAX_DELAY);
        } else {
            if (sGnModelData != NULL && sGnWakenet->detect(sGnModelData, (int16_t *)sGnRecordBuf) != 0) {
                ESP_LOGW(TAG, "Keyword detected");
                if (sdkCallback == NULL)
                    GenieSdk_Get_Callback(&sdkCallback);
                if (sdkCallback != NULL)
                    sdkCallback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);
            }
        }
    }
}

bool GnVoiceEngine_init()
{
    if ((sGnRingbuf = rb_create(GENIE_RECORD_RINGBUF_SIZE, 1)) == NULL) {
        ESP_LOGE(TAG, "rb_create failed");
        return false;
    }
    if ((sGnVadHandle = litevad_create(GENIE_RECORD_SAMPLE_RATE, GENIE_RECORD_CHANNEL_COUNT)) == NULL) {
        ESP_LOGE(TAG, "litevad_create failed");
        return false;
    }

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.i2s_config.sample_rate = ESP_I2S_FIXED_RATE;
    i2s_cfg.i2s_config.bits_per_sample = 16;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_cfg.i2s_port = 1;
#endif
    if ((sGnI2sElement = i2s_stream_init(&i2s_cfg)) == NULL) {
        ESP_LOGE(TAG, "i2s_stream_init failed");
        return false;
    }
    if (i2s_stream_set_clk(sGnI2sElement, ESP_I2S_FIXED_RATE, 16, ESP_I2S_FIXED_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_stream_set_clk failed");
        return false;
    }

    if (ESP_I2S_FIXED_RATE != GENIE_RECORD_SAMPLE_RATE ||
        ESP_I2S_FIXED_CHANNEL != GENIE_RECORD_CHANNEL_COUNT) {
        rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
        rsp_cfg.src_rate = ESP_I2S_FIXED_RATE;
        rsp_cfg.src_ch = ESP_I2S_FIXED_CHANNEL;
        rsp_cfg.dest_rate = GENIE_RECORD_SAMPLE_RATE;
        rsp_cfg.dest_ch = GENIE_RECORD_CHANNEL_COUNT;
        rsp_cfg.mode = RESAMPLE_DECODE_MODE;
        if ((sGnRspElement = rsp_filter_init(&rsp_cfg)) == NULL) {
            ESP_LOGE(TAG, "rsp_filter_init failed");
            return false;
        }
    }

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    if ((sGnRawElement = raw_stream_init(&raw_cfg)) == NULL) {
        ESP_LOGE(TAG, "raw_stream_init failed");
        return false;
    }

    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    if ((pipeline = audio_pipeline_init(&pipeline_cfg)) == NULL) {
        ESP_LOGE(TAG, "audio_pipeline_init failed");
        return false;
    }
    audio_pipeline_register(pipeline, sGnI2sElement, "i2s");
    audio_pipeline_register(pipeline, sGnRawElement, "raw");
    if (sGnRspElement != NULL) {
        audio_pipeline_register(pipeline, sGnRspElement, "resampler");
        const char *link_tag[3] = {"i2s", "resampler", "raw"};
        if (audio_pipeline_link(pipeline, &link_tag[0], 3) != ESP_OK) {
            ESP_LOGE(TAG, "audio_pipeline_link failed");
            return false;
        }
    } else {
        const char *link_tag[2] = {"i2s", "raw"};
        if (audio_pipeline_link(pipeline, &link_tag[0], 2) != ESP_OK) {
            ESP_LOGE(TAG, "audio_pipeline_link failed");
            return false;
        }
    }
    if (audio_pipeline_run(pipeline) != ESP_OK) {
        ESP_LOGE(TAG, "audio_pipeline_run failed");
        return false;
    }

    sGnModelData = sGnWakenet->create(sGnModelCoeffGetter, DET_MODE_90);

    // main app works on cpu#0, and voice engine works on cpu#1
    xTaskCreatePinnedToCore(&GnVoiceEngine_recordTask, "fetch", 4 * 1024, NULL, 5, NULL, 1);
    return true;
}

bool GnVoiceEngine_recorderStart(int sampleRate, int channelCount, int bitsPerSample)
{
    if (sampleRate != GENIE_RECORD_SAMPLE_RATE ||
        channelCount != GENIE_RECORD_CHANNEL_COUNT ||
        bitsPerSample != GENIE_RECORD_SAMPLE_BIT) {
        ESP_LOGE(TAG, "invalid parameters");
        return false;
    }
    litevad_reset(sGnVadHandle);
    rb_reset(sGnRingbuf);
    sGnVadActive = false;
    sGnIsRecording = true;
    return true;
}

int GnVoiceEngine_recorderRead(void *buffer, int size)
{
    return rb_read(sGnRingbuf, buffer, size, portMAX_DELAY);
}

void GnVoiceEngine_recorderStop()
{
    sGnIsRecording = false;
    rb_abort(sGnRingbuf);
}
