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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_element.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/wav_extractor.h"
#include "audio_decoder/wav_decoder.h"
#include "dr_libs/dr_wav.h"

#define TAG "[liteplayer]wav_decoder"

#define WAV_DECODER_INPUT_TIMEOUT_MAX  200 // ms
#define WAV_DECODER_PREFERED_PEROID_MS 20 // ms

struct wav_buf_in {
    char *data;
    int  size;
    int  bytes_want;     // bytes that want to read
    int  bytes_read;     // bytes that have read
    bool eof;            // if end of stream
};

struct wav_buf_out {
    char *data;
    int  size;
    int  bytes_remain;   // bytes that remained to write
    int  bytes_written;  // bytes that have written
};

struct wav_decoder {
    audio_element_handle_t  el;
    drwav                   drwav;
    bool                    drwav_inited;
    int                     drwav_offset;
    int                     block_align;
    struct wav_buf_in       buf_in;
    struct wav_buf_out      buf_out;
    bool                    parsed_header;
    bool                    filled_header;
    bool                    read_timeout;
    struct wav_info        *wav_info;
    int                     sink_bits;
    drwav_uint64            prefered_frames;
};
typedef struct wav_decoder *wav_decoder_handle_t;

static void *drwav_on_malloc(size_t sz, void* pUserData)
{
    return audio_malloc(sz);
}

static void *drwav_on_realloc(void *p, size_t sz, void *pUserData)
{
    return audio_realloc(p, sz);
}

static void drwav_on_free(void *p, void *pUserData)
{
    audio_free(p);
}

static drwav_allocation_callbacks drwav_allocation = {
    .pUserData = NULL,
    .onMalloc = drwav_on_malloc,
    .onRealloc = drwav_on_realloc,
    .onFree = drwav_on_free,
};

static size_t drwav_on_read(void *pUserData, void *pBufferOut, size_t bytesToRead)
{
    wav_decoder_handle_t decoder = (wav_decoder_handle_t)pUserData;
    if (bytesToRead > decoder->buf_in.bytes_read) {
        OS_LOGW(TAG, "Insufficient data: %d/%d", (int)bytesToRead, decoder->buf_in.bytes_read);
        bytesToRead = decoder->buf_in.bytes_read;
    }
    if (bytesToRead > 0) {
        memcpy(pBufferOut, &decoder->buf_in.data[decoder->drwav_offset], bytesToRead);
        decoder->drwav_offset += bytesToRead;
        decoder->buf_in.bytes_read -= bytesToRead;
    }
    return bytesToRead;
}

static drwav_bool32 drwav_on_seek(void *pUserData, int offset, drwav_seek_origin origin)
{
    wav_decoder_handle_t decoder = (wav_decoder_handle_t)pUserData;
    if (origin != drwav_seek_origin_current) {
        OS_LOGE(TAG, "Unsupported seek mode");
        return DRWAV_FALSE;
    }
    if (offset > decoder->buf_in.bytes_read) {
        OS_LOGE(TAG, "Offset overflow: %d:%d", offset, decoder->buf_in.bytes_read);
        return DRWAV_FALSE;
    }
    decoder->drwav_offset += offset;
    decoder->buf_in.bytes_read -= offset;
    return DRWAV_TRUE;
}

static int drwav_run(wav_decoder_handle_t decoder)
{
    struct wav_buf_in *in = &decoder->buf_in;
    drwav_uint64 prefered_insize = decoder->prefered_frames * decoder->wav_info->blockAlign;
    int ret = AEL_IO_OK;

    if (in->eof && in->bytes_read < decoder->block_align) {
        OS_LOGV(TAG, "WAV frame end");
        return AEL_IO_DONE;
    }

    if (!decoder->filled_header) {
        if (in->bytes_read < decoder->wav_info->header_size) {
            if (decoder->wav_info->header_size > prefered_insize)
                return AEL_PROCESS_FAIL;
            memcpy(in->data, decoder->wav_info->header_buff, decoder->wav_info->header_size);
            in->bytes_read = decoder->wav_info->header_size;
        }

        in->bytes_want = prefered_insize - in->bytes_read;
        ret = audio_element_input_chunk(decoder->el, in->data+in->bytes_read, in->bytes_want);
        if (ret == in->bytes_want) {
            in->bytes_read += ret;
        } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        } else if (ret < 0) {
            OS_LOGW(TAG, "Read chunk error: %d/%d", ret, in->bytes_want);
            return ret;
        } else {
            in->eof = true;
            in->bytes_read += ret;
        }

        decoder->drwav_offset = 0;
        decoder->filled_header = true;
    }

    if (in->bytes_read < decoder->block_align) {
        if (in->bytes_read > 0) {
            if (!decoder->read_timeout) {
                //OS_LOGD(TAG, "Refill data with remaining bytes:%d, offset:%d",
                //        in->bytes_read, decoder->drwav_offset);
                memmove(in->data, in->data+decoder->drwav_offset, in->bytes_read);
                in->bytes_want = prefered_insize - in->bytes_read;
                decoder->drwav_offset = in->bytes_read;
            }
        } else {
            in->bytes_want = prefered_insize;
            decoder->drwav_offset = 0;
        }
        ret = audio_element_input_chunk(decoder->el, in->data+decoder->drwav_offset, in->bytes_want);
        if (ret == in->bytes_want) {
            in->bytes_read += ret;
            decoder->drwav_offset = 0;
        } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
            in->eof = true;
            return AEL_IO_DONE;
        } else if (ret < 0) {
            OS_LOGW(TAG, "Read chunk error: %d/%d", ret, in->bytes_want);
            decoder->read_timeout = true;
            return ret;
        } else {
            in->eof = true;
            in->bytes_read += ret;
            decoder->drwav_offset = 0;
        }
        decoder->read_timeout = false;
    }

    if (!decoder->drwav_inited) {
        if (drwav_init_ex(&decoder->drwav,
                          drwav_on_read, drwav_on_seek, NULL,
                          (void *)decoder, NULL, DRWAV_SEQUENTIAL,
                          &drwav_allocation) != DRWAV_TRUE) {
            OS_LOGE(TAG, "Failed to init drwav decoder");
            return AEL_PROCESS_FAIL;
        }

        if (!decoder->parsed_header) {
            audio_element_info_t info = {0};
            info.samplerate = decoder->drwav.sampleRate;
            info.channels   = decoder->drwav.channels;
#if defined(LITEPLAYER_CONFIG_SINK_FIXED_S16LE)
            info.bits       = 16;
#else
            switch (decoder->drwav.bitsPerSample) {
            case 16:
                info.bits = 16;
                break;
            case 24:
            case 32:
                info.bits = 32;
                break;
            default:
                if (decoder->drwav.bitsPerSample > 32)
                    info.bits = 32;
                else
                    info.bits = 16;
                break;
            }
#endif
            OS_LOGV(TAG,"Found wav header: SR=%d, CH=%d, BITS=%d", info.samplerate, info.channels, info.bits);
            audio_element_setinfo(decoder->el, &info);
            audio_element_report_info(decoder->el);

            decoder->sink_bits = info.bits;
            if (decoder->sink_bits > decoder->drwav.bitsPerSample) {
                int prefered_outsize = decoder->prefered_frames*decoder->sink_bits*info.channels/8;
                if (prefered_outsize > decoder->buf_out.size) {
                    audio_free(decoder->buf_out.data);
                    decoder->buf_out.size = prefered_outsize;
                    decoder->buf_out.data = audio_malloc(decoder->buf_out.size);
                    if (decoder->buf_out.data == NULL)
                        return AEL_PROCESS_FAIL;
                }
            }
            decoder->parsed_header = true;
        }
        decoder->block_align = decoder->drwav.channels*decoder->drwav.bitsPerSample/8;
        decoder->drwav_inited = true;
    }

    drwav_uint64 in_frames = (decoder->prefered_frames > in->bytes_read/decoder->block_align) ?
                             in->bytes_read/decoder->block_align : decoder->prefered_frames;
    drwav_uint64 out_frames;
#if defined(LITEPLAYER_CONFIG_SINK_FIXED_S16LE)
    drwav_int16 *out = (drwav_int16 *)(decoder->buf_out.data);
    out_frames = drwav_read_pcm_frames_s16le(&decoder->drwav, in_frames, out);
#else
    switch (decoder->sink_bits) {
    case 16: {
        drwav_int16 *out = (drwav_int16 *)(decoder->buf_out.data);
        out_frames = drwav_read_pcm_frames_s16le(&decoder->drwav, in_frames, out);
        break;
    }
    case 32: {
        drwav_int32 *out = (drwav_int32 *)(decoder->buf_out.data);
        out_frames = drwav_read_pcm_frames_s32le(&decoder->drwav, in_frames, out);
        break;
    }
    default:
        OS_LOGE(TAG, "Unsupported sample bits: %d", decoder->sink_bits);
        return AEL_PROCESS_FAIL;
    }
#endif
    if (out_frames == 0) {
        OS_LOGW(TAG, "WAVDecode dummy data, AEL_IO_DONE");
        return AEL_IO_DONE;
    }
    //OS_LOGV(TAG, "WAVDecode out_frames: %d", out_frames);
    decoder->buf_out.bytes_remain = out_frames * decoder->drwav.channels * (decoder->sink_bits/8);
    return 0;
}

static esp_err_t wav_decoder_destroy(audio_element_handle_t self)
{
    wav_decoder_handle_t decoder = (wav_decoder_handle_t)audio_element_getdata(self);
    OS_LOGV(TAG, "Destroy wav decoder");
    audio_free(decoder->buf_in.data);
    audio_free(decoder->buf_out.data);
    audio_free(decoder);
    return ESP_OK;
}

static esp_err_t wav_decoder_open(audio_element_handle_t self)
{
    OS_LOGV(TAG, "Open wav decoder");
    return ESP_OK;
}

static esp_err_t wav_decoder_close(audio_element_handle_t self)
{
    wav_decoder_handle_t decoder = (wav_decoder_handle_t)audio_element_getdata(self);

    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        if (decoder->drwav_inited) {
            OS_LOGV(TAG, "Close drwav decoder");
            drwav_uninit(&decoder->drwav);
            decoder->drwav_inited = false;
        }
        decoder->parsed_header = false;

        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        info.total_bytes = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static int wav_decoder_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int byte_write = 0;
    int ret = AEL_IO_FAIL;
    wav_decoder_handle_t decoder = (wav_decoder_handle_t)audio_element_getdata(self);

    if (!decoder->drwav_inited) {
        ringbuf_handle rb = audio_element_get_input_ringbuf(self);
        if (rb != NULL) {
            // update prefered frames if the size of input ringbuf is too small
            int max_frames = rb_get_size(rb) / decoder->wav_info->blockAlign;
            if (max_frames > 0 && decoder->prefered_frames > max_frames) {
                OS_LOGW(TAG, "The input ringbuf is too small, update prefered_frames: %d>>%d",
                        (int)decoder->prefered_frames, max_frames);
                decoder->prefered_frames = max_frames;
            }
        }
    }

    if (decoder->buf_out.bytes_remain > 0) {
        /* Output buffer have remain data */
        byte_write = audio_element_output(self,
                        decoder->buf_out.data+decoder->buf_out.bytes_written,
                        decoder->buf_out.bytes_remain);
    } else {
        /* More data need to be wrote */
        ret = drwav_run(decoder);
        if (ret < 0) {
            if (ret == AEL_IO_TIMEOUT) {
                OS_LOGW(TAG, "drwav_run AEL_IO_TIMEOUT");
            } else if (ret != AEL_IO_DONE) {
                OS_LOGE(TAG, "drwav_run failed:%d", ret);
            }
            return ret;
        }

        //OS_LOGV(TAG, "ret=%d, bytes_remain=%d", ret, decoder->buf_out.bytes_remain);
        decoder->buf_out.bytes_written = 0;
        byte_write = audio_element_output(self,
                        decoder->buf_out.data,
                        decoder->buf_out.bytes_remain);
    }

    if (byte_write > 0) {
        decoder->buf_out.bytes_remain -= byte_write;
        decoder->buf_out.bytes_written += byte_write;

        audio_element_info_t audio_info = {0};
        audio_element_getinfo(self, &audio_info);
        audio_info.byte_pos += byte_write;
        audio_element_setinfo(self, &audio_info);
    }

    return byte_write;
}

static esp_err_t wav_decoder_seek(audio_element_handle_t self, long long offset)
{
    wav_decoder_handle_t decoder = (wav_decoder_handle_t)audio_element_getdata(self);
    decoder->drwav_offset = 0;
    decoder->read_timeout = false;
    decoder->buf_in.bytes_read = 0;
    decoder->buf_in.bytes_want = 0;
    decoder->buf_in.eof = false;
    decoder->buf_out.bytes_remain = 0;
    decoder->buf_out.bytes_written = 0;
    return ESP_OK;
}

audio_element_handle_t wav_decoder_init(struct wav_decoder_cfg *config)
{
    OS_LOGV(TAG, "Init wav decoder");

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy     = wav_decoder_destroy;
    cfg.open        = wav_decoder_open;
    cfg.close       = wav_decoder_close;
    cfg.process     = wav_decoder_process;
    cfg.seek        = wav_decoder_seek;
    cfg.buffer_len  = 0;
    cfg.task_stack  = config->task_stack;
    cfg.task_prio   = config->task_prio;
    if (cfg.task_stack == 0)
        cfg.task_stack = WAV_DECODER_TASK_STACK;
    cfg.tag = "wav_decoder";

    wav_decoder_handle_t decoder = audio_calloc(1, sizeof(struct wav_decoder));
    if (decoder == NULL)
        return NULL;

    decoder->prefered_frames = config->wav_info->sampleRate*WAV_DECODER_PREFERED_PEROID_MS/1000;
    decoder->buf_in.size = decoder->prefered_frames * config->wav_info->blockAlign;
    decoder->buf_out.size = decoder->prefered_frames * config->wav_info->blockAlign;
    decoder->buf_in.data = audio_malloc(decoder->buf_in.size);
    decoder->buf_out.data = audio_malloc(decoder->buf_out.size);
    AUDIO_MEM_CHECK(TAG, decoder->buf_in.data && decoder->buf_out.data, goto wav_init_error);

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto wav_init_error);
    decoder->el = el;
    decoder->block_align = config->wav_info->blockAlign;
    decoder->wav_info = config->wav_info;
    audio_element_setdata(el, decoder);

    audio_element_info_t info = { 0 };
    memset(&info, 0x0, sizeof(info));
    audio_element_setinfo(el, &info);

    audio_element_set_input_timeout(el, WAV_DECODER_INPUT_TIMEOUT_MAX);
    return el;

wav_init_error:
    if (decoder->buf_in.data)
        audio_free(decoder->buf_in.data);
    if (decoder->buf_out.data)
        audio_free(decoder->buf_out.data);
    audio_free(decoder);
    return NULL;
}
