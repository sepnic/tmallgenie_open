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

#ifndef __SYSUTILS_OS_TIMER_H__
#define __SYSUTILS_OS_TIMER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "osal_namespace.h"
#include "os_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *os_timer;

struct os_timer_attr {
    const char *name;        // name is assigned to the timer, purely to assist debugging
    unsigned long period_ms; // the timer period in milliseconds
    bool reload;             // if reload set to true then the timer will expire repeatedly
                             // if reload set to false then the timer will be a one-shot timer
};

os_timer os_timer_create(struct os_timer_attr *attr, void (*cb)());

int os_timer_start(os_timer timer);

int os_timer_stop(os_timer timer);

bool os_timer_is_active(os_timer timer);

void os_timer_destroy(os_timer timer);

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_OS_TIMER_H__ */
