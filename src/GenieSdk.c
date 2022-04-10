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

/** GENIE VOICE INTERACTION DIAGRAM
+---------------------------------------------------------------------------------------+
|                                Genie Interaction Service                              |
+-------^-- ^------+----------------^----------------+-----------^------------+---------+
        |   |      |                |                |           |            |
        |   |  9.send stop command: |     5.send start command:  |            |
        |   |    ExpectSpeechStop   |       ExpectSpeechStart    |            |
        |   |      |                |                |           |            |
        |   |      |        upload pcm data:         |           | 2.broadcast wakeup event:
        |   |      |        onMicphoneStreaming()    |           |   GENIE_STATUS_MicphoneWakeup
        |   |      |                |                |           |            |
        |   |   +--v----------------+----------------v--+        |            |
        |   |   |             Genie Recorder            |  4.request record:  |
        |   |   +--+----------------^----------------+--+    onExpectSpeech() |
        |   |      |                |                |           |            |
        |   |      |                +----------------+      +----+------------v----+
        |   |      |            6.start recording loop:     |     Genie Player     |
        |   |      |              read pcm data and upload  +----^------------+----+
        |   |      |              to interaction service         |            |
        |   |      |                                             +------------+
        |   |      +----------->10:stop recording                3.play WAKEUP_REMIND prompt:
        |   |                                                      onCommandNewPromptWakeup()
        |   |                                                      and wait prompt played done
        |   +---------------------+
        |                         |
1.keyword detected:        8.silence detected:
  onMicphoneWakeup()         onMicphoneSilence()
        |                         |
+-------+-------------------------+-------+
|          Vendor Voice Engine            |
+-------^------+-----------+------^-------+
        |      |           |      |
        +------+           +------+
0.start keyword detect,    7.start vad detect,
  waiting sound trigger      waiting micphone silence
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "osal/os_thread.h"
#include "cutils/list.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "json/cJSON.h"

#include "core/GenieService.h"
#include "player/GeniePlayer.h"
#include "recorder/GenieRecorder.h"
#include "GenieSdk.h"

#define TAG "GenieSdk"

#define GENIE_BOOTUP_VOLUME_MIN 20
#define GENIE_BOOTUP_VOLUME_MAX 70

typedef struct {
    os_mutex lock;
    GnVendor_Wrapper_t adapter;
    GnService_Callback_t *serviceCallback;
    struct listnode commandListenerList;
    struct listnode statusListenerList;
    bool isStarted;

    bool isNetworkDisconnected;
    bool isGatewayDisconnected;
    bool isAccountUnauthorized;
} GnSdk_Priv_t;

typedef struct {
    GenieCommandListener commandListener;
    struct listnode listnode;
} GnSdk_CommandListenerNode_t;

typedef struct {
    GenieStatusListener statusListener;
    struct listnode listnode;
} GnSdk_StatusListenerNode_t;

static GnSdk_Priv_t         sGnSdk;
static GenieSdk_Callback_t  sGnCallback;
static bool                 sGnInited = false;

static bool GenieSdk_IsGatewayConnected()
{
    if (sGnSdk.isNetworkDisconnected) {
        GnPlayer_Clear_Playonce();
        GnPlayer_New_Prompt(GENIE_PREBUILT_NETWORK_DISCONNECTED);
        return false;
    }
    if (sGnSdk.isAccountUnauthorized) {
        GnPlayer_Clear_Playonce();
        GnPlayer_New_Prompt(GENIE_PREBUILT_ACCOUNT_UNAUTHORIZED);
        return false;
    }
    if (sGnSdk.isGatewayDisconnected) {
        GnPlayer_Clear_Playonce();
        GnPlayer_New_Prompt(GENIE_PREBUILT_SERVER_DISCONNECTED);
        return false;
    }
    return true;
}

static void GenieSdk_CommandListener(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    switch (command) {
    case GENIE_COMMAND_Volume: {
        if (sGnSdk.adapter.setSpeakerVolume == NULL)
            break;
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) break;
        cJSON *volumeValueJson = cJSON_GetObjectItem(payloadJson, "volumeValue");
        if (volumeValueJson != NULL && cJSON_IsNumber(volumeValueJson)) {
            int valumeValue = cJSON_GetNumberValue(volumeValueJson);
            if (sGnSdk.adapter.setSpeakerVolume(valumeValue))
                sGnSdk.serviceCallback->onSpeakerVolumeChanged(valumeValue);
        }
        cJSON_Delete(payloadJson);
    }
        break;
    default:
        // todo: handle more commands if needed
        break;
    }

    os_mutex_lock(sGnSdk.lock);
    struct listnode *item;
    list_for_each(item, &sGnSdk.commandListenerList) {
        GnSdk_CommandListenerNode_t *node =
                listnode_to_item(item, GnSdk_CommandListenerNode_t, listnode);
        node->commandListener(domain, command, payload);
    }
    os_mutex_unlock(sGnSdk.lock);
}

static void GenieSdk_StatusListener(Genie_Status_t status)
{
    switch (status) {
    case GENIE_STATUS_NetworkConnected:
        sGnSdk.isNetworkDisconnected = false;
        break;
    case GENIE_STATUS_NetworkDisconnected:
        if (!sGnSdk.isNetworkDisconnected) {
            GnPlayer_Clear_Playonce();
            GnPlayer_New_Prompt(GENIE_PREBUILT_NETWORK_DISCONNECTED);
            sGnSdk.isNetworkDisconnected = true;
        }
        break;
    case GENIE_STATUS_GatewayConnected:
        sGnSdk.isGatewayDisconnected = false;
        break;
    case GENIE_STATUS_GatewayDisconnected:
        if (!sGnSdk.isGatewayDisconnected && !sGnSdk.isNetworkDisconnected && !sGnSdk.isAccountUnauthorized) {
            GnPlayer_Clear_Playonce();
            GnPlayer_New_Prompt(GENIE_PREBUILT_SERVER_DISCONNECTED);
            sGnSdk.isGatewayDisconnected = true;
        }
        break;
    case GENIE_STATUS_Authorized:
        sGnSdk.isAccountUnauthorized = false;
        break;
    case GENIE_STATUS_Unauthorized:
        if (!sGnSdk.isAccountUnauthorized) {
            GnPlayer_Clear_Playonce();
            GnPlayer_New_Prompt(GENIE_PREBUILT_ACCOUNT_UNAUTHORIZED);
            sGnSdk.isAccountUnauthorized = true;
        }
        break;
    default:
        break;
    }

    os_mutex_lock(sGnSdk.lock);
    struct listnode *item;
    list_for_each(item, &sGnSdk.statusListenerList) {
        GnSdk_StatusListenerNode_t *node =
                listnode_to_item(item, GnSdk_StatusListenerNode_t, listnode);
        node->statusListener(status);
    }
    os_mutex_unlock(sGnSdk.lock);
}

static void GenieSdk_OnNetworkConnected()
{
    sGnSdk.serviceCallback->onNetworkConnected();
}

static void GenieSdk_OnNetworkDisconnected()
{
    sGnSdk.serviceCallback->onNetworkDisconnected();
}

static void GenieSdk_OnMicphoneWakeup(const char *wakeupWord, int doa, double confidence)
{
    if (GenieSdk_IsGatewayConnected())
        sGnSdk.serviceCallback->onMicphoneWakeup(wakeupWord, doa, confidence);
}

static void GenieSdk_OnMicphoneSilence()
{
    sGnSdk.serviceCallback->onMicphoneSilence();
}

static void GenieSdk_OnSpeakerVolumeChanged(int volume)
{
    sGnSdk.serviceCallback->onSpeakerVolumeChanged(volume);
}

static void GenieSdk_OnSpeakerMutedChanged(bool muted)
{
    sGnSdk.serviceCallback->onSpeakerMutedChanged(muted);
}

static void GenieSdk_OnTextRecognize(const char *inputText)
{
    if (inputText != NULL && GenieSdk_IsGatewayConnected())
        sGnSdk.serviceCallback->onTextRecognize(inputText);
}

static void GenieSdk_OnNewPrompt(const char *url)
{
    if (url != NULL)
        GnPlayer_New_Prompt(url);
}

bool GenieSdk_Init(GnVendor_Wrapper_t *adapter)
{
    if (sGnInited)          return true;
    if (adapter == NULL)    return false;

    memset(&sGnSdk, 0x0, sizeof(sGnSdk));
    memset(&sGnCallback, 0x0, sizeof(sGnCallback));
    memcpy(&sGnSdk.adapter, adapter, sizeof(GnVendor_Wrapper_t));

    sGnCallback.onNetworkConnected      = GenieSdk_OnNetworkConnected;
    sGnCallback.onNetworkDisconnected   = GenieSdk_OnNetworkDisconnected;
    sGnCallback.onMicphoneWakeup        = GenieSdk_OnMicphoneWakeup;
    sGnCallback.onMicphoneSilence       = GenieSdk_OnMicphoneSilence;
    sGnCallback.onSpeakerVolumeChanged  = GenieSdk_OnSpeakerVolumeChanged;
    sGnCallback.onSpeakerMutedChanged   = GenieSdk_OnSpeakerMutedChanged;
    sGnCallback.onTextRecognize         = GenieSdk_OnTextRecognize;
    sGnCallback.onNewPrompt             = GenieSdk_OnNewPrompt;

    list_init(&sGnSdk.commandListenerList);
    list_init(&sGnSdk.statusListenerList);

    if ((sGnSdk.lock = os_mutex_create()) == NULL)
        goto __error_init;

    GnService_Adapter_t serviceAdapter = {
        .bizType = sGnSdk.adapter.bizType,
        .bizGroup = sGnSdk.adapter.bizGroup,
        .bizSecret = sGnSdk.adapter.bizSecret,
        .caCert = sGnSdk.adapter.caCert,
        .macAddr = sGnSdk.adapter.macAddr,
        .uuid = sGnSdk.adapter.uuid,
        .accessToken = sGnSdk.adapter.accessToken,
    };
    if (!GnService_Init(&serviceAdapter)) {
        OS_LOGE(TAG, "Failed to GnService_Init");
        goto __error_init;
    }
    if (!GnService_Get_Callback(&sGnSdk.serviceCallback)) {
        OS_LOGE(TAG, "Failed to GnService_Get_Callback");
        goto __error_init;
    }

    GnVendor_PcmOut_t playerAdapter = {
        .open = sGnSdk.adapter.pcmOutOpen,
        .write = sGnSdk.adapter.pcmOutWrite,
        .close = sGnSdk.adapter.pcmOutClose,
    };
    if (!GnPlayer_Init(&playerAdapter)) {
        OS_LOGE(TAG, "Failed to GnPlayer_Init");
        goto __error_init;
    }

    GnVendor_PcmIn_t recorderAdapter = {
        .open = sGnSdk.adapter.pcmInOpen,
        .read = sGnSdk.adapter.pcmInRead,
        .close = sGnSdk.adapter.pcmInClose,
    };
    if (!GnRecorder_Init(&recorderAdapter)) {
        OS_LOGE(TAG, "Failed to GnRecorder_Init");
        goto __error_init;
    }

    sGnInited = true;
    return true;

__error_init:
    if (sGnSdk.lock != NULL)
        os_mutex_destroy(sGnSdk.lock);
    return false;
}

bool GenieSdk_Get_Callback(GenieSdk_Callback_t **callback)
{
    if (!sGnInited || callback == NULL) {
        OS_LOGE(TAG, "Genie Sdk is NOT inited");
        return false;
    }
    *callback = &sGnCallback;
    return true;
}

bool GenieSdk_Register_CommandListener(GenieCommandListener listener)
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie Sdk is NOT inited");
        return false;
    }

    os_mutex_lock(sGnSdk.lock);
    GnSdk_CommandListenerNode_t *node;
    struct listnode *item;
    bool found = false;
    list_for_each(item, &sGnSdk.commandListenerList) {
        node = listnode_to_item(item, GnSdk_CommandListenerNode_t, listnode);
        if (node->commandListener == listener)
            found = true;
    }
    if (!found) {
        node = OS_MALLOC(sizeof(GnSdk_CommandListenerNode_t));
        if (node == NULL) {
            os_mutex_unlock(sGnSdk.lock);
            return false;
        }
        node->commandListener = listener;
        list_add_tail(&sGnSdk.commandListenerList, &node->listnode);
    }
    os_mutex_unlock(sGnSdk.lock);
    return true;
}

void GenieSdk_Unregister_CommandListener(GenieCommandListener listener)
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie Sdk is NOT inited");
        return;
    }

    os_mutex_lock(sGnSdk.lock);
    GnSdk_CommandListenerNode_t *node;
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnSdk.commandListenerList) {
        node = listnode_to_item(item, GnSdk_CommandListenerNode_t, listnode);
        if (node->commandListener == listener) {
            list_remove(item);
            OS_FREE(node);
            break;
        }
    }
    os_mutex_unlock(sGnSdk.lock);
}

bool GenieSdk_Register_StatusListener(GenieStatusListener listener)
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie Sdk is NOT inited");
        return false;
    }

    os_mutex_lock(sGnSdk.lock);
    GnSdk_StatusListenerNode_t *node;
    struct listnode *item;
    bool found = false;
    list_for_each(item, &sGnSdk.statusListenerList) {
        node = listnode_to_item(item, GnSdk_StatusListenerNode_t, listnode);
        if (node->statusListener == listener)
            found = true;
    }
    if (!found) {
        node = OS_MALLOC(sizeof(GnSdk_StatusListenerNode_t));
        if (node == NULL) {
            os_mutex_unlock(sGnSdk.lock);
            return false;
        }
        node->statusListener = listener;
        list_add_tail(&sGnSdk.statusListenerList, &node->listnode);
    }
    os_mutex_unlock(sGnSdk.lock);
    return true;
}

void GenieSdk_Unregister_StatusListener(GenieStatusListener listener)
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie Sdk is NOT inited");
        return;
    }

    os_mutex_lock(sGnSdk.lock);
    GnSdk_StatusListenerNode_t *node;
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnSdk.statusListenerList) {
        node = listnode_to_item(item, GnSdk_StatusListenerNode_t, listnode);
        if (node->statusListener == listener) {
            list_remove(item);
            OS_FREE(node);
            break;
        }
    }
    os_mutex_unlock(sGnSdk.lock);
}

bool GenieSdk_Start()
{
    if (!sGnInited) {
        OS_LOGE(TAG, "Genie Sdk is NOT inited");
        return false;
    }

    os_mutex_lock(sGnSdk.lock);

    if (sGnSdk.isStarted) {
        OS_LOGD(TAG, "Genie Sdk already started");
        os_mutex_unlock(sGnSdk.lock);
        return true;
    }

    if (sGnSdk.adapter.setSpeakerVolume != NULL && sGnSdk.adapter.getSpeakerVolume != NULL) {
        int volume = sGnSdk.adapter.getSpeakerVolume();
        if (volume > GENIE_BOOTUP_VOLUME_MAX)
            volume = GENIE_BOOTUP_VOLUME_MAX;
        else if (volume < GENIE_BOOTUP_VOLUME_MIN)
            volume = GENIE_BOOTUP_VOLUME_MIN;
        if (sGnSdk.adapter.setSpeakerVolume(volume))
            sGnSdk.serviceCallback->onSpeakerVolumeChanged(volume);
    }
    if (sGnSdk.adapter.setSpeakerMuted != NULL && sGnSdk.adapter.getSpeakerMuted != NULL) {
        bool muted = sGnSdk.adapter.getSpeakerMuted();
        if (sGnSdk.adapter.setSpeakerMuted(muted))
            sGnSdk.serviceCallback->onSpeakerMutedChanged(muted);
    }

    if (!GnService_Start()) {
        OS_LOGE(TAG, "Failed to GnService_Start");
        goto __error_start;
    }
    if (!GnPlayer_Start()) {
        OS_LOGE(TAG, "Failed to GnPlayer_Start");
        goto __error_start;
    }
    if (!GnRecorder_Start()) {
        OS_LOGE(TAG, "Failed to GnRecorder_Start");
        goto __error_start;
    }

    if (!GnService_Register_CommandListener(GenieSdk_CommandListener))
        goto __error_start;
    if (!GnService_Register_StatusListener(GenieSdk_StatusListener))
        goto __error_start;

    sGnSdk.isStarted = true;

    os_mutex_unlock(sGnSdk.lock);
    return true;

__error_start:
    GnRecorder_Stop();
    GnPlayer_Stop();
    GnService_Stop();
    GnService_Unregister_CommandListener(GenieSdk_CommandListener);
    GnService_Unregister_StatusListener(GenieSdk_StatusListener);
    os_mutex_unlock(sGnSdk.lock);
    return false;
}

bool GenieSdk_IsActive()
{
    return sGnInited && sGnSdk.isStarted;
}

void GenieSdk_Stop()
{
    os_mutex_lock(sGnSdk.lock);
    if (sGnSdk.isStarted) {
        sGnSdk.isStarted = false;
        GnRecorder_Stop();
        GnPlayer_Stop();
        GnService_Stop();
        GnService_Unregister_CommandListener(GenieSdk_CommandListener);
        GnService_Unregister_StatusListener(GenieSdk_StatusListener);
    }
    os_mutex_unlock(sGnSdk.lock);
}
