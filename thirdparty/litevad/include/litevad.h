/*
 * Copyright (C) 2018-2022 Qinglong<sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __LITEVAD_H
#define __LITEVAD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LITEVAD_RESULT_ERROR = -1,
    LITEVAD_RESULT_FRAME_SILENCE = 0,
    LITEVAD_RESULT_FRAME_ACTIVE = 1,
    LITEVAD_RESULT_SPEECH_BEGIN = 2,
    LITEVAD_RESULT_SPEECH_END = 3,
} litevad_result_t;

typedef void *litevad_handle_t;

litevad_handle_t litevad_create(int sample_rate, int channel_count);

litevad_result_t litevad_process(litevad_handle_t handle, const void *buff, int size);

void litevad_reset(litevad_handle_t handle);

void litevad_destroy(litevad_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // __LITEVAD_H
