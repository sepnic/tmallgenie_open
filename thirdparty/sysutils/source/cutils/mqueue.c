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

#include <string.h>
#include <stdbool.h>
#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "cutils/list.h"
#include "cutils/mqueue.h"

#define LOG_TAG "mqueue"

struct mqueue {
    char *head;  /**< Head pointer */
    char *read;  /**< Read pointer */
    char *write; /**< Write pointer */
    char *tail;  /**< Tail pointer */

    unsigned int element_size; /**< Size of msg element */
    unsigned int element_count;/**< Number of total slots */
    unsigned int filled_count; /**< Number of filled slots */

    os_cond can_read;
    os_cond can_write;
    os_mutex lock;

    bool is_set;            /**< Whether is queue-set */
    struct listnode list;   /**< List node for queue, list head for queue-set */
    mqset_handle parent_set; /**< Parent queue-set pointer */
};

mq_handle mqueue_create(unsigned int msg_size, unsigned int msg_count)
{
    struct mqueue *queue = OS_CALLOC(1, sizeof(struct mqueue));
    if (queue == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate queue");
        return NULL;
    }

    queue->lock = os_mutex_create();
    if (queue->lock == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create queue mutex");
        goto error;
    }

    queue->can_read = os_cond_create();
    if (queue->can_read == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create queue cond");
        goto error;
    }

    queue->can_write = os_cond_create();
    if (queue->can_write == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create queue cond");
        goto error;
    }

    queue->head = OS_CALLOC(msg_count, msg_size);
    if (queue->head == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate queue buffer");
        goto error;
    }

    queue->read = queue->head;
    queue->write = queue->head;
    queue->tail = queue->head + msg_size * msg_count;
    queue->element_size = msg_size;
    queue->element_count = msg_count;
    queue->filled_count = 0;
    queue->parent_set = NULL;
    queue->is_set = false;
    list_init(&queue->list);

    return queue;

error:
    if (queue->head != NULL)
        OS_FREE(queue->head);
    if (queue->can_write != NULL)
        os_cond_destroy(queue->can_write);
    if (queue->can_read != NULL)
        os_cond_destroy(queue->can_read);
    if (queue->lock != NULL)
        os_mutex_destroy(queue->lock);
    OS_FREE(queue);
    return NULL;
}

int mqueue_destroy(mq_handle queue)
{
    if (mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't destroy queue that isn't empty");
        return -1;
    }

    if (queue->parent_set != NULL) {
        os_mutex_lock(queue->parent_set->lock);
        list_remove(&queue->list);
        os_mutex_unlock(queue->parent_set->lock);
    }

    OS_FREE(queue->head);
    os_cond_destroy(queue->can_write);
    os_cond_destroy(queue->can_read);
    os_mutex_destroy(queue->lock);
    OS_FREE(queue);
    return 0;
}

int mqueue_reset(mq_handle queue)
{
    os_mutex_lock(queue->lock);

    if (queue->parent_set != NULL && mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't reset queue that parent set isn't empty");
        os_mutex_unlock(queue->lock);
        return -1;
    }

    queue->read = queue->head;
    queue->write = queue->head;
    queue->filled_count = 0;
    os_cond_signal(queue->can_write);

    os_mutex_unlock(queue->lock);
    return 0;
}

static void mqueue_copy_msg(mq_handle queue, char *msg)
{
    memcpy(queue->write, msg, queue->element_size);
    queue->filled_count++;
    queue->write += queue->element_size;
    if (queue->write >= queue->tail)
        queue->write = queue->head;
}

int mqueue_send(mq_handle queue, char *msg, unsigned long timeout_ms)
{
    int ret = -1;

    os_mutex_lock(queue->lock);

    if (mqueue_count_available(queue) == 0) {
        if (timeout_ms == 0)
            goto write_done;
        else
            os_cond_timedwait(queue->can_write, queue->lock, timeout_ms*1000);
    }

    if (mqueue_count_available(queue) > 0) {
        ret = 0;

        if (queue->parent_set) {
            mqset_handle set = queue->parent_set;

            os_mutex_lock(set->lock);
            if (mqueue_count_available(set) > 0) {
                mqueue_copy_msg(queue, msg);
                mqueue_copy_msg(set, (char *)&queue);
                os_cond_signal(set->can_read);
            } else {
                OS_LOGE(LOG_TAG, "Failed to send msg to queue that parent set is full");
                ret = -1;
            }
            os_mutex_unlock(set->lock);
        } else {
            mqueue_copy_msg(queue, msg);
        }
    }

write_done:
    if (ret == 0)
        os_cond_signal(queue->can_read);
    else
        OS_LOGE(LOG_TAG, "Failed to send msg to full queue");

    os_mutex_unlock(queue->lock);
    return ret;
}

int mqueue_receive(mq_handle queue, char *msg, unsigned long timeout_ms)
{
    int ret = -1;

    os_mutex_lock(queue->lock);

    if (mqueue_count_filled(queue) == 0) {
        if (timeout_ms == 0)
            goto read_done;
        else
            os_cond_timedwait(queue->can_read, queue->lock, timeout_ms*1000);
    }

    if (mqueue_count_filled(queue) > 0) {
        memcpy(msg, queue->read, queue->element_size);
        queue->filled_count--;
        queue->read += queue->element_size;
        if (queue->read >= queue->tail)
            queue->read = queue->head;
        ret = 0;
    }

read_done:
    if (ret == 0)
        os_cond_signal(queue->can_write);

    os_mutex_unlock(queue->lock);
    return ret;
}

unsigned int mqueue_count_available(mq_handle queue)
{
    return queue->element_count - queue->filled_count;
}

unsigned int mqueue_count_filled(mq_handle queue)
{
    return queue->filled_count;
}

mqset_handle mqueueset_create(unsigned int msg_count)
{
    struct mqueue *queue = mqueue_create(sizeof(mq_handle), msg_count);
    if (queue == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate queue set");
        return NULL;
    }

    queue->is_set = true;
    return queue;
}

int mqueueset_destroy(mqset_handle set)
{
    struct mqueue *queue = NULL;
    struct listnode *item, *tmp;

    list_for_each_safe(item, tmp, &set->list) {
        queue = listnode_to_item(item, struct mqueue, list);
        list_remove(&queue->list);

        os_mutex_lock(queue->lock);
        queue->parent_set = NULL;
        os_mutex_unlock(queue->lock);
    }

    return mqueue_destroy(set);
}

int mqueueset_add_queue(mqset_handle set, mq_handle queue)
{
    if (queue->parent_set != NULL) {
        OS_LOGE(LOG_TAG, "Can't add queue to more than one set");
        return -1;
    }

    if (queue->is_set) {
        OS_LOGE(LOG_TAG, "Can't add a set to another set");
        return -1;
    }

    os_mutex_lock(queue->lock);

    if (mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't add queue that isn't empty");
        os_mutex_unlock(queue->lock);
        return -1;
    }

    queue->parent_set = set;

    os_mutex_lock(set->lock);
    list_add_tail(&set->list, &queue->list);
    os_mutex_unlock(set->lock);

    os_mutex_unlock(queue->lock);
    return 0;
}

int mqueueset_remove_queue(mqset_handle set, mq_handle queue)
{
    if (queue->parent_set != set) {
        OS_LOGE(LOG_TAG, "Can't remove queue that isn't a member of the set");
        return -1;
    }

    os_mutex_lock(queue->lock);

    if (mqueue_count_filled(queue) > 0) {
        OS_LOGE(LOG_TAG, "Can't remove queue that isn't empty");
        os_mutex_unlock(queue->lock);
        return -1;
    }

    queue->parent_set = NULL;

    os_mutex_lock(set->lock);
    list_remove(&queue->list);
    os_mutex_unlock(set->lock);

    os_mutex_unlock(queue->lock);

    return 0;
}

mq_handle mqueueset_select_queue(mqset_handle set, unsigned long timeout_ms)
{
    mq_handle queue = NULL;
    (void) mqueue_receive(set, (char *)&queue, timeout_ms);
    return queue;
}
