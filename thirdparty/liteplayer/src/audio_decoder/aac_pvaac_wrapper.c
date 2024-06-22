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

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "esp_adf/audio_element.h"
#include "audio_decoder/aac_decoder.h"
#include "audio_decoder/m4a_decoder.h"
#include "pvaac/pvmp4audiodecoder_api.h"

#define TAG "[liteplayer]aac_decoder"

struct pvaac_wrapper {
    tPVMP4AudioDecoderExternal pvaac_config;
    void *pvaac_buffer;
};

static int aac_adts_read(aac_decoder_handle_t decoder)
{
    char *data = decoder->buf_in.data;
    int remain = decoder->buf_in.bytes_read;
    int want = AAC_DECODER_INPUT_BUFFER_SIZE - remain;

    if (remain != 0)
        memmove(data, &data[want], remain);

    int ret = audio_element_input(decoder->el, &data[remain], want);
    if (ret > 0) {
        decoder->buf_in.bytes_read += ret;
        return AEL_IO_OK;
    } else if (ret == AEL_IO_TIMEOUT) {
        return AEL_IO_TIMEOUT;
    } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
        decoder->buf_in.eof = true;
        return AEL_IO_DONE;
    } else {
        OS_LOGE(TAG, "AAC read fail, ret=%d", ret);
        return AEL_IO_FAIL;
    }
    return ret;
}

int aac_wrapper_run(aac_decoder_handle_t decoder)
{
    int ret = 0;
    int decode_fail_cnt = 0;
    struct pvaac_wrapper *wrap = (struct pvaac_wrapper *)decoder->handle;

fill_data:
    ret = aac_adts_read(decoder);
    if (ret != AEL_IO_OK) {
        if (decoder->buf_in.eof) {
            OS_LOGV(TAG, "AAC frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    wrap->pvaac_config.pInputBuffer = (unsigned char *)(decoder->buf_in.data);
    wrap->pvaac_config.inputBufferCurrentLength = decoder->buf_in.bytes_read;
    wrap->pvaac_config.inputBufferMaxLength = 0;
    wrap->pvaac_config.inputBufferUsedLength = 0;
    wrap->pvaac_config.remainderBits = 0;
    wrap->pvaac_config.pOutputBuffer = (short *)(decoder->buf_out.data);
    wrap->pvaac_config.pOutputBuffer_plus = &(wrap->pvaac_config.pOutputBuffer[2048]);
    wrap->pvaac_config.repositionFlag = false;
    ret = PVMP4AudioDecodeFrame(&wrap->pvaac_config, wrap->pvaac_buffer);
    if (ret == MP4AUDEC_INCOMPLETE_FRAME) {
        if (decoder->buf_in.eof)
            return AEL_IO_DONE;
        else
            goto fill_data;
    } else if (ret != MP4AUDEC_SUCCESS) {
        OS_LOGE(TAG, "AACDecode error[%d]", ret);
        if(decode_fail_cnt++ >= 4)
            return AEL_PROCESS_FAIL;
        goto fill_data;
    }

    decoder->buf_in.bytes_read -= wrap->pvaac_config.inputBufferUsedLength;
    decoder->buf_out.bytes_remain =
        wrap->pvaac_config.frameLength * sizeof(short) * wrap->pvaac_config.desiredChannels;

    if (!decoder->parsed_header) {
        audio_element_info_t info = {0};
        info.samplerate = wrap->pvaac_config.samplingRate;
        info.channels   = wrap->pvaac_config.desiredChannels;
        info.bits       = 16;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);

        OS_LOGV(TAG,"Found aac header: SR=%d, CH=%d, BITS=%d", info.samplerate, info.channels, info.bits);
        decoder->parsed_header = true;
    }
    return 0;
}

int aac_wrapper_init(aac_decoder_handle_t decoder)
{
    struct pvaac_wrapper *wrap = audio_calloc(1, sizeof(struct pvaac_wrapper));
    if (wrap == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for pvaac decoder");
        return -1;
    }

    wrap->pvaac_config.outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
#if defined(LITEPLAYER_CONFIG_AAC_PLUS)
    wrap->pvaac_config.aacPlusEnabled = 1;
#endif
    // The software decoder doesn't properly support mono output on
    // AACplus files. Always output stereo.
    wrap->pvaac_config.desiredChannels = 2;

    uint32_t memRequirements = PVMP4AudioDecoderGetMemRequirements();
    wrap->pvaac_buffer = audio_malloc(memRequirements);
    if (wrap->pvaac_buffer == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for pvaac decoder");
        audio_free(wrap);
        return -1;
    }
    if (PVMP4AudioDecoderInitLibrary(&wrap->pvaac_config, wrap->pvaac_buffer) != MP4AUDEC_SUCCESS) {
        OS_LOGE(TAG, "Failed to init library for pvaac decoder");
        audio_free(wrap->pvaac_buffer);
        audio_free(wrap);
        return -1;
    }

    decoder->handle = (void *)wrap;
    return 0;
}

void aac_wrapper_deinit(aac_decoder_handle_t decoder)
{
    struct pvaac_wrapper *wrap = (struct pvaac_wrapper *)decoder->handle;
    if (wrap == NULL) return;

    audio_free(wrap->pvaac_buffer);
    audio_free(wrap);
}

static int m4a_mdat_read(m4a_decoder_handle_t decoder)
{
    unsigned int stsz_entries = decoder->m4a_info->stsz_samplesize_entries;
    unsigned int stsz_current = decoder->m4a_info->stsz_samplesize_index;
    struct aac_buf_in *in = &decoder->buf_in;
    int ret = AEL_IO_OK;

    if (stsz_current >= stsz_entries) {
        in->eof = true;
        return AEL_IO_DONE;
    }
    in->bytes_want = decoder->m4a_info->stsz_samplesize[stsz_current];
    in->bytes_read = 0;

    ret = audio_element_input_chunk(decoder->el, in->data, in->bytes_want);
    if (ret == in->bytes_want) {
        in->bytes_read += ret;
        goto read_done;
    } else if (ret == AEL_IO_OK || ret == AEL_IO_DONE || ret == AEL_IO_ABORT) {
        in->eof = true;
        return AEL_IO_DONE;
    } else if (ret < 0) {
        OS_LOGW(TAG, "Read chunk error: %d/%d", ret, in->bytes_want);
        return ret;
    } else {
        OS_LOGW(TAG, "Read chunk insufficient: %d/%d, AEL_IO_DONE", ret, in->bytes_want);
        in->eof = true;
        return AEL_IO_DONE;
    }

read_done:
    decoder->m4a_info->stsz_samplesize_index++;
    return AEL_IO_OK;
}

int m4a_wrapper_run(m4a_decoder_handle_t decoder)
{
    int ret = 0;
    struct pvaac_wrapper *wrap = (struct pvaac_wrapper *)decoder->handle;

    ret = m4a_mdat_read(decoder);
    if (ret != AEL_IO_OK) {
        if (decoder->buf_in.eof) {
            OS_LOGV(TAG, "M4A frame end");
            ret = AEL_IO_DONE;
        }
        return ret;
    }

    wrap->pvaac_config.pInputBuffer = (unsigned char *)(decoder->buf_in.data);
    wrap->pvaac_config.inputBufferCurrentLength = decoder->buf_in.bytes_read;
    wrap->pvaac_config.inputBufferMaxLength = 0;
    wrap->pvaac_config.inputBufferUsedLength = 0;
    wrap->pvaac_config.remainderBits = 0;
    wrap->pvaac_config.pOutputBuffer = (short *)(decoder->buf_out.data);
    wrap->pvaac_config.pOutputBuffer_plus = &(wrap->pvaac_config.pOutputBuffer[2048]);
    wrap->pvaac_config.repositionFlag = false;
    ret = PVMP4AudioDecodeFrame(&wrap->pvaac_config, wrap->pvaac_buffer);
    if (ret != MP4AUDEC_SUCCESS) {
        OS_LOGE(TAG, "AACDecode error[%d]", ret);
        return AEL_PROCESS_FAIL;
    }

    decoder->buf_out.bytes_remain =
        wrap->pvaac_config.frameLength * sizeof(short) * wrap->pvaac_config.desiredChannels;

    if (!decoder->parsed_header) {
        audio_element_info_t info = {0};
        info.samplerate = wrap->pvaac_config.samplingRate;
        info.channels   = wrap->pvaac_config.desiredChannels;
        info.bits       = 16;
        audio_element_setinfo(decoder->el, &info);
        audio_element_report_info(decoder->el);

        OS_LOGV(TAG,"Found aac header: SR=%d, CH=%d, BITS=%d", info.samplerate, info.channels, info.bits);
        decoder->parsed_header = true;
    }
    return 0;
}

int m4a_wrapper_init(m4a_decoder_handle_t decoder)
{
    struct pvaac_wrapper *wrap = audio_calloc(1, sizeof(struct pvaac_wrapper));
    if (wrap == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for pvaac decoder");
        return -1;
    }

    wrap->pvaac_config.outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
#if defined(LITEPLAYER_CONFIG_AAC_PLUS)
    wrap->pvaac_config.aacPlusEnabled = 1;
#endif
    // The software decoder doesn't properly support mono output on
    // AACplus files. Always output stereo.
    wrap->pvaac_config.desiredChannels = 2;

    uint32_t memRequirements = PVMP4AudioDecoderGetMemRequirements();
    wrap->pvaac_buffer = audio_malloc(memRequirements);
    if (wrap->pvaac_buffer == NULL) {
        OS_LOGE(TAG, "Failed to allocate memory for pvaac decoder");
        audio_free(wrap);
        return -1;
    }
    if (PVMP4AudioDecoderInitLibrary(&wrap->pvaac_config, wrap->pvaac_buffer) != MP4AUDEC_SUCCESS) {
        OS_LOGE(TAG, "Failed to init library for pvaac decoder");
        audio_free(wrap->pvaac_buffer);
        audio_free(wrap);
        return -1;
    }
    wrap->pvaac_config.pInputBuffer = decoder->m4a_info->asc.buf;
    wrap->pvaac_config.inputBufferCurrentLength = decoder->m4a_info->asc.size;
    wrap->pvaac_config.inputBufferMaxLength = 0;
    if (PVMP4AudioDecoderConfig(&wrap->pvaac_config, wrap->pvaac_buffer) != MP4AUDEC_SUCCESS) {
        OS_LOGE(TAG, "Failed to decode asc config");
        audio_free(wrap->pvaac_buffer);
        audio_free(wrap);
        return -1;
    }

    decoder->handle = (void *)wrap;
    return 0;
}

void m4a_wrapper_deinit(m4a_decoder_handle_t decoder)
{
    struct pvaac_wrapper *wrap = (struct pvaac_wrapper *)decoder->handle;
    if (wrap == NULL) return;

    audio_free(wrap->pvaac_buffer);
    audio_free(wrap);
}
