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
#include "json/cJSON.h"

#include "GenieSdk.h"
#include "GenieVendor.h"

#define TAG "GenieMain"

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
            OS_LOGI(TAG, "Account already authorized: uuid=%s, accessToken=%s", uuid, accessToken);
            GnVendor_updateAccount(uuid, accessToken);
        }
        cJSON_Delete(payloadJson);
    }
}

int main(int argc, char **argv)
{
    GenieSdk_Callback_t *sdkCallback = NULL;

    if (!GnVendor_init()) {
        OS_LOGE(TAG, "Failed to GnVendor_init");
        return -1;
    }
    GnVendor_Wrapper_t adapter = {
        .bizType = GnVendor_bizType,
        .bizGroup = GnVendor_bizGroup,
        .bizSecret = GnVendor_bizSecret,
        .caCert = GnVendor_caCert,
        .macAddr = GnVendor_macAddr,
        .uuid = GnVendor_uuid,
        .accessToken = GnVendor_accessToken,
        .pcmOutOpen = GnVendor_pcmOutOpen,
        .pcmOutWrite = GnVendor_pcmOutWrite,
        .pcmOutClose = GnVendor_pcmOutClose,
        .pcmInOpen = GnVendor_pcmInOpen,
        .pcmInRead = GnVendor_pcmInRead,
        .pcmInClose = GnVendor_pcmInClose,
        .setSpeakerVolume = GnVendor_setSpeakerVolume,
        .getSpeakerVolume = GnVendor_getSpeakerVolume,
        .setSpeakerMuted = GnVendor_setSpeakerMuted,
        .getSpeakerMuted = GnVendor_getSpeakerMuted,
    };

    if (!GenieSdk_Init(&adapter)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Init");
        return -1;
    }
    if (!GenieSdk_Get_Callback(&sdkCallback)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Get_Callback");
        goto __exit;
    }
    if (!GenieSdk_Register_CommandListener(Genie_Command_Handler)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Register_CommandListener");
        goto __exit;
    }
    if (!GenieSdk_Start()) {
        OS_LOGE(TAG, "Failed to GenieSdk_Start");
        goto __exit;
    }

    // TODO: add network status listener
    sdkCallback->onNetworkConnected();

    char input = 0;
    while (1) {
        OS_LOGW(TAG, "Waiting key event:");
        OS_LOGW(TAG, "  Q|q   : quit");
        OS_LOGW(TAG, "  Enter : trigger voice interaction");
        input = getc(stdin);
        if (input == '\n') {
            sdkCallback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);
        } else if (input == 'Q' || input == 'q') {
            OS_LOGW(TAG, "Quit");
            break;
        }
    }

__exit:
    GenieSdk_Stop();
    OS_MEMORY_DUMP();
    return 0;
}
