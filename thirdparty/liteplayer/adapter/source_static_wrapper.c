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
#include "source_static_wrapper.h"

#define TAG "[liteplayer]static"

struct static_priv {
    char *content_base;
    long content_length;
    long content_offset;
};

const char *static_wrapper_url_protocol()
{
    // example: static://base=0x12345678&length=0x1234
    return "static";
}

source_handle_t static_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    OS_LOGD(TAG, "Opening static:%s, content_pos:%d", url, (int)content_pos);

    unsigned long long base;
    unsigned int length;
    int ret = sscanf(url, "static://base=0x%llx&length=0x%x", &base, &length);
    if (ret != 2) {
        OS_LOGE(TAG, "Invalid url, valid url example:(static://base=0x12345678&length=0x1234)");
        return NULL;
    }

    struct static_priv *priv = OS_CALLOC(1, sizeof(struct static_priv));
    if (priv == NULL)
        return NULL;

    priv->content_base = (char *)base;
    priv->content_length = (long)length;
    priv->content_offset = (long)content_pos;
    return priv;
}

int static_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct static_priv *priv = (struct static_priv *)handle;
    if (priv->content_offset + size > priv->content_length)
        size = priv->content_length - priv->content_offset;
    if (size > 0) {
        memcpy(buffer, priv->content_base+priv->content_offset, size);
        priv->content_offset += size;
    } else {
        OS_LOGD(TAG, "static read done: %d/%d", (int)priv->content_offset, (int)priv->content_length);
        size = 0;
    }
    return size;
}

long long static_wrapper_content_pos(source_handle_t handle)
{
    struct static_priv *priv = (struct static_priv *)handle;
    return priv->content_offset;
}

long long static_wrapper_content_len(source_handle_t handle)
{
    struct static_priv *priv = (struct static_priv *)handle;
    return priv->content_length;
}

int static_wrapper_seek(source_handle_t handle, long offset)
{
    struct static_priv *priv = (struct static_priv *)handle;
    OS_LOGD(TAG, "Seeking static: %ld>>%ld", priv->content_offset, offset);
    if (offset > priv->content_length)
        return -1;
    priv->content_offset = offset;
    return 0;
}

void static_wrapper_close(source_handle_t handle)
{
    struct static_priv *priv = (struct static_priv *)handle;
    OS_LOGD(TAG, "Closing static:%p", priv->content_base);
    OS_FREE(priv);
}
