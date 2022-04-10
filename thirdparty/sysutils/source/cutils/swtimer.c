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
#include <stdarg.h>
#include "osal/os_thread.h"
#include "osal/os_time.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "cutils/list.h"
#include "cutils/swtimer.h"

#define LOG_TAG "swtimer"

struct swtimer {
    os_thread thread_id;
    os_mutex thread_mutex;
    os_cond thread_cond;
    void (*thread_cb)();
    const char *thread_name;
    bool thread_exit;

    unsigned long period_ms;
    bool reload;
    bool started;
};

static void *swtimer_thread_entry(void *arg)
{
    struct swtimer *timer = (struct swtimer *)arg;
    unsigned long long now = 0;
    unsigned long long escape = 0;
    unsigned long long wait = 0;
    unsigned long long period_us = timer->period_ms * 1000;

    OS_LOGD(LOG_TAG, "[%s]: Entry timer thread: thread_id=[%p]", timer->thread_name, timer->thread_id);

    while (!timer->thread_exit) {
        os_mutex_lock(timer->thread_mutex);

        while (!timer->started && !timer->thread_exit)
            os_cond_wait(timer->thread_cond, timer->thread_mutex);

        if (timer->thread_exit) {
            os_mutex_unlock(timer->thread_mutex);
            break;
        }

        wait = period_us - escape;
        os_cond_timedwait(timer->thread_cond, timer->thread_mutex, wait);

        if (timer->started) {
            now = os_monotonic_usec();
            timer->thread_cb();
            escape = os_monotonic_usec() - now;
            if (timer->reload) {
                if (escape >= period_us) {
                    OS_LOGE(LOG_TAG, "[%s]: Handler cost time more than period, stop timer", timer->thread_name);
                    timer->started = false;
                    escape = 0;
                }
            } else {
                timer->started = false;
                escape = 0;
            }
        } else {
            escape = 0;
        }

        os_mutex_unlock(timer->thread_mutex);
    }

    OS_LOGD(LOG_TAG, "[%s]: Leave timer thread: thread_id=[%p]", timer->thread_name, timer->thread_id);
    return NULL;
}

swtimer_handle swtimer_create(struct swtimer_attr *attr, void (*cb)())
{
    struct swtimer *timer = OS_CALLOC(1, sizeof(struct swtimer));
    if (timer == NULL) {
        OS_LOGE(LOG_TAG, "Failed to allocate timer");
        return NULL;
    }

    timer->thread_mutex = os_mutex_create();
    if (timer->thread_mutex == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create thread_mutex");
        goto fail_create;
    }

    timer->thread_cond = os_cond_create();
    if (timer->thread_cond == NULL) {
        OS_LOGE(LOG_TAG, "Failed to create thread_cond");
        goto fail_create;
    }

    if (attr == NULL || cb == NULL) {
        OS_LOGE(LOG_TAG, "Invalid timer attr or callback");
        goto fail_create;
    }

    timer->thread_name = attr->name ? OS_STRDUP(attr->name) : OS_STRDUP("swtimer");
    timer->thread_cb = cb;
    timer->thread_exit = false;
    timer->period_ms = attr->period_ms;
    timer->reload = attr->reload;
    timer->started = false;

    struct os_thread_attr thread_attr = {
        .name = timer->thread_name,
        .priority = OS_THREAD_PRIO_REALTIME,
        .stacksize = os_thread_default_stacksize(),
        .joinable = true,
    };
    timer->thread_id = os_thread_create(&thread_attr, swtimer_thread_entry, timer);
    if (timer->thread_id == NULL) {
        OS_LOGE(LOG_TAG, "[%s]: Failed to run thread timer", timer->thread_name);
        goto fail_create;
    }
    return timer;

fail_create:
    if (timer->thread_cond != NULL)
        os_cond_destroy(timer->thread_cond);
    if (timer->thread_mutex != NULL)
        os_mutex_destroy(timer->thread_mutex);
    if (timer->thread_name != NULL)
        OS_FREE(timer->thread_name);
    OS_FREE(timer);
    return NULL;
}

int swtimer_start(swtimer_handle timer)
{
    os_mutex_lock(timer->thread_mutex);
    timer->started = true;
    os_cond_signal(timer->thread_cond);
    os_mutex_unlock(timer->thread_mutex);
    return 0;
}

int swtimer_stop(swtimer_handle timer)
{
    os_mutex_lock(timer->thread_mutex);
    timer->started = false;
    os_cond_signal(timer->thread_cond);
    os_mutex_unlock(timer->thread_mutex);
    return 0;
}

bool swtimer_is_active(swtimer_handle timer)
{
    return timer->started;
}

void swtimer_destroy(swtimer_handle timer)
{
    {
        os_mutex_lock(timer->thread_mutex);
        timer->started = false;
        timer->thread_exit = true;
        os_cond_signal(timer->thread_cond);
        os_mutex_unlock(timer->thread_mutex);
    }
    os_thread_join(timer->thread_id, NULL);

    os_mutex_destroy(timer->thread_mutex);
    os_cond_destroy(timer->thread_cond);
    OS_FREE(timer->thread_name);
    OS_FREE(timer);
}
