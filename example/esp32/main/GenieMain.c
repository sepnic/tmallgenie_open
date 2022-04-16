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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/mlooper.h"
#include "json/cJSON.h"

#include "GenieSdk.h"
#include "GenieAdapter.h"

#include "esp_peripherals.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "periph_wifi.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "board.h"
#include "board_pins_config.h"

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#else
#define ESP_IDF_VERSION_VAL(major, minor, patch) 1
#endif
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_event.h"
#include "esp_netif.h"
#else
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#endif

#define TAG "GenieMain"

static GnVendor_Wrapper_t   sSdkAdapter;
static GenieSdk_Callback_t *sSdkCallback = NULL;
static mlooper_handle       sSdkLooper = NULL;
static bool                 sIsWifiConnected = false;

enum {
    WHAT_EVENT_WIFI_CONNECTED       = 0,
    WHAT_EVENT_WIFI_DISCONNECTED    = 1,
    WHAT_EVENT_KEY_RECORD           = 2,
    WHAT_EVENT_KEY_MUTE             = 3,
    WHAT_EVENT_KEY_VOLUP            = 4,
    WHAT_EVENT_KEY_VOLDOWN          = 5,
};

static bool Genie_Event_PostMessage(int what, int arg1, int arg2, void *data)
{
    if (sSdkLooper != NULL) {
        struct message *msg = message_obtain(what, arg1, arg2, data);
        if (msg != NULL && mlooper_post_message(sSdkLooper, msg) == 0)
            return true;
        else if (msg != NULL)
            OS_FREE(msg);
    }
    return false;
}

static void Genie_Event_Handler(struct message *msg)
{
    switch (msg->what) {
    case WHAT_EVENT_WIFI_CONNECTED:
        sSdkCallback->onNetworkConnected();
        break;
    case WHAT_EVENT_WIFI_DISCONNECTED:
        sSdkCallback->onNetworkDisconnected();
        break;
    case WHAT_EVENT_KEY_RECORD:
        sSdkCallback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);
        break;
    case WHAT_EVENT_KEY_MUTE:
        if (sSdkAdapter.getSpeakerMuted != NULL && sSdkAdapter.setSpeakerMuted != NULL) {
            bool muted = !sSdkAdapter.getSpeakerMuted();
            if (sSdkAdapter.setSpeakerMuted(muted))
                sSdkCallback->onSpeakerMutedChanged(muted);
        }
        break;
    case WHAT_EVENT_KEY_VOLUP:
        if (sSdkAdapter.getSpeakerVolume != NULL && sSdkAdapter.setSpeakerVolume != NULL) {
            int volume = sSdkAdapter.getSpeakerVolume() + 10;
            volume = volume <= 100 ? volume : 100;
            if (sSdkAdapter.setSpeakerVolume(volume))
                sSdkCallback->onSpeakerVolumeChanged(volume);
        }
        break;
    case WHAT_EVENT_KEY_VOLDOWN:
        if (sSdkAdapter.getSpeakerVolume != NULL && sSdkAdapter.setSpeakerVolume != NULL) {
            int volume = sSdkAdapter.getSpeakerVolume() - 10;
            volume = volume >= 10 ? volume : 10;
            if (sSdkAdapter.setSpeakerVolume(volume))
                sSdkCallback->onSpeakerVolumeChanged(volume);
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown event message: %d", msg->what);
        break;
    }
}

static void Genie_Command_Handler(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    if (command == GENIE_COMMAND_GuestDeviceActivateResp) {
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) return;
        cJSON *uuidJson = cJSON_GetObjectItem(payloadJson, "uuid");
        cJSON *accessTokenJson = cJSON_GetObjectItem(payloadJson, "accessToken");
        char *uuid = NULL;
        char *accessToken = NULL;
        if (uuidJson != NULL) uuid = cJSON_GetStringValue(uuidJson);
        if (accessTokenJson != NULL) accessToken = cJSON_GetStringValue(accessTokenJson);
        if (uuid != NULL && accessToken != NULL) {
            ESP_LOGI(TAG, "Account already authorized: uuid=%s, accessToken=%s", uuid, accessToken);
            GnVendor_updateAccount(uuid, accessToken);
        }
        cJSON_Delete(payloadJson);
    }
}

static void *Genie_Main_Entry(void *arg)
{
    if (!GenieSdk_Init(&sSdkAdapter)) {
        ESP_LOGE(TAG, "Failed to GenieSdk_Init");
        return NULL;
    }
    if (!GenieSdk_Get_Callback(&sSdkCallback)) {
        ESP_LOGE(TAG, "Failed to GenieSdk_Get_Callback");
        goto __exit;
    }
    if (!GenieSdk_Register_CommandListener(Genie_Command_Handler)) {
        ESP_LOGE(TAG, "Failed to GenieSdk_Register_CommandListener");
        goto __exit;
    }
    if (!GenieSdk_Start()) {
        ESP_LOGE(TAG, "Failed to GenieSdk_Start");
        goto __exit;
    }

    if (sIsWifiConnected)
        sSdkCallback->onNetworkConnected();

    struct os_thread_attr thread_attr = {
        .name = "GnVendorEvent",
        .priority = OS_THREAD_PRIO_NORMAL,
        .stacksize = 4096,
        .joinable = true,
    };
    sSdkLooper = mlooper_create(&thread_attr, Genie_Event_Handler, NULL);
    if (sSdkLooper == NULL) {
        ESP_LOGE(TAG, "Failed to mlooper_create");
        goto __exit;
    }
    if (mlooper_start(sSdkLooper) != 0) {
        ESP_LOGE(TAG, "Failed to mlooper_start");
        goto __exit;
    }

    while (1)
        os_thread_sleep_msec(1000);

__exit:
    if (sSdkLooper != NULL) {
        mlooper_stop(sSdkLooper);
        mlooper_destroy(sSdkLooper);
        sSdkLooper = NULL;
    }
    GenieSdk_Stop();
    return NULL;
}

static esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
    switch (event->source_type) {
    case PERIPH_ID_WIFI: {
        if (event->cmd == PERIPH_WIFI_CONNECTED) {
            ESP_LOGW(TAG, "WIFI_CONNECTED");
            sIsWifiConnected = true;
            Genie_Event_PostMessage(WHAT_EVENT_WIFI_CONNECTED, 0, 0, NULL);
        } else if (event->cmd == PERIPH_WIFI_DISCONNECTED) {
            ESP_LOGW(TAG, "WIFI_DISCONNECTED");
            sIsWifiConnected = false;
            Genie_Event_PostMessage(WHAT_EVENT_WIFI_DISCONNECTED, 0, 0, NULL);
        }
    }
        break;
    case PERIPH_ID_BUTTON:
    case PERIPH_ID_ADC_BTN: {
        if (event->cmd == PERIPH_BUTTON_PRESSED || event->cmd == PERIPH_ADC_BUTTON_PRESSED) {
            if ((int)(event->data) == get_input_rec_id()) {
                ESP_LOGW(TAG, "KEY_REC_PRESSED");
                Genie_Event_PostMessage(WHAT_EVENT_KEY_RECORD, 0, 0, NULL);
            } else if ((int)(event->data) == get_input_volup_id()) {
                ESP_LOGW(TAG, "KEY_VOLUP_PRESSED");
                Genie_Event_PostMessage(WHAT_EVENT_KEY_VOLUP, 0, 0, NULL);
            } else if ((int)(event->data) == get_input_voldown_id()) {
                ESP_LOGW(TAG, "KEY_VOLDOWN_PRESSED");
                Genie_Event_PostMessage(WHAT_EVENT_KEY_VOLDOWN, 0, 0, NULL);
            } else if ((int)(event->data) == get_input_set_id()) {
                ESP_LOGW(TAG, "KEY_SET_PRESSED");
                Genie_Event_PostMessage(WHAT_EVENT_KEY_MUTE, 0, 0, NULL);
            } else if ((int)(event->data) == get_input_mode_id()) {
                ESP_LOGW(TAG, "KEY_MODE_PRESSED");
            }
        }
    }
        break;
    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{
    printf("Hello TmallGenie\n");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "Start and wait for Wi-Fi network");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    esp_periph_set_register_callback(set, periph_callback, NULL);
    audio_board_key_init(set);
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    if (!GnVendor_init())
        return;
    sSdkAdapter.bizType = GnVendor_bizType;
    sSdkAdapter.bizGroup = GnVendor_bizGroup;
    sSdkAdapter.bizSecret = GnVendor_bizSecret;
    sSdkAdapter.caCert = GnVendor_caCert;
    sSdkAdapter.macAddr = GnVendor_macAddr;
    sSdkAdapter.uuid = GnVendor_uuid;
    sSdkAdapter.accessToken = GnVendor_accessToken;
    sSdkAdapter.pcmOutOpen = GnVendor_pcmOutOpen;
    sSdkAdapter.pcmOutWrite = GnVendor_pcmOutWrite;
    sSdkAdapter.pcmOutClose = GnVendor_pcmOutClose;
    sSdkAdapter.pcmInOpen = GnVendor_pcmInOpen;
    sSdkAdapter.pcmInRead = GnVendor_pcmInRead;
    sSdkAdapter.pcmInClose = GnVendor_pcmInClose;
    sSdkAdapter.setSpeakerVolume = GnVendor_setSpeakerVolume;
    sSdkAdapter.getSpeakerVolume = GnVendor_getSpeakerVolume;
    sSdkAdapter.setSpeakerMuted = GnVendor_setSpeakerMuted;
    sSdkAdapter.getSpeakerMuted = GnVendor_getSpeakerMuted;

    struct os_thread_attr attr = {
        .name = "GenieMain",
        .priority = OS_THREAD_PRIO_NORMAL,
        .stacksize = 4096,
        .joinable = false,
    };
    os_thread_create(&attr, Genie_Main_Entry, NULL);
}
