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

#ifndef _LITEPLAYER_ADAPTER_STATIC_WRAPPER_H_
#define _LITEPLAYER_ADAPTER_STATIC_WRAPPER_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *static_wrapper_url_protocol();

source_handle_t static_wrapper_open(const char *url, long long content_pos, void *priv_data);

int static_wrapper_read(source_handle_t handle, char *buffer, int size);

long long static_wrapper_content_pos(source_handle_t handle);

long long static_wrapper_content_len(source_handle_t handle);

int static_wrapper_seek(source_handle_t handle, long offset);

void static_wrapper_close(source_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_STATIC_WRAPPER_H_
