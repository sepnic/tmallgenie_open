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

#ifndef __TMALLGENIE_SDK_MAIN_H__
#define __TMALLGENIE_SDK_MAIN_H__

#include <stdio.h>
#include <stdbool.h>

#include "GenieDefine.h"
#include "GenieVendorAdapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // network status changed
    void (*onNetworkConnected)();
    void (*onNetworkDisconnected)();

    // micphone status changed
    void (*onMicphoneWakeup)(const char *wakeupWord, int doa, double confidence);
    void (*onMicphoneSilence)();

    // speaker status changed
    void (*onSpeakerVolumeChanged)(int volume);
    void (*onSpeakerMutedChanged)(bool muted);

    // user request
    void (*onQueryUserInfo)();
    void (*onTextRecognize)(const char *inputText);
    void (*onNewPrompt)(const char *url);
} GenieSdk_Callback_t;

typedef void (*GenieCommandListener)(Genie_Domain_t domain, Genie_Command_t command, const char *payload);

typedef void (*GenieStatusListener)(Genie_Status_t status);

bool GenieSdk_Init(GnVendor_Wrapper_t *adapter);

bool GenieSdk_Get_Callback(GenieSdk_Callback_t **callback);

bool GenieSdk_Register_CommandListener(GenieCommandListener listener);

void GenieSdk_Unregister_CommandListener(GenieCommandListener listener);

bool GenieSdk_Register_StatusListener(GenieStatusListener listener);

void GenieSdk_Unregister_StatusListener(GenieStatusListener listener);

bool GenieSdk_Start();

bool GenieSdk_IsActive();

void GenieSdk_Stop();

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_SDK_MAIN_H__ */
