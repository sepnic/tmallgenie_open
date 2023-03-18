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
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "httpclient/httpclient.h"
#include "source_httpclient_wrapper.h"

#define TAG "[liteplayer]httpclient"

#define HTTPCLIENT_HEADER_BUFFER_SIZE 1024
#define HTTPCLIENT_RETRY_COUNT        5
#define HTTPCLIENT_RETRY_INTERVAL     3000

struct httpclient_priv {
    const char          *url;
    char                 header_buf[HTTPCLIENT_HEADER_BUFFER_SIZE];
    httpclient_t         client;
    httpclient_data_t    client_data;
    long long            content_pos;
    long long            content_len;
    int                  retrieve_len;
    bool                 first_request;
    bool                 first_response;
    int                  retrycount;
};

static int httpclient_wrapper_connect(source_handle_t handle)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;
    HTTPCLIENT_RESULT ret = HTTPCLIENT_OK;

    memset(&priv->client, 0, sizeof(httpclient_t));
    memset(&priv->client_data, 0, sizeof(httpclient_data_t));
    memset(&priv->header_buf[0], 0, sizeof(priv->header_buf));
    priv->client.socket = -1;
    priv->retrieve_len = -1;
    priv->content_len = 0;
    priv->first_request = false;
    priv->first_response = false;

reconnect:
    ret = httpclient_connect(&priv->client, (char *)priv->url);
    if (ret != HTTPCLIENT_OK) {
        OS_LOGE(TAG, "httpclient_connect failed, ret=%d, retry=%d", ret, priv->retrycount);
        if (priv->retrycount++ < HTTPCLIENT_RETRY_COUNT) {
            os_thread_sleep_msec(HTTPCLIENT_RETRY_INTERVAL);
            goto reconnect;
        }
        httpclient_close(&priv->client);
    }

    return ret;
}

static void httpclient_wrapper_disconnect(source_handle_t handle)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;
    httpclient_close(&priv->client);
}

static int httpclient_wrapper_parse_content_length(char *header_buf, long long *content_len)
{
    int val_pos = 0, val_len = 0;
    int ret = httpclient_get_response_header_value(header_buf, "Content-Length", &val_pos, &val_len);

    if (ret == 0) {
        char header_content[64] = {0};
        memcpy(header_content, header_buf+val_pos, val_len);
        header_content[val_len] = '\0';
        *content_len = atoi(header_content);
        OS_LOGV(TAG, "Content-Length=%d", (int)(*content_len));
    }
    return ret;
}

const char *httpclient_wrapper_url_protocol()
{
    return "http";
}

source_handle_t httpclient_wrapper_open(const char *url, long long content_pos, void *priv_data)
{
    struct httpclient_priv *priv = OS_CALLOC(1, sizeof(struct httpclient_priv));
    if (priv == NULL)
        return NULL;

    priv->url = OS_STRDUP(url);
    priv->content_pos = content_pos;
    OS_LOGD(TAG, "Connecting url:%s, content_pos:%d", url, (int)content_pos);
    if (httpclient_wrapper_connect(priv) != HTTPCLIENT_OK) {
        OS_FREE(priv->url);
        OS_FREE(priv);
        return NULL;
    }
    return priv;
}

int httpclient_wrapper_read(source_handle_t handle, char *buffer, int size)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;
    httpclient_t *client = &priv->client;
    httpclient_data_t *client_data = &priv->client_data;
    char *url = (char *)priv->url;
    int resp_len = 0;
    HTTPCLIENT_RESULT ret = HTTPCLIENT_ERROR;

contiune_read:
    client_data->header_buf       = priv->header_buf;
    client_data->header_buf_len   = HTTPCLIENT_HEADER_BUFFER_SIZE;
    client_data->response_buf     = buffer;
    client_data->response_buf_len = size;

    if (!priv->first_request) {
        if (priv->content_pos > 0) {
            char tmp_buf[64] = {0};
            snprintf(tmp_buf, sizeof(tmp_buf), "Range: bytes=%ld-\r\n", (long)priv->content_pos);
            OS_LOGV(TAG, "Set http range: %s", tmp_buf);
            httpclient_set_custom_header(client, tmp_buf);
        }

        ret = httpclient_send_request(client, url, HTTPCLIENT_GET, client_data);
        if (ret < 0) {
            OS_LOGE(TAG, "httpclient_send_request failed, ret=%d, retry=%d", ret, priv->retrycount);
            if (priv->retrycount++ >= HTTPCLIENT_RETRY_COUNT)
                return -1;
            os_thread_sleep_msec(HTTPCLIENT_RETRY_INTERVAL);
            goto reconnect;
        }
        priv->first_request = true;
    }

    //OS_LOGD(TAG, "httpclient reading: %d/%d", (int)priv->content_pos, (int)priv->content_len);
    if (priv->content_len > 0 && priv->content_pos >= priv->content_len) {
        OS_LOGD(TAG, "httpclient read done: %d/%d", (int)priv->content_pos, (int)priv->content_len);
        return 0;
    }

    ret = httpclient_recv_response(client, client_data);
    if (ret < 0) {
        OS_LOGE(TAG, "httpclient_recv_response failed, ret=%d, retry=%d", ret, priv->retrycount);
        if (priv->retrycount++ >= HTTPCLIENT_RETRY_COUNT)
            return -1;
        goto reconnect;
    }

    if (!priv->first_response) {
        ret = httpclient_wrapper_parse_content_length(client_data->header_buf, &priv->content_len);
        if (ret != 0)
            priv->content_len = client_data->response_content_len;
        priv->content_len += priv->content_pos;
        OS_LOGD(TAG, "content_pos=%d, response_content_len=%d, content_len=%d",
                 (int)priv->content_pos, (int)client_data->response_content_len, (int)priv->content_len);
        priv->first_response = true;
        priv->retrycount = 0;
    }

    if (priv->retrieve_len == -1)
        priv->retrieve_len = client_data->response_content_len;
    resp_len = priv->retrieve_len - client_data->retrieve_len;
    priv->retrieve_len = client_data->retrieve_len;

    //OS_LOGV(TAG, "resp_len=%d, is_more=%d, response_buf_len=%d, retrieve_len=%d, content_len=%d",
    //         resp_len, client_data->is_more,
    //         client_data->response_buf_len, priv->retrieve_len, (int)priv->content_len);

    if (resp_len < 0 || resp_len > client_data->response_buf_len) {
        OS_LOGE(TAG, "Invalid resp_len:%d", resp_len);
        return HTTPCLIENT_ERROR;
    }

    priv->content_pos += resp_len;

    //OS_LOGV(TAG, "-->resp_len len=%d", resp_len);
    return resp_len;

reconnect:
    httpclient_wrapper_disconnect(priv);
    ret = httpclient_wrapper_connect(priv);
    if (ret != HTTPCLIENT_OK) {
        OS_LOGE(TAG, "httpclient reconnect failed, ret=%d", ret);
        return ret;
    } else {
        goto contiune_read;
    }
    return -1;
}

long long httpclient_wrapper_content_pos(source_handle_t handle)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;
    return priv->content_pos;
}

long long httpclient_wrapper_content_len(source_handle_t handle)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;
    return priv->content_len;
}

int httpclient_wrapper_seek(source_handle_t handle, long offset)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;

    OS_LOGD(TAG, "Seeking http client, content_pos=%ld", offset);
    priv->content_pos = offset;
    httpclient_wrapper_disconnect(priv);
    os_thread_sleep_msec(50);
    return httpclient_wrapper_connect(priv);
}

void httpclient_wrapper_close(source_handle_t handle)
{
    struct httpclient_priv *priv = (struct httpclient_priv *)handle;

    OS_LOGD(TAG, "Closing http client");
    httpclient_close(&priv->client);
    OS_FREE(priv->url);
    OS_FREE(priv);
    OS_LOGV(TAG, "Closed http client");
}
