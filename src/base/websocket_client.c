
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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "osal/os_time.h"
#include "osal/os_thread.h"
#include "cutils/list.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"

#include "nopoll.h"
#include "websocket_client.h"

#define TAG "websocket"

//#define NOPOLL_HAVE_LIBRARY_DEBUG_ENABLED

#define WS_CLIENT_TASK_STACKSIZE      8192
#define WS_CLIENT_TASK_PRIORITY       OS_THREAD_PRIO_NORMAL

#define WS_CLIENT_SOCKET_CONN_TIMEOUT 10   // s
#define WS_CLIENT_SOCKET_RECV_TIMEOUT 20   // ms
#define WS_CLIENT_SOCKET_PONG_TIMEOUT 3000 // ms
//#define WS_CLIENT_SOCKET_DISCONN_IF_PONG_TIMEOUT

#define WS_CLIENT_QUEUE_MESSAGE_MAX   100
#define WS_CLIENT_QUEUE_RECV_TIMEOUT  20   // ms
#define WS_CLIENT_QUEUE_SEND_TIMEOUT  0x7fffffff // ms

enum {
    WS_CLIENT_CMD_CONNECT,
    WS_CLIENT_CMD_SEND_TEXT,
    WS_CLIENT_CMD_SEND_BINARY,
    WS_CLIENT_CMD_DISCONNECT,
};

typedef struct {
    char *unique_data;
    int len;
    ws_binary_type_t type;
    // @data must be the last member of ws_xfer_t, as user of this structure
    // will cast a buffer to xfer->data pointer
    char data[0];
} ws_xfer_t;

typedef struct {
    struct listnode listnode;
    int what;
    // @xfer must be the last member of ws_msg_t
    ws_xfer_t xfer;
} ws_msg_t;

typedef struct ws_client {
    noPollCtx *ctx;
    noPollConn *conn;
    noPollConnOpts *conn_opts;

    ws_conn_state_t conn_state;
    ws_user_info_t user_info;
    unsigned long long active_time;
    int ping_interval;
    bool pong_recv;
    char *prev_text;
    int prev_text_size;
    int prev_recv_type;

    struct listnode msg_list;
    os_mutex lock;
    os_cond cond;
    os_thread thread;
    bool thread_exit;
} ws_client_t;

static void ws_nopoll_handle_message(ws_client_t *client, noPollMsg *msg)
{
    noPollOpCode opcode = nopoll_msg_opcode(msg);
    char *content = (char *)nopoll_msg_get_payload(msg);
    int size = nopoll_msg_get_payload_size(msg);
    bool final = nopoll_msg_is_final(msg);

    if (client->prev_text_size > 0 && client->prev_text != NULL) {
        if (size > 0 && (opcode == NOPOLL_TEXT_FRAME || opcode == NOPOLL_CONTINUATION_FRAME)) {
            char *text_buffer = OS_REALLOC(client->prev_text, client->prev_text_size + size);
            if (text_buffer == NULL) {
                OS_LOGW(TAG, "Failed to realloc text buffer, deliver original message to user");
                if (client->user_info.callback.on_received_text != NULL)
                    client->user_info.callback.on_received_text(client->prev_text, client->prev_text_size);
                OS_FREE(client->prev_text);
                client->prev_text = NULL;
                client->prev_text_size = 0;
                goto current_msg;
            }

            memcpy(text_buffer + client->prev_text_size, content, size);
            client->prev_text = text_buffer;
            client->prev_text_size += size;

            if (final) {
                OS_LOGD(TAG, "Recv text: %s", client->prev_text);
                if (client->user_info.callback.on_received_text != NULL)
                    client->user_info.callback.on_received_text(client->prev_text, client->prev_text_size);
                OS_FREE(client->prev_text);
                client->prev_text = NULL;
                client->prev_text_size = 0;
            }
            return;
        } else {
            OS_LOGW(TAG, "Expect text frame, but receive opcode %d", opcode);
            if (client->user_info.callback.on_received_text != NULL)
                client->user_info.callback.on_received_text(client->prev_text, client->prev_text_size);
            OS_FREE(client->prev_text);
            client->prev_text = NULL;
            client->prev_text_size = 0;
            goto current_msg;
        }
    } else if (size > 0 && opcode == NOPOLL_TEXT_FRAME && !final) {
        if (client->prev_text != NULL) {
            OS_FREE(client->prev_text);
            client->prev_text_size = 0;
        }

        client->prev_text = OS_CALLOC(1, size);
        if (client->prev_text == NULL) {
            OS_LOGW(TAG, "Failed to realloc text buffer, deliver original message to user");
            goto current_msg;
        }
        memcpy(client->prev_text, content, size);
        client->prev_text_size = size;
        client->prev_recv_type = NOPOLL_TEXT_FRAME;
        return;
    }

current_msg:
    switch (opcode) {
    case NOPOLL_TEXT_FRAME:
        OS_LOGD(TAG, "Recv text: %s", content);
        client->prev_recv_type = NOPOLL_TEXT_FRAME;
        if (client->user_info.callback.on_received_text != NULL)
            client->user_info.callback.on_received_text(content, size);
        break;
    case NOPOLL_BINARY_FRAME:
        client->prev_recv_type = NOPOLL_BINARY_FRAME;
        if (client->user_info.callback.on_received_binary != NULL) {
            if (final)
                client->user_info.callback.on_received_binary(content, size, WS_BINARY_FRAGMENT_FINISH);
            else
                client->user_info.callback.on_received_binary(content, size, WS_BINARY_FRAGMENT_START);
        }
        break;
    case NOPOLL_CONTINUATION_FRAME:
        if (client->prev_recv_type == NOPOLL_TEXT_FRAME) {
            if (client->user_info.callback.on_received_text != NULL)
                client->user_info.callback.on_received_text(content, size);
        } else if (client->prev_recv_type == NOPOLL_BINARY_FRAME) {
            if (client->user_info.callback.on_received_binary != NULL) {
                if (final)
                    client->user_info.callback.on_received_binary(content, size, WS_BINARY_FRAGMENT_FINISH);
                else
                    client->user_info.callback.on_received_binary(content, size, WS_BINARY_FRAGMENT_CONTINUE);
            }
        }
        break;
    case NOPOLL_PONG_FRAME:
        OS_LOGD(TAG, "Recv pong");
        client->pong_recv = true;
        break;
    default:
        break;
    }
}

static int ws_nopoll_open_conn(ws_client_t *client)
{
    client->ctx = nopoll_ctx_new();
    if (client->ctx == NULL) {
        OS_LOGE(TAG, "Failed to nopoll_ctx_new");
        goto __err_exit;
    }

    client->conn_opts = nopoll_conn_opts_new();
    if (client->conn_opts == NULL) {
        OS_LOGE(TAG, "Failed to nopoll_conn_opts_new");
        goto __err_exit;
    }

#if defined(NOPOLL_HAVE_LIBRARY_DEBUG_ENABLED)
    nopoll_log_enable(client->ctx, nopoll_true);
#endif

    nopoll_bool ret;
    char port[8];
    memset(port, 0x0, sizeof(port));
    snprintf(port, sizeof(port), "%u", client->user_info.port);

    if (client->user_info.cacert == NULL) {
        if (client->user_info.path != NULL)
            client->conn = nopoll_conn_new_opts(client->ctx,
                    client->conn_opts,
                    client->user_info.host,
                    port,
                    NULL,
                    client->user_info.path,
                    NULL, NULL);
        else
            client->conn = nopoll_conn_new(client->ctx,
                    client->user_info.host,
                    port,
                    client->user_info.host,
                    NULL, NULL, NULL);
    } else {
#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
        ret = nopoll_conn_opts_set_ssl_certs(client->conn_opts,
                NULL, 0,
                NULL, 0,
                NULL, 0,
                client->user_info.cacert, strlen(client->user_info.cacert)+1);
#else
        if (access(client->user_info.cacert, R_OK) != 0) {
            OS_LOGE(TAG, "Failed to access ca: %s", client->user_info.cacert);
            goto __err_exit;
        }
        ret = nopoll_conn_opts_set_ssl_certs(client->conn_opts,
                NULL,
                NULL,
                NULL,
                client->user_info.cacert);
#endif
        if (ret != nopoll_true) {
            OS_LOGE(TAG, "Failed to nopoll_conn_opts_set_ssl_certs: %s", client->user_info.cacert);
            goto __err_exit;
        }

        nopoll_conn_opts_ssl_peer_verify(client->conn_opts, nopoll_false);
        client->conn = nopoll_conn_tls_new(client->ctx,
                client->conn_opts,
                client->user_info.host,
                port,
                NULL,
                client->user_info.path,
                NULL, NULL);
    }

    if (client->conn == NULL) {
        OS_LOGE(TAG, "Failed to nopoll_conn_new");
        goto __err_exit;
    }

    if (!nopoll_conn_wait_until_connection_ready(client->conn, WS_CLIENT_SOCKET_CONN_TIMEOUT)) {
        OS_LOGE(TAG, "Failed to nopoll_conn_wait_until_connection_ready");
        goto __err_exit;
    }

    client->active_time = os_monotonic_usec() / 1000;
    client->pong_recv = false;
    client->conn_state = WS_CONN_STATE_CONNECTED;
    if (client->user_info.callback.on_connected != NULL)
        client->user_info.callback.on_connected();
    return 0;

__err_exit:
    if (client->conn != NULL) {
        nopoll_conn_close(client->conn);
        client->conn = NULL;
    }
    if (client->conn_opts != NULL) {
        nopoll_conn_opts_free(client->conn_opts);
        client->conn_opts = NULL;
    }
    if (client->ctx != NULL) {
        nopoll_ctx_unref(client->ctx);
        client->ctx = NULL;
    }
    client->conn_state = WS_CONN_STATE_DISCONNECTED;
    return -1;
}

static void ws_nopoll_close_conn(ws_client_t *client)
{
    nopoll_conn_close(client->conn);
    nopoll_conn_opts_free(client->conn_opts);
    nopoll_ctx_unref(client->ctx);
    client->conn = NULL;
    client->conn_opts = NULL;
    client->ctx = NULL;

    client->conn_state = WS_CONN_STATE_DISCONNECTED;
    if (client->user_info.callback.on_disconnected != NULL)
        client->user_info.callback.on_disconnected();
}

static int ws_nopoll_complete_pending_write(noPollConn *conn)
{
    int tries = 0;
    while (tries < 5 && errno == NOPOLL_EWOULDBLOCK &&
            nopoll_conn_pending_write_bytes (conn) > 0) {
        nopoll_sleep(50000);
        if (nopoll_conn_complete_pending_write (conn) == 0)
            return 0;
        tries++;
    }
    return -1;
}

static void ws_nopoll_send_text(ws_client_t *client, ws_xfer_t *xfer)
{
    int ret = 0;
    char *content = xfer->unique_data != NULL ? xfer->unique_data : xfer->data;

    OS_LOGD(TAG, "Send text: %s", content);
    client->active_time = os_monotonic_usec() / 1000;

    ret = nopoll_conn_send_text(client->conn, content, xfer->len);
    if (ret != xfer->len) {
        if (ws_nopoll_complete_pending_write(client->conn))
            OS_LOGW(TAG, "Send text: fewer bytes than expected (%d < %d)", ret, xfer->len);
    }

    if (xfer->unique_data != NULL)
        OS_FREE(xfer->unique_data);
}

static void ws_nopoll_send_binary(ws_client_t *client, ws_xfer_t *xfer)
{
    int ret = 0;
    char *content = xfer->unique_data != NULL ? xfer->unique_data : xfer->data;

    OS_LOGD(TAG, "Send binary: type=%d, data=%p, len=%d", xfer->type, content, xfer->len);
    client->active_time = os_monotonic_usec() / 1000;
    switch (xfer->type) {
    case WS_BINARY_FRAGMENT_START:
        ret = nopoll_conn_send_frame(client->conn, nopoll_false, nopoll_true,
                NOPOLL_BINARY_FRAME, xfer->len, content, 0);
        break;
    case WS_BINARY_FRAGMENT_CONTINUE:
        ret = nopoll_conn_send_frame(client->conn, nopoll_false, nopoll_true,
                NOPOLL_CONTINUATION_FRAME, xfer->len, content, 0);
        break;
    case WS_BINARY_FRAGMENT_FINISH:
        ret = nopoll_conn_send_frame(client->conn, nopoll_true, nopoll_true,
                NOPOLL_CONTINUATION_FRAME, xfer->len, content, 0);
        break;
    case WS_BINARY_WHOLE:
        ret = nopoll_conn_send_frame(client->conn, nopoll_true, nopoll_true,
                NOPOLL_BINARY_FRAME, xfer->len, content, 0);
        break;
    default:
        break;
    }
    if (ret != xfer->len) {
        if (ws_nopoll_complete_pending_write(client->conn))
            OS_LOGW(TAG, "Send binary: fewer bytes than expected (%d < %d)", ret, xfer->len);
    }

    if (xfer->unique_data != NULL)
        OS_FREE(xfer->unique_data);
}

static int ws_nopoll_try_get_msg(ws_client_t *client)
{
    if (!nopoll_conn_is_ok(client->conn)) {
        if (client->conn_state == WS_CONN_STATE_CONNECTED) {
            OS_LOGE(TAG, "Received websocket connection close");
            ws_nopoll_close_conn(client);
        }
        return -1;
    }

    noPollMsg *msg = nopoll_conn_get_msg(client->conn);
    if (msg == NULL)
        return -1;
    client->active_time = os_monotonic_usec() / 1000;

    ws_nopoll_handle_message(client, msg);

    nopoll_msg_unref(msg);
    return 0;
}

static int ws_nopoll_try_ping(ws_client_t *client)
{
    if (!nopoll_conn_is_ok(client->conn)) {
        if (client->conn_state == WS_CONN_STATE_CONNECTED) {
            OS_LOGE(TAG, "Received websocket connection close");
            ws_nopoll_close_conn(client);
        }
        return -1;
    }

    unsigned long long now_time = os_monotonic_usec() / 1000;
    if (now_time <= (client->active_time + client->ping_interval))
        return -1;

    OS_LOGD(TAG, "Send ping");
    client->active_time = now_time;
    nopoll_conn_send_ping(client->conn);
    return 0;
}

static void ws_client_clear_msglist(ws_client_t *client)
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &client->msg_list) {
        ws_msg_t *msg = listnode_to_item(item, ws_msg_t, listnode);
        list_remove(item);
        if (msg->xfer.unique_data != NULL)
            OS_FREE(msg->xfer.unique_data);
        OS_FREE(msg);
    }
}

static void ws_client_clear_msglist_l(ws_client_t *client)
{
    os_mutex_lock(client->lock);
    ws_client_clear_msglist(client);
    os_mutex_unlock(client->lock);
}

static void *ws_client_thread_entry(void *arg)
{
    ws_client_t *client = (ws_client_t *)arg;
    bool ping_sent_flag = false;
    int ping_check_time = 0;
    int ping_escape_time = 0;
    ws_msg_t *msg = NULL;

    OS_LOGV(TAG, "websocket thread enter");

    while (!client->thread_exit) {
        os_mutex_lock(client->lock);
        if (list_empty(&client->msg_list))
            os_cond_timedwait(client->cond, client->lock, WS_CLIENT_QUEUE_RECV_TIMEOUT*1000);
        if (!list_empty(&client->msg_list)) {
            struct listnode *front = list_head(&client->msg_list);
            msg = listnode_to_item(front, ws_msg_t, listnode);
            list_remove(front);
        }
        os_mutex_unlock(client->lock);

        if (msg == NULL) {
            if (ws_nopoll_try_get_msg(client) != 0)
                os_thread_sleep_msec(WS_CLIENT_SOCKET_RECV_TIMEOUT);

            if (client->ping_interval > 0) {
                ping_check_time += (WS_CLIENT_SOCKET_RECV_TIMEOUT+WS_CLIENT_QUEUE_RECV_TIMEOUT)*2;
                if (ping_check_time > client->ping_interval) {
                    if (ws_nopoll_try_ping(client) == 0) {
                        ping_sent_flag = true;
                        ping_check_time = 0;
                        ping_escape_time = 0;
                        client->pong_recv = false;
                    }
                }
                if (ping_sent_flag) {
                    if (ping_escape_time > WS_CLIENT_SOCKET_PONG_TIMEOUT) {
                        if (!client->pong_recv) {
                            OS_LOGW(TAG, "Received websocket pong timeout");
#if defined(WS_CLIENT_SOCKET_DISCONN_IF_PONG_TIMEOUT)
                            ws_nopoll_close_conn(client);
#endif
                        }
                        ping_sent_flag = false;
                        ping_escape_time = 0;
                    } else {
                        ping_escape_time += WS_CLIENT_SOCKET_RECV_TIMEOUT;
                    }
                }
            }

            continue;
        }

        switch (msg->what) {
        case WS_CLIENT_CMD_CONNECT:
            if (client->conn_state == WS_CONN_STATE_CONNECTING) {
                if (ws_nopoll_open_conn(client) != 0) {
                    client->thread_exit = true;
                }
            } else {
                OS_LOGW(TAG, "WS_CLIENT_CMD_CONNECT: websocket not connecting");
            }
            break;
        case WS_CLIENT_CMD_SEND_TEXT:
            if (client->conn_state == WS_CONN_STATE_CONNECTED)
                ws_nopoll_send_text(client, &msg->xfer);
            else
                OS_LOGE(TAG, "WS_CLIENT_CMD_SEND_TEXT: websocket not connected");
            break;
        case WS_CLIENT_CMD_SEND_BINARY:
            if (client->conn_state == WS_CONN_STATE_CONNECTED)
                ws_nopoll_send_binary(client, &msg->xfer);
            else
                OS_LOGE(TAG, "WS_CLIENT_CMD_SEND_BINARY: websocket not connected");
            break;
        case WS_CLIENT_CMD_DISCONNECT:
            if (client->conn_state == WS_CONN_STATE_CONNECTED)
                ws_nopoll_close_conn(client);
            else
                OS_LOGW(TAG, "WS_CLIENT_CMD_DISCONNECT: websocket not connected");
            // force state as disconnected in case that previous state is connecting
            client->conn_state = WS_CONN_STATE_DISCONNECTED;
            break;
        default:
            OS_LOGE(TAG, "%d: unsupported websocket client command", msg->what);
            break;
        }
        OS_FREE(msg);
        msg = NULL;
    }

    if (client->conn_state == WS_CONN_STATE_CONNECTED)
        ws_nopoll_close_conn(client);
    client->conn_state = WS_CONN_STATE_DISCONNECTED;
    ws_client_clear_msglist_l(client); // clear all messages

    OS_LOGV(TAG, "websocket thread leave");
    return NULL;
}

ws_client_handle_t ws_client_create()
{
    OS_LOGD(TAG, "Websocket creating");
    ws_client_t *client = OS_CALLOC(1, sizeof(ws_client_t));
    if (client == NULL) {
        OS_LOGE(TAG, "Failed to allocate websocket handle");
        return NULL;
    }

    if ((client->lock = os_mutex_create()) == NULL)
        goto __error_exit;
    if ((client->cond = os_cond_create()) == NULL)
        goto __error_exit;

    list_init(&client->msg_list);
    client->conn_state = WS_CONN_STATE_DISCONNECTED;
    return client;

__error_exit:
    if (client->cond != NULL)
        os_cond_destroy(client->cond);
    if (client->lock != NULL)
        os_mutex_destroy(client->lock);
    OS_FREE(client);
    return NULL;
}

int ws_client_connect(ws_client_handle_t handle, ws_user_info_t *info)
{
    if (handle == NULL || info == NULL || info->host == NULL)
        return -1;

    OS_LOGD(TAG, "Websocket connecting, host:%s, port:%u, path:%s",
            info->host, info->port, info->path ? info->path : "null");

    if (handle->conn_state != WS_CONN_STATE_DISCONNECTED) {
        OS_LOGE(TAG, "Connect: websocket connected or connecting");
        return -1;
    }

    if (handle->thread != NULL) {
        OS_LOGV(TAG, "Connect: waiting previous connection exited");
        handle->thread_exit = true;
        os_thread_join(handle->thread, NULL);
    }

    struct os_thread_attr attr;
    attr.name = "websocket";
    attr.priority = WS_CLIENT_TASK_PRIORITY;
    attr.stacksize = WS_CLIENT_TASK_STACKSIZE;
    attr.joinable = true;

    handle->thread_exit = false;
    handle->thread = os_thread_create(&attr, ws_client_thread_entry, handle);
    if (handle->thread == NULL) {
        OS_LOGE(TAG, "Failed to create handler thread");
        handle->conn_state = WS_CONN_STATE_DISCONNECTED;
        return -1;
    }

    if (handle->prev_text != NULL) {
        OS_FREE(handle->prev_text);
        handle->prev_text = NULL;
        handle->prev_text_size = 0;
    }
    if (handle->user_info.host != NULL) {
        OS_FREE(handle->user_info.host);
        handle->user_info.host = NULL;
    }
    if (handle->user_info.path != NULL) {
        OS_FREE(handle->user_info.path);
        handle->user_info.path = NULL;
    }
    if (handle->user_info.cacert != NULL) {
        OS_FREE(handle->user_info.cacert);
        handle->user_info.cacert = NULL;
    }

    memcpy(&handle->user_info, info, sizeof(handle->user_info));
    if (info->host != NULL) {
        handle->user_info.host = OS_STRDUP(info->host);
        if (handle->user_info.host == NULL)
            goto __error_exit;
    }
    if (info->path != NULL) {
        handle->user_info.path = OS_STRDUP(info->path);
        if (handle->user_info.path == NULL)
            goto __error_exit;
    }
    if (info->cacert != NULL) {
        int cacert_size = strlen(info->cacert);
        handle->user_info.cacert = OS_MALLOC(cacert_size + 1);
        if (handle->user_info.cacert == NULL)
            goto __error_exit;
        memcpy(handle->user_info.cacert, info->cacert, cacert_size);
        handle->user_info.cacert[cacert_size] = 0;
    }

    ws_msg_t *msg = OS_CALLOC(1, sizeof(ws_msg_t));
    if (msg == NULL)
        goto __error_exit;
    msg->what = WS_CLIENT_CMD_CONNECT;
    handle->conn_state = WS_CONN_STATE_CONNECTING;

    os_mutex_lock(handle->lock);
    ws_client_clear_msglist(handle);
    list_add_tail(&handle->msg_list, &msg->listnode);
    os_cond_signal(handle->cond);
    os_mutex_unlock(handle->lock);
    return 0;

__error_exit:
    OS_LOGE(TAG, "Failed to connect websocket");

    handle->thread_exit = true;
    os_thread_join(handle->thread, NULL);
    handle->thread = NULL;

    if (handle->user_info.host != NULL) {
        OS_FREE(handle->user_info.host);
        handle->user_info.host = NULL;
    }
    if (handle->user_info.path != NULL) {
        OS_FREE(handle->user_info.path);
        handle->user_info.path = NULL;
    }
    if (handle->user_info.cacert != NULL) {
        OS_FREE(handle->user_info.cacert);
        handle->user_info.cacert = NULL;
    }
    return -1;
}

ws_conn_state_t ws_client_conn_state(ws_client_handle_t handle)
{
    if (handle == NULL)
        return WS_CONN_STATE_DISCONNECTED;
    return handle->conn_state;
}

void ws_client_set_heartbeat(ws_client_handle_t handle, int millisecond)
{
    if (handle == NULL)
        return;
    OS_LOGD(TAG, "Websocket setting heartbeat %d ms", millisecond);
    if (millisecond < WS_CLIENT_SOCKET_PONG_TIMEOUT) {
        OS_LOGW(TAG, "Ping interval is too short, force %d ms", WS_CLIENT_SOCKET_PONG_TIMEOUT);
        millisecond = WS_CLIENT_SOCKET_PONG_TIMEOUT;
    }
    handle->ping_interval = millisecond;
}

int ws_client_send_text(ws_client_handle_t handle, char *data, int len)
{
    if (handle == NULL || data == NULL || len <= 0)
        return -1;

    if (handle->conn_state != WS_CONN_STATE_CONNECTED) {
        OS_LOGE(TAG, "Send text: websocket not connected");
        return -1;
    }

    ws_msg_t *msg = OS_CALLOC(1, sizeof(ws_msg_t) + len + sizeof(long));
    if (msg == NULL)
        return -1;
    msg->what = WS_CLIENT_CMD_SEND_TEXT;
    msg->xfer.len = len;
    memcpy(msg->xfer.data, data, msg->xfer.len);

    os_mutex_lock(handle->lock);
    list_add_tail(&handle->msg_list, &msg->listnode);
    os_cond_signal(handle->cond);
    os_mutex_unlock(handle->lock);
    return 0;
}

int ws_client_send_text_unique(ws_client_handle_t handle, char *unique_data, int len)
{
    if (handle == NULL || unique_data == NULL || len <= 0)
        return -1;

    if (handle->conn_state != WS_CONN_STATE_CONNECTED) {
        OS_LOGE(TAG, "Send text: websocket not connected");
        OS_FREE(unique_data);
        return -1;
    }

    ws_msg_t *msg = OS_CALLOC(1, sizeof(ws_msg_t));
    if (msg == NULL)
        return -1;
    msg->what = WS_CLIENT_CMD_SEND_TEXT;
    msg->xfer.len = len;
    msg->xfer.unique_data = unique_data;

    os_mutex_lock(handle->lock);
    list_add_tail(&handle->msg_list, &msg->listnode);
    os_cond_signal(handle->cond);
    os_mutex_unlock(handle->lock);
    return 0;
}

int ws_client_send_binary(ws_client_handle_t handle, char *data, int len, ws_binary_type_t type)
{
    if (handle == NULL || data == NULL || len < 0)
        return -1;

    if (handle->conn_state != WS_CONN_STATE_CONNECTED) {
        OS_LOGE(TAG, "Send binary: websocket not connected");
        return -1;
    }

    ws_msg_t *msg = OS_CALLOC(1, sizeof(ws_msg_t) + len + sizeof(long));
    if (msg == NULL)
        return -1;
    msg->what = WS_CLIENT_CMD_SEND_BINARY;
    msg->xfer.len = len;
    msg->xfer.type = type;
    if (msg->xfer.len > 0)
        memcpy(msg->xfer.data, data, msg->xfer.len);

    os_mutex_lock(handle->lock);
    list_add_tail(&handle->msg_list, &msg->listnode);
    os_cond_signal(handle->cond);
    os_mutex_unlock(handle->lock);
    return 0;
}

int ws_client_send_binary_unique(ws_client_handle_t handle, char *unique_data, int len, ws_binary_type_t type)
{
    if (handle == NULL || unique_data == NULL || len < 0)
        return -1;

    if (handle->conn_state != WS_CONN_STATE_CONNECTED) {
        OS_LOGE(TAG, "Send binary: websocket not connected");
        OS_FREE(unique_data);
        return -1;
    }

    ws_msg_t *msg = OS_CALLOC(1, sizeof(ws_msg_t));
    if (msg == NULL)
        return -1;
    msg->what = WS_CLIENT_CMD_SEND_BINARY;
    msg->xfer.len = len;
    msg->xfer.type = type;
    msg->xfer.unique_data = unique_data;

    os_mutex_lock(handle->lock);
    list_add_tail(&handle->msg_list, &msg->listnode);
    os_cond_signal(handle->cond);
    os_mutex_unlock(handle->lock);
    return 0;
}

void ws_client_disconnect(ws_client_handle_t handle)
{
    if (handle == NULL)
        return;

    OS_LOGD(TAG, "Websocket disconnecting, host:%s",
            handle->user_info.host ? handle->user_info.host : "null");

    if (handle->thread != NULL) {
        handle->thread_exit = true;
        os_thread_join(handle->thread, NULL);
        handle->thread = NULL;
    }

    if (handle->prev_text != NULL) {
        OS_FREE(handle->prev_text);
        handle->prev_text = NULL;
        handle->prev_text_size = 0;
    }
    if (handle->user_info.host != NULL) {
        OS_FREE(handle->user_info.host);
        handle->user_info.host = NULL;
    }
    if (handle->user_info.path != NULL) {
        OS_FREE(handle->user_info.path);
        handle->user_info.path = NULL;
    }
    if (handle->user_info.cacert != NULL) {
        OS_FREE(handle->user_info.cacert);
        handle->user_info.cacert = NULL;
    }

    ws_client_clear_msglist_l(handle);
}

void ws_client_destory(ws_client_handle_t handle)
{
    if (handle == NULL)
        return;
    OS_LOGD(TAG, "Websocket destroying");
    ws_client_disconnect(handle);
    os_mutex_destroy(handle->lock);
    os_cond_destroy(handle->cond);
    OS_FREE(handle);
}
