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
#include <string.h>

#include "osal/os_thread.h"
#include "cutils/list.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "liteplayer_config.h"
#include "liteplayer_adapter_internal.h"
#include "liteplayer_adapter.h"

#define TAG "[liteplayer]adapter"

// Buffer size must be larger than decoder input buffer:
//  #define MP3_DECODER_INPUT_BUFFER_SIZE   (1940)
//  #define AAC_DECODER_INPUT_BUFFER_SIZE   (1536)
#define MIN_SOURCE_ASYNC_BUFFER_SIZE  ( 1024*32 )
#define MAX_SOURCE_ASYNC_BUFFER_SIZE  ( 1024*1024 )
#define MIN_SOURCE_SYNC_BUFFER_SIZE   ( 1024*2 )
#define MAX_SOURCE_SYNC_BUFFER_SIZE   ( 1024*16 )

#define DEFAULT_SOURCE_URL_PROTOCOL "file"

struct liteplayer_adapter_priv {
    struct liteplayer_adapter adapter;
    struct listnode source_list;
    struct listnode sink_list;
    os_mutex lock;
};

struct source_wrapper_node {
    struct source_wrapper wrapper;
    struct listnode listnode;
};

struct sink_wrapper_node {
    struct sink_wrapper wrapper;
    struct listnode listnode;
};

struct file_wrapper_priv {
    const char *url;
    FILE *file;
    long content_pos;
    long content_len;
};

static const char *file_wrapper_url_protocol()
{
    return DEFAULT_SOURCE_URL_PROTOCOL;
}

static source_handle_t file_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    struct file_wrapper_priv *priv = audio_calloc(1, sizeof(struct file_wrapper_priv));
    FILE *file = NULL;
    if (priv == NULL)
        return NULL;

    OS_LOGD(TAG, "Opening file:%s, content_pos:%d", url, (int)content_pos);
    file = fopen(url, "rb");
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open file:%s", url);
        audio_free(priv);
        return NULL;
    }

    priv->url = url;
    priv->file = file;
    priv->content_pos = (long)content_pos;

    fseek(priv->file, 0, SEEK_END);
    priv->content_len = ftell(priv->file);
    fseek(priv->file, priv->content_pos, SEEK_SET);
    return priv;
}

static int file_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
        OS_LOGD(TAG, "File read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
        return 0;
    }
    size_t bytes_read = fread(buffer, 1, size, priv->file);
    if (bytes_read > 0)
        priv->content_pos += bytes_read;
    return bytes_read;
}

static long long file_wrapper_content_pos(source_handle_t handle)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    return priv->content_pos;
}

static long long file_wrapper_content_len(source_handle_t handle)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    return priv->content_len;
}

static int file_wrapper_seek(source_handle_t handle, long offset)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    int ret = fseek(priv->file, offset, SEEK_SET);
    if (ret == 0)
        priv->content_pos = offset;
    return ret;
}

static void file_wrapper_close(source_handle_t handle)
{
    struct file_wrapper_priv *priv = (struct file_wrapper_priv *)handle;
    OS_LOGD(TAG, "Closing file:%p", priv->file);
    fclose(priv->file);
    audio_free(priv);
}

static int add_source_wrapper(liteplayer_adapter_handle_t self, struct source_wrapper *wrapper)
{
    if (wrapper == NULL || wrapper->url_protocol() == NULL)
        return ESP_FAIL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct source_wrapper_node *node = NULL;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->source_list) {
        node = listnode_to_item(item, struct source_wrapper_node, listnode);
        if (strcasecmp(wrapper->url_protocol(), node->wrapper.url_protocol()) == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        node = audio_calloc(1, sizeof(struct source_wrapper_node));
        if (node == NULL) {
            os_mutex_unlock(priv->lock);
            return ESP_FAIL;
        }
        list_add_head(&priv->source_list, &node->listnode);
    }
    node->wrapper.buffer_size = (wrapper->buffer_size/1024)*1024;
    if (wrapper->async_mode) {
        if (wrapper->buffer_size < MIN_SOURCE_ASYNC_BUFFER_SIZE)
            node->wrapper.buffer_size = MIN_SOURCE_ASYNC_BUFFER_SIZE;
        else if (wrapper->buffer_size > MAX_SOURCE_ASYNC_BUFFER_SIZE)
            node->wrapper.buffer_size = MAX_SOURCE_ASYNC_BUFFER_SIZE;
    } else {
        if (wrapper->buffer_size < MIN_SOURCE_SYNC_BUFFER_SIZE)
            node->wrapper.buffer_size = MIN_SOURCE_SYNC_BUFFER_SIZE;
        else if (wrapper->buffer_size > MAX_SOURCE_SYNC_BUFFER_SIZE)
            node->wrapper.buffer_size = MAX_SOURCE_SYNC_BUFFER_SIZE;
    }
    memcpy(&node->wrapper, wrapper, sizeof(struct source_wrapper));

    os_mutex_unlock(priv->lock);
    return ESP_OK;
}

static struct source_wrapper *find_source_wrapper(liteplayer_adapter_handle_t self, const char *url)
{
    if (url == NULL)
        return NULL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct source_wrapper_node *node = NULL;
    struct listnode *item;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->source_list) {
        node = listnode_to_item(item, struct source_wrapper_node, listnode);
        if (strncasecmp(url, node->wrapper.url_protocol(), strlen(node->wrapper.url_protocol())) == 0) {
            goto find_out;
        }
    }
    // if found no source wrapper, now we treat it as file url
    list_for_each(item, &priv->source_list) {
        node = listnode_to_item(item, struct source_wrapper_node, listnode);
        if (strcasecmp(DEFAULT_SOURCE_URL_PROTOCOL, node->wrapper.url_protocol()) == 0) {
            goto find_out;
        }
    }
    node = NULL; // no file wrapper is found

find_out:
    os_mutex_unlock(priv->lock);
    return node != NULL ? &node->wrapper : NULL;
}

int add_sink_wrapper(liteplayer_adapter_handle_t self, struct sink_wrapper *wrapper)
{
    if (wrapper == NULL || wrapper->name() == NULL)
        return ESP_FAIL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct sink_wrapper_node *node = NULL;
    struct listnode *item;
    bool found = false;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->sink_list) {
        node = listnode_to_item(item, struct sink_wrapper_node, listnode);
        if (strcasecmp(wrapper->name(), node->wrapper.name()) == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        node = audio_calloc(1, sizeof(struct sink_wrapper_node));
        if (node == NULL) {
            os_mutex_unlock(priv->lock);
            return ESP_FAIL;
        }
        list_add_head(&priv->sink_list, &node->listnode);
    }
    memcpy(&node->wrapper, wrapper, sizeof(struct sink_wrapper));

    os_mutex_unlock(priv->lock);
    return ESP_OK;
}

static struct sink_wrapper *find_sink_wrapper(liteplayer_adapter_handle_t self, const char *name)
{
    if (name == NULL)
        return NULL;

    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct sink_wrapper_node *node = NULL;
    struct listnode *item;

    os_mutex_lock(priv->lock);

    list_for_each(item, &priv->sink_list) {
        node = listnode_to_item(item, struct sink_wrapper_node, listnode);
        if (strncasecmp(name, node->wrapper.name(), strlen(node->wrapper.name())) == 0) {
            goto find_out;
        }
    }
    // if found no sink wrapper, now we use the first one in the list
    if (!list_empty(&priv->sink_list))
        node = listnode_to_item(list_head(&priv->sink_list), struct sink_wrapper_node, listnode);
    else
        node = NULL;

find_out:
    os_mutex_unlock(priv->lock);
    return node != NULL ? &node->wrapper : NULL;
}

static void liteplayer_adapter_destory(liteplayer_adapter_handle_t self)
{
    struct liteplayer_adapter_priv *priv = (struct liteplayer_adapter_priv *)self;
    struct source_wrapper_node *source = NULL;
    struct sink_wrapper_node *sink = NULL;
    struct listnode *item, *tmp;

    list_for_each_safe(item, tmp, &priv->source_list) {
        source = listnode_to_item(item, struct source_wrapper_node, listnode);
        list_remove(item);
        audio_free(source);
    }

    list_for_each_safe(item, tmp, &priv->sink_list) {
        sink = listnode_to_item(item, struct sink_wrapper_node, listnode);
        list_remove(item);
        audio_free(sink);
    }

    os_mutex_destroy(priv->lock);
    audio_free(priv);
}

liteplayer_adapter_handle_t liteplayer_adapter_init()
{
    struct liteplayer_adapter_priv *priv = audio_calloc(1, sizeof(struct liteplayer_adapter_priv));
    if (priv == NULL)
        return NULL;
    
    priv->lock = os_mutex_create();
    if (priv->lock == NULL) {
        audio_free(priv);
        return NULL;
    }

    list_init(&priv->source_list);
    list_init(&priv->sink_list);

    priv->adapter.add_source_wrapper = add_source_wrapper;
    priv->adapter.find_source_wrapper = find_source_wrapper;
    priv->adapter.add_sink_wrapper = add_sink_wrapper;
    priv->adapter.find_sink_wrapper = find_sink_wrapper;
    priv->adapter.destory = liteplayer_adapter_destory;

    struct source_wrapper file_wrapper = {
        .async_mode = false,
        .buffer_size = MIN_SOURCE_SYNC_BUFFER_SIZE,
        .priv_data = NULL,
        .url_protocol = file_wrapper_url_protocol,
        .open = file_wrapper_open,
        .read = file_wrapper_read,
        .content_pos = file_wrapper_content_pos,
        .content_len = file_wrapper_content_len,
        .seek = file_wrapper_seek,
        .close = file_wrapper_close,
    };
    add_source_wrapper((liteplayer_adapter_handle_t)priv, &file_wrapper);

    return (liteplayer_adapter_handle_t)priv;
}
