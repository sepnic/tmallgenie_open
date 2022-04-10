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

#ifndef _LITEPLAYER_MAIN_H_
#define _LITEPLAYER_MAIN_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

enum liteplayer_state {
    LITEPLAYER_IDLE            = 0x00,
    LITEPLAYER_INITED          = 0x01,
    LITEPLAYER_PREPARED        = 0x02,
    LITEPLAYER_STARTED         = 0x03,
    LITEPLAYER_PAUSED          = 0x04,
    LITEPLAYER_SEEKCOMPLETED   = 0x05,
    LITEPLAYER_NEARLYCOMPLETED = 0x06,
    LITEPLAYER_COMPLETED       = 0x07,
    LITEPLAYER_STOPPED         = 0x08,
    LITEPLAYER_ERROR           = 0xFF,
};

typedef int (*liteplayer_state_cb)(enum liteplayer_state state, int errcode, void *priv);

typedef struct liteplayer *liteplayer_handle_t;

liteplayer_handle_t liteplayer_create();

int liteplayer_register_source_wrapper(liteplayer_handle_t handle, struct source_wrapper *wrapper);

int liteplayer_register_sink_wrapper(liteplayer_handle_t handle, struct sink_wrapper *wrapper);

int liteplayer_register_state_listener(liteplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv);

int liteplayer_set_data_source(liteplayer_handle_t handle, const char *url);

int liteplayer_prepare(liteplayer_handle_t handle);

int liteplayer_prepare_async(liteplayer_handle_t handle);

int liteplayer_start(liteplayer_handle_t handle);

int liteplayer_pause(liteplayer_handle_t handle);

int liteplayer_resume(liteplayer_handle_t handle);

int liteplayer_seek(liteplayer_handle_t handle, int msec);

int liteplayer_stop(liteplayer_handle_t handle);

int liteplayer_reset(liteplayer_handle_t handle);

int liteplayer_get_position(liteplayer_handle_t handle, int *msec);

int liteplayer_get_duration(liteplayer_handle_t handle, int *msec);

void liteplayer_destroy(liteplayer_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_MAIN_H_
