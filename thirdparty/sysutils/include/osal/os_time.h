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

#ifndef __SYSUTILS_OS_TIME_H__
#define __SYSUTILS_OS_TIME_H__

#include <stdio.h>
#include <stdlib.h>
#include "osal_namespace.h"
#include "os_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_wall_time {
    int year;
    int mon;
    int day;
    int hour;
    int min;
    int sec;
    int msec;
};

// get real-time then translate to wall-time
void os_realtime_to_walltime(struct os_wall_time *time);

// monotonictime: clock that cannot be set and represents monotonic time since system bootup
unsigned long long os_monotonic_usec();

// realtime: system-wide clock that measures real time (utc timestamp) since 1970.1.1-00:00:00
unsigned long long os_realtime_usec();

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_OS_TIME_H__ */
