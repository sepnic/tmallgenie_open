// Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
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
#include <stdbool.h>
#include <string.h>

#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "liteplayer_main.h"
#include "liteplayer_ttsplayer.h"
#if defined(HAVE_LINUX_ALSA_ENABLED)
#include "sink_alsa_wrapper.h"
#elif defined(HAVE_PORT_AUDIO_ENABLED)
#include "sink_portaudio_wrapper.h"
#else
#include "sink_wave_wrapper.h"
#endif

#define TAG "tts_demo"

struct tts_source_priv {
    ttsplayer_handle_t player;
    const char *url;
    enum liteplayer_state *state;
};

static int tts_demo_state_listener(enum liteplayer_state state, int errcode, void *priv)
{
    enum liteplayer_state *player_state = (enum liteplayer_state *)priv;
    bool state_sync = true;

    switch (state) {
    case LITEPLAYER_IDLE:
        OS_LOGD(TAG, "-->LITEPLAYER_IDLE");
        break;
    case LITEPLAYER_INITED:
        OS_LOGD(TAG, "-->LITEPLAYER_INITED");
        break;
    case LITEPLAYER_PREPARED:
        OS_LOGD(TAG, "-->LITEPLAYER_PREPARED");
        break;
    case LITEPLAYER_STARTED:
        OS_LOGD(TAG, "-->LITEPLAYER_STARTED");
        break;
    case LITEPLAYER_PAUSED:
        OS_LOGD(TAG, "-->LITEPLAYER_PAUSED");
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        state_sync = false;
        break;
    case LITEPLAYER_COMPLETED:
        OS_LOGD(TAG, "-->LITEPLAYER_COMPLETED");
        break;
    case LITEPLAYER_STOPPED:
        OS_LOGD(TAG, "-->LITEPLAYER_STOPPED");
        break;
    case LITEPLAYER_ERROR:
        OS_LOGE(TAG, "-->LITEPLAYER_ERROR: %d", errcode);
        break;
    default:
        OS_LOGE(TAG, "-->LITEPLAYER_UNKNOWN: %d", state);
        state_sync = false;
        break;
    }

    if (state_sync)
        *player_state = state;
    return 0;
}

static void *tts_source_thread(void *arg)
{
    struct tts_source_priv *priv = (struct tts_source_priv *)arg;
    FILE *file = fopen(priv->url, "rb");
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open url: %s", priv->url);
        *(priv->state) = LITEPLAYER_ERROR;
        goto thread_out;
    }

    char buffer[2048];
    while (*(priv->state) < LITEPLAYER_COMPLETED) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read > 0) {
            ttsplayer_write(priv->player, buffer, bytes_read, false);
        } else if (bytes_read == 0) {
            ttsplayer_write(priv->player, buffer, bytes_read, true);
            break;
        } else {
            *(priv->state) = LITEPLAYER_ERROR;
            break;
        }
    }

thread_out:
    if (file != NULL)
        fclose(file);
    return NULL;
}

static int tts_demo(const char *url)
{
    int ret = -1;
    ttsplayer_handle_t player = ttsplayer_create(NULL);
    if (player == NULL)
        return ret;

    enum liteplayer_state player_state = LITEPLAYER_IDLE;
    ttsplayer_register_state_listener(player, tts_demo_state_listener, (void *)&player_state);

#if defined(HAVE_LINUX_ALSA_ENABLED)
    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = alsa_wrapper_name,
        .open = alsa_wrapper_open,
        .write = alsa_wrapper_write,
        .close = alsa_wrapper_close,
    };
#elif defined(HAVE_PORT_AUDIO_ENABLED)
    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = portaudio_wrapper_name,
        .open = portaudio_wrapper_open,
        .write = portaudio_wrapper_write,
        .close = portaudio_wrapper_close,
    };
#else
    struct sink_wrapper sink_ops = {
        .priv_data = NULL,
        .name = wave_wrapper_name,
        .open = wave_wrapper_open,
        .write = wave_wrapper_write,
        .close = wave_wrapper_close,
    };
#endif
    ttsplayer_register_sink_wrapper(player, &sink_ops);

    if (ttsplayer_prepare_async(player) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    struct os_thread_attr thread_attr = {
        .name = "tts_source",
        .priority = OS_THREAD_PRIO_NORMAL,
        .stacksize = 4096,
        .joinable = false,
    };
    struct tts_source_priv tts_source = {
        .player = player,
        .state = &player_state,
        .url = url,
    };
    os_thread_create(&thread_attr, tts_source_thread, &tts_source);

    while (player_state != LITEPLAYER_PREPARED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }
    if (player_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (ttsplayer_start(player) != 0) {
        OS_LOGE(TAG, "Failed to start player");
        goto test_done;
    }
    OS_MEMORY_DUMP();
    while (player_state != LITEPLAYER_COMPLETED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }

    if (ttsplayer_stop(player) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_STOPPED) {
        os_thread_sleep_msec(100);
    }

    ret = 0;

test_done:
    ttsplayer_reset(player);
    while (player_state != LITEPLAYER_IDLE) {
        os_thread_sleep_msec(100);
    }

    os_thread_sleep_msec(1000);
    ttsplayer_destroy(player);

    os_thread_sleep_msec(100);
    OS_MEMORY_DUMP();
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        OS_LOGW(TAG, "Usage: %s [url]", argv[0]);
        return -1;
    }

    tts_demo(argv[1]);
    return 0;
}
