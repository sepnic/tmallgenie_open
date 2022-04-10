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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "cutils/ringbuf.h"

#define LOG_TAG "ringbuf"

struct ringbuf {
    char *p_o;                   /**< Original pointer */
    char *volatile p_r;          /**< Read pointer */
    char *volatile p_w;          /**< Write pointer */
    int  fill_cnt;               /**< Number of filled slots */
    int  threshold_cnt;          /**< Number of threshold slots */
    int  size;                   /**< Buffer size */
    os_cond can_read;
    os_cond can_write;
    os_mutex lock;
    bool abort_read;
    bool abort_write;
    bool is_done_write;          /**< To signal that we are done writing */
    bool unblock_reader_flag;    /**< To unblock instantly from rb_read */
    bool is_reach_threshold;
};

ringbuf_handle rb_create(int size)
{
    ringbuf_handle rb;
    char *buf = NULL;
    bool _success =
        (
            (rb             = OS_CALLOC(1, sizeof(struct ringbuf))) &&
            (buf            = OS_CALLOC(1, size)) &&
            (rb->lock       = os_mutex_create()) &&
            (rb->can_read   = os_cond_create()) &&
            (rb->can_write  = os_cond_create())
        );

    if (!_success) {
        rb_destroy(rb);
        return NULL;
    }

    rb->p_o = rb->p_r = rb->p_w = buf;
    rb->size = size;
    rb->is_done_write = false;
    rb->unblock_reader_flag = false;
    rb->abort_read = false;
    rb->abort_write = false;
    return rb;
}

void rb_destroy(ringbuf_handle rb)
{
    if (rb == NULL)
        return;
    if (rb->p_o)
        OS_FREE(rb->p_o);
    if (rb->can_read)
        os_cond_destroy(rb->can_read);
    if (rb->can_write)
        os_cond_destroy(rb->can_write);
    if (rb->lock)
        os_mutex_destroy(rb->lock);
    OS_FREE(rb);
}

void rb_reset(ringbuf_handle rb)
{
    os_mutex_lock(rb->lock);
    rb->p_r = rb->p_w = rb->p_o;
    rb->fill_cnt = 0;
    rb->is_done_write = false;
    rb->unblock_reader_flag = false;
    rb->abort_read = false;
    rb->abort_write = false;
    os_cond_signal(rb->can_write);
    os_mutex_unlock(rb->lock);
}

int rb_bytes_available(ringbuf_handle rb)
{
    return (rb->size - rb->fill_cnt);
}

int rb_bytes_filled(ringbuf_handle rb)
{
    return rb->fill_cnt;
}

int rb_read(ringbuf_handle rb, char *buf, int buf_len, unsigned int timeout_ms)
{
    int read_size = 0;
    int total_read_size = 0;
    int ret_val = 0;

    //take buffer lock
    os_mutex_lock(rb->lock);

    while (buf_len > 0) {
        if (rb->fill_cnt < buf_len) {
            read_size = rb->fill_cnt;
            /**
             * When non-multiple of 4(word size) bytes are written to I2S, there is noise.
             * Below is the kind of workaround to read only in multiple of 4. Avoids noise when rb is read in small chunks.
             * Note that, when we have buf_len bytes available in rb, we still read those irrespective of if it's multiple of 4.
             */
            //read_size = read_size & 0xfffffffc;
            //if ((read_size == 0) && rb->is_done_write) {
            //    read_size = rb->fill_cnt;
            //}
        } else {
            read_size = buf_len;
        }

        if (read_size == 0 || !rb->is_reach_threshold) {
            if (rb->is_done_write) {
                ret_val = RB_DONE;
                goto read_err;
            }
            if (rb->abort_read) {
                ret_val = RB_ABORT;
                goto read_err;
            }
            if (rb->unblock_reader_flag) {
                //reader_unblock is nothing but forced timeout
                ret_val = RB_TIMEOUT;
                goto read_err;
            }
            os_cond_signal(rb->can_write);
            //wait till some data available to read
            if (timeout_ms == 0)
                ret_val = os_cond_wait(rb->can_read, rb->lock);
            else
                ret_val = os_cond_timedwait(rb->can_read, rb->lock, timeout_ms*1000);
            if (ret_val != 0) {
                ret_val = RB_TIMEOUT;
                goto read_err;
            }
            continue;
        }

        if ((rb->p_r + read_size) > (rb->p_o + rb->size)) {
            int rlen1 = rb->p_o + rb->size - rb->p_r;
            int rlen2 = read_size - rlen1;
            memcpy(buf, rb->p_r, rlen1);
            memcpy(buf + rlen1, rb->p_o, rlen2);
            rb->p_r = rb->p_o + rlen2;
        } else {
            memcpy(buf, rb->p_r, read_size);
            rb->p_r = rb->p_r + read_size;
        }

        buf_len -= read_size;
        rb->fill_cnt -= read_size;
        total_read_size += read_size;
        buf += read_size;
    }

read_err:
    if (total_read_size > 0) {
        os_cond_signal(rb->can_write);
    }
    os_mutex_unlock(rb->lock);
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_read_size = ret_val;
    }
    return total_read_size > 0 ? total_read_size : ret_val;
}

int rb_write(ringbuf_handle rb, char *buf, int buf_len, unsigned int timeout_ms)
{
    int write_size = 0;
    int total_write_size = 0;
    int ret_val = 0;

    //take buffer lock
    os_mutex_lock(rb->lock);

    while (buf_len > 0) {
        write_size = rb_bytes_available(rb);
        if (buf_len < write_size) {
            write_size = buf_len;
        }

        if (write_size == 0) {
            if (rb->is_done_write) {
                ret_val = RB_DONE;
                rb->is_reach_threshold = true;
                goto write_err;
            }
            if (rb->abort_write) {
                ret_val = RB_ABORT;
                rb->is_reach_threshold = true;
                goto write_err;
            }
            os_cond_signal(rb->can_read);
            //wait till we have some empty space to write
            if (timeout_ms == 0)
                ret_val = os_cond_wait(rb->can_write, rb->lock);
            else
                ret_val = os_cond_timedwait(rb->can_write, rb->lock, timeout_ms*1000);
            if (ret_val != 0) {
                ret_val = RB_TIMEOUT;
                goto write_err;
            }
            continue;
        }

        if ((rb->p_w + write_size) > (rb->p_o + rb->size)) {
            int wlen1 = rb->p_o + rb->size - rb->p_w;
            int wlen2 = write_size - wlen1;
            memcpy(rb->p_w, buf, wlen1);
            memcpy(rb->p_o, buf + wlen1, wlen2);
            rb->p_w = rb->p_o + wlen2;
        } else {
            memcpy(rb->p_w, buf, write_size);
            rb->p_w = rb->p_w + write_size;
        }

        buf_len -= write_size;
        rb->fill_cnt += write_size;
        total_write_size += write_size;
        buf += write_size;

        if (!rb->is_reach_threshold && rb->fill_cnt >= rb->threshold_cnt)
            rb->is_reach_threshold = true;
    }

write_err:
    if (rb->is_reach_threshold && total_write_size > 0) {
        os_cond_signal(rb->can_read);
    }
    os_mutex_unlock(rb->lock);
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_write_size = ret_val;
    }
    return total_write_size > 0 ? total_write_size : ret_val;
}

int rb_read_chunk(ringbuf_handle rb, char *buf, int size, unsigned int timeout_ms)
{
    int read_size = size;
    int total_read_size = 0;
    int ret_val = 0;

    //take buffer lock
    os_mutex_lock(rb->lock);

wait_filled:
    if (rb->fill_cnt < size) {
        if (rb->is_done_write)
            read_size = rb->fill_cnt;
        else
            read_size = 0;
    } else {
        read_size = size;
    }

    if (read_size == 0 || !rb->is_reach_threshold) {
        if (rb->is_done_write) {
            ret_val = RB_DONE;
            goto read_done;
        }
        if (rb->abort_read) {
            ret_val = RB_ABORT;
            goto read_done;
        }
        if (rb->unblock_reader_flag) {
            //reader_unblock is nothing but forced timeout
            ret_val = RB_TIMEOUT;
            goto read_done;
        }
        if (size > rb->size) {
            ret_val = RB_FAIL;
            goto read_done;
        }
        os_cond_signal(rb->can_write);
        //wait till some data available to read
        if (timeout_ms == 0)
            ret_val = os_cond_wait(rb->can_read, rb->lock);
        else
            ret_val = os_cond_timedwait(rb->can_read, rb->lock, timeout_ms*1000);
        if (ret_val != 0) {
            ret_val = RB_TIMEOUT;
            goto read_done;
        }
        goto wait_filled;
    }

    if ((rb->p_r + read_size) > (rb->p_o + rb->size)) {
        int rlen1 = rb->p_o + rb->size - rb->p_r;
        int rlen2 = read_size - rlen1;
        memcpy(buf, rb->p_r, rlen1);
        memcpy(buf + rlen1, rb->p_o, rlen2);
        rb->p_r = rb->p_o + rlen2;
    } else {
        memcpy(buf, rb->p_r, read_size);
        rb->p_r = rb->p_r + read_size;
    }
    rb->fill_cnt -= read_size;
    total_read_size += read_size;

read_done:
    if (total_read_size > 0) {
        os_cond_signal(rb->can_write);
    }
    os_mutex_unlock(rb->lock);
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_read_size = ret_val;
    }
    return total_read_size > 0 ? total_read_size : ret_val;
}

int rb_write_chunk(ringbuf_handle rb, char *buf, int size, unsigned int timeout_ms)
{
    int write_size = 0;
    int total_write_size = 0;
    int ret_val = 0;

    //take buffer lock
    os_mutex_lock(rb->lock);

wait_available:
    if (rb_bytes_available(rb) < size) {
        if (rb->is_done_write)
            write_size = rb_bytes_available(rb);
        else
            write_size = 0;
    } else {
        write_size = size;
    }

    if (write_size == 0) {
        if (rb->is_done_write) {
            ret_val = RB_DONE;
            rb->is_reach_threshold = true;
            goto write_done;
        }
        if (rb->abort_write) {
            ret_val = RB_ABORT;
            rb->is_reach_threshold = true;
            goto write_done;
        }
        if (size > rb->size) {
            ret_val = RB_FAIL;
            rb->is_reach_threshold = true;
            goto write_done;
        }
        os_cond_signal(rb->can_read);
        //wait till we have some empty space to write
        if (timeout_ms == 0)
            ret_val = os_cond_wait(rb->can_write, rb->lock);
        else
            ret_val = os_cond_timedwait(rb->can_write, rb->lock, timeout_ms*1000);
        if (ret_val != 0) {
            ret_val = RB_TIMEOUT;
            goto write_done;
        }
        goto wait_available;
    }

    if ((rb->p_w + write_size) > (rb->p_o + rb->size)) {
        int wlen1 = rb->p_o + rb->size - rb->p_w;
        int wlen2 = write_size - wlen1;
        memcpy(rb->p_w, buf, wlen1);
        memcpy(rb->p_o, buf + wlen1, wlen2);
        rb->p_w = rb->p_o + wlen2;
    } else {
        memcpy(rb->p_w, buf, write_size);
        rb->p_w = rb->p_w + write_size;
    }
    rb->fill_cnt += write_size;
    total_write_size += write_size;

    if (!rb->is_reach_threshold && rb->fill_cnt >= rb->threshold_cnt)
        rb->is_reach_threshold = true;

write_done:
    if (rb->is_reach_threshold && total_write_size > 0) {
        os_cond_signal(rb->can_read);
    }
    os_mutex_unlock(rb->lock);
    if ((ret_val == RB_FAIL) || (ret_val == RB_ABORT)) {
        total_write_size = ret_val;
    }
    return total_write_size > 0 ? total_write_size : ret_val;
}

static void rb_abort_read(ringbuf_handle rb)
{
    os_mutex_lock(rb->lock);
    rb->abort_read = true;
    os_cond_signal(rb->can_read);
    os_mutex_unlock(rb->lock);
}

static void rb_abort_write(ringbuf_handle rb)
{
    os_mutex_lock(rb->lock);
    rb->abort_write = true;
    os_cond_signal(rb->can_write);
    os_mutex_unlock(rb->lock);
}

void rb_abort(ringbuf_handle rb)
{
    rb_abort_read(rb);
    rb_abort_write(rb);
}

bool rb_is_full(ringbuf_handle rb)
{
    return (rb->size == rb->fill_cnt);
}

void rb_done_write(ringbuf_handle rb)
{
    os_mutex_lock(rb->lock);
    rb->is_done_write = true;
    os_cond_signal(rb->can_read);
    os_mutex_unlock(rb->lock);
}

void rb_done_read(ringbuf_handle rb)
{
    os_mutex_lock(rb->lock);
    rb->is_done_write = true;
    os_cond_signal(rb->can_write);
    os_mutex_unlock(rb->lock);
}

void rb_unblock_reader(ringbuf_handle rb)
{
    os_mutex_lock(rb->lock);
    rb->unblock_reader_flag = true;
    os_cond_signal(rb->can_read);
    os_mutex_unlock(rb->lock);
}

bool rb_is_done_write(ringbuf_handle rb)
{
    return rb->is_done_write;
}

int rb_get_size(ringbuf_handle rb)
{
    return rb->size;
}

void rb_set_threshold(ringbuf_handle rb, int threshold)
{
    os_mutex_lock(rb->lock);
    rb->threshold_cnt = threshold <= rb->size ? threshold : rb->size;
    os_mutex_unlock(rb->lock);
}

int rb_get_threshold(ringbuf_handle rb)
{
    return rb->threshold_cnt;
}

bool rb_reach_threshold(ringbuf_handle rb)
{
    return rb->is_reach_threshold;
}
