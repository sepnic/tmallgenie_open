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

#ifndef _LITEPLAYER_MEDIAPARSER_H_
#define _LITEPLAYER_MEDIAPARSER_H_

#include "esp_adf/audio_common.h"
#include "audio_extractor/mp3_extractor.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"
#include "audio_extractor/wav_extractor.h"
#include "liteplayer_source.h"

#ifdef __cplusplus
extern "C" {
#endif

enum media_parser_state {
    MEDIA_PARSER_FAILED = -1,
    MEDIA_PARSER_SUCCEED = 0,
};

struct media_codec_info {
    audio_codec_t       codec_type;
    int                 codec_samplerate;
    int                 codec_channels;
    int                 codec_bits;
    long                content_pos;
    long                content_len;
    int                 bytes_per_sec;
    int                 duration_ms;
    union {
        struct wav_info wav_info;
        struct mp3_info mp3_info;
        struct aac_info aac_info;
        struct m4a_info m4a_info;
        //struct opus_info opus_info;
        //struct flac_info flac_info;
    } detail;
};

typedef void (*media_parser_state_cb)(enum media_parser_state state, struct media_codec_info *info, void *priv);

typedef void *media_parser_handle_t;

int media_parser_get_codec_info(struct media_source_info *source, struct media_codec_info *codec);

long long media_parser_get_seek_offset(struct media_codec_info *codec, int seek_msec);

media_parser_handle_t media_parser_start_async(struct media_source_info *source,
                                               media_parser_state_cb listener,
                                               void *listener_priv);

void media_parser_stop(media_parser_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _LITEPLAYER_MEDIAPARSER_H_
