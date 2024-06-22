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

#ifndef __TMALLGENIE_CORE_PROTOCOL_H__
#define __TMALLGENIE_CORE_PROTOCOL_H__

#include "GenieDefine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GENIE_STATESYNC_REASON_START = 0,
    GENIE_STATESYNC_REASON_RECONNECT,
} Genie_StateSyncReason_t;

typedef enum {
    GENIE_SPEAKERSYNC_REASON_VOLUMECHANGED = 0,
    GENIE_SPEAKERSYNC_REASON_MUTEDCHANGED,
} Genie_SpeakerSyncReason_t;

typedef enum {
    GENIE_PLAYERSYNC_REASON_STARTED = 0,
    GENIE_PLAYERSYNC_REASON_PAUSED,
    GENIE_PLAYERSYNC_REASON_RESUMED,
    GENIE_PLAYERSYNC_REASON_NEARLYFINISHED,
    GENIE_PLAYERSYNC_REASON_FINISHED,
    GENIE_PLAYERSYNC_REASON_STOPPED,
    GENIE_PLAYERSYNC_REASON_FAILED,
} Genie_PlayerSyncReason_t;

typedef enum {
    GENIE_MICROPHONEACTIVE_REASON_USER = 0,
    GENIE_MICROPHONEACTIVE_REASON_SERVER,
} Genie_MicrophoneActiveReason_t;

int Genie_Domain_StringToInt(const char *domainString);

int Genie_Command_StringToInt(const char *commandString);

int Genie_Event_StringToInt(const char *eventString);

char *Genie_Get_DefaultCaCert();

char *Genie_Create_GatewayUrl(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *bizSecret/*non-nullable*/);
char *Genie_Create_GatewayUrl2();

char *Genie_Create_GuestActivateEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *bizSecret/*non-nullable*/,
        const char *macAddr/*non-nullable*/);
char *Genie_Create_GuestActivateEvent2(
        const char *macAddr/*non-nullable*/);

char *Genie_Create_QueryUserInfoEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/);
char *Genie_Create_QueryUserInfoEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/);

char *Genie_Create_StateSyncEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_StateSyncReason_t reason,
        Genie_SpeakerContext_t *speakerContext,/*nullable*/
        Genie_PlayerContext_t *playerContext/*nullable*/);
char *Genie_Create_StateSyncEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_StateSyncReason_t reason,
        Genie_SpeakerContext_t *speakerContext,/*nullable*/
        Genie_PlayerContext_t *playerContext/*nullable*/);

char *Genie_Create_SpeakerSyncEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_SpeakerSyncReason_t reason,
        Genie_SpeakerContext_t *speakerContext/*non-nullable*/);
char *Genie_Create_SpeakerSyncEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_SpeakerSyncReason_t reason,
        Genie_SpeakerContext_t *speakerContext/*non-nullable*/);

char *Genie_Create_PlayerSyncEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_PlayerSyncReason_t reason,
        Genie_PlayerContext_t *playerContext/*non-nullable*/);
char *Genie_Create_PlayerSyncEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_PlayerSyncReason_t reason,
        Genie_PlayerContext_t *playerContext/*non-nullable*/);

char *Genie_Create_TextRecognizeEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        const char *inputText/*non-nullable*/);
char *Genie_Create_TextRecognizeEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        const char *inputText/*non-nullable*/);

char *Genie_Create_MicrophoneActiveEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_MicrophoneActiveReason_t reason);
char *Genie_Create_MicrophoneActiveEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_MicrophoneActiveReason_t reason);

char *Genie_Create_MicrophoneListenStartedEvent(
        const char *bizType/*non-nullable*/,
        const char *bizGroup/*non-nullable*/,
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_SpeechContext_t *speechContext/*non-nullable*/);
char *Genie_Create_MicrophoneListenStartedEvent2(
        const char *uuid/*non-nullable*/,
        const char *accessToken/*non-nullable*/,
        Genie_SpeechContext_t *speechContext/*non-nullable*/);

char *Genie_Create_MicrophoneBinaryHeader(int *length/*non-nullable*/);

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_CORE_PROTOCOL_H__ */
