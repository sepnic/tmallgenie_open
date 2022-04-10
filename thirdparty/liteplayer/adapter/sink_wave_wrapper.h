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

#ifndef _LITEPLAYER_ADAPTER_WAVE_WRAPPER_H_
#define _LITEPLAYER_ADAPTER_WAVE_WRAPPER_H_

#include "liteplayer_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *wave_wrapper_name();

sink_handle_t wave_wrapper_open(int samplerate, int channels, int bits, void *priv_data);

int wave_wrapper_write(sink_handle_t handle, char *buffer, int size);

void wave_wrapper_close(sink_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_WAVE_WRAPPER_H_
