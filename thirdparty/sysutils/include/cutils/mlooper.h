/*
 * Copyright (c) 2018-2022 Qinglong<sysu.zqlong@gmail.com>
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

#ifndef __SYSUTILS_MLOOPER_H__
#define __SYSUTILS_MLOOPER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "osal/os_thread.h"
#include "cutil_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mlooper *mlooper_handle;
struct message;
typedef void (*message_cb)(struct message *msg);

enum message_state {
    MESSAGE_STATE_UNKNOWN = -1,
    MESSAGE_STATE_PENDING,
    MESSAGE_STATE_HANDLING,
    MESSAGE_STATE_HANDLED,
    MESSAGE_STATE_TIMEOUT,
    MESSAGE_STATE_DISCARDED,
};

/** 
 *  Please use message_obtain() to allocate message.
 *  Note that message as first member of structure message_node, we will
 *  cast a message to message_node pointer in contexts where it's known
 *  the message references a message_node.
 *
 * struct message *message_obtain(int what, int arg1, int arg2, void *data)
 * {
 *     struct message *msg = OS_CALLOC(1, sizeof(struct message_node));
 *     if (!msg) {
 *         OS_LOGE(LOG_TAG, "Failed to allocate message");
 *         return NULL;
 *     }
 *     msg->what = what;
 *     msg->arg1 = arg1;
 *     msg->arg2 = arg2;
 *     msg->data = data;
 *     return msg;
 * }
 */
struct message {
    int what;
    int arg1;
    int arg2;
    void *data;
    unsigned long timeout_ms; // 0: means never timeout
    enum message_state state;

    message_cb on_handle;
    message_cb on_free;
    message_cb on_discard;
    message_cb on_timeout;
};
struct message *message_obtain(int what, int arg1, int arg2, void *data);
// message_obtain_buffer_obtain:
//   Obtain a message and obtain a piece of buffer that points to msg->data,
//   user can access this buffer before message free.
//   Note that user can't free msg->data, buffer will be clear automatically
//   when message free.
struct message *message_obtain_buffer_obtain(int what, int arg1, int arg2, unsigned int size);
void message_set_handle_cb(struct message *msg, message_cb on_handle);
void message_set_free_cb(struct message *msg, message_cb on_free);
void message_set_discard_cb(struct message *msg, message_cb on_discard);
void message_set_timeout_cb(struct message *msg, message_cb on_timeout, unsigned long timeout_ms);

mlooper_handle mlooper_create(struct os_thread_attr *attr, message_cb on_handle, message_cb on_free);
void mlooper_destroy(mlooper_handle looper);

int mlooper_start(mlooper_handle looper);
void mlooper_stop(mlooper_handle looper);

unsigned int mlooper_message_count(mlooper_handle looper);
void mlooper_dump(mlooper_handle looper);

int mlooper_post_message(mlooper_handle looper, struct message *msg);
int mlooper_post_message_front(mlooper_handle looper, struct message *msg);
int mlooper_post_message_delay(mlooper_handle looper, struct message *msg, unsigned long msec);

// mlooper_remove_self_message:
//   Will check owner thread of the message, can't remove if not matched
int mlooper_remove_self_message(mlooper_handle looper, int what);
int mlooper_remove_self_message_if(mlooper_handle looper, bool (*on_match)(struct message *msg));
int mlooper_clear_self_message(mlooper_handle looper);

// mlooper_remove_message:
//   Remove the message if msg->what matched, won't check message owner
int mlooper_remove_message(mlooper_handle looper, int what);
int mlooper_remove_message_if(mlooper_handle looper, bool (*on_match)(struct message *msg));
int mlooper_clear_message(mlooper_handle looper);

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_MLOOPER_H__ */
