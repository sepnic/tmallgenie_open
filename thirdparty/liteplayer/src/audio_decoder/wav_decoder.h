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

#ifndef _WAV_DECODER_H_
#define _WAV_DECODER_H_

#include "osal/os_thread.h"
#include "esp_adf/audio_element.h"
#include "audio_extractor/wav_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * brief      WAV Decoder configurations
 */
struct wav_decoder_cfg {
    int task_stack;     /*!< Task stack size */
    int task_prio;      /*!< Task priority (based on freeRTOS priority) */
    struct wav_info *wav_info;
};

#define WAV_DECODER_TASK_PRIO           (OS_THREAD_PRIO_NORMAL)
#define WAV_DECODER_TASK_STACK          (4 * 1024)

#define DEFAULT_WAV_DECODER_CONFIG() {\
    .task_prio          = WAV_DECODER_TASK_PRIO,\
    .task_stack         = WAV_DECODER_TASK_STACK,\
}

/**
 * @brief      Create an Audio Element handle to decode incoming WAV data
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t wav_decoder_init(struct wav_decoder_cfg *config);


#ifdef __cplusplus
}
#endif

#endif
