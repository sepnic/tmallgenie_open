/*
 * Copyright (C) 2023-, Qinglong<sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "cutils/lockfree_ringbuf.h"

#if defined(__STDC_NO_ATOMICS__)
// IMPORTANT:
//   IF ATOMIC NOT SUPPORTED, DON'T USE THIS LOCKFREE-RINGBUF WHEN READ
//   THREAD AND WIRTE THREAD ARE DIFFERENT.
//   When read and write are in a single thread, you can use it without
//   any risk.
#warning __STDC_NO_ATOMICS__
#define ATOMIC_DECLARE(obj)         int obj
#define ATOMIC_INIT(obj, val)       obj = val
#define ATOMIC_LOAD(obj)            obj
#define ATOMIC_STORE(obj, val)      obj = val
#define ATOMIC_FETCH_ADD(obj, val)  obj += val
#define ATOMIC_FETCH_SUB(obj, val)  obj -= val

#else
#include <stdatomic.h>
#define ATOMIC_DECLARE(obj)         atomic_int obj
#define ATOMIC_INIT(obj, val)       atomic_init(&(obj), val)
#define ATOMIC_LOAD(obj)            atomic_load(&(obj))
#define ATOMIC_STORE(obj, val)      atomic_store(&(obj), val)
#define ATOMIC_FETCH_ADD(obj, val)  atomic_fetch_add(&(obj), val)
#define ATOMIC_FETCH_SUB(obj, val)  atomic_fetch_sub(&(obj), val)
#endif

struct lockfree_ringbuf {
    char *p_o;                   /**< Original pointer */
    char *volatile p_r;          /**< Read pointer */
    char *volatile p_w;          /**< Write pointer */
    ATOMIC_DECLARE(filled_size); /**< Number of filled slots */
    int  buffer_size;            /**< Buffer buffer_size */
};

void *lockfree_ringbuf_create(int size)
{
    if (size <= 0)
        return NULL;
    struct lockfree_ringbuf *rb = OS_MALLOC(sizeof(struct lockfree_ringbuf));
    if (rb == NULL)
        return NULL;
    rb->buffer_size = size;
    ATOMIC_INIT(rb->filled_size, 0);
    rb->p_o = rb->p_r = rb->p_w = OS_MALLOC(size);
    if (rb->p_o == NULL) {
        OS_FREE(rb);
        rb = NULL;
    }
    return rb;
}

void lockfree_ringbuf_destroy(void *handle)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL)
        return;
    if (rb->p_o != NULL)
        OS_FREE(rb->p_o);
    OS_FREE(rb);
}

int lockfree_ringbuf_get_size(void *handle)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    return rb->buffer_size;
}

int lockfree_ringbuf_bytes_available(void *handle)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    return (rb->buffer_size - ATOMIC_LOAD(rb->filled_size));
}

int lockfree_ringbuf_bytes_filled(void *handle)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    return ATOMIC_LOAD(rb->filled_size);
}

void lockfree_ringbuf_unsafe_reset(void *handle)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL)
        return;
    rb->p_r = rb->p_w = rb->p_o;
    ATOMIC_STORE(rb->filled_size, 0);
}

int lockfree_ringbuf_unsafe_discard(void *handle, int len)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL || len <= 0)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    int filled = ATOMIC_LOAD(rb->filled_size);
    len = (len > filled) ? filled : len;
    if (len > 0) {
        if ((rb->p_r + len) > (rb->p_o + rb->buffer_size)) {
            int rlen1 = rb->p_o + rb->buffer_size - rb->p_r;
            int rlen2 = len - rlen1;
            rb->p_r = rb->p_o + rlen2;
        } else {
            rb->p_r = rb->p_r + len;
        }
        ATOMIC_FETCH_SUB(rb->filled_size, len);
    }
    return len;
}

int lockfree_ringbuf_unsafe_overwrite(void *handle, char *buf, int len)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL || buf == NULL || len <= 0)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    if (len <= rb->buffer_size) {
        int available = rb->buffer_size - ATOMIC_LOAD(rb->filled_size);
        if (len > available)
            lockfree_ringbuf_unsafe_discard(rb, len-available);
        if ((rb->p_w + len) > (rb->p_o + rb->buffer_size)) {
            int wlen1 = rb->p_o + rb->buffer_size - rb->p_w;
            int wlen2 = len - wlen1;
            memcpy(rb->p_w, buf, wlen1);
            memcpy(rb->p_o, buf + wlen1, wlen2);
            rb->p_w = rb->p_o + wlen2;
        } else {
            memcpy(rb->p_w, buf, len);
            rb->p_w = rb->p_w + len;
        }
        ATOMIC_FETCH_ADD(rb->filled_size, len);
    } else {
        buf = buf + len - rb->buffer_size;
        memcpy(rb->p_o, buf, rb->buffer_size);
        rb->p_w = rb->p_o + rb->buffer_size;
        rb->p_r = rb->p_o;
        ATOMIC_STORE(rb->filled_size, rb->buffer_size);
    }
    return len;
}

int lockfree_ringbuf_write(void *handle, char *buf, int len)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL || buf == NULL || len <= 0)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    if (len > (rb->buffer_size-ATOMIC_LOAD(rb->filled_size)))
        return LOCKFREE_RINGBUF_ERROR_INSUFFICIENT_WRITEABLE_BUFFER;
    if ((rb->p_w + len) > (rb->p_o + rb->buffer_size)) {
        int wlen1 = rb->p_o + rb->buffer_size - rb->p_w;
        int wlen2 = len - wlen1;
        memcpy(rb->p_w, buf, wlen1);
        memcpy(rb->p_o, buf + wlen1, wlen2);
        rb->p_w = rb->p_o + wlen2;
    } else {
        memcpy(rb->p_w, buf, len);
        rb->p_w = rb->p_w + len;
    }
    ATOMIC_FETCH_ADD(rb->filled_size, len);
    return len;
}

int lockfree_ringbuf_read(void *handle, char *buf, int len)
{
    struct lockfree_ringbuf *rb = (struct lockfree_ringbuf *)handle;
    if (rb == NULL || buf == NULL || len <= 0)
        return LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER;
    int filled = ATOMIC_LOAD(rb->filled_size);
    len = (len > filled) ? filled : len;
    if (len > 0) {
        if ((rb->p_r + len) > (rb->p_o + rb->buffer_size)) {
            int rlen1 = rb->p_o + rb->buffer_size - rb->p_r;
            int rlen2 = len - rlen1;
            memcpy(buf, rb->p_r, rlen1);
            memcpy(buf + rlen1, rb->p_o, rlen2);
            rb->p_r = rb->p_o + rlen2;
        } else {
            memcpy(buf, rb->p_r, len);
            rb->p_r = rb->p_r + len;
        }
        ATOMIC_FETCH_SUB(rb->filled_size, len);
    }
    return len;
}
