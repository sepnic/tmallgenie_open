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

#ifndef __TMALLGENIE_PLAYER_UTPMANAGER_H__
#define __TMALLGENIE_PLAYER_UTPMANAGER_H__

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GENIE_PLAYER_STREAM_INVALID         = -1,
    GENIE_PLAYER_STREAM_TTS             = 0,
    GENIE_PLAYER_STREAM_PROMPT          = 1,
    GENIE_PLAYER_STREAM_MUSIC           = 2,
    GENIE_PLAYER_STREAM_PROMPT_WAKEUP   = 3,
} GnPlayer_Stream_t;

typedef enum {
    GENIE_PLAYER_STATE_INVALID          = -1,
    GENIE_PLAYER_STATE_IDLE             = 0,
    GENIE_PLAYER_STATE_PREPARED         = 1,
    GENIE_PLAYER_STATE_STARTED          = 2,
    GENIE_PLAYER_STATE_PAUSED           = 3,
    GENIE_PLAYER_STATE_RESUMED          = 4,
    GENIE_PLAYER_STATE_NEARLYCOMPLETED  = 5,
    GENIE_PLAYER_STATE_COMPLETED        = 6,
    GENIE_PLAYER_STATE_STOPPED          = 7,
    GENIE_PLAYER_STATE_ERROR            = 0xff,
} GnPlayer_State_t;

typedef struct {
    void *(*create)(GnPlayer_Stream_t stream);
    bool (*registerStateListener)(void *handle, void (*listener)(GnPlayer_Stream_t stream, GnPlayer_State_t state));
    bool (*setDataSource)(void *handle, const char *url);
    bool (*prepareAsync)(void *handle);
    bool (*write)(void *handle, char *buffer, int size, bool final);
    bool (*start)(void *handle);
    bool (*pause)(void *handle);
    bool (*resume)(void *handle);
    bool (*seek)(void *handle, int positonMs);
    bool (*stop)(void *handle);
    bool (*reset)(void *handle);
    bool (*getPosition)(void *handle, int *positonMs);
    bool (*getDuration)(void *handle, int *durationMs);
    void (*destroy)(void *handle);
} GnPlayer_Adapter_t;

typedef struct {
    void (*onCommandNewTtsHeader)(bool expectSpeech);
    void (*onCommandNewTtsFrame)(char *buffer, int size, bool final);
    void (*onCommandNewPrompt)(const char *url);
    void (*onCommandNewMusic)(const char *url);
    void (*onCommandNewPromptWakeup)(const char *url);
    void (*onCommandPause)();               // pause music content
    void (*onCommandResume)();              // resume music content if need
    void (*onCommandStop)();                // stop & clear all content
    void (*onCommandStopPlayonce)();        // stop & clear playonce content

    void (*onGatewayConnected)();
    void (*onGatewayDisconnected)();
    void (*onMicphoneStarted)();
    void (*onMicphoneStopped)();
    void (*onSpeakerMuted)();
    void (*onSpeakerUnmuted)();
} GnUtpManager_Callback_t;

bool GnUtpManager_Init(GnPlayer_Adapter_t *adapter);

bool GnUtpManager_IsInit();

bool GnUtpManager_Register_StateListener(void (*listener)(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech));

void GnUtpManager_Unregister_StateListener(void (*listener)(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech));

bool GnUtpManager_Get_Callback(GnUtpManager_Callback_t **callback);

bool GnUtpManager_Start();

bool GnUtpManager_IsActive();

void GnUtpManager_Stop();

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_PLAYER_UTPMANAGER_H__ */
