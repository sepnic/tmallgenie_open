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

#ifndef __TMALLGENIE_DEFINE_H__
#define __TMALLGENIE_DEFINE_H__

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GENIE_DOMAIN_Account = 0,               // AliGenie.Account
    GENIE_DOMAIN_System,                    // AliGenie.System
    GENIE_DOMAIN_Microphone,                // AliGenie.Microphone
    GENIE_DOMAIN_Speaker,                   // AliGenie.Speaker
    GENIE_DOMAIN_Text,                      // AliGenie.Text
    GENIE_DOMAIN_Audio,                     // AliGenie.Audio
    GENIE_DOMAIN_SystemControl,             // AliGenie.System.Control
    GENIE_DOMAIN_DotMatrixDisplay,          // AliGenie.DotMatrixDisplay

    GENIE_DOMAIN_Network,                   // AliGenie.Network
    GENIE_DOMAIN_Light,                     // AliGenie.Light
    GENIE_DOMAIN_Data,                      // AliGenie.Data
    GENIE_DOMAIN_Alarm,                     // AliGenie.Alarm
} Genie_Domain_t;

typedef enum {
// AliGenie.Account
    GENIE_COMMAND_GuestDeviceActivateResp   = 0x0,
    GENIE_COMMAND_MemberDeviceActivateResp  = 0x1,
    GENIE_COMMAND_UserInfoResp              = 0x2,
// AliGenie.Microphone
    GENIE_COMMAND_StopListen                = 0x10,
    GENIE_COMMAND_ExpectSpeechStart         = 0x11,
    GENIE_COMMAND_ExpectSpeechStop          = 0x12,
// AliGenie.Speaker
    GENIE_COMMAND_Speak                     = 0x20,
    GENIE_COMMAND_SetVolume                 = 0x21,
    GENIE_COMMAND_AdjustVolume              = 0x22,
    GENIE_COMMAND_SetMute                   = 0x23,
// AliGenie.Text
    GENIE_COMMAND_ListenResult              = 0x30,
// AliGenie.Audio
    GENIE_COMMAND_Play                      = 0x40,
    GENIE_COMMAND_PlayOnce                  = 0x41,
    GENIE_COMMAND_ClearQueue                = 0x42,
// AliGenie.DotMatrixDisplay
    GENIE_COMMAND_Render                    = 0x50,
// AliGenie.Light
    GENIE_COMMAND_Adjust                    = 0x60,
// AliGenie.Data
    GENIE_COMMAND_DataSync                  = 0x70,
// AliGenie.Alarm
    GENIE_COMMAND_Set                       = 0x80,
// AliGenie.Network
    GENIE_COMMAND_NetworkConfig             = 0x100,
// AliGenie.System
    GENIE_COMMAND_Setting                   = 0x1000,
    GENIE_COMMAND_ThrowException            = 0x1001,
    GENIE_COMMAND_Success                   = 0x1002,
// AliGenie.System.Control
    GENIE_COMMAND_Pause                     = 0x2000,
    GENIE_COMMAND_Resume                    = 0x2001,
    GENIE_COMMAND_Exit                      = 0x2002,
    GENIE_COMMAND_Standby                   = 0x2003,
    GENIE_COMMAND_Volume                    = 0x2004,
} Genie_Command_t;

typedef enum {
// AliGenie.Account
    GENIE_EVENT_GuestDeviceActivate         = 0x0,
    GENIE_EVENT_MemberDeviceActivate        = 0x1,
    GENIE_EVENT_UserInfo                    = 0x2,
// AliGenie.Microphone
    GENIE_EVENT_Active                      = 0x10,
    GENIE_EVENT_ListenStarted               = 0x11,
// AliGenie.Speaker
    GENIE_EVENT_VolumeChanged               = 0x20,
    GENIE_EVENT_MuteChanged                 = 0x21,
// AliGenie.Text
    GENIE_EVENT_Recognize                   = 0x30,
// AliGenie.Audio
    GENIE_EVENT_PlaybackStarted             = 0x40,
    GENIE_EVENT_PlaybackPaused              = 0x41,
    GENIE_EVENT_PlaybackResumed             = 0x42,
    GENIE_EVENT_PlaybackNearlyFinished      = 0x43,
    GENIE_EVENT_PlaybackFinished            = 0x44,
    GENIE_EVENT_PlaybackStopped             = 0x45,
    GENIE_EVENT_PlaybackFailed              = 0x46,
    GENIE_EVENT_PlaybackQueueCleared        = 0x47,
// AliGenie.DotMatrixDisplay
// AliGenie.Light
// AliGenie.Data
    GENIE_EVENT_DataRequest                 = 0x70,
// AliGenie.Alarm
    GENIE_EVENT_AlarmStarted                = 0x80,
    GENIE_EVENT_AlarmStopped                = 0x81,
// AliGenie.Network
// AliGenie.System
    GENIE_EVENT_SynchronizeState            = 0x1000,
    GENIE_EVENT_StatusReport                = 0x1001,
// AliGenie.System.Control
} Genie_Event_t;

typedef enum {
    GENIE_ERROR_General                     = -1,
    GENIE_ERROR_BadRequest                  = 400,    // 请求失败：如参数问题 (缺少必填、格式有误)
    GENIE_ERROR_Unauthorized                = 401,    // 客户端请求认证失败：未被授权或token已过期
    GENIE_ERROR_Forbidden                   = 403,    // 对事件⽆访问权限
    GENIE_ERROR_NotFound                    = 404,    // 找不到该事件
    GENIE_ERROR_ThrottlingException         = 429,    // 流量超限
    GENIE_ERROR_InternalServerError         = 500,    // 服务器内部错误
    GENIE_ERROR_ServiceUnavailable          = 503,    // 服务不可⽤
    GENIE_ERROR_GatewayTimeout              = 504,    // ⽹关超时
    GENIE_ERROR_AsrRecognizeError           = 600,    // ASR调⽤失败
    GENIE_ERROR_NluExecuteError             = 601,    // NLU调⽤失败
    GENIE_ERROR_TtsSynthesizeError          = 602,    // TTS调⽤失败
    GENIE_ERROR_AuthCodeNotValid            = 603,    // 设备绑定时⾮有效授权码
    GENIE_ERROR_NluNotUnderstandSpeech      = 604,    // 语义服务不⽀持语⾳指令
    GENIE_ERROR_Success                     = 100000, // 成功
} Genie_Error_t;

typedef enum {
    GENIE_STATUS_NetworkDisconnected        = 0,
    GENIE_STATUS_NetworkConnected           = 1,
    GENIE_STATUS_GatewayDisconnected        = 2,
    GENIE_STATUS_GatewayConnected           = 3,
    GENIE_STATUS_Unauthorized               = 4,
    GENIE_STATUS_Authorized                 = 5,
    GENIE_STATUS_SpeakerUnmuted             = 6,
    GENIE_STATUS_SpeakerMuted               = 7,
    GENIE_STATUS_MicphoneWakeup             = 8,
    GENIE_STATUS_MicphoneStarted            = 9,
    GENIE_STATUS_MicphoneStopped            = 10,
} Genie_Status_t;

typedef enum {
    GENIE_PLAYER_SOURCE_NONE = 0,
    GENIE_PLAYER_SOURCE_CLOUD,
    GENIE_PLAYER_SOURCE_BLUETOOTH,
} Genie_PlayerSource_t;

typedef enum {
    GENIE_SPEECH_FORMAT_WAV = 0,
    GENIE_SPEECH_FORMAT_SPEEXOGG,
} Genie_SpeechFormat_t;

typedef struct {
    Genie_PlayerSource_t source;
    const char *audioUrl;
    const char *audioAnchor;
    const char *audioExt;
    const char *audioId;
    const char *audioName;
    const char *audioType;
    const char *audioAlbum;
    const char *audioSource;
    int progress;
    int audioLength;
} Genie_PlayerContext_t;

typedef struct {
    int volume;
    bool isMute;
} Genie_SpeakerContext_t;

typedef struct {
    Genie_SpeechFormat_t format;
    const char *wakeupWord;
    int doa;
    double confidence;
} Genie_SpeechContext_t;

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_DEFINE_H__ */
