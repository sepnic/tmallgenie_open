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
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "osal/os_thread.h"
#include "osal/os_time.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "cutils/list.h"
#include "cutils/mlooper.h"
#include "json/cJSON.h"

#include "base/websocket_client.h"
#include "GenieProtocol.h"
#include "GenieService.h"

#define TAG "GenieService"

#define GENIE_SERVICE_THREAD_NAME           "GnService"
#define GENIE_SERVICE_THREAD_PRIO           OS_THREAD_PRIO_NORMAL
#define GENIE_SERVICE_THREAD_STACK          8192

#define GENIE_WEBSOCKET_HOST_NAME           "g-aicloud.alibaba.com"
#define GENIE_WEBSOCKET_HOST_PORT           443
#define GENIE_WEBSOCKET_PING_INTERVAL       20000 // 20s
#define GENIE_WEBSOCKET_RECONNECT_INTERVAL  10000 // 10s
#define GENIE_WEBSOCKET_RECONNECT_MAX       5

#define GENIE_MICPHONE_CHECKSTATE_DELAY     10000 // 10s

typedef struct {
    bool useDefaultBiz;
    const char *bizType;
    const char *bizGroup;
    const char *bizSecret;
    const char *caCert;
    const char *macAddr;
    const char *uuid;
    const char *accessToken;

    ws_client_handle_t websocket;
    mlooper_handle looper;
    os_mutex threadLock;
    os_mutex stateLock;

    int websocketReconnectCount;

    bool isLooperStarted;
    bool isNetworkConnected;  // true: connect gateway; false: disconnect gateway
    bool isWebsocketConnected;// true: send statesync event; false: try connect gateway again
    bool isAccountAuthorized; // true: allow interaction; false: disconnect gateway
    bool isMicphoneWakeup;
    bool isMicphoneStarted;   // true: add other events to pending list, don't send other events until mic inactive
    bool isSpeakerMuted;      // true: ignore mic active event and gateway commands, add other events to pending list
    bool isPlayerStarted;
    bool isStateSynced;

    struct listnode commandListenerList;
    struct listnode ttsbinaryListenerList;
    struct listnode statusListenerList;
    struct listnode pendingMsgList;           // if micphone active, add other events to pending list

    Genie_SpeechContext_t speechContext;      // update speechContext when micphone active
    Genie_SpeakerContext_t speakerContext;    // update speakerContext when speaker volume/muted changed
    Genie_PlayerContext_t playerContext;      // update playerContext when player started
    Genie_PlayerContext_t playerContextCache; // update playerContextCache when receiving audio command from gateway
} GnService_Priv_t;

typedef struct {
    void (*commandListener)(Genie_Domain_t domain, Genie_Command_t command, const char *payload);
    struct listnode listnode;
} GnService_CommandListenerNode_t;

typedef struct {
    void (*ttsbinaryListener)(char *data, int len, bool final);
    struct listnode listnode;
} GnService_TtsbinaryListenerNode_t;

typedef struct {
    void (*statusListener)(Genie_Status_t status);
    struct listnode listnode;
} GnService_StatusListenerNode_t;

typedef struct {
    struct message *msg;
    struct listnode listnode;
} GnService_PendingMsgNode_t;

static GnService_Priv_t     sGnService;
static GnService_Callback_t sGnCallback;
static bool                 sGnInited = false;

enum {
    WHAT_STATUS_WEBSOCKET_CONNECTED     = 0,
    WHAT_STATUS_WEBSOCKET_DISCONNECTED  = 1,
    WHAT_STATUS_ACCOUNT_AUTHORIZED      = 2,
    WHAT_STATUS_ACCOUNT_UNAUTHORIZED    = 3,
    WHAT_STATUS_MICPHONE_STARTED        = 4,
    WHAT_STATUS_MICPHONE_STREAMING      = 5,
    WHAT_STATUS_MICPHONE_STOPPED        = 6,
    WHAT_STATUS_SPEAKER_CHANGED         = 7,
    WHAT_STATUS_PLAYER_CHANGED          = 8,

    WHAT_COMMAND_WEBSOCKET_CONNECT      = 10,
    WHAT_COMMAND_WEBSOCKET_DISCONNECT   = 11,
    WHAT_COMMAND_WEBSOCKET_CHECKSTATE   = 12,
    WHAT_COMMAND_MICPHONE_CHECKSTATE    = 13,

    WHAT_EVENT_TEXTRECOGNIZE            = 100,
    WHAT_EVENT_QUERYUSERINFO            = 101,
};

static const char *GnStatus_ToString(Genie_Status_t status)
{
    switch (status) {
    case GENIE_STATUS_NetworkDisconnected:
        return "NetworkDisconnected";
    case GENIE_STATUS_NetworkConnected:
        return "NetworkConnected";
    case GENIE_STATUS_GatewayDisconnected:
        return "GatewayDisconnected";
    case GENIE_STATUS_GatewayConnected:
        return "GatewayConnected";
    case GENIE_STATUS_Unauthorized:
        return "Unauthorized";
    case GENIE_STATUS_Authorized:
        return "Authorized";
    case GENIE_STATUS_SpeakerUnmuted:
        return "SpeakerUnmuted";
    case GENIE_STATUS_SpeakerMuted:
        return "SpeakerMuted";
    case GENIE_STATUS_MicphoneWakeup:
        return "MicphoneWakeup";
    case GENIE_STATUS_MicphoneStarted:
        return "MicphoneStarted";
    case GENIE_STATUS_MicphoneStopped:
        return "MicphoneStopped";
    default:
        return "InvalidStatus";
    }
}

static void GnLooper_Update_PlayerContext()
{
    if (sGnService.playerContext.audioUrl != NULL)      OS_FREE(sGnService.playerContext.audioUrl);
    if (sGnService.playerContext.audioAnchor != NULL)   OS_FREE(sGnService.playerContext.audioAnchor);
    if (sGnService.playerContext.audioExt != NULL)      OS_FREE(sGnService.playerContext.audioExt);
    if (sGnService.playerContext.audioId != NULL)       OS_FREE(sGnService.playerContext.audioId);
    if (sGnService.playerContext.audioName != NULL)     OS_FREE(sGnService.playerContext.audioName);
    if (sGnService.playerContext.audioType != NULL)     OS_FREE(sGnService.playerContext.audioType);
    if (sGnService.playerContext.audioAlbum != NULL)    OS_FREE(sGnService.playerContext.audioAlbum);
    if (sGnService.playerContext.audioSource != NULL)   OS_FREE(sGnService.playerContext.audioSource);
    memcpy(&sGnService.playerContext, &sGnService.playerContextCache, sizeof(sGnService.playerContextCache));
    memset(&sGnService.playerContextCache, 0x0, sizeof(sGnService.playerContextCache));
}

static void GnLooper_Notify_CommandListener(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    os_mutex_lock(sGnService.threadLock);
    struct listnode *item;
    list_for_each(item, &sGnService.commandListenerList) {
        GnService_CommandListenerNode_t *node =
                listnode_to_item(item, GnService_CommandListenerNode_t, listnode);
        node->commandListener(domain, command, payload);
    }
    os_mutex_unlock(sGnService.threadLock);
}

static void GnLooper_Notify_TtsbinaryListener(char *data, int len, bool final)
{
    os_mutex_lock(sGnService.threadLock);
    struct listnode *item;
    list_for_each(item, &sGnService.ttsbinaryListenerList) {
        GnService_TtsbinaryListenerNode_t *node =
                listnode_to_item(item, GnService_TtsbinaryListenerNode_t, listnode);
        node->ttsbinaryListener(data, len, final);
    }
    os_mutex_unlock(sGnService.threadLock);
}

static void GnLooper_Notify_StatusListener(Genie_Status_t status)
{
    os_mutex_lock(sGnService.threadLock);
    OS_LOGI(TAG, "Notified status: %s", GnStatus_ToString(status));
    struct listnode *item;
    list_for_each(item, &sGnService.statusListenerList) {
        GnService_StatusListenerNode_t *node =
                listnode_to_item(item, GnService_StatusListenerNode_t, listnode);
        node->statusListener(status);
    }
    os_mutex_unlock(sGnService.threadLock);
}

static void GnLooper_Free_MessageData(struct message *msg)
{
    if (msg->data != NULL) {
        switch (msg->what) {
        case WHAT_STATUS_MICPHONE_STREAMING:
        case WHAT_EVENT_TEXTRECOGNIZE:
            OS_FREE(msg->data);
            break;
        default:
            OS_LOGW(TAG, "Unfree message(%d) data(%p), maybe memory leak", msg->what, msg->data);
            break;
        }
    }
}

static void GnLooper_Free_PendingMessage(struct message *msg)
{
    GnLooper_Free_MessageData(msg);
    OS_FREE(msg);
}

static void GnLooper_Clear_AllMessages()
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnService.pendingMsgList) {
        GnService_PendingMsgNode_t *node = listnode_to_item(item, GnService_PendingMsgNode_t, listnode);
        list_remove(item);
        GnLooper_Free_PendingMessage(node->msg);
        OS_FREE(node);
    }
    mlooper_clear_message(sGnService.looper);

    if (sGnService.isMicphoneWakeup) {
        if (sGnService.isMicphoneStarted)
            GnLooper_Notify_CommandListener(GENIE_DOMAIN_Microphone, GENIE_COMMAND_ExpectSpeechStop, "{}");
        else
            sGnService.isMicphoneWakeup = false;
    }

}

static void GnLooper_Clear_AllMessages_l()
{
    os_mutex_lock(sGnService.stateLock);
    GnLooper_Clear_AllMessages();
    os_mutex_unlock(sGnService.stateLock);
}

static bool GnLooper_Post_Message(int what, int arg1, int arg2, void *data)
{
    struct message *msg = message_obtain(what, arg1, arg2, data);
    if (msg == NULL)
        return false;
    if (mlooper_post_message(sGnService.looper, msg) != 0) {
        GnLooper_Free_PendingMessage(msg);
        return false;
    }
    return true;
}

static bool GnLooper_Post_DelayMessage(int what, int arg1, int arg2, void *data, unsigned long delayMs)
{
    struct message *msg = message_obtain(what, arg1, arg2, data);
    if (msg == NULL)
        return false;
    if (mlooper_post_message_delay(sGnService.looper, msg, delayMs) != 0) {
        GnLooper_Free_PendingMessage(msg);
        return false;
    }
    return true;
}

static bool GnLooper_Add_MessageToPendingList(int what, int arg1, int arg2, void *data)
{
    switch (what) {
    case WHAT_STATUS_WEBSOCKET_CONNECTED:
    case WHAT_STATUS_WEBSOCKET_DISCONNECTED:
    case WHAT_STATUS_ACCOUNT_AUTHORIZED:
    case WHAT_STATUS_ACCOUNT_UNAUTHORIZED:
    case WHAT_STATUS_MICPHONE_STARTED:
    case WHAT_STATUS_MICPHONE_STREAMING:
    case WHAT_STATUS_MICPHONE_STOPPED:
        OS_LOGE(TAG, "Network/Websocket/Micphone status should not add to pending list");
        return false;
    default:
        break;
    }

    OS_LOGI(TAG, "Message[%d] added to pending list due to micphone active or speaker muted", what);

    GnService_PendingMsgNode_t *node;
    struct message *msg;
    struct listnode *item;
    list_for_each(item, &sGnService.pendingMsgList) {
        node = listnode_to_item(item, GnService_PendingMsgNode_t, listnode);
        msg = node->msg;
        if (msg->what == what) {
            switch (msg->what) {
            case WHAT_STATUS_PLAYER_CHANGED:
                msg->arg1 = arg1;
                return true;
            case WHAT_STATUS_SPEAKER_CHANGED:
                if (msg->arg1 == arg1)
                    return true;
                break;
            case WHAT_EVENT_QUERYUSERINFO:
                return true;
            default:
                break;
            }
        }
    }

    msg = message_obtain(what, arg1, arg2, data);
    if (msg == NULL) {
        return false;
    }
    node = OS_MALLOC(sizeof(GnService_PendingMsgNode_t));
    if (node == NULL) {
        GnLooper_Free_PendingMessage(msg);
        return false;
    }
    node->msg = msg;
    list_add_tail(&sGnService.pendingMsgList, &node->listnode);
    return true;
}

static void GnCallback_OnNetworkConnected()
{
    OS_LOGI(TAG, "-->OnNetworkConnected");
    os_mutex_lock(sGnService.stateLock);

    sGnService.websocketReconnectCount = 0;
    if (sGnService.isNetworkConnected) {
        OS_LOGV(TAG, "Network already connected, ignore OnNetworkConnected");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    sGnService.isNetworkConnected = true;
    GnLooper_Clear_AllMessages();
    GnLooper_Post_Message(WHAT_COMMAND_WEBSOCKET_CONNECT, 0, 0, NULL);
    GnLooper_Notify_StatusListener(GENIE_STATUS_NetworkConnected);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnNetworkDisconnected()
{
    OS_LOGI(TAG, "-->OnNetworkDisconnected");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isNetworkConnected) {
        OS_LOGV(TAG, "Network already disconnected, ignore OnNetworkDisconnected");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    sGnService.isNetworkConnected = false;
    GnLooper_Clear_AllMessages();
    GnLooper_Post_Message(WHAT_COMMAND_WEBSOCKET_DISCONNECT, 0, 0, NULL);
    GnLooper_Notify_StatusListener(GENIE_STATUS_NetworkDisconnected);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnMicphoneWakeup(const char *wakeupWord, int doa, double confidence)
{
    OS_LOGI(TAG, "OnMicphoneWakeup");
    os_mutex_lock(sGnService.stateLock);

    sGnService.speechContext.doa = doa;
    sGnService.speechContext.confidence = confidence;
    if (wakeupWord != NULL) {
        if (sGnService.speechContext.wakeupWord != NULL) {
            if (strcmp(sGnService.speechContext.wakeupWord, wakeupWord) != 0) {
                OS_FREE(sGnService.speechContext.wakeupWord);
                sGnService.speechContext.wakeupWord = OS_STRDUP(wakeupWord);
            }
        } else {
            sGnService.speechContext.wakeupWord = OS_STRDUP(wakeupWord);
        }
    } else if (sGnService.speechContext.wakeupWord == NULL) {
        sGnService.speechContext.wakeupWord = OS_STRDUP("tian mao jing ling");
    }

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnMicphoneWakeup");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }
    if (sGnService.isMicphoneWakeup) {
        OS_LOGE(TAG, "Micphone already wakeup, ignore OnMicphoneWakeup");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }
    if (sGnService.isSpeakerMuted) {
        OS_LOGE(TAG, "Speaker muted, ignore OnMicphoneWakeup");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    sGnService.isMicphoneWakeup = true;
    GnLooper_Post_DelayMessage(WHAT_COMMAND_MICPHONE_CHECKSTATE, 0, 0, NULL, GENIE_MICPHONE_CHECKSTATE_DELAY);
    GnLooper_Notify_StatusListener(GENIE_STATUS_MicphoneWakeup);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnMicphoneStreaming(Genie_SpeechFormat_t format, char *buffer, int len, bool final)
{
    if (buffer == NULL || len < 0) return;
    OS_LOGV(TAG, "OnMicphoneStreaming: buffer=%p, len=%d, final=%d", buffer, len, final);
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isMicphoneWakeup) {
        OS_LOGE(TAG, "Micphone not wakeup, ignore OnMicphoneStreaming");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (!sGnService.isMicphoneStarted) {
        sGnService.isMicphoneStarted = true;
        sGnService.speechContext.format = format;
        GnLooper_Notify_StatusListener(GENIE_STATUS_MicphoneStarted);
        mlooper_remove_message(sGnService.looper, WHAT_COMMAND_MICPHONE_CHECKSTATE);
        GnLooper_Post_Message(WHAT_STATUS_MICPHONE_STARTED, 0, 0, NULL);
    }

    char *data = NULL;
    if (len > 0 && (data = OS_MALLOC(len)) != NULL)
        memcpy(data, buffer, len);
    GnLooper_Post_Message(WHAT_STATUS_MICPHONE_STREAMING, len, final, data);

    if (final) {
        sGnService.isMicphoneWakeup = false;
        sGnService.isMicphoneStarted = false;
        GnLooper_Post_Message(WHAT_STATUS_MICPHONE_STOPPED, 0, 0, NULL);
        GnLooper_Notify_StatusListener(GENIE_STATUS_MicphoneStopped);
        struct listnode *item, *tmp;
        list_for_each_safe(item, tmp, &sGnService.pendingMsgList) {
            GnService_PendingMsgNode_t *node = listnode_to_item(item, GnService_PendingMsgNode_t, listnode);
            list_remove(item);
            if (mlooper_post_message(sGnService.looper, node->msg) != 0)
                GnLooper_Free_PendingMessage(node->msg);
            OS_FREE(node);
        }
    }

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnExpectSpeech()
{
    OS_LOGI(TAG, "OnExpectSpeech");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnExpectSpeech");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (!sGnService.isMicphoneStarted && !sGnService.isSpeakerMuted) {
        sGnService.isMicphoneWakeup = true;
        GnLooper_Notify_CommandListener(GENIE_DOMAIN_Microphone, GENIE_COMMAND_ExpectSpeechStart, "{}");
    }

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnMicphoneSilence()
{
    OS_LOGI(TAG, "OnMicphoneSilence");
    GnLooper_Notify_CommandListener(GENIE_DOMAIN_Microphone, GENIE_COMMAND_ExpectSpeechStop, "{}");
}

static void GnCallback_OnSpeakerVolumeChanged(int volume)
{
    OS_LOGI(TAG, "OnSpeakerVolumeChanged: volume=%d", volume);
    os_mutex_lock(sGnService.stateLock);

    if (sGnService.speakerContext.volume == volume) {
        os_mutex_unlock(sGnService.stateLock);
        return;
    }
    sGnService.speakerContext.volume = volume;

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnSpeakerVolumeChanged");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_SPEAKER_CHANGED, GENIE_SPEAKERSYNC_REASON_VOLUMECHANGED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_SPEAKER_CHANGED, GENIE_SPEAKERSYNC_REASON_VOLUMECHANGED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnSpeakerMutedChanged(bool muted)
{
    OS_LOGI(TAG, "OnSpeakerMutedChanged: muted=%d", muted);
    os_mutex_lock(sGnService.stateLock);

    if (sGnService.speakerContext.isMute == muted) {
        os_mutex_unlock(sGnService.stateLock);
        return;
    }
    sGnService.isSpeakerMuted = muted;
    sGnService.speakerContext.isMute = muted;
    if (muted)
        GnLooper_Notify_StatusListener(GENIE_STATUS_SpeakerMuted);
    else
        GnLooper_Notify_StatusListener(GENIE_STATUS_SpeakerUnmuted);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnSpeakerMutedChanged");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_SPEAKER_CHANGED, GENIE_SPEAKERSYNC_REASON_MUTEDCHANGED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_SPEAKER_CHANGED, GENIE_SPEAKERSYNC_REASON_MUTEDCHANGED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerStarted()
{
    OS_LOGI(TAG, "OnPlayerStarted");
    os_mutex_lock(sGnService.stateLock);

    sGnService.isPlayerStarted = true;
    GnLooper_Update_PlayerContext();

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerStarted");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_STARTED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_STARTED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerPaused()
{
    OS_LOGI(TAG, "OnPlayerPaused");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerPaused");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_PAUSED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_PAUSED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerResumed()
{
    OS_LOGI(TAG, "OnPlayerResumed");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerResumed");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_RESUMED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_RESUMED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerNearlyFinished()
{
    OS_LOGI(TAG, "OnPlayerNearlyFinished");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerNearlyFinished");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_NEARLYFINISHED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_NEARLYFINISHED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerFinished()
{
    OS_LOGI(TAG, "OnPlayerFinished");
    os_mutex_lock(sGnService.stateLock);

    sGnService.isPlayerStarted = false;

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerFinished");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_FINISHED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_FINISHED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerStopped()
{
    OS_LOGI(TAG, "OnPlayerStopped");
    os_mutex_lock(sGnService.stateLock);

    sGnService.isPlayerStarted = false;

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerStopped");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_STOPPED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_STOPPED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnPlayerFailed()
{
    OS_LOGI(TAG, "OnPlayerFailed");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isPlayerStarted)
        GnLooper_Update_PlayerContext();
    else
        sGnService.isPlayerStarted = false;

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnPlayerFailed");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
        GnLooper_Add_MessageToPendingList(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_FAILED, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_STATUS_PLAYER_CHANGED, GENIE_PLAYERSYNC_REASON_FAILED, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnQueryUserInfo()
{
    OS_LOGI(TAG, "OnQueryUserInfo");
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnQueryUserInfo");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    if (sGnService.isMicphoneWakeup)
        GnLooper_Add_MessageToPendingList(WHAT_EVENT_QUERYUSERINFO, 0, 0, NULL);
    else
        GnLooper_Post_Message(WHAT_EVENT_QUERYUSERINFO, 0, 0, NULL);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnCallback_OnTextRecognize(const char *inputText)
{
    if (inputText == NULL) return;
    OS_LOGI(TAG, "OnTextRecognize: %s", inputText);
    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected || !sGnService.isAccountAuthorized) {
        OS_LOGE(TAG, "Websocket disconnected or account unauthorized, ignore OnTextRecognize");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    char *msgData = OS_STRDUP(inputText);
    if (msgData != NULL) {
        if (sGnService.isMicphoneWakeup || sGnService.isSpeakerMuted)
            GnLooper_Add_MessageToPendingList(WHAT_EVENT_TEXTRECOGNIZE, 0, 0, msgData);
        else
            GnLooper_Post_Message(WHAT_EVENT_TEXTRECOGNIZE, 0, 0, msgData);
    }

    os_mutex_unlock(sGnService.stateLock);
}

static void GnWebsocket_OnConnected()
{
    OS_LOGI(TAG, "-->OnWebsocketConnected");
    os_mutex_lock(sGnService.stateLock);

    sGnService.websocketReconnectCount = 0;
    if (sGnService.isWebsocketConnected) {
        OS_LOGV(TAG, "Websocket already connected, ignore OnWebsocketConnected");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    sGnService.isWebsocketConnected = true;
    GnLooper_Clear_AllMessages();
    GnLooper_Post_Message(WHAT_STATUS_WEBSOCKET_CONNECTED, 0, 0, NULL);
    GnLooper_Notify_StatusListener(GENIE_STATUS_GatewayConnected);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnWebsocket_OnDisconnected()
{
    OS_LOGI(TAG, "-->OnWebsocketDisconnected");
    if (!sGnService.isLooperStarted)
        return; // avoid deadlock when calling GnService_Stop()

    os_mutex_lock(sGnService.stateLock);

    if (!sGnService.isWebsocketConnected) {
        OS_LOGV(TAG, "Websocket already disconnected, ignore OnWebsocketDisconnected");
        os_mutex_unlock(sGnService.stateLock);
        return;
    }

    sGnService.isWebsocketConnected = false;
    GnLooper_Clear_AllMessages();
    GnLooper_Post_Message(WHAT_STATUS_WEBSOCKET_DISCONNECTED, 0, 0, NULL);
    GnLooper_Notify_StatusListener(GENIE_STATUS_GatewayDisconnected);

    os_mutex_unlock(sGnService.stateLock);
}

static void GnWebsocket_OnReceivedText(char *text, int size)
{
    OS_LOGI(TAG, "OnWebsocketReceivedText");
    if (text == NULL) return;

    cJSON *rootJson = cJSON_Parse(text);
    if (rootJson == NULL) return;
    cJSON *commandDomainJson = cJSON_GetObjectItem(rootJson, "commandDomain");
    cJSON *commandNameJson = cJSON_GetObjectItem(rootJson, "commandName");
    cJSON *payloadJson = cJSON_GetObjectItem(rootJson, "payload");
    if (commandDomainJson == NULL || commandNameJson == NULL || payloadJson == NULL) {
        cJSON_Delete(rootJson);
        return;
    }
    char *commandDomainStr = cJSON_GetStringValue(commandDomainJson);
    char *commandNameStr = cJSON_GetStringValue(commandNameJson);
    char *payloadStr = cJSON_PrintUnformatted(payloadJson);
    if (commandDomainStr == NULL || commandNameStr == NULL || payloadStr == NULL) {
        if (payloadStr != NULL) OS_FREE(payloadStr);
        cJSON_Delete(rootJson);
        return;
    }
    Genie_Domain_t commandDomain = Genie_Domain_StringToInt(commandDomainStr);
    Genie_Command_t commandName = Genie_Command_StringToInt(commandNameStr);
    if (commandDomain == -1 || commandName == -1) {
        OS_LOGE(TAG, "Unsupported command: {%s - %s}, payload: %s", commandDomainStr, commandNameStr, payloadStr);
        OS_FREE(payloadStr);
        cJSON_Delete(rootJson);
        return;
    }

    if (commandDomain != GENIE_DOMAIN_Account)
        OS_LOGI(TAG, "Receive command: {%s - %s}, payload: %s", commandDomainStr, commandNameStr, payloadStr);
    else
        OS_LOGI(TAG, "Receive command: {%s - %s}", commandDomainStr, commandNameStr);

    os_mutex_lock(sGnService.stateLock);

    switch (commandDomain) {
    case GENIE_DOMAIN_Audio:
        if (commandName == GENIE_COMMAND_Play) {
            if (sGnService.playerContextCache.audioUrl != NULL)      OS_FREE(sGnService.playerContextCache.audioUrl);
            if (sGnService.playerContextCache.audioAnchor != NULL)   OS_FREE(sGnService.playerContextCache.audioAnchor);
            if (sGnService.playerContextCache.audioExt != NULL)      OS_FREE(sGnService.playerContextCache.audioExt);
            if (sGnService.playerContextCache.audioId != NULL)       OS_FREE(sGnService.playerContextCache.audioId);
            if (sGnService.playerContextCache.audioName != NULL)     OS_FREE(sGnService.playerContextCache.audioName);
            if (sGnService.playerContextCache.audioType != NULL)     OS_FREE(sGnService.playerContextCache.audioType);
            if (sGnService.playerContextCache.audioAlbum != NULL)    OS_FREE(sGnService.playerContextCache.audioAlbum);
            if (sGnService.playerContextCache.audioSource != NULL)   OS_FREE(sGnService.playerContextCache.audioSource);
            memset(&sGnService.playerContextCache, 0x0, sizeof(sGnService.playerContextCache));

            cJSON *audioAnchorJson  = cJSON_GetObjectItem(payloadJson, "audioAnchor");
            cJSON *audioExtJson     = cJSON_GetObjectItem(payloadJson, "audioExt");
            cJSON *audioUrlJson     = cJSON_GetObjectItem(payloadJson, "audioUrl");
            cJSON *audioNameJson    = cJSON_GetObjectItem(payloadJson, "audioName");
            cJSON *audioIdJson      = cJSON_GetObjectItem(payloadJson, "audioId");
            cJSON *audioAlbumJson   = cJSON_GetObjectItem(payloadJson, "audioAlbum");
            cJSON *audioTypeJson    = cJSON_GetObjectItem(payloadJson, "audioType");
            cJSON *audioSourceJson  = cJSON_GetObjectItem(payloadJson, "audioSource");
            cJSON *progressJson     = cJSON_GetObjectItem(payloadJson, "progress");
            cJSON *audioLengthJson  = cJSON_GetObjectItem(payloadJson, "audioLength");
            char *stringvalue = NULL;
            if (audioAnchorJson != NULL && (stringvalue = cJSON_GetStringValue(audioAnchorJson)) != NULL)
                sGnService.playerContextCache.audioAnchor   = OS_STRDUP(stringvalue);
            if (audioExtJson != NULL    && (stringvalue = cJSON_GetStringValue(audioExtJson)) != NULL)
                sGnService.playerContextCache.audioExt      = OS_STRDUP(stringvalue);
            if (audioUrlJson != NULL    && (stringvalue = cJSON_GetStringValue(audioUrlJson)) != NULL)
                sGnService.playerContextCache.audioUrl      = OS_STRDUP(stringvalue);
            if (audioNameJson != NULL   && (stringvalue = cJSON_GetStringValue(audioNameJson)) != NULL)
                sGnService.playerContextCache.audioName     = OS_STRDUP(stringvalue);
            if (audioIdJson != NULL     && (stringvalue = cJSON_GetStringValue(audioIdJson)) != NULL)
                sGnService.playerContextCache.audioId       = OS_STRDUP(stringvalue);
            if (audioAlbumJson != NULL  && (stringvalue = cJSON_GetStringValue(audioAlbumJson)) != NULL)
                sGnService.playerContextCache.audioAlbum    = OS_STRDUP(stringvalue);
            if (audioTypeJson != NULL   && (stringvalue = cJSON_GetStringValue(audioTypeJson)) != NULL)
                sGnService.playerContextCache.audioType     = OS_STRDUP(stringvalue);
            if (audioSourceJson != NULL && (stringvalue = cJSON_GetStringValue(audioSourceJson)) != NULL)
                sGnService.playerContextCache.audioSource   = OS_STRDUP(stringvalue);
            if (progressJson != NULL)
                sGnService.playerContextCache.progress      = cJSON_GetNumberValue(progressJson);
            if (audioLengthJson != NULL)
                sGnService.playerContextCache.audioLength   = cJSON_GetNumberValue(audioLengthJson);
        }
        break;

    case GENIE_DOMAIN_Account:
        if (commandName == GENIE_COMMAND_GuestDeviceActivateResp ||
            commandName == GENIE_COMMAND_MemberDeviceActivateResp) {
            cJSON *dataJson = cJSON_GetObjectItem(payloadJson, "data");
            if (dataJson == NULL) break;
            cJSON *uuidJson = cJSON_GetObjectItem(dataJson, "uuid");
            cJSON *accessTokenJson = cJSON_GetObjectItem(dataJson, "accessToken");
            char *stringvalue = NULL;

            if (sGnService.uuid != NULL)        { OS_FREE(sGnService.uuid);        sGnService.uuid = NULL; }
            if (sGnService.accessToken != NULL) { OS_FREE(sGnService.accessToken); sGnService.accessToken = NULL; }

            if (uuidJson != NULL && (stringvalue = cJSON_GetStringValue(uuidJson)) != NULL)
                sGnService.uuid = OS_STRDUP(stringvalue);
            if (accessTokenJson != NULL && (stringvalue = cJSON_GetStringValue(accessTokenJson)) != NULL)
                sGnService.accessToken = OS_STRDUP(stringvalue);

            GnLooper_Clear_AllMessages();
            if (sGnService.uuid != NULL && sGnService.accessToken != NULL) {
                sGnService.isAccountAuthorized = true;
                if (commandName == GENIE_COMMAND_GuestDeviceActivateResp)
                    GnLooper_Post_Message(WHAT_EVENT_QUERYUSERINFO, 0, 0, NULL);
                GnLooper_Post_Message(WHAT_STATUS_ACCOUNT_AUTHORIZED, 0, 0, NULL);
            } else {
                sGnService.isAccountAuthorized = false;
                sGnService.isStateSynced = false;
                GnLooper_Post_Message(WHAT_STATUS_ACCOUNT_UNAUTHORIZED, 0, 0, NULL);
            }

            Genie_Status_t accountStatus =
                sGnService.isAccountAuthorized ? GENIE_STATUS_Authorized : GENIE_STATUS_Unauthorized;
            GnLooper_Notify_StatusListener(accountStatus);

            OS_FREE(payloadStr);
            unsigned int payloadLen = 128;
            if (sGnService.uuid != NULL && sGnService.accessToken != NULL)
                payloadLen += (strlen(sGnService.uuid) + strlen(sGnService.accessToken));
            payloadStr = OS_MALLOC(payloadLen);
            if (payloadStr != NULL) {
                if (sGnService.uuid != NULL && sGnService.accessToken != NULL)
                    snprintf(payloadStr, payloadLen,
                        "{\"uuid\":\"%s\", \"accessToken\":\"%s\"}", sGnService.uuid, sGnService.accessToken);
                else
                    snprintf(payloadStr, payloadLen, "{}");
            }
        }
        break;

    case GENIE_DOMAIN_System:
        if (commandName == GENIE_COMMAND_ThrowException) {
            cJSON *errorCodeJson = cJSON_GetObjectItem(payloadJson, "errorCode");
            if (errorCodeJson == NULL) break;
            Genie_Error_t errorCode = (Genie_Error_t)cJSON_GetNumberValue(errorCodeJson);

            if (errorCode == GENIE_ERROR_Unauthorized) {
                sGnService.isStateSynced = false;
                sGnService.isAccountAuthorized = false;
                if (sGnService.uuid != NULL)        { OS_FREE(sGnService.uuid);        sGnService.uuid = NULL; }
                if (sGnService.accessToken != NULL) { OS_FREE(sGnService.accessToken); sGnService.accessToken = NULL; }

                GnLooper_Clear_AllMessages();
                GnLooper_Post_Message(WHAT_STATUS_ACCOUNT_UNAUTHORIZED, 0, 0, NULL);
                GnLooper_Notify_StatusListener(GENIE_STATUS_Unauthorized);
            }
        }
        break;

    default:
        break;
    }

    os_mutex_unlock(sGnService.stateLock);

    GnLooper_Notify_CommandListener(commandDomain, commandName, payloadStr);

    if (payloadStr != NULL)
        OS_FREE(payloadStr);
    cJSON_Delete(rootJson);
}

static void GnWebsocket_OnReceivedBinary(char *data, int size, ws_binary_type_t type)
{
    OS_LOGI(TAG, "OnWebsocketReceivedBinary: data=%p, size=%d, type=%d", data, size, type);
    if (type == WS_BINARY_FRAGMENT_START)
        return; // tmallgenie: WS_BINARY_FRAGMENT_START indicates tts coming, not real tts stream
    GnLooper_Notify_TtsbinaryListener(data, size, type != WS_BINARY_FRAGMENT_CONTINUE);
}

static void GnLooper_Handle_WebsocketStatusMessage(struct message *msg)
{
    if (msg->what == WHAT_STATUS_WEBSOCKET_CONNECTED) {
        if (sGnService.uuid == NULL || sGnService.accessToken == NULL) {
            // uuid or accessToken is empty means unauthorized, try to guest activate
            char *content = NULL;
            if (sGnService.useDefaultBiz)
                content = Genie_Create_GuestActivateEvent2(sGnService.macAddr);
            else
                content = Genie_Create_GuestActivateEvent(sGnService.bizType, sGnService.bizGroup, sGnService.bizSecret,
                    sGnService.macAddr);
            if (content != NULL)
                ws_client_send_text_unique(sGnService.websocket, content, strlen(content));
        } else {
            // already authorized, send statesync event
            GnLooper_Post_Message(WHAT_STATUS_ACCOUNT_AUTHORIZED, 0, 0, NULL);
        }
    } else if (msg->what == WHAT_STATUS_WEBSOCKET_DISCONNECTED) {
        if (sGnService.isNetworkConnected)
            GnLooper_Post_DelayMessage(WHAT_COMMAND_WEBSOCKET_CONNECT, 0, 0, NULL, GENIE_WEBSOCKET_RECONNECT_INTERVAL);
        // todo: if always unauthorized, don't connect gateway any more
    }
}

static void GnLooper_Handle_AccountStatusMessage(struct message *msg)
{
    if (msg->what == WHAT_STATUS_ACCOUNT_AUTHORIZED) {
        if (sGnService.isWebsocketConnected && sGnService.isAccountAuthorized) {
            Genie_StateSyncReason_t reason =
                sGnService.isStateSynced ? GENIE_STATESYNC_REASON_RECONNECT : GENIE_STATESYNC_REASON_START;
            char *content = NULL;
            if (sGnService.useDefaultBiz)
                content = Genie_Create_StateSyncEvent2(
                    sGnService.uuid, sGnService.accessToken, reason, &sGnService.speakerContext, &sGnService.playerContext);
            else
                content = Genie_Create_StateSyncEvent(sGnService.bizType, sGnService.bizGroup,
                    sGnService.uuid, sGnService.accessToken, reason, &sGnService.speakerContext, &sGnService.playerContext);
            if (content != NULL)
                ws_client_send_text_unique(sGnService.websocket, content, strlen(content));
            sGnService.isStateSynced = true;
        }
    } else if (msg->what == WHAT_STATUS_ACCOUNT_UNAUTHORIZED) {
        GnLooper_Post_Message(WHAT_COMMAND_WEBSOCKET_DISCONNECT, 0, 0, NULL);
        // todo: retry to connect gateway and guestactivate
    }
}

static void GnLooper_Handle_MicphoneStatusMessage(struct message *msg)
{
    if (sGnService.isWebsocketConnected && sGnService.isAccountAuthorized) {
        char *content = NULL;
        int len = 0;
        switch (msg->what) {
        case WHAT_STATUS_MICPHONE_STARTED:
            if (sGnService.useDefaultBiz)
                content = Genie_Create_MicrophoneActiveEvent2(
                    sGnService.uuid, sGnService.accessToken, GENIE_MICROPHONEACTIVE_REASON_USER);
            else
                content = Genie_Create_MicrophoneActiveEvent(sGnService.bizType, sGnService.bizGroup,
                    sGnService.uuid, sGnService.accessToken, GENIE_MICROPHONEACTIVE_REASON_USER);
            if (content != NULL)
                ws_client_send_text_unique(sGnService.websocket, content, strlen(content));

            if (sGnService.useDefaultBiz)
                content = Genie_Create_MicrophoneListenStartedEvent2(
                    sGnService.uuid, sGnService.accessToken, &sGnService.speechContext);
            else
                content = Genie_Create_MicrophoneListenStartedEvent(sGnService.bizType, sGnService.bizGroup,
                    sGnService.uuid, sGnService.accessToken, &sGnService.speechContext);
            if (content != NULL)
                ws_client_send_text_unique(sGnService.websocket, content, strlen(content));

            content = Genie_Create_MicrophoneBinaryHeader(&len);
            if (content != NULL)
                ws_client_send_binary_unique(sGnService.websocket, content, len, WS_BINARY_FRAGMENT_START);
            break;

        case WHAT_STATUS_MICPHONE_STREAMING:
            if (!msg->arg2)
                ws_client_send_binary_unique(sGnService.websocket, msg->data, msg->arg1, WS_BINARY_FRAGMENT_CONTINUE);
            else
                ws_client_send_binary_unique(sGnService.websocket, msg->data, msg->arg1, WS_BINARY_FRAGMENT_FINISH);
            msg->data = NULL; // websocket will free this data
            break;

        case WHAT_STATUS_MICPHONE_STOPPED:
            break;

        default:
            return;
        }
    }
}

static void GnLooper_Handle_SpeakerStatusMessage(struct message *msg)
{
    if (sGnService.isWebsocketConnected && sGnService.isAccountAuthorized) {
        Genie_SpeakerSyncReason_t reason = msg->arg1;
        char *content = NULL;
        if (sGnService.useDefaultBiz)
            content = Genie_Create_SpeakerSyncEvent2(
                sGnService.uuid, sGnService.accessToken, reason, &sGnService.speakerContext);
        else
            content = Genie_Create_SpeakerSyncEvent(sGnService.bizType, sGnService.bizGroup,
                sGnService.uuid, sGnService.accessToken, reason, &sGnService.speakerContext);
        if (content != NULL)
            ws_client_send_text_unique(sGnService.websocket, content, strlen(content));
    }
}

static void GnLooper_Handle_PlayerStatusMessage(struct message *msg)
{
    if (sGnService.isWebsocketConnected && sGnService.isAccountAuthorized) {
        Genie_PlayerSyncReason_t reason = msg->arg1;
        char *content = NULL;
        if (sGnService.useDefaultBiz)
            content = Genie_Create_PlayerSyncEvent2(
                sGnService.uuid, sGnService.accessToken, reason, &sGnService.playerContext);
        else
            content = Genie_Create_PlayerSyncEvent(sGnService.bizType, sGnService.bizGroup,
                sGnService.uuid, sGnService.accessToken, reason, &sGnService.playerContext);
        if (content != NULL)
            ws_client_send_text_unique(sGnService.websocket, content, strlen(content));
    }
}

static void GnLooper_Handle_WebsocketDoConnect(struct message *msg)
{
    ws_user_info_t info;
    info.host = GENIE_WEBSOCKET_HOST_NAME;
    info.port = GENIE_WEBSOCKET_HOST_PORT;
    if (sGnService.useDefaultBiz)
        info.path = Genie_Create_GatewayUrl2();
    else
        info.path = Genie_Create_GatewayUrl(sGnService.bizType, sGnService.bizGroup, sGnService.bizSecret);
    info.cacert = (char *)sGnService.caCert;
    info.callback.on_connected = GnWebsocket_OnConnected;
    info.callback.on_disconnected = GnWebsocket_OnDisconnected;
    info.callback.on_received_text = GnWebsocket_OnReceivedText;
    info.callback.on_received_binary = GnWebsocket_OnReceivedBinary;

    ws_client_connect(sGnService.websocket, &info);
    ws_client_set_heartbeat(sGnService.websocket, GENIE_WEBSOCKET_PING_INTERVAL);
    GnLooper_Post_DelayMessage(WHAT_COMMAND_WEBSOCKET_CHECKSTATE, 0, 0, NULL, GENIE_WEBSOCKET_RECONNECT_INTERVAL);
    OS_FREE(info.path);
}

static void GnLooper_Handle_WebsocketDoDisconnect(struct message *msg)
{
    ws_client_disconnect(sGnService.websocket);

    os_mutex_lock(sGnService.stateLock);
    if (sGnService.isWebsocketConnected) {
        sGnService.isWebsocketConnected = false;
        GnLooper_Notify_StatusListener(GENIE_STATUS_GatewayDisconnected);
    }
    os_mutex_unlock(sGnService.stateLock);
}

static void GnLooper_Handle_WebsocketDoCheckstate(struct message *msg)
{
    if (ws_client_conn_state(sGnService.websocket) != WS_CONN_STATE_CONNECTED) {
        GnLooper_Clear_AllMessages_l();
        if (sGnService.websocketReconnectCount < GENIE_WEBSOCKET_RECONNECT_MAX) {
            OS_LOGW(TAG, "Retry to connect gateway, retryCount=%d", sGnService.websocketReconnectCount);
            GnLooper_Post_Message(WHAT_COMMAND_WEBSOCKET_CONNECT, 0, 0, NULL);
        } else {
            OS_LOGE(TAG, "Failed to connect gateway, retryCount=%d", sGnService.websocketReconnectCount);
        }
        sGnService.websocketReconnectCount++;
    }
}

static void GnLooper_Handle_MicphoneDoCheckstate(struct message *msg)
{
    os_mutex_lock(sGnService.stateLock);
    if (sGnService.isMicphoneWakeup && !sGnService.isMicphoneStarted) {
        OS_LOGW(TAG, "Micphone already wakeup, but not started, reset isMicphoneWakeup");
        sGnService.isMicphoneWakeup = false;
        GnLooper_Notify_StatusListener(GENIE_STATUS_MicphoneStopped);
        struct listnode *item, *tmp;
        list_for_each_safe(item, tmp, &sGnService.pendingMsgList) {
            GnService_PendingMsgNode_t *node = listnode_to_item(item, GnService_PendingMsgNode_t, listnode);
            list_remove(item);
            if (mlooper_post_message(sGnService.looper, node->msg) != 0)
                GnLooper_Free_PendingMessage(node->msg);
            OS_FREE(node);
        }
    }
    os_mutex_unlock(sGnService.stateLock);
}

static void GnLooper_Handle_UserEvent(struct message *msg)
{
    if (sGnService.isWebsocketConnected && sGnService.isAccountAuthorized) {
        char *content = NULL;
        switch (msg->what) {
        case WHAT_EVENT_TEXTRECOGNIZE:
            if (sGnService.useDefaultBiz)
                content = Genie_Create_TextRecognizeEvent2(
                    sGnService.uuid, sGnService.accessToken, msg->data);
            else
                content = Genie_Create_TextRecognizeEvent(sGnService.bizType, sGnService.bizGroup,
                    sGnService.uuid, sGnService.accessToken, msg->data);
            if (content != NULL)
                ws_client_send_text_unique(sGnService.websocket, content, strlen(content));
            OS_FREE(msg->data);
            msg->data = NULL;
            break;
        case WHAT_EVENT_QUERYUSERINFO:
            if (sGnService.useDefaultBiz)
                content = Genie_Create_QueryUserInfoEvent2(
                    sGnService.uuid, sGnService.accessToken);
            else
                content = Genie_Create_QueryUserInfoEvent(sGnService.bizType, sGnService.bizGroup,
                    sGnService.uuid, sGnService.accessToken);
            if (content != NULL)
                ws_client_send_text_unique(sGnService.websocket, content, strlen(content));
            break;
        default:
            break;
        }
    }
}

static void GnLooper_Handle_Message(struct message *msg)
{
    OS_LOGV(TAG, "+++Enter handle message(%d)", msg->what);
    switch (msg->what) {
    case WHAT_STATUS_WEBSOCKET_CONNECTED:
    case WHAT_STATUS_WEBSOCKET_DISCONNECTED:
        GnLooper_Handle_WebsocketStatusMessage(msg);
        break;
    case WHAT_STATUS_ACCOUNT_AUTHORIZED:
    case WHAT_STATUS_ACCOUNT_UNAUTHORIZED:
        GnLooper_Handle_AccountStatusMessage(msg);
        break;
    case WHAT_STATUS_MICPHONE_STARTED:
    case WHAT_STATUS_MICPHONE_STREAMING:
    case WHAT_STATUS_MICPHONE_STOPPED:
        GnLooper_Handle_MicphoneStatusMessage(msg);
        break;
    case WHAT_STATUS_SPEAKER_CHANGED:
        GnLooper_Handle_SpeakerStatusMessage(msg);
        break;
    case WHAT_STATUS_PLAYER_CHANGED:
        GnLooper_Handle_PlayerStatusMessage(msg);
        break;
    case WHAT_COMMAND_WEBSOCKET_CONNECT:
        GnLooper_Handle_WebsocketDoConnect(msg);
        break;
    case WHAT_COMMAND_WEBSOCKET_DISCONNECT:
        GnLooper_Handle_WebsocketDoDisconnect(msg);
        break;
    case WHAT_COMMAND_WEBSOCKET_CHECKSTATE:
        GnLooper_Handle_WebsocketDoCheckstate(msg);
        break;
    case WHAT_COMMAND_MICPHONE_CHECKSTATE:
        GnLooper_Handle_MicphoneDoCheckstate(msg);
        break;
    case WHAT_EVENT_TEXTRECOGNIZE:
    case WHAT_EVENT_QUERYUSERINFO:
        GnLooper_Handle_UserEvent(msg);
        break;
    default:
        OS_LOGE(TAG, "Unknown message: what=%d", msg->what);
        break;
    }
    OS_LOGV(TAG, "---Leave handle message(%d)", msg->what);
}

static bool GnService_IsUuid(const char *uuid, unsigned int length)
{
    unsigned int offset = 0;
    for (offset = 0; offset < length; offset++) {
        if (uuid[offset] != '\0') {
            if ((uuid[offset] >= 'a' && uuid[offset] <= 'f') ||
                (uuid[offset] >= 'A' && uuid[offset] <= 'F') ||
                (uuid[offset] >= '0' && uuid[offset] <= '9') ||
                (uuid[offset] == '-'))
                continue;
            else
                return false;
        } else {
            break;
        }
    }
    return offset > 0;
}

bool GnService_Init(GnService_Adapter_t *adapter)
{
    if (sGnInited) return true;

    memset(&sGnService, 0x0, sizeof(sGnService));
    memset(&sGnCallback, 0x0, sizeof(sGnCallback));

    if (adapter == NULL) return false;

    if (adapter->bizType != NULL && adapter->bizType() != NULL &&
        adapter->bizGroup != NULL && adapter->bizGroup() != NULL &&
        adapter->bizSecret != NULL && adapter->bizSecret() != NULL) {
        sGnService.bizType = OS_STRDUP(adapter->bizType());
        sGnService.bizGroup = OS_STRDUP(adapter->bizGroup());
        sGnService.bizSecret = OS_STRDUP(adapter->bizSecret());
        if (sGnService.bizType == NULL || sGnService.bizGroup == NULL || sGnService.bizSecret == NULL)
            goto __error_init;
    } else {
        OS_LOGW(TAG, "Using default biz");
        sGnService.useDefaultBiz = true;
    }

    if (adapter->caCert != NULL && adapter->caCert() != NULL)
        sGnService.caCert = OS_STRDUP(adapter->caCert());
    else if (Genie_Get_DefaultCaCert() != NULL)
        sGnService.caCert = OS_STRDUP(Genie_Get_DefaultCaCert());
    if (sGnService.caCert == NULL)
        goto __error_init;

    if (adapter->macAddr != NULL && adapter->macAddr() != NULL) {
        unsigned int mac_int[6] = { 0 };
        char mac_str[32];
        int ret = sscanf(adapter->macAddr(), "%x:%x:%x:%x:%x:%x",
            &mac_int[0], &mac_int[1], &mac_int[2], &mac_int[3], &mac_int[4], &mac_int[5]);
        if (ret != 6) {
            OS_LOGE(TAG, "MAC address is invalid, it should be "
                "[A-F0-9a-f]{2}:[A-F0-9a-f]{2}:[A-F0-9a-f]{2}:[A-F0-9a-f]{2}:[A-F0-9a-f]{2}:[A-F0-9a-f]{2}");
            goto __error_init;
        }
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_int[0]&0xff, mac_int[1]&0xff, mac_int[2]&0xff,
            mac_int[3]&0xff, mac_int[4]&0xff, mac_int[5]&0xff);
        sGnService.macAddr = OS_STRDUP(mac_str);
    }
    if (sGnService.macAddr == NULL)
        goto __error_init;

    if (adapter->uuid != NULL && adapter->uuid() != NULL &&
        adapter->accessToken != NULL && adapter->accessToken() != NULL) {
        if (GnService_IsUuid(adapter->uuid(), strlen(adapter->uuid())))
            sGnService.uuid = OS_STRDUP(adapter->uuid());
        else
            OS_LOGE(TAG, "Uuid is invalid, it should be "
                "[A-F0-9a-f]{8}-[A-F0-9a-f]{4}-[A-F0-9a-f]{4}-[A-F0-9a-f]{4}-[A-F0-9a-f]{12}");

        if (GnService_IsUuid(adapter->accessToken(), strlen(adapter->accessToken())))
            sGnService.accessToken = OS_STRDUP(adapter->accessToken());
        else
            OS_LOGE(TAG, "AccessToken is invalid, it should be "
                "[A-F0-9a-f]{8}-[A-F0-9a-f]{4}-[A-F0-9a-f]{4}-[A-F0-9a-f]{4}-[A-F0-9a-f]{12}");

        if (sGnService.uuid != NULL && sGnService.accessToken != NULL) {
            //OS_LOGD(TAG, "uuid=%s, accessToken=%s", sGnService.uuid, sGnService.accessToken);
            sGnService.isAccountAuthorized = true;
        }
    }

    if ((sGnService.websocket = ws_client_create()) == NULL)
        goto __error_init;
    if ((sGnService.threadLock = os_mutex_create()) == NULL)
        goto __error_init;
    if ((sGnService.stateLock = os_mutex_create()) == NULL)
        goto __error_init;
    struct os_thread_attr thread_attr = {
        .name = GENIE_SERVICE_THREAD_NAME,
        .priority = GENIE_SERVICE_THREAD_PRIO,
        .stacksize = GENIE_SERVICE_THREAD_STACK,
        .joinable = true,
    };
    sGnService.looper = mlooper_create(&thread_attr, GnLooper_Handle_Message, GnLooper_Free_MessageData);
    if (sGnService.looper == NULL)
        goto __error_init;

    list_init(&sGnService.commandListenerList);
    list_init(&sGnService.ttsbinaryListenerList);
    list_init(&sGnService.statusListenerList);
    list_init(&sGnService.pendingMsgList);

    sGnCallback.onNetworkConnected      = GnCallback_OnNetworkConnected;
    sGnCallback.onNetworkDisconnected   = GnCallback_OnNetworkDisconnected;
    sGnCallback.onMicphoneWakeup        = GnCallback_OnMicphoneWakeup;
    sGnCallback.onMicphoneStreaming     = GnCallback_OnMicphoneStreaming;
    sGnCallback.onSpeakerVolumeChanged  = GnCallback_OnSpeakerVolumeChanged;
    sGnCallback.onSpeakerMutedChanged   = GnCallback_OnSpeakerMutedChanged;
    sGnCallback.onPlayerStarted         = GnCallback_OnPlayerStarted;
    sGnCallback.onPlayerPaused          = GnCallback_OnPlayerPaused;
    sGnCallback.onPlayerResumed         = GnCallback_OnPlayerResumed;
    sGnCallback.onPlayerNearlyFinished  = GnCallback_OnPlayerNearlyFinished;
    sGnCallback.onPlayerFinished        = GnCallback_OnPlayerFinished;
    sGnCallback.onPlayerStopped         = GnCallback_OnPlayerStopped;
    sGnCallback.onPlayerFailed          = GnCallback_OnPlayerFailed;
    sGnCallback.onQueryUserInfo         = GnCallback_OnQueryUserInfo;
    sGnCallback.onTextRecognize         = GnCallback_OnTextRecognize;
    sGnCallback.onExpectSpeech          = GnCallback_OnExpectSpeech;
    sGnCallback.onMicphoneSilence       = GnCallback_OnMicphoneSilence;

    sGnInited = true;
    return true;

__error_init:
    if (sGnService.bizType != NULL)     OS_FREE(sGnService.bizType);
    if (sGnService.bizGroup != NULL)    OS_FREE(sGnService.bizGroup);
    if (sGnService.bizSecret != NULL)   OS_FREE(sGnService.bizSecret);
    if (sGnService.caCert != NULL)      OS_FREE(sGnService.caCert);
    if (sGnService.macAddr != NULL)     OS_FREE(sGnService.macAddr);
    if (sGnService.uuid != NULL)        OS_FREE(sGnService.uuid);
    if (sGnService.accessToken != NULL) OS_FREE(sGnService.accessToken);
    if (sGnService.websocket != NULL)   ws_client_destory(sGnService.websocket);
    if (sGnService.threadLock != NULL)  os_mutex_destroy(sGnService.threadLock);
    if (sGnService.stateLock != NULL)   os_mutex_destroy(sGnService.stateLock);
    if (sGnService.looper != NULL)      mlooper_destroy(sGnService.looper);
    return false;
}

bool GnService_IsInit()
{
    return sGnInited;
}

bool GnService_Register_CommandListener(void (*listener)(Genie_Domain_t domain, Genie_Command_t command, const char *payload))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return false;
    }

    GnService_CommandListenerNode_t *node;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(sGnService.threadLock);
    list_for_each(item, &sGnService.commandListenerList) {
        node = listnode_to_item(item, GnService_CommandListenerNode_t, listnode);
        if (node->commandListener == listener)
            found = true;
    }
    if (!found) {
        node = OS_MALLOC(sizeof(GnService_CommandListenerNode_t));
        if (node == NULL) {
            os_mutex_unlock(sGnService.threadLock);
            return false;
        }
        node->commandListener = listener;
        list_add_tail(&sGnService.commandListenerList, &node->listnode);
    }
    os_mutex_unlock(sGnService.threadLock);
    return true;
}

bool GnService_Register_TtsbinaryListener(void (*listener)(char *data, int len, bool final))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return false;
    }

    GnService_TtsbinaryListenerNode_t *node;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(sGnService.threadLock);
    list_for_each(item, &sGnService.ttsbinaryListenerList) {
        node = listnode_to_item(item, GnService_TtsbinaryListenerNode_t, listnode);
        if (node->ttsbinaryListener == listener)
            found = true;
    }
    if (!found) {
        node = OS_MALLOC(sizeof(GnService_TtsbinaryListenerNode_t));
        if (node == NULL) {
            os_mutex_unlock(sGnService.threadLock);
            return false;
        }
        node->ttsbinaryListener = listener;
        list_add_tail(&sGnService.ttsbinaryListenerList, &node->listnode);
    }
    os_mutex_unlock(sGnService.threadLock);
    return true;
}

bool GnService_Register_StatusListener(void (*listener)(Genie_Status_t status))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return false;
    }

    GnService_StatusListenerNode_t *node;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(sGnService.threadLock);
    list_for_each(item, &sGnService.statusListenerList) {
        node = listnode_to_item(item, GnService_StatusListenerNode_t, listnode);
        if (node->statusListener == listener)
            found = true;
    }
    if (!found) {
        node = OS_MALLOC(sizeof(GnService_StatusListenerNode_t));
        if (node == NULL) {
            os_mutex_unlock(sGnService.threadLock);
            return false;
        }
        node->statusListener = listener;
        list_add_tail(&sGnService.statusListenerList, &node->listnode);
    }
    os_mutex_unlock(sGnService.threadLock);
    return true;
}

void GnService_Unregister_CommandListener(void (*listener)(Genie_Domain_t domain, Genie_Command_t command, const char *payload))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return;
    }

    GnService_CommandListenerNode_t *node;
    struct listnode *item, *tmp;
    os_mutex_lock(sGnService.threadLock);
    list_for_each_safe(item, tmp, &sGnService.commandListenerList) {
        node = listnode_to_item(item, GnService_CommandListenerNode_t, listnode);
        if (node->commandListener == listener) {
            list_remove(item);
            OS_FREE(node);
            break;
        }
    }
    os_mutex_unlock(sGnService.threadLock);
}

void GnService_Unregister_TtsbinaryListener(void (*listener)(char *data, int len, bool final))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return;
    }

    GnService_TtsbinaryListenerNode_t *node;
    struct listnode *item, *tmp;
    os_mutex_lock(sGnService.threadLock);
    list_for_each_safe(item, tmp, &sGnService.ttsbinaryListenerList) {
        node = listnode_to_item(item, GnService_TtsbinaryListenerNode_t, listnode);
        if (node->ttsbinaryListener == listener) {
            list_remove(item);
            OS_FREE(node);
            break;
        }
    }
    os_mutex_unlock(sGnService.threadLock);
}

void GnService_Unregister_StatusListener(void (*listener)(Genie_Status_t status))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return;
    }

    GnService_StatusListenerNode_t *node;
    struct listnode *item, *tmp;
    os_mutex_lock(sGnService.threadLock);
    list_for_each_safe(item, tmp, &sGnService.statusListenerList) {
        node = listnode_to_item(item, GnService_StatusListenerNode_t, listnode);
        if (node->statusListener == listener) {
            list_remove(item);
            OS_FREE(node);
            break;
        }
    }
    os_mutex_unlock(sGnService.threadLock);
}

bool GnService_Get_Callback(GnService_Callback_t **callback)
{
    if (!sGnInited || callback == NULL) {
        OS_LOGE(TAG, "Genie service is NOT inited");
        return false;
    }
    *callback = &sGnCallback;
    return true;
}

bool GnService_Start()
{
    if (!sGnInited) { OS_LOGE(TAG, "Genie service is NOT inited"); return false; }

    os_mutex_lock(sGnService.threadLock);
    if (!sGnService.isLooperStarted) {
        if (mlooper_start(sGnService.looper) != 0) {
            os_mutex_unlock(sGnService.threadLock);
            return false;
        }
        sGnService.isLooperStarted = true;
    }
    if (sGnService.isNetworkConnected && !sGnService.isWebsocketConnected) {
        sGnService.websocketReconnectCount = 0;
        GnLooper_Clear_AllMessages_l();
        GnLooper_Post_Message(WHAT_COMMAND_WEBSOCKET_CONNECT, 0, 0, NULL);
    }
    os_mutex_unlock(sGnService.threadLock);
    return true;
}

bool GnService_IsActive()
{
    if (!sGnInited) { OS_LOGE(TAG, "Genie service is NOT inited"); return false; }
    return sGnService.isWebsocketConnected && sGnService.isAccountAuthorized;
}

void GnService_Stop()
{
    if (!sGnInited) { OS_LOGE(TAG, "Genie service is NOT inited"); return; }

    os_mutex_lock(sGnService.threadLock);
    if (sGnService.isLooperStarted) {
        mlooper_stop(sGnService.looper);
        sGnService.isLooperStarted = false;
    }
    ws_client_disconnect(sGnService.websocket);
    sGnService.isWebsocketConnected = false;
    GnLooper_Clear_AllMessages_l();
    os_mutex_unlock(sGnService.threadLock);
}
