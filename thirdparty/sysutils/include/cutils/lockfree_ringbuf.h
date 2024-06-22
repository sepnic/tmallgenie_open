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

#ifndef __LOCKFREE_RINGBUF_H__
#define __LOCKFREE_RINGBUF_H__

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LOCKFREE_RINGBUF_NO_ERROR = 0,
    LOCKFREE_RINGBUF_ERROR_INVALID_PARAMETER = -1,
    LOCKFREE_RINGBUF_ERROR_INSUFFICIENT_WRITEABLE_BUFFER = -2,
};

void *lockfree_ringbuf_create(int size);

void lockfree_ringbuf_destroy(void *handle);

int lockfree_ringbuf_get_size(void *handle);

int lockfree_ringbuf_bytes_available(void *handle);

int lockfree_ringbuf_bytes_filled(void *handle);

void lockfree_ringbuf_unsafe_reset(void *handle);

int lockfree_ringbuf_unsafe_discard(void *handle, int len);

int lockfree_ringbuf_unsafe_overwrite(void *handle, char *buf, int len);

int lockfree_ringbuf_write(void *handle, char *buf, int len);

int lockfree_ringbuf_read(void *handle, char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif // __LOCKFREE_RINGBUF_H__
