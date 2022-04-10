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

#ifndef __TMALLGENIE_PLAYER_MAIN_H__
#define __TMALLGENIE_PLAYER_MAIN_H__

#include <stdio.h>
#include <stdbool.h>
#include "GenieUtpManager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GENIE_PREBUILT_WAKEUP_REMIND        "prebuilt://WAKEUP_REMIND.mp3"
#define GENIE_PREBUILT_RECORD_REMIND        "prebuilt://RECORD_REMIND.mp3"
#define GENIE_PREBUILT_NETWORK_DISCONNECTED "prebuilt://NETWORK_DISCONNECTED.mp3"
#define GENIE_PREBUILT_SERVER_DISCONNECTED  "prebuilt://SERVER_DISCONNECTED.mp3"
#define GENIE_PREBUILT_ACCOUNT_UNAUTHORIZED "prebuilt://ACCOUNT_UNAUTHORIZED.mp3"

typedef struct {
    void *(*open)(int sampleRate, int channelCount, int bitsPerSample);
    int   (*write)(void *handle, void *buf, unsigned int size); // return bytes written, <0 means fail
    void  (*close)(void *handle);
} GnVendor_PcmOut_t;

bool GnPlayer_Init(GnVendor_PcmOut_t *pcmOut);

bool GnPlayer_IsInit();

bool GnPlayer_Start();

bool GnPlayer_New_Prompt(const char *url);

bool GnPlayer_Clear_Playonce();

bool GnPlayer_IsActive();

void GnPlayer_Stop();

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_PLAYER_MAIN_H__ */
