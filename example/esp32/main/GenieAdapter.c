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
#include "GenieSdk.h"
#include "GenieVoiceEngine.h"
#include "GenieAdapter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "board.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_log.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "ringbuf.h"
#include "i2s_stream.h"
#include "filter_resample.h"

#define TAG "GenieAdapter"

#define GENIE_NVS_NAMESPACE         "TMALLGENIE"
#define GENIE_NVS_KEY_UUID          "uuid"
#define GENIE_NVS_KEY_ACCESSTOKEN   "accessToken"
#define GENIE_NVS_KEY_VOLUME        "volume"

#define PLAYBACK_RINGBUFFER_SIZE     4096
#define PLAYBACK_RESAMPLE_COMPLEXITY 2

static nvs_handle              sNvsHandle;

static audio_board_handle_t    sOutAudioCodec = NULL;
static audio_pipeline_handle_t sOutPipeline = NULL;
static ringbuf_handle_t        sOutSrcRingbuf = NULL;
static audio_element_handle_t  sOutRspElement = NULL;
static audio_element_handle_t  sOutI2sElement = NULL;

static int              sSpeakerVolume = 0;
static bool             sSpeakerMuted = false;

static bool             sGnAccountAuthorized = true;
static char             sGnAccountUuid[256];
static char             sGnAccountAccessToken[256];

static bool             sIsInited = false;

bool GnVendor_init()
{
    if (sIsInited)
        return true;

    if (nvs_open(GENIE_NVS_NAMESPACE, NVS_READWRITE, &sNvsHandle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return false;
    }

    size_t uuidLen = sizeof(sGnAccountUuid);
    memset(sGnAccountUuid, 0x0, uuidLen);
    if (nvs_get_str(sNvsHandle, GENIE_NVS_KEY_UUID, sGnAccountUuid, &uuidLen) != ESP_OK)
        sGnAccountAuthorized = false;
    size_t accessTokenLen = sizeof(sGnAccountAccessToken);
    memset(sGnAccountAccessToken, 0x0, accessTokenLen);
    if (nvs_get_str(sNvsHandle, GENIE_NVS_KEY_ACCESSTOKEN, sGnAccountAccessToken, &accessTokenLen) != ESP_OK)
        sGnAccountAuthorized = false;

    ESP_LOGI(TAG, "Start audio codec chip");
    sOutAudioCodec = audio_board_init();
    audio_hal_ctrl_codec(sOutAudioCodec->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    audio_hal_set_volume(sOutAudioCodec->adc_hal, 100); // set micphone gain
#endif

    ESP_LOGI(TAG, "Start player pipeline");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    if ((sOutPipeline = audio_pipeline_init(&pipeline_cfg)) == NULL) {
        ESP_LOGE(TAG, "audio_pipeline_init failed");
        return false;
    }
    if ((sOutSrcRingbuf = rb_create(PLAYBACK_RINGBUFFER_SIZE, 1)) == NULL) {
        ESP_LOGE(TAG, "rb_create failed");
        return false;
    }
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = ESP_I2S_FIXED_RATE;
    rsp_cfg.src_ch = ESP_I2S_FIXED_CHANNEL;
    rsp_cfg.dest_rate = ESP_I2S_FIXED_RATE;
    rsp_cfg.dest_ch = ESP_I2S_FIXED_CHANNEL;
    rsp_cfg.mode = RESAMPLE_DECODE_MODE;
    rsp_cfg.complexity = PLAYBACK_RESAMPLE_COMPLEXITY;
    if ((sOutRspElement = rsp_filter_init(&rsp_cfg)) == NULL) {
        ESP_LOGE(TAG, "rsp_filter_init failed");
        return false;
    }
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    if ((sOutI2sElement = i2s_stream_init(&i2s_cfg)) == NULL) {
        ESP_LOGE(TAG, "i2s_stream_init failed");
        return false;
    }
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(sOutI2sElement, &i2s_info);
    i2s_info.bits = 16;
    i2s_info.channels = ESP_I2S_FIXED_CHANNEL;
    i2s_info.sample_rates = ESP_I2S_FIXED_RATE;
    audio_element_setinfo(sOutI2sElement, &i2s_info);
    if (audio_pipeline_register(sOutPipeline, sOutRspElement, "resample_filter") != ESP_OK ||
        audio_pipeline_register(sOutPipeline, sOutI2sElement, "i2s_writer") != ESP_OK) {
        ESP_LOGE(TAG, "audio_pipeline_register failed");
        return false;
    }
    const char *link_tag[2] = {"resample_filter", "i2s_writer"};
    if (audio_pipeline_link(sOutPipeline, &link_tag[0], 2) != ESP_OK) {
        ESP_LOGE(TAG, "audio_pipeline_link failed");
        return false;
    }
    if (audio_element_set_input_ringbuf(sOutRspElement, sOutSrcRingbuf) != ESP_OK) {
        ESP_LOGE(TAG, "audio_element_set_input_ringbuf failed");
        return false;
    }

    if (!GnVoiceEngine_init()) {
        ESP_LOGE(TAG, "GnVoiceEngine_init failed");
        return false;
    }

    sIsInited = true;
    return true;
}

// system & account info
const char *GnVendor_bizType()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_bizGroup()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_bizSecret()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_caCert()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_macAddr()
{
    static char mac[32] = {0};
    if (esp_read_mac((uint8_t *)mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return mac;
    }
    return NULL;
}

const char *GnVendor_uuid()
{
    if (sGnAccountAuthorized)
        return sGnAccountUuid;
    else
        return NULL;
}

const char *GnVendor_accessToken()
{
    if (sGnAccountAuthorized)
        return sGnAccountAccessToken;
    else
        return NULL;
}

void GnVendor_updateAccount(const char *uuid, const char *accessToken)
{
    nvs_set_str(sNvsHandle, GENIE_NVS_KEY_UUID, uuid);
    nvs_set_str(sNvsHandle, GENIE_NVS_KEY_ACCESSTOKEN, accessToken);

    if (strlen(uuid) < sizeof(sGnAccountUuid) &&
        strlen(accessToken) < sizeof(sGnAccountAccessToken)) {
        snprintf(sGnAccountUuid, sizeof(sGnAccountUuid), "%s", uuid);
        snprintf(sGnAccountAccessToken, sizeof(sGnAccountAccessToken), "%s", accessToken);
        sGnAccountAuthorized = true;
    }
}

// audio pcm out
void *GnVendor_pcmOutOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    ESP_LOGD(TAG, "Opening pcm_out: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    if (bitsPerSample != 16) {
        ESP_LOGE(TAG, "invalid bitsPerSample");
        return NULL;
    }
    if (audio_pipeline_reset_ringbuffer(sOutPipeline) != ESP_OK) {
        ESP_LOGE(TAG, "audio_pipeline_reset_ringbuffer failed");
        return NULL;
    }
    if (rsp_filter_set_src_info(sOutRspElement, sampleRate, channelCount) != ESP_OK) {
        ESP_LOGE(TAG, "rsp_filter_set_src_info failed");
        return NULL;
    }
    if (i2s_stream_set_clk(sOutI2sElement, ESP_I2S_FIXED_RATE, 16, ESP_I2S_FIXED_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_stream_set_clk failed");
        return NULL;
    }
    if (audio_pipeline_run(sOutPipeline) != ESP_OK) {
        ESP_LOGE(TAG, "audio_pipeline_run failed");
        return NULL;
    }
    return (void *)0xffff;
}

int GnVendor_pcmOutWrite(void *handle, void *buffer, unsigned int size)
{
    return rb_write(sOutSrcRingbuf, buffer, size, portMAX_DELAY);
}

void GnVendor_pcmOutClose(void *handle)
{
    ESP_LOGD(TAG, "closing pcm_out");
    rb_done_write(sOutSrcRingbuf);
    while (1) {
        audio_element_state_t state = audio_element_get_state(sOutI2sElement);
        if (state == AEL_STATE_STOPPED || state == AEL_STATE_FINISHED || state == AEL_STATE_ERROR)
            break;
        vTaskDelay(20/portTICK_RATE_MS);
    }
    audio_pipeline_stop(sOutPipeline);
    audio_pipeline_wait_for_stop(sOutPipeline);
    audio_pipeline_terminate(sOutPipeline);
}

// audio pcm in
void *GnVendor_pcmInOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    ESP_LOGD(TAG, "Opening i2s_in: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    if (GnVoiceEngine_recorderStart(sampleRate, channelCount, bitsPerSample))
        return (void *)0xffff;
    else
        return NULL;
}

int GnVendor_pcmInRead(void *handle, void *buffer, unsigned int size)
{
    int bytes_read = GnVoiceEngine_recorderRead(buffer, size);
    if (bytes_read < 0)
        ESP_LOGE(TAG, "i2s_read failed");
    return bytes_read;
}

void GnVendor_pcmInClose(void *handle)
{
    ESP_LOGD(TAG, "closing i2s_in");
    GnVoiceEngine_recorderStop();
}

// audio system
bool GnVendor_setSpeakerVolume(int volume)
{
    if (sSpeakerVolume == volume)
        return false;
    audio_hal_set_volume(sOutAudioCodec->audio_hal, volume);
    nvs_set_u8(sNvsHandle, GENIE_NVS_KEY_VOLUME, volume);
    sSpeakerVolume = volume;
    return true;
}

int GnVendor_getSpeakerVolume()
{
    uint8_t volume = (uint8_t)sSpeakerVolume;
    if (nvs_get_u8(sNvsHandle, GENIE_NVS_KEY_VOLUME, &volume) == ESP_OK)
        return (int)volume;
    return sSpeakerVolume;
}

bool GnVendor_setSpeakerMuted(bool muted)
{
    sSpeakerMuted = muted;
    return true;
}

bool GnVendor_getSpeakerMuted()
{
    return sSpeakerMuted;
}
