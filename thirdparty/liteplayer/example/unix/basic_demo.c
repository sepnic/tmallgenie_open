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
#include "source_httpclient_wrapper.h"
#include "source_file_wrapper.h"
#if defined(HAVE_LINUX_ALSA_ENABLED)
#include "sink_alsa_wrapper.h"
#elif defined(HAVE_PORT_AUDIO_ENABLED)
#include "sink_portaudio_wrapper.h"
#else
#include "sink_wave_wrapper.h"
#endif

#define TAG "basic_demo"

static int basic_demo_state_listener(enum liteplayer_state state, int errcode, void *priv)
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

static int basic_demo(const char *url)
{
    int ret = -1;
    liteplayer_handle_t player = liteplayer_create();
    if (player == NULL)
        return ret;

    enum liteplayer_state player_state = LITEPLAYER_IDLE;
    liteplayer_register_state_listener(player, basic_demo_state_listener, (void *)&player_state);

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
    liteplayer_register_sink_wrapper(player, &sink_ops);

    struct source_wrapper file_ops = {
        .async_mode = false,
        .buffer_size = 2*1024,
        .priv_data = NULL,
        .url_protocol = file_wrapper_url_protocol,
        .open = file_wrapper_open,
        .read = file_wrapper_read,
        .content_pos = file_wrapper_content_pos,
        .content_len = file_wrapper_content_len,
        .seek = file_wrapper_seek,
        .close = file_wrapper_close,
    };
    liteplayer_register_source_wrapper(player, &file_ops);

    struct source_wrapper http_ops = {
        .async_mode = true,
        .buffer_size = 256*1024,
        .priv_data = NULL,
        .url_protocol = httpclient_wrapper_url_protocol,
        .open = httpclient_wrapper_open,
        .read = httpclient_wrapper_read,
        .content_pos = httpclient_wrapper_content_pos,
        .content_len = httpclient_wrapper_content_len,
        .seek = httpclient_wrapper_seek,
        .close = httpclient_wrapper_close,
    };
    liteplayer_register_source_wrapper(player, &http_ops);

    if (liteplayer_set_data_source(player, url) != 0) {
        OS_LOGE(TAG, "Failed to set data source");
        goto test_done;
    }

    if (liteplayer_prepare_async(player) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_PREPARED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }
    if (player_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto test_done;
    }

    if (liteplayer_start(player) != 0) {
        OS_LOGE(TAG, "Failed to start player");
        goto test_done;
    }
    OS_MEMORY_DUMP();
    while (player_state != LITEPLAYER_COMPLETED && player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }

    if (liteplayer_stop(player) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto test_done;
    }
    while (player_state != LITEPLAYER_STOPPED) {
        os_thread_sleep_msec(100);
    }

    ret = 0;

test_done:
    liteplayer_reset(player);
    while (player_state != LITEPLAYER_IDLE) {
        os_thread_sleep_msec(100);
    }

    os_thread_sleep_msec(1000);
    liteplayer_destroy(player);

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

    basic_demo(argv[1]);
    return 0;
}
