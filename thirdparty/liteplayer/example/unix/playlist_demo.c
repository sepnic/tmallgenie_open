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
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "liteplayer_listplayer.h"
#include "source_httpclient_wrapper.h"
#include "source_file_wrapper.h"
#if defined(HAVE_LINUX_ALSA_ENABLED)
#include "sink_alsa_wrapper.h"
#elif defined(HAVE_PORT_AUDIO_ENABLED)
#include "sink_portaudio_wrapper.h"
#else
#include "sink_wave_wrapper.h"
#endif

#define TAG "playlist_demo"

#define PLAYLIST_FILE "playlist_demo.playlist"

#define PLAYLIST_DEMO_TASK_PRIO    (OS_THREAD_PRIO_NORMAL)
#define PLAYLIST_DEMO_TASK_STACK   (8192)

struct playlist_demo_priv {
    const char *url;
    listplayer_handle_t player_handle;
    enum liteplayer_state player_state;
    bool exit;
};

static int generate_playlist(const char *path)
{
    DIR *dir = NULL;
    if ((dir = opendir(path)) == NULL) {
        OS_LOGE(TAG, "Failed to open dir[%s]", path);
        return -1;
    } else {
        struct dirent *entry;
        char buffer[512];
        FILE *file = fopen(PLAYLIST_FILE, "wb+");
        if (file == NULL) {
            OS_LOGE(TAG, "Failed to open playlist file");
            closedir(dir);
            return -1;
        }

        while ((entry = readdir(dir)) != NULL) {
            OS_LOGD(TAG, "-->d_name=[%s], d_type=[%d]", entry->d_name, entry->d_type);
            if (entry->d_type == DT_REG &&
                (strstr(entry->d_name, ".mp3") != NULL ||
                 strstr(entry->d_name, ".m4a") != NULL ||
                 strstr(entry->d_name, ".wav") != NULL)) {
                snprintf(buffer, sizeof(buffer), "%s/%s\n", path, entry->d_name);
                fwrite(buffer, 1, strlen(buffer), file);
            }
        }
        buffer[0] = '\n';
        fwrite(buffer, 1, 1, file);

        fflush(file);
        fclose(file);
        closedir(dir);
        return 0;
    }

    return -1;
}

static int playlist_demo_state_callback(enum liteplayer_state state, int errcode, void *priv)
{
    struct playlist_demo_priv *demo = (struct playlist_demo_priv *)priv;
    bool state_sync = true;

    switch (state) {
    case LITEPLAYER_IDLE:
        OS_LOGI(TAG, "-->LITEPLAYER_IDLE");
        break;
    case LITEPLAYER_INITED:
        OS_LOGI(TAG, "-->LITEPLAYER_INITED");
        break;
    case LITEPLAYER_PREPARED:
        OS_LOGI(TAG, "-->LITEPLAYER_PREPARED");
        break;
    case LITEPLAYER_STARTED:
        OS_LOGI(TAG, "-->LITEPLAYER_STARTED");
        break;
    case LITEPLAYER_PAUSED:
        OS_LOGI(TAG, "-->LITEPLAYER_PAUSED");
        break;
    case LITEPLAYER_SEEKCOMPLETED:
        OS_LOGI(TAG, "-->LITEPLAYER_SEEKCOMPLETED");
        break;
    case LITEPLAYER_NEARLYCOMPLETED:
        OS_LOGI(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
        state_sync = false;
        break;
    case LITEPLAYER_COMPLETED:
        OS_LOGI(TAG, "-->LITEPLAYER_COMPLETED");
        break;
    case LITEPLAYER_STOPPED:
        OS_LOGI(TAG, "-->LITEPLAYER_STOPPED");
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
        demo->player_state = state;
    return 0;
}

static void *playlist_demo_thread(void *arg)
{
    struct playlist_demo_priv *demo = (struct playlist_demo_priv *)arg;

    OS_LOGD(TAG, "listplayer demo thread enter");

    struct listplayer_cfg cfg = DEFAULT_LISTPLAYER_CFG();
    demo->player_handle = listplayer_create(&cfg);
    if (demo->player_handle == NULL)
        return NULL;

    listplayer_register_state_listener(demo->player_handle, playlist_demo_state_callback, (void *)demo);

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
    listplayer_register_sink_wrapper(demo->player_handle, &sink_ops);

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
    listplayer_register_source_wrapper(demo->player_handle, &file_ops);

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
    listplayer_register_source_wrapper(demo->player_handle, &http_ops);

    if (listplayer_set_data_source(demo->player_handle, demo->url) != 0) {
        OS_LOGE(TAG, "Failed to set data source");
        goto thread_exit;
    }

    if (listplayer_prepare_async(demo->player_handle) != 0) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto thread_exit;
    }
    while (demo->player_state != LITEPLAYER_PREPARED && demo->player_state != LITEPLAYER_ERROR) {
        os_thread_sleep_msec(100);
    }
    if (demo->player_state == LITEPLAYER_ERROR) {
        OS_LOGE(TAG, "Failed to prepare player");
        goto thread_exit;
    }

    if (listplayer_start(demo->player_handle) != 0) {
        OS_LOGE(TAG, "Failed to start player");
        goto thread_exit;
    }
    OS_MEMORY_DUMP();
    while (demo->player_state != LITEPLAYER_COMPLETED && demo->player_state != LITEPLAYER_ERROR) {
        if (demo->player_state == LITEPLAYER_STOPPED || demo->player_state == LITEPLAYER_IDLE) {
            goto thread_exit;
        }
        os_thread_sleep_msec(100);
    }

    if (listplayer_stop(demo->player_handle) != 0) {
        OS_LOGE(TAG, "Failed to stop player");
        goto thread_exit;
    }
    while (demo->player_state != LITEPLAYER_STOPPED) {
        os_thread_sleep_msec(100);
    }

thread_exit:
    demo->exit = true;

    listplayer_reset(demo->player_handle);
    while (demo->player_state != LITEPLAYER_IDLE) {
        os_thread_sleep_msec(100);
    }
    listplayer_destroy(demo->player_handle);
    demo->player_handle = NULL;

    os_thread_sleep_msec(100);
    OS_MEMORY_DUMP();

    OS_LOGD(TAG, "listplayer demo thread leave");
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        OS_LOGI(TAG, "Usage: %s [url]", argv[0]);
        return 0;
    }

    struct os_thread_attr attr = {
        .name = "listplayer_demo",
        .priority = PLAYLIST_DEMO_TASK_PRIO,
        .stacksize = PLAYLIST_DEMO_TASK_STACK,
        .joinable = true,
    };

    const char *filename = OS_STRDUP(argv[1]);
    const char *url = filename;
    if (strstr(filename, "http") == NULL) {
        struct stat statbuf;
        if (stat(filename, &statbuf) < 0) {
            OS_LOGE(TAG, "Failed to stat path[%s]", filename);
            goto demo_out;
        }

        if (S_ISDIR(statbuf.st_mode) && generate_playlist(filename) == 0) {
            url = PLAYLIST_FILE;
        }
    }

    struct playlist_demo_priv demo;
    memset(&demo, 0x0, sizeof(demo));
    demo.url = url;
    os_thread tid = os_thread_create(&attr, playlist_demo_thread, (void *)&demo);
    if (tid == NULL)
        goto demo_out;

    char input = 0;
    while (!demo.exit) {
        if (input != '\n') {
            OS_LOGW(TAG, "Waiting enter command:");
            OS_LOGW(TAG, "  Q|q: quit");
            OS_LOGW(TAG, "  P|p: pause");
            OS_LOGW(TAG, "  R|r: resume");
            OS_LOGW(TAG, "  S|s: seek");
            OS_LOGW(TAG, "  N|n: switch next");
            OS_LOGW(TAG, "  V|v: switch prev");
            OS_LOGW(TAG, "  O:   looping enable");
            OS_LOGW(TAG, "  o:   looping disable");
        }
        input = getc(stdin);

        if (input == 'Q' || input == 'q') {
           OS_LOGI(TAG, "Quit");
            if (demo.player_handle)
                listplayer_reset(demo.player_handle);
            break;
        } else if (input == 'P' || input == 'p') {
           OS_LOGI(TAG, "Pause");
            if (demo.player_handle)
                listplayer_pause(demo.player_handle);
        } else if (input == 'R' || input == 'r') {
           OS_LOGI(TAG, "Resume");
            if (demo.player_handle)
                listplayer_resume(demo.player_handle);
        } else if (input == 'S' || input == 's') {
           OS_LOGI(TAG, "Seek 10s");
            if (demo.player_handle) {
                int position = 0;
                if (listplayer_get_position(demo.player_handle, &position) == 0)
                    listplayer_seek(demo.player_handle, position+10000);
            }
        } else if (input == 'N' || input == 'n') {
           OS_LOGI(TAG, "Next");
            if (demo.player_handle)
                listplayer_switch_next(demo.player_handle);
        } else if (input == 'V' || input == 'v') {
           OS_LOGI(TAG, "Prev");
            if (demo.player_handle)
                listplayer_switch_prev(demo.player_handle);
        } else if (input == 'O') {
           OS_LOGI(TAG, "Enable looping");
            if (demo.player_handle)
                listplayer_set_single_looping(demo.player_handle, true);
        } else if (input == 'o') {
           OS_LOGI(TAG, "Disable looping");
            if (demo.player_handle)
                listplayer_set_single_looping(demo.player_handle, false);
        } else {
            if (input != '\n')
                OS_LOGW(TAG, "Unknown command: %c", input);
        }
    }

    os_thread_join(tid, NULL);

demo_out:
    OS_FREE(filename);
    OS_MEMORY_DUMP();
    OS_LOGD(TAG, "playlist_demo main thread leave");
    return 0;
}
