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

#ifndef __SYSUTILS_WEBSOCKET_CLIENT_H__
#define __SYSUTILS_WEBSOCKET_CLIENT_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ws_client *ws_client_handle_t;

typedef enum {
    WS_BINARY_FRAGMENT_START,
    WS_BINARY_FRAGMENT_CONTINUE,
    WS_BINARY_FRAGMENT_FINISH,
    WS_BINARY_WHOLE,
} ws_binary_type_t;

typedef struct {
    void (*on_connected)();
    void (*on_disconnected)();
    void (*on_received_text)(char *text, int size);
    void (*on_received_binary)(char *data, int size, ws_binary_type_t type);
} ws_callback_t;

typedef struct {
    unsigned int port;
    char *host;
    char *path;
    char *cacert;
    ws_callback_t callback;
} ws_user_info_t;

typedef enum {
    WS_CONN_STATE_INVALID = -1,
    WS_CONN_STATE_DISCONNECTED = 0,
    WS_CONN_STATE_CONNECTING,
    WS_CONN_STATE_CONNECTED,
} ws_conn_state_t;

ws_client_handle_t ws_client_create();

int ws_client_connect(ws_client_handle_t handle, ws_user_info_t *info);

// websocket allocate a new buffer, and copy 'data' to the new buffer
int ws_client_send_text(ws_client_handle_t handle, char *data, int len);

// websocket send 'unique_data' directly, and free 'unique_data' after sending
int ws_client_send_text_unique(ws_client_handle_t handle, char *unique_data, int len);

// websocket allocate a new buffer, and copy 'data' to the new buffer
int ws_client_send_binary(ws_client_handle_t handle, char *data, int len, ws_binary_type_t type);

// websocket send 'unique_data' directly, and free 'unique_data' after sending
int ws_client_send_binary_unique(ws_client_handle_t handle, char *unique_data, int len, ws_binary_type_t type);

ws_conn_state_t ws_client_conn_state(ws_client_handle_t handle);

void ws_client_set_heartbeat(ws_client_handle_t handle, int millisecond);

void ws_client_disconnect(ws_client_handle_t handle);

void ws_client_destory(ws_client_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_WEBSOCKET_CLIENT_H__ */
