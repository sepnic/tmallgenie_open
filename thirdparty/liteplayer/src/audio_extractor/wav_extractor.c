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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/wav_extractor.h"

#define TAG "[liteplayer]wav_extractor"

#define WAV_PARSE_BUFFER_SIZE (2048)

#define BSWAP_16(x) \
    (uint_16)( \
              (((uint_16)(x) & 0x00ff) << 8) | \
              (((uint_16)(x) & 0xff00) >> 8)   \
             )

#define BSWAP_32(x) \
    (uint_32)( \
              (((uint_32)(x) & 0xff000000) >> 24) | \
              (((uint_32)(x) & 0x00ff0000) >> 8)  | \
              (((uint_32)(x) & 0x0000ff00) << 8)  | \
              (((uint_32)(x) & 0x000000ff) << 24)   \
             )

#if !defined(BYTE_ORDER_BIG_ENDIAN)
#define COMPOSE_INT(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define COMPOSE_SHORT(a,b)   ((a) | ((b)<<8))
#define LE_SHORT(v)          (v)
#define LE_INT(v)            (v)
#define BE_SHORT(v)          BSWAP_16(v)
#define BE_INT(v)            BSWAP_32(v)
#else
#define COMPOSE_INT(a,b,c,d) ((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define COMPOSE_SHORT(a,b)   ((b) | ((a)<<8))
#define LE_SHORT(v)          BSWAP_16(v)
#define LE_INT(v)            BSWAP_32(v)
#define BE_SHORT(v)          (v)
#define BE_INT(v)            (v)
#endif

#define WAV_CHUNK_RIFF        COMPOSE_INT('R','I','F','F')
#define WAV_CHUNK_WAVE        COMPOSE_INT('W','A','V','E')
#define WAV_CHUNK_FMT         COMPOSE_INT('f','m','t',' ')
#define WAV_CHUNK_LIST        COMPOSE_INT('L','I','S','T')
#define WAV_CHUNK_FACT        COMPOSE_INT('f','a','c','t')
#define WAV_CHUNK_PEAK        COMPOSE_INT('P','E','A','K')
#define WAV_CHUNK_DATA        COMPOSE_INT('d','a','t','a')

/* WAV_FMT_EXTENSIBLE format */
#define WAV_GUID_TAG "/x00/x00/x00/x00/x10/x00/x80/x00/x00/xAA/x00/x38/x9B/x71"

static void wav_dump_info(struct wav_info *info)
{
    OS_LOGD(TAG, "WAV INFO:");
    OS_LOGD(TAG, "  >audioFormat          : %u", info->audioFormat);
    OS_LOGD(TAG, "  >sampleRate           : %u", info->sampleRate);
    OS_LOGD(TAG, "  >channels             : %u", info->channels);
    OS_LOGD(TAG, "  >bits                 : %u", info->bits);
    OS_LOGD(TAG, "  >byteRate             : %u", info->byteRate);
    OS_LOGD(TAG, "  >blockAlign           : %u", info->blockAlign);
    OS_LOGD(TAG, "  >dataSize             : %u", info->dataSize);
    OS_LOGD(TAG, "  >dataOffset           : %u", info->dataOffset);
}

int wav_parse_header(char *buf, int buf_size, struct wav_info *info)
{
    if (buf_size < sizeof(wav_header_t))
        return -1;

    uint8_t *temp = (uint8_t *)buf;
    uint32_t ChunkID, ChunkSize, Format;
    uint32_t i = 0;

    ChunkID = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
    i += 4;
    ChunkSize = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
    i += 4;
    Format = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
    i += 4;

    if (ChunkID != WAV_CHUNK_RIFF || Format != WAV_CHUNK_WAVE) {
        OS_LOGE(TAG, "Invalid wave format, ChunkID:0x%x, Format:0x%x", ChunkID, Format);
        return -1;
    }

    for ( ; i < buf_size - 4; ) {
        ChunkID = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);

        switch (ChunkID) {
        case WAV_CHUNK_FMT:
            OS_LOGV(TAG, "Found fmt  Chunk");
            i += 4;
            ChunkSize = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            info->audioFormat = COMPOSE_SHORT(temp[i+0], temp[i+1]);
            i += 2;
            info->channels = COMPOSE_SHORT(temp[i+0], temp[i+1]);
            i += 2;
            info->sampleRate = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            info->byteRate = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            info->blockAlign = COMPOSE_SHORT(temp[i+0], temp[i+1]);
            i += 2;
            info->bits = COMPOSE_SHORT(temp[i+0], temp[i+1]);
            i += 2;
            i += (ChunkSize - 16);
            break;

        case WAV_CHUNK_LIST:
            OS_LOGV(TAG, "Found LIST Chunk");
            i += 4;
            ChunkSize = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            i += ChunkSize;
            break;

        case WAV_CHUNK_FACT:
            OS_LOGV(TAG, "Found fact Chunk");
            i += 4;
            ChunkSize = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            i += 4;
            break;

        case WAV_CHUNK_PEAK:
            OS_LOGV(TAG, "Found PEAK Chunk");
            i += 4;
            ChunkSize = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            i += ChunkSize;
            break;

        case WAV_CHUNK_DATA:
            OS_LOGV(TAG, "Found data Chunk");
            i += 4;
            info->dataSize = COMPOSE_INT(temp[i+0], temp[i+1], temp[i+2], temp[i+3]);
            i += 4;
            info->dataOffset = i;
            goto parse_done;
            break;

        default:
            i++;
            break;
        }
    }

parse_done:
    wav_dump_info(info);

    if (info->audioFormat != WAV_FMT_PCM && info->audioFormat != WAV_FMT_IEEE_FLOAT &&
        info->audioFormat != WAV_FMT_ADPCM && info->audioFormat != WAV_FMT_DVI_ADPCM) {
        OS_LOGE(TAG, "Unsupported AudioFormat:0x%x", info->audioFormat);
        return -1;
    }
    if (info->channels < 1 || info->channels > WAV_MAX_CHANNEL_COUNT) {
        OS_LOGE(TAG, "Unsupported NumOfChannels:%u", info->channels);
        return -1;
    }
    if (info->blockAlign != info->bits*info->channels/8) {
        OS_LOGE(TAG, "Invalid blockAlign:%u", info->blockAlign);
        return -1;
    }
    if (info->byteRate != info->blockAlign*info->sampleRate) {
        OS_LOGE(TAG, "Invalid ByteRate:%u", info->byteRate);
        return -1;
    }
    if (info->dataOffset < sizeof(wav_header_t)) {
        OS_LOGE(TAG, "Invalid dataOffset:%u", info->dataOffset);
        return -1;
    }

    info->header_size = info->dataOffset;
    info->header_buff = audio_calloc(1, info->header_size);
    if (info->header_buff == NULL) {
        OS_LOGE(TAG, "Failed to allocate header buffer");
        return -1;
    }
    memcpy(info->header_buff, buf, info->header_size);
    return 0;
}

int wav_extractor(wav_fetch_cb fetch_cb, void *fetch_priv, struct wav_info *info)
{
    char buf[WAV_PARSE_BUFFER_SIZE];
    int buf_size = sizeof(buf);
    int ret = -1;

    buf_size = fetch_cb(buf, buf_size, 0, fetch_priv);
    ret = wav_parse_header(buf, buf_size, info);
    if (ret != 0) {
        OS_LOGE(TAG, "Failed to parse wav header");
    }
    return ret;
}
