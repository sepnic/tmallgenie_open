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

#ifndef __SYSUTILS_SW_TIMER_H__
#define __SYSUTILS_SW_TIMER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cutil_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct swtimer *swtimer_handle;

struct swtimer_attr {
    const char *name;        // name is assigned to the timer, purely to assist debugging
    unsigned long period_ms; // the timer period in milliseconds
    bool reload;             // if reload set to true then the timer will expire repeatedly
                             // if reload set to false then the timer will be a one-shot timer
};

swtimer_handle swtimer_create(struct swtimer_attr *attr, void (*cb)());

int swtimer_start(swtimer_handle timer);

int swtimer_stop(swtimer_handle timer);

bool swtimer_is_active(swtimer_handle timer);

void swtimer_destroy(swtimer_handle timer);

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_SW_TIMER_H__ */
