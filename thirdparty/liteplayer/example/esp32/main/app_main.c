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
#include <unistd.h>
#include <string.h>
#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "liteplayer_main.h"
#include "source_httpclient_wrapper.h"
#include "sink_esp32_i2s_wrapper.h"

#include "esp_peripherals.h"
#include "esp_wifi.h"
#include "periph_wifi.h"
#include "nvs_flash.h"
#include "board.h"
#include "driver/i2s.h"
#include "board_pins_config.h"
#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#else
#define ESP_IDF_VERSION_VAL(major, minor, patch) 1
#endif
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#define TAG "liteplayer_demo"

#define I2S_PORT        I2S_NUM_0
#define MCLK_GPIO       GPIO_NUM_0
#define DEFAULT_VOLUME  50

#define HTTP_URL "http://ailabsaicloudservice.alicdn.com/player/resources/23a2d715f019c0e345235f379fa26a30.mp3"

static int liteplayer_demo_state_listener(enum liteplayer_state state, int errcode, void *priv)
{
    enum liteplayer_state *player_state = (enum liteplayer_state *)priv;
    bool state_sync = true;

    switch (state) {
    case LITEPLAYER_IDLE:
        OS_LOGD(TAG, "-->LITEPLAYER_IDLE");
        break;
    case LITEPLAYER_INITED:
        OS_LOGD(TAG, "-->LITEPLAYER_INITED");
        break;
    case LITEPLAYER_PREPARED:
        OS_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        break;
    case LITEPLAYER_STARTED:
        OS_LOGD(TAG, "-->LITEPLAYER_STARTED");
        break;
    case LITEPLAYER_PAUSED:
        OS_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        state_sync = false;
        break;
    case LITEPLAYER_COMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        break;
    case LITEPLAYER_STOPPED:
        OS_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        break;
    case LITEPLAYER_ERROR:
        OS_LOGE(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        break;
    default:
        OS_LOGE(TAG, "-->LITEPLAYER_UNKNOWN: %d", state);
        state_sync = false;
        break;
    }

    if (state_sync)
        *player_state = state;
    return 0;
}

static void *liteplayer_demo_thread(void *arg)
{
    OS_LOGI(TAG, "liteplayer_demo_thread enter");
    const char *url = (const char *)arg;

    liteplayer_handle_t player = liteplayer_create();
    if (player == NULL)
        return NULL;

    enum liteplayer_state player_state = LITEPLAYER_IDLE;
    liteplayer_register_state_listener(player, liteplayer_demo_state_listener, (void *)&player_state);

    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = esp32_i2s_out_wrapper_name,
        .open = esp32_i2s_out_wrapper_open,
        .write = esp32_i2s_out_wrapper_write,
        .close = esp32_i2s_out_wrapper_close,
    };
    liteplayer_register_sink_wrapper(player, &sink_ops);

    struct source_wrapper http_ops = {
        .async_mode = true,
        .buffer_size = 32*1024,
        .priv_data = NULL,
        .url_protocol = httpclient_wrapper_url_protocol,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .content_pos = httpclient_wrapper_content_pos,
        .content_len = httpclient_wrapper_content_len,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_register_source_wrapper(player, &http_ops);

    if (liteplayer_set_data_source(player, url) != 0) {
        OS_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_prepare_async(player) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_PREPARED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }
    if (player_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (liteplayer_start(player) != 0) {
        OS_LOGE(TAG, "Failed to start player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_COMPLETED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }

    if (liteplayer_stop(player) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_STOPPED) {
        os_thread_sleep_msec(100);
    }

test_done:
    liteplayer_reset(player);
    while (player_state != LITEPLAYER_IDLE) {
        os_thread_sleep_msec(100);
    }
    os_thread_sleep_msec(1000);
    liteplayer_destroy(player);
    os_thread_sleep_msec(100);

    OS_LOGI(TAG, "liteplayer_demo_thread leave");
    return NULL;
}

void app_main()
{
    OS_LOGI(TAG, "Hello liteplayer\n");

    OS_LOGI(TAG, "Init nvs flash");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    OS_LOGI(TAG, "Start and wait for Wi-Fi network");
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    OS_LOGI(TAG, "Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, DEFAULT_VOLUME);

    OS_LOGI(TAG, "Start i2s driver");
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
        .dma_buf_count = 3,
        .dma_buf_len = 300,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };
    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != 0) {
        OS_LOGE(TAG, "i2s_set_clk failed");
        return;
    }
    i2s_pin_config_t i2s_pin_cfg = {0};
    get_i2s_pins(I2S_PORT, &i2s_pin_cfg);
    if (i2s_set_pin(I2S_PORT, &i2s_pin_cfg) != 0) {
        OS_LOGE(TAG, "i2s_set_pin failed");
        i2s_driver_uninstall(I2S_PORT);
        return;
    }
    i2s_mclk_gpio_select(I2S_PORT, MCLK_GPIO);

    OS_LOGI(TAG, "Start liteplayer demo thread");
    struct os_thread_attr attr = {
        .name = "liteplayer_demo",
        .priority = OS_THREAD_PRIO_NORMAL,
        .stacksize = 8192,
        .joinable = false,
    };
    os_thread_create(&attr, liteplayer_demo_thread, HTTP_URL);
}
