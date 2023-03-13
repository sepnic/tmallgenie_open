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
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "json/cJSON.h"

#include "core/GenieService.h"
#include "vendorplayer/GenieVendorPlayer.h"
#include "GeniePlayer.h"

#define TAG "GeniePlayer"

typedef struct {
    os_mutex lock;
    GnUtpManager_Callback_t *utpCallback;
    GnService_Callback_t    *serviceCallback;
    bool musicCompleted;
} GnPlayer_priv_t;

static GnPlayer_priv_t sGnPlayer;
static bool            sGnInited = false;

static void GnService_CommandListener(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    switch (command) {
    case GENIE_COMMAND_Speak: {
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) return;
        cJSON *expectSpeechJson = cJSON_GetObjectItem(payloadJson, "expectSpeech");
        bool expectSpeech = false;
        if (expectSpeechJson != NULL && cJSON_IsTrue(expectSpeechJson))
            expectSpeech = true;
        sGnPlayer.utpCallback->onCommandNewTtsHeader(expectSpeech);
        cJSON_Delete(payloadJson);
    }
        break;
    case GENIE_COMMAND_Play: {
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) return;
        cJSON *audioUrlJson = cJSON_GetObjectItem(payloadJson, "audioUrl");
        if (audioUrlJson != NULL && cJSON_IsString(audioUrlJson))
            sGnPlayer.utpCallback->onCommandNewMusic(cJSON_GetStringValue(audioUrlJson));
        cJSON_Delete(payloadJson);
    }
        break;
    case GENIE_COMMAND_PlayOnce: {
        cJSON *payloadJson = cJSON_Parse(payload);
        if (payloadJson == NULL) return;
        cJSON *audioUrlJson = cJSON_GetObjectItem(payloadJson, "audioUrl");
        if (audioUrlJson != NULL && cJSON_IsString(audioUrlJson))
            sGnPlayer.utpCallback->onCommandNewPrompt(cJSON_GetStringValue(audioUrlJson));
        cJSON_Delete(payloadJson);
    }
        break;
    case GENIE_COMMAND_Pause:
        sGnPlayer.utpCallback->onCommandPause();
        break;
    case GENIE_COMMAND_Resume:
        sGnPlayer.utpCallback->onCommandResume();
        break;
    case GENIE_COMMAND_Exit:
    case GENIE_COMMAND_Standby:
        sGnPlayer.utpCallback->onCommandStop();
        break;
    case GENIE_COMMAND_SetMute:
    case GENIE_COMMAND_ClearQueue:
    default:
        break;
    }
}

static void GnService_TtsbinaryListener(char *data, int len, bool final)
{
    sGnPlayer.utpCallback->onCommandNewTtsFrame(data, len, final);
}

static void GnService_StatusListener(Genie_Status_t status)
{
    switch (status) {
    case GENIE_STATUS_GatewayDisconnected:
        sGnPlayer.utpCallback->onGatewayDisconnected();
        break;
    case GENIE_STATUS_GatewayConnected:
        sGnPlayer.utpCallback->onGatewayConnected();
        break;
    case GENIE_STATUS_SpeakerUnmuted:
        sGnPlayer.utpCallback->onSpeakerUnmuted();
        break;
    case GENIE_STATUS_SpeakerMuted:
        sGnPlayer.utpCallback->onSpeakerMuted();
        break;
    case GENIE_STATUS_MicphoneWakeup:
        // play WAKEUP_REMIND, when WAKEUP_REMIND played done, send onExpectSpeech() to open recorder
        sGnPlayer.utpCallback->onCommandNewPromptWakeup(GENIE_PREBUILT_WAKEUP_REMIND);
        break;
    case GENIE_STATUS_MicphoneStarted:
        sGnPlayer.utpCallback->onMicphoneStarted();
        break;
    case GENIE_STATUS_MicphoneStopped:
        sGnPlayer.utpCallback->onMicphoneStopped();
        break;
    case GENIE_STATUS_NetworkDisconnected:
    case GENIE_STATUS_NetworkConnected:
    case GENIE_STATUS_Unauthorized:
    case GENIE_STATUS_Authorized:
    default:
        break;
    }
}

static void GnPlayer_StateListener(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech)
{
    switch (state) {
    case GENIE_PLAYER_STATE_IDLE:
        if (stream == GENIE_PLAYER_STREAM_MUSIC) {
            if (!sGnPlayer.musicCompleted)
                sGnPlayer.serviceCallback->onPlayerStopped();
            sGnPlayer.musicCompleted = false;
        } else if (stream == GENIE_PLAYER_STREAM_PROMPT_WAKEUP) {
            // WAKEUP_REMIND/RECORD_REMIND play done, notify recorder to start streaming
            sGnPlayer.serviceCallback->onExpectSpeech();
        }
        if (expectSpeech && stream != GENIE_PLAYER_STREAM_PROMPT_WAKEUP) {
            // play RECORD_REMIND, when RECORD_REMIND played done, send onExpectSpeech() to open recorder
            sGnPlayer.utpCallback->onCommandNewPromptWakeup(GENIE_PREBUILT_RECORD_REMIND);
        }
        break;
    case GENIE_PLAYER_STATE_STARTED:
        if (stream == GENIE_PLAYER_STREAM_MUSIC)
            sGnPlayer.serviceCallback->onPlayerStarted();
        break;
    case GENIE_PLAYER_STATE_PAUSED:
        if (stream == GENIE_PLAYER_STREAM_MUSIC)
            sGnPlayer.serviceCallback->onPlayerPaused();
        break;
    case GENIE_PLAYER_STATE_RESUMED:
        if (stream == GENIE_PLAYER_STREAM_MUSIC)
            sGnPlayer.serviceCallback->onPlayerResumed();
        break;
    case GENIE_PLAYER_STATE_NEARLYCOMPLETED:
        if (stream == GENIE_PLAYER_STREAM_MUSIC)
            sGnPlayer.serviceCallback->onPlayerNearlyFinished();
        break;
    case GENIE_PLAYER_STATE_COMPLETED:
        if (stream == GENIE_PLAYER_STREAM_MUSIC) {
            sGnPlayer.serviceCallback->onPlayerFinished();
            sGnPlayer.musicCompleted = true;
        }
        break;
    case GENIE_PLAYER_STATE_ERROR:
        if (stream == GENIE_PLAYER_STREAM_MUSIC) {
            sGnPlayer.serviceCallback->onPlayerFailed();
            sGnPlayer.musicCompleted = true;
        }
        break;
    case GENIE_PLAYER_STATE_STOPPED:
        break;
    default:
        break;
    }
}

bool GnPlayer_Init(GnVendor_PcmOut_t *pcmOut)
{
    if (sGnInited) return true;
    memset(&sGnPlayer, 0x0, sizeof(sGnPlayer));
    if ((sGnPlayer.lock = os_mutex_create()) == NULL)
        goto __error_init;
    if (!GnUtpManager_Init(GnVendorPlayer_GetInstance(pcmOut)))
        return false;
    if (!GnUtpManager_Get_Callback(&sGnPlayer.utpCallback))
        return false;
    sGnInited = true;
    return true;
__error_init:
    if (sGnPlayer.lock != NULL)
        os_mutex_destroy(sGnPlayer.lock);
    return false;
}

bool GnPlayer_IsInit()
{
    return sGnInited && GnUtpManager_IsInit();
}

bool GnPlayer_Start()
{
    if (!GnPlayer_IsInit() || !GnService_IsInit()) {
        OS_LOGE(TAG, "Genie Service/Player is NOT inited");
        return false;
    }
    os_mutex_lock(sGnPlayer.lock);
    if (!GnService_Get_Callback(&sGnPlayer.serviceCallback))
        goto __error_start;
    if (!GnUtpManager_Start())
        goto __error_start;
    if (!GnUtpManager_Register_StateListener(GnPlayer_StateListener))
        goto __error_start;
    if (!GnService_Register_CommandListener(GnService_CommandListener))
        goto __error_start;
    if (!GnService_Register_TtsbinaryListener(GnService_TtsbinaryListener))
        goto __error_start;
    if (!GnService_Register_StatusListener(GnService_StatusListener))
        goto __error_start;
    os_mutex_unlock(sGnPlayer.lock);
    return true;
__error_start:
    GnService_Unregister_CommandListener(GnService_CommandListener);
    GnService_Unregister_TtsbinaryListener(GnService_TtsbinaryListener);
    GnService_Unregister_StatusListener(GnService_StatusListener);
    GnUtpManager_Unregister_StateListener(GnPlayer_StateListener);
    GnUtpManager_Stop();
    os_mutex_unlock(sGnPlayer.lock);
    return false;
}

bool GnPlayer_New_Prompt(const char *url)
{
    if (!GnPlayer_IsActive()) {
        OS_LOGE(TAG, "Genie Player is NOT active");
        return false;
    }
    sGnPlayer.utpCallback->onCommandNewPrompt(url);
    return true;
}

bool GnPlayer_Clear_Playonce()
{
    if (!GnPlayer_IsActive()) {
        OS_LOGE(TAG, "Genie Player is NOT active");
        return false;
    }
    sGnPlayer.utpCallback->onCommandStopPlayonce();
    return true;
}

bool GnPlayer_IsActive()
{
    return sGnInited && GnUtpManager_IsActive();
}

bool GnPlayer_IsMusicPlaying()
{
    return GnUtpManager_IsPlaying(GENIE_PLAYER_STREAM_MUSIC);
}

bool GnPlayer_IsPromptPlaying()
{
    return GnUtpManager_IsPlaying(GENIE_PLAYER_STREAM_PROMPT);
}

bool GnPlayer_IsTtsPlaying()
{
    return GnUtpManager_IsPlaying(GENIE_PLAYER_STREAM_TTS);
}

void GnPlayer_Stop()
{
    os_mutex_lock(sGnPlayer.lock);
    GnService_Unregister_CommandListener(GnService_CommandListener);
    GnService_Unregister_TtsbinaryListener(GnService_TtsbinaryListener);
    GnService_Unregister_StatusListener(GnService_StatusListener);
    GnUtpManager_Unregister_StateListener(GnPlayer_StateListener);
    GnUtpManager_Stop();
    os_mutex_unlock(sGnPlayer.lock);
}
