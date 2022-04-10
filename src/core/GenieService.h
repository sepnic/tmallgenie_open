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

#ifndef __TMALLGENIE_CORE_SERVICE_H__
#define __TMALLGENIE_CORE_SERVICE_H__

#include <stdio.h>
#include <stdbool.h>
#include "GenieDefine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *(*bizType)();
    const char *(*bizGroup)();
    const char *(*bizSecret)();
    const char *(*caCert)();
    const char *(*macAddr)();
    const char *(*uuid)();
    const char *(*accessToken)();
} GnService_Adapter_t;

typedef struct {
    // network status changed
    void (*onNetworkConnected)();
    void (*onNetworkDisconnected)();

    // micphone status changed
    void (*onMicphoneWakeup)(const char *wakeupWord, int doa, double confidence);
    void (*onMicphoneSilence)();
    void (*onMicphoneStreaming)(Genie_SpeechFormat_t format, char *buffer, int len, bool final);

    // speaker status changed
    void (*onSpeakerVolumeChanged)(int volume);
    void (*onSpeakerMutedChanged)(bool muted);

    // player status changed
    void (*onPlayerStarted)();
    void (*onPlayerPaused)();
    void (*onPlayerResumed)();
    void (*onPlayerNearlyFinished)();
    void (*onPlayerFinished)();
    void (*onPlayerStopped)();
    void (*onPlayerFailed)();

    // user request event
    void (*onTextRecognize)(const char *inputText);
    void (*onExpectSpeech)();
} GnService_Callback_t;

bool GnService_Init(GnService_Adapter_t *adapter);

bool GnService_IsInit();

bool GnService_Register_CommandListener(void (*listener)(Genie_Domain_t domain, Genie_Command_t command, const char *payload));

bool GnService_Register_TtsbinaryListener(void (*listener)(char *data, int len, bool final));

bool GnService_Register_StatusListener(void (*listener)(Genie_Status_t status));

void GnService_Unregister_CommandListener(void (*listener)(Genie_Domain_t domain, Genie_Command_t command, const char *payload));

void GnService_Unregister_TtsbinaryListener(void (*listener)(char *data, int len, bool final));

void GnService_Unregister_StatusListener(void (*listener)(Genie_Status_t status));

bool GnService_Get_Callback(GnService_Callback_t **callback);

bool GnService_Start();

bool GnService_IsActive();

void GnService_Stop();

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_CORE_SERVICE_H__ */
