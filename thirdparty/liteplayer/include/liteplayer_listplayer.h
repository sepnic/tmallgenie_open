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

#ifndef _LITEPLAYER_LISTPLAYER_H_
#define _LITEPLAYER_LISTPLAYER_H_

#include "liteplayer_adapter.h"
#include "liteplayer_main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_PLAYLIST_FILE_SUFFIX  ".playlist"
#define DEFAULT_PLAYLIST_URL_MAX      (200)

#define DEFAULT_LISTPLAYER_CFG() {\
    .playlist_url_suffix = DEFAULT_PLAYLIST_FILE_SUFFIX,\
    .playlist_url_max    = DEFAULT_PLAYLIST_URL_MAX,\
}

struct listplayer_cfg {
    const char *playlist_url_suffix;
    int         playlist_url_max;
};

typedef struct listplayer *listplayer_handle_t;

listplayer_handle_t listplayer_create(struct listplayer_cfg *cfg);

int listplayer_register_source_wrapper(listplayer_handle_t handle, struct source_wrapper *wrapper);

int listplayer_register_sink_wrapper(listplayer_handle_t handle, struct sink_wrapper *wrapper);

int listplayer_register_state_listener(listplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv);

int listplayer_set_data_source(listplayer_handle_t handle, const char *url);

int listplayer_prepare_async(listplayer_handle_t handle);

int listplayer_start(listplayer_handle_t handle);

int listplayer_pause(listplayer_handle_t handle);

int listplayer_resume(listplayer_handle_t handle);

int listplayer_seek(listplayer_handle_t handle, int msec);

int listplayer_switch_next(listplayer_handle_t handle);

int listplayer_switch_prev(listplayer_handle_t handle);

int listplayer_set_single_looping(listplayer_handle_t handle, bool enable);

int listplayer_stop(listplayer_handle_t handle);

int listplayer_reset(listplayer_handle_t handle);

int listplayer_get_position(listplayer_handle_t handle, int *msec);

int listplayer_get_duration(listplayer_handle_t handle, int *msec);

void listplayer_destroy(listplayer_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_LISTPLAYER_H_
