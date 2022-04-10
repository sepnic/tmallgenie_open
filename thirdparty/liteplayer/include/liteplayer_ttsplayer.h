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

#ifndef _LITEPLAYER_TTSPLAYER_H_
#define _LITEPLAYER_TTSPLAYER_H_

#include <stdbool.h>
#include "liteplayer_adapter.h"
#include "liteplayer_main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TTSPLAYER_RINGBUF_SIZE (1024*32)

#define DEFAULT_TTSPLAYER_CFG() {\
    .ringbuf_size = DEFAULT_TTSPLAYER_RINGBUF_SIZE,\
}

struct ttsplayer_cfg {
    int ringbuf_size;
};

typedef struct ttsplayer *ttsplayer_handle_t;

ttsplayer_handle_t ttsplayer_create(struct ttsplayer_cfg *cfg);

int ttsplayer_register_sink_wrapper(ttsplayer_handle_t handle, struct sink_wrapper *wrapper);

int ttsplayer_register_state_listener(ttsplayer_handle_t handle, liteplayer_state_cb listener, void *listener_priv);

int ttsplayer_prepare_async(ttsplayer_handle_t handle);

int ttsplayer_write(ttsplayer_handle_t handle, char *buffer, int size, bool final);

int ttsplayer_start(ttsplayer_handle_t handle);

int ttsplayer_stop(ttsplayer_handle_t handle);

int ttsplayer_reset(ttsplayer_handle_t handle);

void ttsplayer_destroy(ttsplayer_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_TTSPLAYER_H_
