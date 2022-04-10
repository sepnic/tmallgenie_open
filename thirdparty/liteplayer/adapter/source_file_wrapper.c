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

#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "source_file_wrapper.h"

#define TAG "[liteplayer]file"

struct file_priv {
    const char *url;
    FILE *file;
    long content_pos;
    long content_len;
};

const char *file_wrapper_url_protocol()
{
    return "file";
}

source_handle_t file_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    struct file_priv *priv = OS_CALLOC(1, sizeof(struct file_priv));
    FILE *file = NULL;

    if (priv == NULL)
        return NULL;

    OS_LOGD(TAG, "Opening file:%s, content_pos:%d", url, (int)content_pos);

    file = fopen(url, "rb");
    if (file == NULL) {
        OS_LOGE(TAG, "Failed to open file:%s", url);
        OS_FREE(priv);
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

int file_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct file_priv *priv = (struct file_priv *)handle;
    if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
        OS_LOGD(TAG, "file read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
        return 0;
    }
    size_t bytes_read = fread(buffer, 1, size, priv->file);
    if (bytes_read > 0)
        priv->content_pos += bytes_read;
    return bytes_read;
}

long long file_wrapper_content_pos(source_handle_t handle)
{
    struct file_priv *priv = (struct file_priv *)handle;
    return priv->content_pos;
}

long long file_wrapper_content_len(source_handle_t handle)
{
    struct file_priv *priv = (struct file_priv *)handle;
    return priv->content_len;
}

int file_wrapper_seek(source_handle_t handle, long offset)
{
    struct file_priv *priv = (struct file_priv *)handle;
    OS_LOGD(TAG, "Seeking file:%p, offset:%ld", priv->file, offset);
    int ret = fseek(priv->file, offset, SEEK_SET);
    if (ret == 0)
        priv->content_pos = offset;
    return ret;
}

void file_wrapper_close(source_handle_t handle)
{
    struct file_priv *priv = (struct file_priv *)handle;
    OS_LOGD(TAG, "Closing file:%p", priv->file);
    fclose(priv->file);
    OS_FREE(priv);
}
