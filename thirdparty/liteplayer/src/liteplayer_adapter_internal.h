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

#ifndef _LITEPLAYER_ADAPTER_INTERNAL_H_
#define _LITEPLAYER_ADAPTER_INTERNAL_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

struct liteplayer_adapter;
typedef struct liteplayer_adapter *liteplayer_adapter_handle_t;

struct liteplayer_adapter {
    int                    (*add_source_wrapper)(liteplayer_adapter_handle_t self, struct source_wrapper *wrapper);
    struct source_wrapper *(*find_source_wrapper)(liteplayer_adapter_handle_t self, const char *url);
    int                    (*add_sink_wrapper)(liteplayer_adapter_handle_t self, struct sink_wrapper *wrapper);
    struct sink_wrapper   *(*find_sink_wrapper)(liteplayer_adapter_handle_t self, const char *name);
    void                   (*destory)(liteplayer_adapter_handle_t self);
};

liteplayer_adapter_handle_t liteplayer_adapter_init();

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_INTERNAL_H_
