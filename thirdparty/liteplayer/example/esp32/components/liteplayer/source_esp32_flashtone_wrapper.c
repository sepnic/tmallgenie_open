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
#include <string.h>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "source_esp32_flashtone_wrapper.h"
#include "tone_partition.h"

#define TAG "[liteplayer]esp32_flashtone"

#define FLASHTONE_PARTITION_LABEL "flash_tone"
#define FLASHTONE_URL_PREFIX      "flash://tone/"

struct esp32_flashtone_priv {
    tone_file_info_t cur_file;
    long content_pos;
    long content_len;
};

static tone_partition_handle_t g_tone_handle = NULL;

const char *esp32_flashtone_wrapper_url_protocol()
{
    return FLASHTONE_URL_PREFIX;
}

source_handle_t esp32_flashtone_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    OS_LOGD(TAG, "Opening file:%s, content_pos:%d", url, (int)content_pos);
    if (g_tone_handle == NULL) {
        g_tone_handle = tone_partition_init(FLASHTONE_PARTITION_LABEL, false);
        if (g_tone_handle == NULL) {
            OS_LOGE(TAG, "Can't find tone partition(%s)", FLASHTONE_PARTITION_LABEL);
            return NULL;
        }
    }

    char *flash_url = (char *)url;
    flash_url += strlen(FLASHTONE_URL_PREFIX);
    char *temp = strchr(flash_url, '_');
    char find_num[2] = { 0, 0 };
    int file_index = 0;
    if (temp != NULL) {
        strncpy(find_num, flash_url, temp - flash_url);
        file_index = strtoul(find_num, 0, 10);
    } else {
        OS_LOGE(TAG, "Tone file name is not correct");
        return NULL;
    }

    struct esp32_flashtone_priv *priv = OS_CALLOC(1, sizeof(struct esp32_flashtone_priv));
    if (priv == NULL)
        return NULL;
    tone_partition_get_file_info(g_tone_handle, file_index, &priv->cur_file);
    OS_LOGV(TAG, "Tone offset:%08x, length:%d, index:%d",
            priv->cur_file.song_adr, priv->cur_file.song_len, file_index);
    if (priv->cur_file.song_len <= 0) {
        OS_LOGE(TAG, "The flash tone is empty, please ensure the flash's contex");
        OS_FREE(priv);
        return NULL;
    }
    priv->content_pos = (long)content_pos;
    priv->content_len = (long)priv->cur_file.song_len;
    return priv;
}

int esp32_flashtone_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct esp32_flashtone_priv *priv = (struct esp32_flashtone_priv *)handle;
    if (priv->content_pos >= priv->content_len)
        return 0;
    if (priv->content_pos + size > priv->content_len)
        size = priv->content_len - priv->content_pos;
    if (0 != tone_partition_file_read(g_tone_handle, &priv->cur_file, (uint32_t)priv->content_pos, buffer, size)) {
        OS_LOGE(TAG, "Read tone file failed");
        return -1;
    }
    priv->content_pos += size;
    return size;
}

long long esp32_flashtone_wrapper_content_pos(source_handle_t handle)
{
    struct esp32_flashtone_priv *priv = (struct esp32_flashtone_priv *)handle;
    return priv->content_pos;
}

long long esp32_flashtone_wrapper_content_len(source_handle_t handle)
{
    struct esp32_flashtone_priv *priv = (struct esp32_flashtone_priv *)handle;
    return priv->content_len;
}

int esp32_flashtone_wrapper_seek(source_handle_t handle, long offset)
{
    struct esp32_flashtone_priv *priv = (struct esp32_flashtone_priv *)handle;
    OS_LOGD(TAG, "Seeking, offset:%ld>>%ld", priv->content_pos, offset);
    priv->content_pos = offset;
    return 0;
}

void esp32_flashtone_wrapper_close(source_handle_t handle)
{
    struct esp32_flashtone_priv *priv = (struct esp32_flashtone_priv *)handle;
    OS_LOGD(TAG, "Closing file");
    OS_FREE(priv);
}
