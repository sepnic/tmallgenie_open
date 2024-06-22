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
#include <stdlib.h>
#include <string.h>

#include "pvmp3/include/pvmp3decoder_api.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_extractor/mp3_extractor.h"
#include "audio_decoder/mp3_decoder.h"

#define TAG "[liteplayer]mp3_decoder"

struct pvmp3_wrapper {
    tPVMP3DecoderExternal pvmp3_config;
    void *pvmp3_buffer;
    bool new_frame;    // if reading new frame
    int  frame_size;
    char seek_buffer[MP3_DECODER_INPUT_BUFFER_SIZE];
    int  bytes_seek;
};

static int mp3_frame_size(char *buf)
{
    unsigned char ver, layer, brIdx, srIdx, padding;
    int sample_rate = 0, bit_rate = 0, frame_size = 0;

    if ((buf[0] & 0xFF) != 0xFF || (buf[1] & 0xE0) != 0xE0) {
        OS_LOGE(TAG, "Invalid mp3 sync word");
        return -1;
    }

    // read header fields - use bitmasks instead of GetBits() for speed, since format never varies
    ver     = (buf[1] >> 3) & 0x03;
    layer   = (buf[1] >> 1) & 0x03;
    brIdx   = (buf[2] >> 4) & 0x0f;
    srIdx   = (buf[2] >> 2) & 0x03;
    padding = (buf[2] >> 1) & 0x01;
    //sMode   = (buf[3] >> 6) & 0x03;

    // check parameters to avoid indexing tables with bad values
    if (ver == 1 ||  srIdx >= 3 || layer == 0 || brIdx == 15 || brIdx == 0) {
        OS_LOGE(TAG, "Invalid mp3 frame header");
        return -1;
    }

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

    return frame_size;
}

static int mp3_find_sync_offset(char *buf, int buf_size, struct mp3_info *info)
{
    struct mp3_info temp;
    bool found = false;
    int last_position = 0;
    int sync_offset = 0;

find_syncword:
    if (last_position + 4 > buf_size) {
        OS_LOGE(TAG, "Not enough data to parse, size:%d", buf_size);
        goto finish;
    }
    sync_offset = mp3_find_syncword(&buf[last_position], buf_size-last_position);
    if (sync_offset >= 0) {
        last_position += sync_offset;
        int ret = mp3_parse_header(&buf[last_position], buf_size-last_position, &temp);
        if (ret == 0 && temp.frame_size <= MP3_DECODER_INPUT_BUFFER_SIZE &&
            temp.sample_rate == info->sample_rate && temp.channels == info->channels) {
            found = true;
            goto finish;
        } else {
            OS_LOGD(TAG, "Retry to find sync word");
            last_position++;
            goto find_syncword;
        }
    } else {
        OS_LOGE(TAG, "Can't find mp3 sync word");
        goto finish;
    }

finish:
    if (found) {
        info->frame_size = temp.frame_size;
        info->frame_start_offset = last_position;
    }
    return found ? 0 : -1;
}

static int mp3_data_read(mp3_decoder_handle_t decoder)
{
    struct pvmp3_wrapper *wrap = (struct pvmp3_wrapper *)(decoder->handle);
    struct mp3_buf_in *in = &decoder->buf_in;
    int ret = 0;

    if (in->eof)
        return AEL_IO_DONE;

    if (decoder->seek_mode) {
        wrap->bytes_seek = sizeof(wrap->seek_buffer);
        ret = audio_element_input_chunk(decoder->el, wrap->seek_buffer, wrap->bytes_seek);
        if (ret == wrap->bytes_seek) {
            OS_LOGV(TAG, "SEEK_MODE: Read chunk succeed: %d/%d", ret, wrap->bytes_seek);
        } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        } else if (ret < 0) {
            OS_LOGW(TAG, "SEEK_MODE: Read chunk error: %d/%d", ret, wrap->bytes_seek);
            return ret;
        } else {
            OS_LOGW(TAG, "SEEK_MODE: Read chunk insufficient: %d/%d", ret, wrap->bytes_seek);
            in->eof = true;
            return AEL_IO_DONE;
        }

        struct mp3_info *info = decoder->mp3_info;
        ret = mp3_find_sync_offset(wrap->seek_buffer, wrap->bytes_seek, info);
        if (ret != 0) {
            OS_LOGE(TAG, "SEEK_MODE: Failed to find sync word after seeking");
            return AEL_IO_FAIL;
        }

        OS_LOGV(TAG, "SEEK_MODE: Found sync offset: %d/%d, frame_size=%d",
                info->frame_start_offset, wrap->bytes_seek, info->frame_size);

        wrap->bytes_seek -= info->frame_start_offset;
        if (wrap->bytes_seek > 0)
            memmove(wrap->seek_buffer, &wrap->seek_buffer[info->frame_start_offset], wrap->bytes_seek);
        in->bytes_want = 0;
        in->bytes_read = 0;
        wrap->frame_size = info->frame_size;
        decoder->seek_mode = false;
    }

    if (wrap->bytes_seek > 0) {
        if (wrap->bytes_seek < wrap->frame_size) {
            int remain = wrap->frame_size - wrap->bytes_seek;
            OS_LOGD(TAG, "SEEK_MODE: Insufficient data, request more for whole frame, remain/frame: %d/%d",
                    remain, wrap->frame_size);
            ret = audio_element_input_chunk(decoder->el,
                        wrap->seek_buffer+wrap->bytes_seek,
                        remain);
            if (ret == remain) {
                OS_LOGV(TAG, "SEEK_MODE: Read chunk succeed: %d/%d", ret, remain);
                wrap->bytes_seek = wrap->frame_size;
            } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
                in->eof = true;
                return AEL_IO_DONE;
            } else if (ret < 0) {
                OS_LOGW(TAG, "SEEK_MODE: Read chunk error: %d/%d", ret, remain);
                return ret;
            } else {
                OS_LOGW(TAG, "SEEK_MODE: Read chunk insufficient: %d/%d", ret, remain);
                in->eof = true;
                return AEL_IO_DONE;
            }
        }

        wrap->frame_size = mp3_frame_size(wrap->seek_buffer);
        if (wrap->frame_size <= 0 || wrap->frame_size > wrap->bytes_seek) {
            OS_LOGW(TAG, "SEEK_MODE: MP3 demux dummy data, AEL_IO_DONE");
            //in->eof = true;
            wrap->bytes_seek = 0;
            return AEL_IO_DONE;
        }

        memcpy(in->data, wrap->seek_buffer, wrap->frame_size);
        in->bytes_read = wrap->frame_size;
        in->bytes_want = 0;

        wrap->bytes_seek -= wrap->frame_size;
        if (wrap->bytes_seek > 0)
            memmove(wrap->seek_buffer, wrap->seek_buffer+wrap->frame_size, wrap->bytes_seek);

        if (wrap->bytes_seek >= 4) {
            wrap->frame_size = mp3_frame_size(wrap->seek_buffer);
            if (wrap->frame_size <= 0 || wrap->frame_size > MP3_DECODER_INPUT_BUFFER_SIZE) {
                OS_LOGW(TAG, "SEEK_MODE: MP3 demux dummy data, AEL_IO_DONE");
                //in->eof = true;
                wrap->bytes_seek = 0;
                return AEL_IO_DONE;
            }
        }
        return AEL_IO_OK;
    }

    if (in->bytes_want > 0) {
        if (wrap->new_frame) {
            OS_LOGD(TAG, "Remain %d/4 bytes header needed to read", in->bytes_want);
            goto fill_header;
        } else {
            OS_LOGD(TAG, "Remain %d/%d bytes frame needed to read", in->bytes_want, wrap->frame_size);
            goto fill_frame;
        }
    }

    in->bytes_want = 4;
    in->bytes_read = 0;
    wrap->new_frame = true;
fill_header:
    ret = audio_element_input(decoder->el, in->data+in->bytes_read, in->bytes_want);
    if (ret < in->bytes_want) {
        if (ret > 0) {
            in->bytes_read += ret;
            in->bytes_want -= ret;
            return AEL_IO_TIMEOUT;
        } else if (ret == AEL_IO_TIMEOUT) {
            return AEL_IO_TIMEOUT;
        } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        } else {
            return AEL_IO_FAIL;
        }
    }

    wrap->frame_size = mp3_frame_size(in->data);
    if (wrap->frame_size <= 0 || wrap->frame_size > MP3_DECODER_INPUT_BUFFER_SIZE) {
        OS_LOGW(TAG, "MP3 demux dummy data, AEL_IO_DONE");
        //in->eof = true;
        return AEL_IO_DONE;
    }

    in->bytes_read = 4;
    in->bytes_want = wrap->frame_size - in->bytes_read;
    wrap->new_frame = false;
fill_frame:
    ret = audio_element_input(decoder->el, in->data+in->bytes_read, in->bytes_want);
    if (ret < in->bytes_want) {
        if (ret > 0) {
            in->bytes_read += ret;
            in->bytes_want -= ret;
            return AEL_IO_TIMEOUT;
        } else if (ret == AEL_IO_TIMEOUT) {
            return AEL_IO_TIMEOUT;
        } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        } else {
            return AEL_IO_FAIL;
        }
    }

    in->bytes_read = wrap->frame_size;
    in->bytes_want = 0;
    return AEL_IO_OK;
}

int mp3_wrapper_run(mp3_decoder_handle_t decoder) 
{
    struct pvmp3_wrapper *wrap = (struct pvmp3_wrapper *)(decoder->handle);
    int ret = 0;

    ret = mp3_data_read(decoder);
    if (ret != AEL_IO_OK) {
        if (decoder->buf_in.eof) {
            OS_LOGV(TAG, "MP3 frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    wrap->pvmp3_config.inputBufferCurrentLength = decoder->buf_in.bytes_read;
    wrap->pvmp3_config.inputBufferMaxLength = MP3_DECODER_INPUT_BUFFER_SIZE;
    wrap->pvmp3_config.inputBufferUsedLength = 0;
    wrap->pvmp3_config.pInputBuffer = (uint8 *)decoder->buf_in.data;
    wrap->pvmp3_config.pOutputBuffer = (int16 *)decoder->buf_out.data;
    wrap->pvmp3_config.outputFrameSize = MP3_DECODER_OUTPUT_BUFFER_SIZE / sizeof(int16_t);
    wrap->pvmp3_config.crcEnabled = false;
    ERROR_CODE decoderErr = pvmp3_framedecoder(&wrap->pvmp3_config, wrap->pvmp3_buffer);
    if (decoderErr != NO_DECODING_ERROR) {
        OS_LOGE(TAG, "PVMP3Decoder encountered error: %d", decoderErr);
        return AEL_PROCESS_FAIL;
    }
    decoder->buf_out.bytes_remain = wrap->pvmp3_config.outputFrameSize * sizeof(short);

    if (wrap->pvmp3_config.inputBufferUsedLength != wrap->pvmp3_config.inputBufferCurrentLength) {
        OS_LOGW(TAG, "PVMP3Decoder data remaining: input_size=%d, used_size=%d",
            wrap->pvmp3_config.inputBufferCurrentLength, wrap->pvmp3_config.inputBufferUsedLength);
    }

    if (!decoder->parsed_header) {
        audio_element_info_t info = {0};
        info.samplerate = wrap->pvmp3_config.samplingRate;
        info.channels   = wrap->pvmp3_config.num_channels;
        info.bits       = 16;
        OS_LOGV(TAG,"Found mp3 header: SR=%d, CH=%d, BITS=%d", info.samplerate, info.channels, info.bits);
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);
        decoder->parsed_header = true;
    }

    return AEL_IO_OK;
}

int mp3_wrapper_init(mp3_decoder_handle_t decoder) 
{
    struct pvmp3_wrapper *wrap = audio_calloc(1, sizeof(struct pvmp3_wrapper));
    if (wrap == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for pvmp3 decoder");
        return -1;
    }

    uint32_t memRequirements = pvmp3_decoderMemRequirements();
    wrap->pvmp3_buffer = audio_malloc(memRequirements);
    if (wrap->pvmp3_buffer == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for pvmp3 decoder");
        audio_free(wrap);
        return -1;
    }
    pvmp3_InitDecoder(&wrap->pvmp3_config, wrap->pvmp3_buffer);

    decoder->handle = (void *)wrap;
    return 0;
} 

void mp3_wrapper_deinit(mp3_decoder_handle_t decoder)
{
    struct pvmp3_wrapper *wrap = (struct pvmp3_wrapper *)decoder->handle;
    if (wrap == NULL) return;

    audio_free(wrap->pvmp3_buffer);
    audio_free(wrap);
}
