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

#ifndef _LITEPLAYER_ADAPTER_ESP32_TONE_SOURCE_WRAPPER_H_
#define _LITEPLAYER_ADAPTER_ESP32_TONE_SOURCE_WRAPPER_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *esp32_flashtone_wrapper_url_protocol();

source_handle_t esp32_flashtone_wrapper_open(const char *url, long long content_pos, void *priv_data);

int esp32_flashtone_wrapper_read(source_handle_t handle, char *buffer, int size);

long long esp32_flashtone_wrapper_content_pos(source_handle_t handle);

long long esp32_flashtone_wrapper_content_len(source_handle_t handle);

int esp32_flashtone_wrapper_seek(source_handle_t handle, long offset);

void esp32_flashtone_wrapper_close(source_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_ESP32_TONE_SOURCE_WRAPPER_H_
