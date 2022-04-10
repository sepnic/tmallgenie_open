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

/* Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com> */

#ifndef __SYSUTILS_RINGBUF_H__
#define __SYSUTILS_RINGBUF_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cutil_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RB_OK           ( 0)
#define RB_FAIL         (-1)
#define RB_DONE         (-2)
#define RB_ABORT        (-3)
#define RB_TIMEOUT      (-4)

typedef struct ringbuf *ringbuf_handle;

/**
 * @brief      Create ringbuffer
 *
 * @param[in]  size   Size of ringbuffer
 *
 * @return     ringbuf_handle
 */
ringbuf_handle rb_create(int size);

/**
 * @brief      Cleanup and free all memory created by ringbuf_handle
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_destroy(ringbuf_handle rb);

/**
 * @brief      Abort waiting until there is space for reading or writing of the ringbuffer
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_abort(ringbuf_handle rb);

/**
 * @brief      Reset ringbuffer, clear all values as initial state
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_reset(ringbuf_handle rb);

/**
 * @brief      Get total bytes available of Ringbuffer
 *
 * @param[in]  rb    The Ringbuffer handle
 *
 * @return     total bytes available
 */
int rb_bytes_available(ringbuf_handle rb);

/**
 * @brief      Get the number of bytes that have filled the ringbuffer
 *
 * @param[in]  rb    The Ringbuffer handle
 *
 * @return     The number of bytes that have filled the ringbuffer
 */
int rb_bytes_filled(ringbuf_handle rb);

/**
 * @brief      Get total size of Ringbuffer (in bytes)
 *
 * @param[in]  rb    The Ringbuffer handle
 *
 * @return     total size of Ringbuffer
 */
int rb_get_size(ringbuf_handle rb);

/**
 * @brief      Read from Ringbuffer to `buf` with len and wait `timeout_ms` milliseconds until enough bytes to read
 *             if the ringbuffer bytes available is less than `len`.
 *
 * @param[in]  rb             The Ringbuffer handle
 * @param      buf            The buffer pointer to read out data
 * @param[in]  len            The length request
 * @param[in]  timeout_ms     The time to wait, if zero, wait forever
 *
 * @return     Number of bytes read
 */
int rb_read(ringbuf_handle rb, char *buf, int len, unsigned int timeout_ms);

/**
 * @brief      Write to Ringbuffer from `buf` with `len` and wait `timeout_ms` milliseconds until enough space to write
 *             if the ringbuffer space available is less than `len`
 *
 * @param[in]  rb             The Ringbuffer handle
 * @param      buf            The buffer
 * @param[in]  len            The length
 * @param[in]  timeout_ms     The time to wait, if zero, wait forever
 *
 * @return     Number of bytes written
 */
int rb_write(ringbuf_handle rb, char *buf, int len, unsigned int timeout_ms);

/**
 * @brief      Read chunk from Ringbuffer to `buf` with len.
 *
 * @param[in]  rb             The Ringbuffer handle
 * @param      buf            The buffer pointer to read out data
 * @param[in]  size           The length request
 * @param[in]  timeout_ms     The time to wait, if zero, wait forever
 *
 * @return     Number of bytes read
 */
int rb_read_chunk(ringbuf_handle rb, char *buf, int size, unsigned int timeout_ms);

/**
 * @brief      Write chunk to Ringbuffer from `buf` with `len`
 *
 * @param[in]  rb             The Ringbuffer handle
 * @param      buf            The buffer
 * @param[in]  size           The length
 * @param[in]  timeout_ms     The time to wait, if zero, wait forever
 *
 * @return     Number of bytes written
 */
int rb_write_chunk(ringbuf_handle rb, char *buf, int size, unsigned int timeout_ms);

/**
 * @brief      Set status of writing to ringbuffer is done
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_done_write(ringbuf_handle rb);

/**
 * @brief      Set status of reading to ringbuffer is done
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_done_read(ringbuf_handle rb);

/**
 * @brief      Unblock from rb_read
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_unblock_reader(ringbuf_handle rb);

/**
 * @brief      Set reader threshold
 *
 * @param[in]  rb    The Ringbuffer handle
 */
void rb_set_threshold(ringbuf_handle rb, int threshold);

/**
 * @brief      Get reader threshold
 *
 * @param[in]  rb    The Ringbuffer handle
 */
int rb_get_threshold(ringbuf_handle rb);

/**
 * @brief      Check whether reader reach threshold
 *
 * @param[in]  rb    The Ringbuffer handle
 */
bool rb_reach_threshold(ringbuf_handle rb);

bool rb_is_full(ringbuf_handle rb);

bool rb_is_done_write(ringbuf_handle rb);

#ifdef __cplusplus
}
#endif

#endif // __SYSUTILS_RINGBUF_H__
