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
    switch (command) {
    case GENIE_COMMAND_GuestDeviceActivateResp:
    case GENIE_COMMAND_MemberDeviceActivateResp: {
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) return;
        cJSON *uuidJson = cJSON_GetObjectItem(payloadJson, "uuid");
        cJSON *accessTokenJson = cJSON_GetObjectItem(payloadJson, "accessToken");
        char *uuid = NULL;
        char *accessToken = NULL;
        if (uuidJson != NULL)
            uuid = cJSON_GetStringValue(uuidJson);
        if (accessTokenJson != NULL)
            accessToken = cJSON_GetStringValue(accessTokenJson);
        if (uuid != NULL && accessToken != NULL) {
            OS_LOGI(TAG, "Account already authorized: uuid=%s, accessToken=%s", uuid, accessToken);
            GnVendor_updateAccount(uuid, accessToken);
        }
        cJSON_Delete(payloadJson);
    }
        break;
    case GENIE_COMMAND_UserInfoResp: {
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) return;
        OS_LOGI(TAG, "UserInfoResp: payload=%s", payload);
        cJSON *userTypeJson = cJSON_GetObjectItem(payloadJson, "userType");
        char *userType = NULL;
        if (userTypeJson != NULL)
            userType = cJSON_GetStringValue(userTypeJson);
        if (userType != NULL && strcmp(userType, "guest") == 0) {
            cJSON *qrCodeJson = cJSON_GetObjectItem(payloadJson, "qrCode");
            char *qrCode = NULL;
            if (qrCodeJson != NULL)
                qrCode = cJSON_GetStringValue(qrCodeJson);
            if (qrCode != NULL)
                OS_LOGW(TAG, "User type is guest, please scan the qrCode with tmallgenie app to"
                        " bind the device as a member: qrCode=%s", qrCode);
        }
        cJSON_Delete(payloadJson);
    }
        break;
    default:
        break;
    }
}

void Genie_Status_Handler(Genie_Status_t status)
{
    switch (status) {
    case GENIE_STATUS_NetworkDisconnected:
        OS_LOGI(TAG, "-->NetworkDisconnected");
        break;
    case GENIE_STATUS_NetworkConnected:
        OS_LOGI(TAG, "-->NetworkConnected");
        break;
    case GENIE_STATUS_GatewayDisconnected:
        OS_LOGI(TAG, "-->GatewayDisconnected");
        break;
    case GENIE_STATUS_GatewayConnected:
        OS_LOGI(TAG, "-->GatewayConnected");
        break;
    case GENIE_STATUS_Unauthorized:
        OS_LOGI(TAG, "-->Unauthorized");
        break;
    case GENIE_STATUS_Authorized:
        OS_LOGI(TAG, "-->Authorized");
        break;
    case GENIE_STATUS_SpeakerUnmuted:
        OS_LOGI(TAG, "-->SpeakerUnmuted");
        break;
    case GENIE_STATUS_SpeakerMuted:
        OS_LOGI(TAG, "-->SpeakerMuted");
        break;
    case GENIE_STATUS_MicphoneWakeup:
        OS_LOGI(TAG, "-->MicphoneWakeup");
        break;
    case GENIE_STATUS_MicphoneStarted:
        OS_LOGI(TAG, "-->MicphoneStarted");
        break;
    case GENIE_STATUS_MicphoneStopped:
        OS_LOGI(TAG, "-->MicphoneStopped");
        break;
    default:
        break;
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
    if (!GenieSdk_Register_StatusListener(Genie_Status_Handler)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Register_StatusListener");
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
        OS_LOGW(TAG, "  T|t   : trigger text recognition");
        OS_LOGW(TAG, "  V|v   : trigger voice interaction");
        OS_LOGW(TAG, "  U|u   : query user info");
        input = getc(stdin);
        if (input == 'V' || input == 'v') {
            sdkCallback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);
        } else if (input == 'T' || input == 't') {
            sdkCallback->onTextRecognize("来点音乐");
        } else if (input == 'U' || input == 'u') {
            sdkCallback->onQueryUserInfo();
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
