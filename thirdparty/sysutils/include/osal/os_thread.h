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

#ifndef __SYSUTILS_OS_THREAD_H__
#define __SYSUTILS_OS_THREAD_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "osal_namespace.h"
#include "os_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * os_thread;
typedef void * os_mutex;
typedef void * os_cond;

enum os_thread_prio {
    OS_THREAD_PRIO_REALTIME,
    OS_THREAD_PRIO_HIGH,
    OS_THREAD_PRIO_NORMAL,
    OS_THREAD_PRIO_LOW,
    OS_THREAD_PRIO_IDLE,
};

struct os_thread_attr {
    const char *name;
    enum os_thread_prio priority;
    unsigned long stacksize;
    bool joinable;
};

os_thread os_thread_create(struct os_thread_attr *attr, void *(*cb)(void *arg), void *arg);
os_thread os_thread_self();
unsigned long os_thread_default_stacksize();
int os_thread_join(os_thread thread, void **retval);
int os_thread_detach(os_thread thread);

os_mutex os_mutex_create();
int os_mutex_lock(os_mutex mutex);
int os_mutex_trylock(os_mutex mutex);
int os_mutex_unlock(os_mutex mutex);
void os_mutex_destroy(os_mutex mutex);

os_cond os_cond_create();
int os_cond_wait(os_cond cond, os_mutex mutex);
int os_cond_timedwait(os_cond cond, os_mutex mutex, unsigned long usec);
int os_cond_signal(os_cond cond);
int os_cond_broadcast(os_cond cond);
void os_cond_destroy(os_cond cond);

void os_thread_sleep_usec(unsigned long usec);
void os_thread_sleep_msec(unsigned long msec);

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_OS_THREAD_H__ */
