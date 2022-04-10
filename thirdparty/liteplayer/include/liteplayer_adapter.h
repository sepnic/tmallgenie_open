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

#ifndef _LITEPLAYER_ADAPTER_H_
#define _LITEPLAYER_ADAPTER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *source_handle_t;
typedef void *sink_handle_t;

struct source_wrapper {
    bool            async_mode; // for network stream, it's better to set async mode
    int             buffer_size; // size of the buffer that save source data
    void            *priv_data;
    const char *    (*url_protocol)(); // "http", "tts", "rtsp", "rtmp", "file"
    source_handle_t (*open)(const char *url, long long content_pos, void *priv_data);
    int             (*read)(source_handle_t handle, char *buffer, int size);//note: 0<=ret<size means eof
    long long       (*content_pos)(source_handle_t handle);
    long long       (*content_len)(source_handle_t handle);
    int             (*seek)(source_handle_t handle, long offset);
    void            (*close)(source_handle_t handle);
};

struct sink_wrapper {
    void            *priv_data;
    const char *    (*name)(); // "alsa", "wave", "opensles", "audiotrack"
    sink_handle_t   (*open)(int samplerate, int channels, int bits, void *priv_data);
    int             (*write)(sink_handle_t handle, char *buffer, int size);//return actual written size
    void            (*close)(sink_handle_t handle);
};

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_ADAPTER_H_
