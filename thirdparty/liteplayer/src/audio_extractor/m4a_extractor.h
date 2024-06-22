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

#ifndef _M4A_EXTRACTOR_H_
#define _M4A_EXTRACTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "cutils/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

// Return the data size obtained
typedef int (*m4a_fetch_cb)(char *buf, int wanted_size, long offset, void *fetch_priv);

struct time2sample {
    uint32_t sample_count;
    uint32_t sample_duration;
};

struct sample2chunk {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
    uint32_t sample_description_index;
};

struct chunk2offset {
    uint32_t sample_index;
    uint32_t chunk_offset;
};

struct audio_specific_config {
    uint8_t buf[15];
    uint8_t size;
    uint32_t samplerate;
    uint32_t channels;
};

struct m4a_info {
    uint32_t    samplerate;
    uint32_t    channels;
    uint32_t    bits;
    uint32_t    bitrate_max;
    uint32_t    bitrate_avg;
    uint32_t    time_scale;
    uint32_t    duration;

    // stsz box: samplesize table
    uint32_t    stsz_samplesize_entries;
    uint32_t    stsz_samplesize_index;
    uint16_t   *stsz_samplesize; // need to free when resetting player
    uint32_t    stsz_samplesize_max;

    // stts box: time2sample table
    uint32_t    stts_time2sample_entries;
    struct time2sample *stts_time2sample;

    // stsc box: sample2chunk table
    uint32_t    stsc_sample2chunk_entries;
    struct sample2chunk *stsc_sample2chunk;

    // stco box: chunk2offset table
    uint32_t    stco_chunk2offset_entries;
    struct chunk2offset *stco_chunk2offset;

    // Audio Specific Config data:
    struct audio_specific_config asc;

    bool        parsed_once;
    bool        moov_tail;
    uint32_t    moov_offset;
    uint32_t    mdat_size;
    uint32_t    mdat_offset;
};

int m4a_parse_header(ringbuf_handle rb, struct m4a_info *info);

int m4a_get_seek_offset(int seek_ms, struct m4a_info *info, uint32_t *sample_index, uint32_t *sample_offset);

int m4a_extractor(m4a_fetch_cb fetch_cb, void *fetch_priv, struct m4a_info *info);

#ifdef __cplusplus
}
#endif

#endif
