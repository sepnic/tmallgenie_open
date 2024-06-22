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
#include "cutils/list.h"
#include "cutils/log_helper.h"
#include "cutils/mlooper.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_adapter_internal.h"
#include "liteplayer_adapter.h"
#include "liteplayer_config.h"
#include "liteplayer_main.h"
#include "liteplayer_listplayer.h"

#define TAG "[liteplayer]listplayer"

#define DEFAULT_PLAYLIST_URL_LEN  128

struct listplayer {
    struct listplayer_cfg       cfg;
    liteplayer_handle_t         player;
    liteplayer_adapter_handle_t adapter;
    mlooper_handle              looper;
    os_mutex                    lock;
    enum liteplayer_state       state;
    liteplayer_state_cb         listener;
    void                       *listener_priv;
    struct source_wrapper      *file_ops;

    struct listnode      url_list;
    struct listnode     *url_curr;
    int                  url_count;

    bool                 is_list;
    bool                 is_paused;
    bool                 is_looping;
    bool                 has_inited;
    bool                 has_prepared;
    bool                 has_started;
};

struct url_node {
    const char *url;
    struct listnode listnode;
};

enum {
    PLAYER_DO_SET_SOURCE = 0,
    PLAYER_DO_PREPARE,
    PLAYER_DO_START,
    PLAYER_DO_PAUSE,
    PLAYER_DO_RESUME,
    PLAYER_DO_SEEK,
    PLAYER_DO_NEXT,
    PLAYER_DO_PREV,
    PLAYER_DO_STOP,
    PLAYER_DO_RESET,
};

static void playlist_clear(listplayer_handle_t handle)
{
    struct url_node *node = NULL;
    struct listnode *item, *tmp;
    os_mutex_lock(handle->lock);
    list_for_each_safe(item, tmp, &handle->url_list) {
        node = listnode_to_item(item, struct url_node, listnode);
        list_remove(item);
        audio_free(node->url);
        audio_free(node);
    }
    handle->url_count = 0;
    handle->is_list = false;
    os_mutex_unlock(handle->lock);
}

static int playlist_insert(listplayer_handle_t handle, const char *url)
{
    os_mutex_lock(handle->lock);

    if (handle->url_count >= handle->cfg.playlist_url_max) {
        OS_LOGE(TAG, "Reach max url count: %d, aborting url: %s", handle->url_count, url);
        os_mutex_unlock(handle->lock);
        return -1;
    }

    struct url_node *node = audio_calloc(1, sizeof(struct url_node));
    if (node == NULL) {
        os_mutex_unlock(handle->lock);
        return -1;
    }
    node->url = audio_strdup(url);
    if (node->url == NULL) {
        audio_free(node);
        os_mutex_unlock(handle->lock);
        return -1;
    }

    list_add_tail(&handle->url_list, &node->listnode);
    handle->url_count++;

    os_mutex_unlock(handle->lock);
    return 0;
}

static char *playlist_get_line(char *buffer, int *index, int *remain)
{
    char c, *out = NULL;
    if (*remain > 0) {
        bool line_end = false;
        out = buffer + *index;
        int idx = *index;
        while ((c = buffer[idx]) != 0) {
            if (c == '\r' || c == '\n') {
                buffer[idx] = 0;
                line_end = true;
            } else if (line_end) {
                *remain -= idx - *index;
                *index = idx;
                return out;
            }
            idx++;
            if (idx == (*index + *remain)) {
                *remain = 0;
                return out;
            }
        }
    }
    return NULL;
}

static int playlist_resolve(listplayer_handle_t handle, const char *filename)
{
    int ret = -1;
    source_handle_t file = NULL;
    int bufsize = handle->cfg.playlist_url_max * DEFAULT_PLAYLIST_URL_LEN;
    char *content = audio_malloc(bufsize);
    if (content == NULL) {
        OS_LOGE(TAG, "Failed to allocate playlist parser buffer");
        goto resolve_done;
    }

    file = handle->file_ops->open(filename, 0, handle->file_ops->priv_data);
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open playlist");
        goto resolve_done;
    }

    int bytes_read = handle->file_ops->read(file, content, bufsize);
    if (bytes_read <= 0) {
        OS_LOGE(TAG, "Failed to read playlist");
        goto resolve_done;
    }
    OS_LOGV(TAG, "Succeed to read playlist:\n%s", content);

    int index = 0, remain = bytes_read;
    char *line = NULL;
    while ((line = playlist_get_line(content, &index, &remain)) != NULL) {
        playlist_insert(handle, line);
    }

    os_mutex_lock(handle->lock);
    if (handle->url_count > 0) {
        struct listnode *item;
        int i = 0;
        list_for_each(item, &handle->url_list) {
            struct url_node *node = listnode_to_item(item, struct url_node, listnode);
            OS_LOGD(TAG, "-->playlist: url[%d]=[%s]", i, node->url);
            i++;
        }
        handle->is_list = true;
        ret = 0;
    }
    os_mutex_unlock(handle->lock);

resolve_done:
    if (file != NULL)
        handle->file_ops->close(file);
    if (content != NULL)
        audio_free(content);
    return ret;
}

static int listplayer_state_callback(enum liteplayer_state state, int errcode, void *priv)
{
    listplayer_handle_t handle = (listplayer_handle_t)priv;
    bool state_sync = true;

    os_mutex_lock(handle->lock);

    switch (state) {
    case LITEPLAYER_INITED:
        if (handle->has_inited) {
            struct message *msg = message_obtain(PLAYER_DO_PREPARE, 0, 0, handle);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(handle->looper, msg);
            }
        }
        handle->has_inited = true;
        break;

    case LITEPLAYER_PREPARED:
        if (handle->has_prepared) {
            struct message *msg = message_obtain(PLAYER_DO_START, 0, 0, handle);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(handle->looper, msg);
            }
        }
        handle->has_prepared = true;
        break;

    case LITEPLAYER_STARTED:
        if (handle->has_started) {
            state_sync = handle->is_paused;
        }
        handle->is_paused = false;
        handle->has_started = true;
        break;

    case LITEPLAYER_PAUSED:
        handle->is_paused = true;
        break;

    case LITEPLAYER_SEEKCOMPLETED:
        state_sync = false;
        if (!handle->is_paused) {
            struct message *msg = message_obtain(PLAYER_DO_START, 0, 0, handle);
            if (msg != NULL)
                mlooper_post_message(handle->looper, msg);
        }
        break;

    case LITEPLAYER_NEARLYCOMPLETED:
        if (handle->is_list || handle->is_looping) {
            state_sync = false;
        }
        break;

    case LITEPLAYER_COMPLETED:
        if (handle->is_list || handle->is_looping) {
            struct message *msg = message_obtain(PLAYER_DO_STOP, 0, 0, handle);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(handle->looper, msg);
            }
        }
        break;

    case LITEPLAYER_ERROR: {
        struct listnode *curr = handle->url_curr;
        if (curr == list_head(&handle->url_list))
            handle->url_curr = list_tail(&handle->url_list);
        else
            handle->url_curr = curr->prev;
        list_remove(curr);
        handle->url_count--;

        struct url_node *node = listnode_to_item(curr, struct url_node, listnode);
        OS_LOGW(TAG, "Failed to play url: %s, remove this url from list", node->url);
        audio_free(node->url);
        audio_free(node);

        if ((handle->is_list || handle->is_looping) && handle->url_count > 0) {
            struct message *msg = message_obtain(PLAYER_DO_STOP, 0, 0, handle);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(handle->looper, msg);
            }
        }
    }
        break;

    case LITEPLAYER_STOPPED:
        if ((handle->is_list || handle->is_looping) && handle->url_count > 0) {
            struct message *msg = message_obtain(PLAYER_DO_RESET, 0, 0, handle);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(handle->looper, msg);
            }
        }
        break;

    case LITEPLAYER_IDLE:
        if ((handle->is_list || handle->is_looping) && handle->url_count > 0) {
            if (!handle->is_looping) {
                if (handle->url_curr == list_tail(&handle->url_list)) {
                    handle->url_curr = list_head(&handle->url_list);
                } else {
                    handle->url_curr = handle->url_curr->next;
                }
            }
            struct message *msg = message_obtain(PLAYER_DO_SET_SOURCE, 0, 0, handle);
            if (msg != NULL) {
                state_sync = false;
                mlooper_post_message(handle->looper, msg);
            }
        }
        break;

    default:
        state_sync = false;
        break;
    }

    handle->state = state;

    os_mutex_unlock(handle->lock);

    if (state_sync && handle->listener)
        handle->listener(state, errcode, handle->listener_priv);
    return 0;
}

static void listplayer_looper_handle(struct message *msg)
{
    listplayer_handle_t handle = (listplayer_handle_t)msg->data;

    switch (msg->what) {
    case PLAYER_DO_SET_SOURCE: {
        const char *url = NULL;
        os_mutex_lock(handle->lock);
        struct url_node *node = listnode_to_item(handle->url_curr, struct url_node, listnode);
        url = audio_strdup(node->url);
        os_mutex_unlock(handle->lock);
        if (url != NULL) {
            liteplayer_set_data_source(handle->player, url);
            audio_free(url);
        }
        break;
    }

    case PLAYER_DO_PREPARE:
        liteplayer_prepare_async(handle->player);
        break;

    case PLAYER_DO_START:
        liteplayer_start(handle->player);
        break;

    case PLAYER_DO_PAUSE:
        liteplayer_pause(handle->player);
        break;

    case PLAYER_DO_RESUME:
        liteplayer_resume(handle->player);
        break;

    case PLAYER_DO_SEEK:
        liteplayer_seek(handle->player, msg->arg1);
        break;

    case PLAYER_DO_NEXT: {
        os_mutex_lock(handle->lock);
        if (handle->is_list) {
            if (handle->is_looping) {
                if (handle->url_curr == list_tail(&handle->url_list)) {
                    handle->url_curr = list_head(&handle->url_list);
                } else {
                    handle->url_curr = handle->url_curr->next;
                }
            }
        }
        os_mutex_unlock(handle->lock);
        if (handle->is_list)
            liteplayer_stop(handle->player);
        break;
    }

    case PLAYER_DO_PREV: {
        os_mutex_lock(handle->lock);
        if (handle->is_list) {
            if (handle->url_curr == list_head(&handle->url_list)) {
                handle->url_curr = list_tail(&handle->url_list);
            } else {
                handle->url_curr = handle->url_curr->prev;
            }
            if (!handle->is_looping) {
                if (handle->url_curr == list_head(&handle->url_list)) {
                    handle->url_curr = list_tail(&handle->url_list);
                } else {
                    handle->url_curr = handle->url_curr->prev;
                }
            }
        }
        os_mutex_unlock(handle->lock);
        if (handle->is_list)
            liteplayer_stop(handle->player);
        break;
    }

    case PLAYER_DO_STOP:
        liteplayer_stop(handle->player);
        break;

    case PLAYER_DO_RESET:
        liteplayer_reset(handle->player);
        break;

    default:
        break;
    }
}

static void listplayer_looper_free(struct message *msg)
{
    // nothing to free
}

listplayer_handle_t listplayer_create(struct listplayer_cfg *cfg)
{
    listplayer_handle_t handle = audio_calloc(1, sizeof(struct listplayer));
    if (handle != NULL) {
        if (cfg != NULL && cfg->playlist_url_suffix != NULL) {
            handle->cfg.playlist_url_suffix = audio_strdup(cfg->playlist_url_suffix);
            handle->cfg.playlist_url_max =
                cfg->playlist_url_max > 0 ? cfg->playlist_url_max : DEFAULT_PLAYLIST_URL_MAX;
        } else {
            handle->cfg.playlist_url_max = 1;
        }

        list_init(&handle->url_list);

        handle->lock = os_mutex_create();
        if (handle->lock == NULL)
            goto failed;

        handle->adapter = liteplayer_adapter_init();
        if (handle->adapter == NULL)
            goto failed;

        handle->player = liteplayer_create();
        if (handle->player == NULL)
            goto failed;

        struct os_thread_attr attr = {
            .name = "ael-listplayer",
            .priority = DEFAULT_LISTPLAYER_TASK_PRIO,
            .stacksize = DEFAULT_LISTPLAYER_TASK_STACKSIZE,
            .joinable = true,
        };
        handle->looper = mlooper_create(&attr, listplayer_looper_handle, listplayer_looper_free);
        if (handle->looper == NULL)
            goto failed;

        if (mlooper_start(handle->looper) != 0)
            goto failed;
    }
    return handle;

failed:
    listplayer_destroy(handle);
    return NULL;
}

int listplayer_register_source_wrapper(listplayer_handle_t handle, struct source_wrapper *wrapper)
{
    if (handle == NULL || wrapper == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't register sink wrapper in state=[%d]", handle->state);
        os_mutex_unlock(handle->lock);
        return -1;
    }
    os_mutex_unlock(handle->lock);

    handle->adapter->add_source_wrapper(handle->adapter, wrapper);
    return liteplayer_register_source_wrapper(handle->player, wrapper);
}

int listplayer_register_sink_wrapper(listplayer_handle_t handle, struct sink_wrapper *wrapper)
{
    if (handle == NULL || wrapper == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't register source wrapper in state=[%d]", handle->state);
        os_mutex_unlock(handle->lock);
        return -1;
    }
    os_mutex_unlock(handle->lock);

    handle->adapter->add_sink_wrapper(handle->adapter, wrapper);
    return liteplayer_register_sink_wrapper(handle->player, wrapper);
}

int listplayer_register_state_listener(listplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv)
{
    if (handle == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (handle->state != LITEPLAYER_IDLE) {
        OS_LOGE(TAG, "Can't set listener in state=[%d]", handle->state);
        os_mutex_unlock(handle->lock);
        return -1;
    }
    os_mutex_unlock(handle->lock);

    handle->listener = listener;
    handle->listener_priv = listener_priv;
    return 0;
}

int listplayer_set_data_source(listplayer_handle_t handle, const char *url)
{
    if (handle == NULL || url == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (handle->url_count > 0) {
        OS_LOGE(TAG, "Failed to set source, playlist isn't empty");
        os_mutex_unlock(handle->lock);
        return -1;
    }
    handle->is_list = false;
    handle->is_paused = false;
    handle->has_inited = false;
    handle->has_prepared = false;
    handle->has_started = false;

    if (handle->file_ops == NULL) {
        handle->file_ops =
            handle->adapter->find_source_wrapper(handle->adapter, url);
        if (handle->file_ops == NULL) {
            OS_LOGE(TAG, "Can't find source wrapper for this url");
            os_mutex_unlock(handle->lock);
            return -1;
        }
    }
    os_mutex_unlock(handle->lock);

    if (handle->cfg.playlist_url_suffix != NULL &&
        strstr(url, handle->cfg.playlist_url_suffix) != NULL) {
        if (playlist_resolve(handle, url) != 0) {
            OS_LOGE(TAG, "Failed to resolve playlist");
            return -1;
        }
    } else {
        if (playlist_insert(handle, url) != 0) {
            OS_LOGE(TAG, "Failed to insert playlist");
            return -1;
        }
    }

    if (!list_empty(&handle->url_list))
        handle->url_curr = list_head(&handle->url_list);
    else
        return -1;

    liteplayer_register_state_listener(handle->player, listplayer_state_callback, (void *)handle);
    struct message *msg = message_obtain(PLAYER_DO_SET_SOURCE, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_prepare_async(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;

    struct message *msg = message_obtain(PLAYER_DO_PREPARE, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_start(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_START, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_pause(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_PAUSE, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_resume(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_RESUME, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_seek(listplayer_handle_t handle, int msec)
{
    if (handle == NULL || msec < 0)
        return -1;
    struct message *msg = message_obtain(PLAYER_DO_SEEK, msec, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_switch_next(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (!handle->is_list) {
        OS_LOGE(TAG, "Failed to switch next without playlist");
        os_mutex_unlock(handle->lock);
        return -1;
    }
    os_mutex_unlock(handle->lock);

    struct message *msg = message_obtain(PLAYER_DO_NEXT, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_switch_prev(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (!handle->is_list) {
        OS_LOGE(TAG, "Failed to switch prev without playlist");
        os_mutex_unlock(handle->lock);
        return -1;
    }
    os_mutex_unlock(handle->lock);

    struct message *msg = message_obtain(PLAYER_DO_PREV, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_set_single_looping(listplayer_handle_t handle, bool enable)
{
    if (handle == NULL)
        return -1;

    os_mutex_lock(handle->lock);
    if (handle->is_looping != enable) {
        if (handle->state == LITEPLAYER_INITED || handle->state == LITEPLAYER_PREPARED ||
            handle->state == LITEPLAYER_COMPLETED || handle->state == LITEPLAYER_STOPPED ||
            handle->state == LITEPLAYER_ERROR) {
            OS_LOGE(TAG, "Failed to set looping in critical state");
            os_mutex_unlock(handle->lock);
            return -1;
        }
        handle->is_looping = enable;
    }
    os_mutex_unlock(handle->lock);
    return 0;
}

int listplayer_stop(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;

    playlist_clear(handle);

    struct message *msg = message_obtain(PLAYER_DO_STOP, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_reset(listplayer_handle_t handle)
{
    if (handle == NULL)
        return -1;

    playlist_clear(handle);

    struct message *msg = message_obtain(PLAYER_DO_RESET, 0, 0, handle);
    if (msg != NULL) {
        mlooper_post_message(handle->looper, msg);
        return 0;
    }
    return -1;
}

int listplayer_get_position(listplayer_handle_t handle, int *msec)
{
    if (handle == NULL || msec == NULL)
        return -1;
    return liteplayer_get_position(handle->player, msec);
}

int listplayer_get_duration(listplayer_handle_t handle, int *msec)
{
    if (handle == NULL || msec == NULL)
        return -1;
    return liteplayer_get_duration(handle->player, msec);
}

void listplayer_destroy(listplayer_handle_t handle)
{
    if (handle == NULL)
        return;
    if (handle->looper != NULL)
        mlooper_destroy(handle->looper);
    if (handle->player != NULL)
        liteplayer_destroy(handle->player);
    if (handle->adapter != NULL)
        handle->adapter->destory(handle->adapter);
    if (handle->lock != NULL)
        os_mutex_destroy(handle->lock);
    if (handle->cfg.playlist_url_suffix != NULL)
        audio_free(handle->cfg.playlist_url_suffix);
    audio_free(handle);
}
