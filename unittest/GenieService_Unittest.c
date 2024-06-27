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

#include "core/GenieService.h"

#define TAG "GenieService_Unittest"

#define AUDIO_FORMAT_WAV

static const char *bizType()       { return NULL; } // FIXME: apply for your device key from https://product.aligenie.com
static const char *bizGroup()      { return NULL; } // FIXME: apply for your device key from https://product.aligenie.com
static const char *bizSecret()     { return NULL; } // FIXME: apply for your device key from https://product.aligenie.com
static const char *caCert()        { return NULL; } // FIXME: apply for your device key from https://product.aligenie.com
static const char *macAddr()       { return "11:22:33:44:55:66"; } // FIXME: fill your real mac address
static const char *uuid()          { return NULL; }
static const char *accessToken()   { return NULL; }

static void commandListener(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    OS_LOGD(TAG, "Received command: domain=%d, command=%d", domain, command);
}
static void ttsListener(char *data, int len, bool final)
{
    OS_LOGD(TAG, "Received tts: data=%p, len=%d, final=%d", data, len, final);
}
static void statusListener(Genie_Status_t status)
{
    OS_LOGD(TAG, "Received status: status=%d", status);
}

int main(int argc, char **argv)
{
    FILE *audio_file = NULL;
#ifdef AUDIO_FORMAT_WAV
    audio_file = fopen("test.wav", "rb");
#else
    audio_file = fopen("test.ogg", "rb");
#endif

    GnService_Adapter_t adapter = {
        .bizType = bizType,
        .bizGroup = bizGroup,
        .bizSecret = bizSecret,
        .caCert = caCert,
        .macAddr = macAddr,
        .uuid = uuid,
        .accessToken = accessToken,
    };
    GnService_Callback_t *callback;

    if (!GnService_Init(&adapter)) {
        OS_LOGE(TAG, "Failed to GnService_Init");
        goto __exit;
    }
    if (!GnService_Get_Callback(&callback)) {
        OS_LOGE(TAG, "Failed to GnService_Get_Callback");
        goto __exit;
    }

    GnService_Register_CommandListener(commandListener);
    GnService_Register_TtsbinaryListener(ttsListener);
    GnService_Register_StatusListener(statusListener);

    if (!GnService_Start()) {
        OS_LOGE(TAG, "Failed to GnService_Start");
        goto __exit;
    }
    callback->onNetworkConnected();

    os_thread_sleep_msec(5000);
    callback->onTextRecognize("今天天气怎么样");

    if (audio_file != NULL) {
        os_thread_sleep_msec(5000);
#ifdef AUDIO_FORMAT_WAV
        Genie_SpeechFormat_t format = GENIE_SPEECH_FORMAT_WAV;
#else
        Genie_SpeechFormat_t format = GENIE_SPEECH_FORMAT_SPEEXOGG;
#endif
        callback->onMicphoneWakeup("ni hao tian mao", 0, 0.600998834);

        while (1) {
            char buffer[2048];
            int nread = fread(buffer, 1, sizeof(buffer), audio_file);
            if (nread == sizeof(buffer))
                callback->onMicphoneStreaming(format, buffer, nread, false);
            else if (nread < 0)
                goto __exit;
            else {
                callback->onMicphoneStreaming(format, buffer, nread, true);
                break;
            }
        }
    }

    os_thread_sleep_msec(30*1000); // waiting 30s

__exit:
    GnService_Stop();
    if (audio_file != NULL)
        fclose(audio_file);
    OS_MEMORY_DUMP();
    return 0;
}
