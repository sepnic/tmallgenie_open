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

#ifndef __TMALLGENIE_VOICEENGINE_ESP32_H__
#define __TMALLGENIE_VOICEENGINE_ESP32_H__

#include <stdbool.h>

#define ESP_I2S_FIXED_RATE    16000
#define ESP_I2S_FIXED_CHANNEL 1

#ifdef __cplusplus
extern "C" {
#endif

bool GnVoiceEngine_init();

bool GnVoiceEngine_recorderStart();

int GnVoiceEngine_recorderRead(void *buffer, int size);

void GnVoiceEngine_recorderStop();

#ifdef __cplusplus
}
#endif

#endif // __TMALLGENIE_VOICEENGINE_ESP32_H__
