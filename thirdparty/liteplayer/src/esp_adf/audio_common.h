/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

// Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com>

#ifndef _AUDIO_COMMON_H_
#define _AUDIO_COMMON_H_

#include <stdio.h>

#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_err_t;

/* Definitions for error constants. */
#define ESP_OK          0       /*!< esp_err_t value indicating success (no error) */
#define ESP_FAIL        -1      /*!< Generic esp_err_t code indicating failure */
#define ESP_ERR_NO_MEM              0x101   /*!< Out of memory */
#define ESP_ERR_INVALID_ARG         0x102   /*!< Invalid argument */
#define ESP_ERR_INVALID_STATE       0x103   /*!< Invalid state */
#define ESP_ERR_INVALID_SIZE        0x104   /*!< Invalid size */
#define ESP_ERR_NOT_FOUND           0x105   /*!< Requested resource not found */
#define ESP_ERR_NOT_SUPPORTED       0x106   /*!< Operation or feature not supported */
#define ESP_ERR_TIMEOUT             0x107   /*!< Operation timed out */
#define ESP_ERR_INVALID_RESPONSE    0x108   /*!< Received response was invalid */
#define ESP_ERR_INVALID_CRC         0x109   /*!< CRC or checksum was invalid */
#define ESP_ERR_INVALID_VERSION     0x10A   /*!< Version was invalid */
#define ESP_ERR_INVALID_MAC         0x10B   /*!< MAC address was invalid */
#define ESP_ERR_WIFI_BASE           0x3000  /*!< Starting number of WiFi error codes */
#define ESP_ERR_MESH_BASE           0x4000  /*!< Starting number of MESH error codes */

#define AUDIO_MAX_DELAY         0x7FFFFFFF

#define ELEMENT_SUB_TYPE_OFFSET 16

typedef enum {
    AUDIO_ELEMENT_TYPE_UNKNOW = 0x01<<ELEMENT_SUB_TYPE_OFFSET,
    AUDIO_ELEMENT_TYPE_ELEMENT= 0x01<<(ELEMENT_SUB_TYPE_OFFSET+1),
    AUDIO_ELEMENT_TYPE_PLAYER = 0x01<<(ELEMENT_SUB_TYPE_OFFSET+2),
    AUDIO_ELEMENT_TYPE_SERVICE = 0x01<<(ELEMENT_SUB_TYPE_OFFSET+3),
    AUDIO_ELEMENT_TYPE_PERIPH = 0x01<<(ELEMENT_SUB_TYPE_OFFSET+4),
} audio_element_type_t;

typedef enum {
    AUDIO_STREAM_NONE = 0,
    AUDIO_STREAM_READER,
    AUDIO_STREAM_WRITER
} audio_stream_type_t;

typedef enum {
    AUDIO_CODEC_TYPE_NONE = 0,
    AUDIO_CODEC_TYPE_DECODER,
    AUDIO_CODEC_TYPE_ENCODER
} audio_codec_type_t;

typedef enum {
    AUDIO_CODEC_NONE = 0,
    AUDIO_CODEC_WAV,
    AUDIO_CODEC_MP3,
    AUDIO_CODEC_AAC,
    AUDIO_CODEC_M4A,
    AUDIO_CODEC_OPUS,
    AUDIO_CODEC_FLAC,
} audio_codec_t;

#define AUDIO_CHECK(TAG, a, action, msg) if (!(a)) {              \
        OS_LOGE(TAG,"(%s:%d): %s", __FUNCTION__, __LINE__, msg); \
        action;                                                   \
    }
#define AUDIO_MEM_CHECK(TAG, a, action)  AUDIO_CHECK(TAG, a, action, "Memory exhausted")
#define AUDIO_NULL_CHECK(TAG, a, action) AUDIO_CHECK(TAG, a, action, "Got NULL Pointer")
#define AUDIO_ERR_CHECK(TAG, a, action) if (!(a)) {                \
        OS_LOGE(TAG,"(%s:%d): Error", __FUNCTION__, __LINE__);    \
        action;                                                    \
    }
#define AUDIO_ERROR(TAG, str) OS_LOGE(TAG, "(%s:%d): %s", __FUNCTION__, __LINE__, str)

#define audio_malloc(size)       OS_MALLOC(size)
#define audio_free(ptr)          OS_FREE(ptr)
#define audio_calloc(n, size)    OS_CALLOC(n, size)
#define audio_realloc(ptr, size) OS_REALLOC(ptr, size)
#define audio_strdup(str)        OS_STRDUP(str)

#ifdef __cplusplus
}
#endif

#endif
