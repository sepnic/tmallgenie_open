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

#ifndef _LITEPLAYER_MEDIASOURCE_H_
#define _LITEPLAYER_MEDIASOURCE_H_

#include "cutils/ringbuf.h"
#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

enum media_source_state {
    MEDIA_SOURCE_READ_SUCCEED,
    MEDIA_SOURCE_READ_FAILED,
    MEDIA_SOURCE_READ_DONE,
    MEDIA_SOURCE_WRITE_SUCCEED,
    MEDIA_SOURCE_WRITE_FAILED,
    MEDIA_SOURCE_WRITE_DONE,
};

typedef void (*media_source_state_cb)(enum media_source_state state, void *priv);

struct media_source_info {
    const char *url;
    source_handle_t source_handle;
    struct source_wrapper *source_ops;
    long long content_pos;
    ringbuf_handle out_ringbuf;
};

typedef void *media_source_handle_t;

media_source_handle_t media_source_start_async(struct media_source_info *info,
                                               media_source_state_cb listener,
                                               void *listener_priv);

void media_source_stop(media_source_handle_t handle);

int m3u_get_first_url(struct media_source_info *info, char *buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_MEDIASOURCE_H_
