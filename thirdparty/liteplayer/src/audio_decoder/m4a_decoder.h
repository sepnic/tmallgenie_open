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

#ifndef _M4A_DECODER_H_
#define _M4A_DECODER_H_

#include "audio_extractor/m4a_extractor.h"
#include "aac_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      M4A Decoder configurations
 */
struct m4a_decoder_cfg {
    int   task_stack;     /*!< Task stack size */
    int   task_prio;      /*!< Task priority (based on freeRTOS priority) */
    struct m4a_info *m4a_info;
};

#define DEFAULT_M4A_DECODER_CONFIG() {\
    .task_stack     = AAC_DECODER_TASK_STACK,\
    .task_prio      = AAC_DECODER_TASK_PRIO,\
}

struct m4a_decoder {
    void                   *handle;
    audio_element_handle_t  el;
    struct aac_buf_in       buf_in;
    struct aac_buf_out      buf_out;
    struct m4a_info        *m4a_info;
    bool                    parsed_header;
};

typedef struct m4a_decoder *m4a_decoder_handle_t;

int m4a_wrapper_run(m4a_decoder_handle_t decoder);
void m4a_wrapper_deinit(m4a_decoder_handle_t decoder);
int m4a_wrapper_init(m4a_decoder_handle_t decoder);

/**
 * @brief      Create an Audio Element handle to decode incoming M4A data
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t m4a_decoder_init(struct m4a_decoder_cfg *config);

#ifdef __cplusplus
}
#endif

#endif
