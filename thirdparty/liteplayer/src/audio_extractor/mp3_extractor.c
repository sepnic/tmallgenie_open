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
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/mp3_extractor.h"

#define TAG "[liteplayer]mp3_extractor"

#define DEFAULT_MP3_PARSER_BUFFER_SIZE 2048

int mp3_find_syncword(char *buf, int size)
{
    if (size < 2)
        return -1;

    for (int i = 0; i < size - 1; i++) {
        if ((buf[i] & 0xFF) == 0xFF && (buf[i+1] & 0xE0) == 0xE0)
            return i;
    }
    return -1;
}

int mp3_parse_header(char *buf, int buf_size, struct mp3_info *info)
{
    unsigned char ver, layer, brIdx, srIdx, sMode, padding;
    int sample_rate, bit_rate, frame_size;
    bool double_check = false;

    if (buf_size < 4)
        return -1;

check_next_frame:
    if ((buf[0] & 0xFF) != 0xFF || (buf[1] & 0xE0) != 0xE0) {
        OS_LOGE(TAG, "Invalid sync word");
        return -1;
    }

    // read header fields - use bitmasks instead of GetBits() for speed, since format never varies
    ver     = (buf[1] >> 3) & 0x03;
    layer   = (buf[1] >> 1) & 0x03;
    brIdx   = (buf[2] >> 4) & 0x0f;
    srIdx   = (buf[2] >> 2) & 0x03;
    padding = (buf[2] >> 1) & 0x01;
    sMode   = (buf[3] >> 6) & 0x03;

    // check parameters to avoid indexing tables with bad values
    if (ver == 1 ||  srIdx >= 3 || layer == 0 || brIdx == 15 || brIdx == 0) {
        OS_LOGE(TAG, "Invalid mp3 header");
        return -1;
    }

    if (double_check)
        goto success;

    OS_LOGV(TAG, "srIdx=%d, ver=%d, layer=%d, brIdx=%d", srIdx, ver, layer, brIdx);

    static const int kSamplingRateV1[] = {44100, 48000, 32000};
    sample_rate = kSamplingRateV1[srIdx];
    if (ver == 2 /* V2 */) {
        sample_rate /= 2;
    } else if (ver == 0 /* V2.5 */) {
        sample_rate /= 4;
    }

    if (layer == 3) {
        // layer I
        static const int kBitrateV1[] = {
            32, 64, 96, 128, 160, 192, 224, 256,  288, 320, 352, 384, 416, 448
        };
        static const int kBitrateV2[] = {
            32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256
        };
        bit_rate = (ver == 3) ? kBitrateV1[brIdx - 1] : kBitrateV2[brIdx - 1];
        frame_size = (12000 * bit_rate / sample_rate + padding) * 4;
    } else {
        // layer II or III
        static const int kBitrateV1L2[] = {
            32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384
        };
        static const int kBitrateV1L3[] = {
            32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
        };
        static const int kBitrateV2[] = {
            8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160
        };
        if (ver == 3 /* V1 */) {
            bit_rate = (layer == 2) ? kBitrateV1L2[brIdx - 1] : kBitrateV1L3[brIdx - 1];
        } else {
            // V2 (or 2.5)
            bit_rate = kBitrateV2[brIdx - 1];
        }

        if (ver == 3 /* V1 */) {
            frame_size = 144000 * bit_rate / sample_rate + padding;
        } else {
            // V2 or V2.5
            int tmp = (layer == 1 /* L3 */) ? 72000 : 144000;
            frame_size = tmp * bit_rate / sample_rate + padding;
        }
    }

    info->channels = (sMode == 0x03) ? 1 : 2;
    info->sample_rate = sample_rate;
    info->bit_rate = bit_rate;
    info->frame_size = frame_size;

    OS_LOGD(TAG, "channels=%d, sample_rate=%d, bit_rate=%d, frame_size=%d",
             info->channels, info->sample_rate, info->bit_rate, info->frame_size);

    if (frame_size + 4 > buf_size) {
        OS_LOGD(TAG, "Not enough data to double check, but go on");
        return 0;
    }
    buf += frame_size;
    double_check = true;
    goto check_next_frame;

success:
    return 0;
}

static void mp3_dump_info(struct mp3_info *info)
{
    OS_LOGD(TAG, "MP3 INFO:");
    OS_LOGD(TAG, "  >channels          : %d", info->channels);
    OS_LOGD(TAG, "  >sample_rate       : %d", info->sample_rate);
    OS_LOGD(TAG, "  >bit_rate          : %d", info->bit_rate);
    OS_LOGD(TAG, "  >frame_size        : %d", info->frame_size);
    OS_LOGD(TAG, "  >frame_start_offset: %d", info->frame_start_offset);
}

int mp3_extractor(mp3_fetch_cb fetch_cb, void *fetch_priv, struct mp3_info *info)
{
    int frame_start_offset = 0;
    int id3v2_len = 0;
    bool found = false;
    char buf[DEFAULT_MP3_PARSER_BUFFER_SIZE];
    int buf_size = sizeof(buf);
    int last_position = 0;
    int sync_offset = 0;

    buf_size = fetch_cb(buf, buf_size, 0, fetch_priv);
    if (buf_size < 4) {
        OS_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
        goto finish;
    }

    if (strncmp((const char *)buf, "ID3", 3) == 0) {
        id3v2_len =
                ((((int)(buf[6])) & 0x7F) << 21) +
                ((((int)(buf[7])) & 0x7F) << 14) +
                ((((int)(buf[8])) & 0x7F) <<  7) +
                 (((int)(buf[9])) & 0x7F);

        frame_start_offset = id3v2_len + 10;
        OS_LOGV(TAG, "ID3 tag find with length[%d]", id3v2_len);
    }

    if (frame_start_offset + 4 <= buf_size) {
        int remain_size = buf_size - frame_start_offset;
        int ret = mp3_parse_header(&buf[frame_start_offset], remain_size, info);
        if (ret == 0) {
            found = true;
            goto finish;
        }
    }

    if (frame_start_offset != 0) {
        OS_LOGV(TAG, "Request more data to parse frame header");
        buf_size = sizeof(buf);
        buf_size = fetch_cb(buf, buf_size, frame_start_offset, fetch_priv);
        if (buf_size < 4) {
            OS_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
            goto finish;
        }
    }

find_syncword:
    if (last_position + 4 > buf_size) {
        OS_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
        goto finish;
    }

    sync_offset = mp3_find_syncword(&buf[last_position], buf_size - last_position);
    if (sync_offset >= 0) {
        last_position += sync_offset;
        int ret = mp3_parse_header(&buf[last_position], buf_size - last_position, info);
        if (ret == 0) {
            found = true;
            goto finish;
        } else {
            OS_LOGV(TAG, "Retry to find sync word");
            last_position++;
            goto find_syncword;
        }
    } else {
        OS_LOGE(TAG, "Can't find mp3 sync word");
        goto finish;
    }

finish:
    if (found) {
        info->frame_start_offset = frame_start_offset + last_position;
        mp3_dump_info(info);
    }
    return found ? 0 : -1;
}
