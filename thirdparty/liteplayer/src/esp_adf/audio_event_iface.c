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

#include "cutils/log_helper.h"
#include "esp_adf/audio_event_iface.h"
#include "esp_adf/audio_common.h"

#define TAG  "[liteplayer]audio_event"


typedef struct audio_event_iface_item {
    STAILQ_ENTRY(audio_event_iface_item)    next;
    mq_handle                                queue;
    int                                     queue_size;
    int                                     mark_to_remove;
} audio_event_iface_item_t;

typedef STAILQ_HEAD(audio_event_iface_list, audio_event_iface_item) audio_event_iface_list_t;

/**
 * Audio event structure
 */
struct audio_event_iface {
    mq_handle                   internal_queue;
    mq_handle                   external_queue;
    mqset_handle                queue_set;
    int                         internal_queue_size;
    int                         external_queue_size;
    int                         queue_set_size;
    audio_event_iface_list_t    listening_queues;
    void                        *context;
    on_event_iface_func         on_cmd;
    unsigned int                timeout_ms;
    int                         type;
};

audio_event_iface_handle_t audio_event_iface_init(audio_event_iface_cfg_t *config)
{
    audio_event_iface_handle_t evt = audio_calloc(1, sizeof(struct audio_event_iface));
    AUDIO_MEM_CHECK(TAG, evt, return NULL);
    evt->queue_set_size   = config->queue_set_size;
    evt->internal_queue_size = config->internal_queue_size;
    evt->external_queue_size = config->external_queue_size;
    evt->context = config->context;
    evt->on_cmd = config->on_cmd;
    evt->type = config->type;
    if (evt->queue_set_size) {
        evt->queue_set = mqueueset_create(evt->queue_set_size);
    }
    if (evt->internal_queue_size) {
        evt->internal_queue = mqueue_create(sizeof(audio_event_iface_msg_t), evt->internal_queue_size);
        AUDIO_MEM_CHECK(TAG, evt->internal_queue, goto _event_iface_init_failed);
    }
    if (evt->external_queue_size) {
        evt->external_queue = mqueue_create(sizeof(audio_event_iface_msg_t), evt->external_queue_size);
        AUDIO_MEM_CHECK(TAG, evt->external_queue, goto _event_iface_init_failed);
    } else {
        OS_LOGD(TAG, "This emiiter have no queue set,%p", evt);
    }

    STAILQ_INIT(&evt->listening_queues);
    return evt;
_event_iface_init_failed:
    if (evt->internal_queue) {
        mqueue_destroy(evt->internal_queue);
    }
    if (evt->external_queue) {
        mqueue_destroy(evt->external_queue);
    }
    return NULL;
}

static esp_err_t audio_event_iface_cleanup_listener(audio_event_iface_handle_t listen)
{
    audio_event_iface_item_t *item, *tmp;
    audio_event_iface_discard(listen);
    STAILQ_FOREACH_SAFE(item, &listen->listening_queues, next, tmp) {
        if (listen->queue_set && mqueueset_remove_queue(listen->queue_set, item->queue) != 0) {
            OS_LOGE(TAG, "Error remove listener");
            return ESP_FAIL;
        }
    }
    if (listen->queue_set) {
        mqueueset_destroy(listen->queue_set);
        listen->queue_set = NULL;
    }
    return ESP_OK;
}

static esp_err_t audio_event_iface_update_listener(audio_event_iface_handle_t listen)
{
    audio_event_iface_item_t *item;
    int queue_size = 0;
    STAILQ_FOREACH(item, &listen->listening_queues, next) {
        queue_size += item->queue_size;
    }
    if (queue_size) {
        listen->queue_set = mqueueset_create(queue_size);
    }
    STAILQ_FOREACH(item, &listen->listening_queues, next) {
        if (item->queue) {
            audio_event_iface_msg_t dummy;
            while (mqueue_receive(item->queue, (char *)&dummy, 0) == 0);
        }
        if (listen->queue_set && item->queue && mqueueset_add_queue(listen->queue_set, item->queue) != 0) {
            OS_LOGE(TAG, "Error add queue items to queue set");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t audio_event_iface_read(audio_event_iface_handle_t evt, audio_event_iface_msg_t *msg, unsigned int timeout_ms)
{
    if (evt->queue_set) {
        mq_handle active_queue;
        active_queue = mqueueset_select_queue(evt->queue_set, timeout_ms);
        if (active_queue) {
            if (mqueue_receive(active_queue, (char *)msg, 0) == 0) {
                return ESP_OK;
            }
        }
    }
    return ESP_FAIL;
}

esp_err_t audio_event_iface_destroy(audio_event_iface_handle_t evt)
{
    audio_event_iface_cleanup_listener(evt);
    audio_event_iface_item_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item, &evt->listening_queues, next, tmp) {
        STAILQ_REMOVE(&evt->listening_queues, item, audio_event_iface_item, next);
        audio_free(item);
    }
    if (evt->internal_queue) {
        audio_event_iface_set_cmd_waiting_timeout(evt, 0);
        mqueue_destroy(evt->internal_queue);
    }
    if (evt->external_queue) {
        mqueue_destroy(evt->external_queue);
    }
    if (evt->queue_set) {
        mqueueset_destroy(evt->queue_set);
    }
    audio_free(evt);
    return ESP_OK;
}

esp_err_t audio_event_iface_set_listener(audio_event_iface_handle_t evt, audio_event_iface_handle_t listener)
{
    if ((NULL == evt->external_queue)
        || (0 == evt->external_queue_size)) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_event_iface_item_t *item = audio_calloc(1, sizeof(audio_event_iface_item_t));
    AUDIO_MEM_CHECK(TAG, item, return ESP_ERR_NO_MEM);

    if (audio_event_iface_cleanup_listener(listener) != ESP_OK) {
        AUDIO_ERROR(TAG, "Error cleanup listener");
        return ESP_FAIL;
    }
    item->queue = evt->external_queue;
    item->queue_size = evt->external_queue_size;
    STAILQ_INSERT_TAIL(&listener->listening_queues, item, next);
    return audio_event_iface_update_listener(listener);
}

esp_err_t audio_event_iface_set_msg_listener(audio_event_iface_handle_t evt, audio_event_iface_handle_t listener)
{
    if ((NULL == evt->internal_queue)
        || (0 == evt->internal_queue_size)) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_event_iface_item_t *item = audio_calloc(1, sizeof(audio_event_iface_item_t));
    AUDIO_MEM_CHECK(TAG, item, return ESP_ERR_NO_MEM);
    if (audio_event_iface_cleanup_listener(listener) != ESP_OK) {
        AUDIO_ERROR(TAG, "Error cleanup listener");
        return ESP_FAIL;
    }
    item->queue = evt->internal_queue;
    item->queue_size = evt->internal_queue_size;
    STAILQ_INSERT_TAIL(&listener->listening_queues, item, next);
    return audio_event_iface_update_listener(listener);
}

esp_err_t audio_event_iface_remove_listener(audio_event_iface_handle_t listen, audio_event_iface_handle_t evt)
{
    if ((NULL == evt->external_queue)
        || (0 == evt->external_queue_size)) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_event_iface_item_t *item, *tmp;
    if (audio_event_iface_cleanup_listener(listen) != ESP_OK) {
        return ESP_FAIL;
    }
    STAILQ_FOREACH_SAFE(item, &listen->listening_queues, next, tmp) {
        if (evt->external_queue == item->queue) {
            STAILQ_REMOVE(&listen->listening_queues, item, audio_event_iface_item, next);
            audio_free(item);
        }
    }
    return audio_event_iface_update_listener(listen);
}

esp_err_t audio_event_iface_set_cmd_waiting_timeout(audio_event_iface_handle_t evt, unsigned int timeout_ms)
{
    evt->timeout_ms = timeout_ms;
    return ESP_OK;
}

esp_err_t audio_event_iface_waiting_cmd_msg(audio_event_iface_handle_t evt)
{
    audio_event_iface_msg_t msg;
    if (evt->internal_queue && (mqueue_receive(evt->internal_queue, (char *)&msg, evt->timeout_ms) == 0)) {
        if (evt->on_cmd && evt->on_cmd((void *)&msg, evt->context) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t audio_event_iface_cmd(audio_event_iface_handle_t evt, audio_event_iface_msg_t *msg)
{
    if (evt->internal_queue && (mqueue_send(evt->internal_queue, (char *)msg, 0) != 0)) {
        OS_LOGD(TAG, "There are no space to dispatch queue");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t audio_event_iface_sendout(audio_event_iface_handle_t evt, audio_event_iface_msg_t *msg)
{
    if (evt->external_queue) {
        if (mqueue_send(evt->external_queue, (char *)msg, 0) != 0) {
            OS_LOGD(TAG, "There is no space in external queue");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t audio_event_iface_discard(audio_event_iface_handle_t evt)
{
    audio_event_iface_msg_t msg;
    if (evt->external_queue && evt->external_queue_size) {
        while (mqueue_receive(evt->external_queue, (char *)&msg, 0) == 0);
    }
    if (evt->internal_queue && evt->internal_queue_size) {
        while (mqueue_receive(evt->internal_queue, (char *)&msg, 0) == 0);
    }
    if (evt->queue_set && evt->queue_set_size) {
        while (audio_event_iface_read(evt, &msg, 0) == ESP_OK);
    }
    return ESP_OK;
}

esp_err_t audio_event_iface_listen(audio_event_iface_handle_t evt, audio_event_iface_msg_t *msg, unsigned int timeout_ms)
{
    if (!evt) {
        return ESP_FAIL;
    }
    if (audio_event_iface_read(evt, msg, timeout_ms) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

mq_handle audio_event_iface_get_queue_handle(audio_event_iface_handle_t evt)
{
    if (!evt) {
        return NULL;
    }
    return evt->external_queue;
}

mq_handle audio_event_iface_get_msg_queue_handle(audio_event_iface_handle_t evt)
{
    if (!evt) {
        return NULL;
    }
    return evt->internal_queue;
}
