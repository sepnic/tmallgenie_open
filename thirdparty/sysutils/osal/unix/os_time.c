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

#include <time.h>
#include "osal/os_time.h"

void os_realtime_to_walltime(struct os_wall_time *time)
{
    if (time != NULL) {
        struct tm now;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        localtime_r(&ts.tv_sec, &now);
        time->year = now.tm_year + 1900;
        time->mon  = now.tm_mon + 1;
        time->day  = now.tm_mday;
        time->hour = now.tm_hour;
        time->min  = now.tm_min;
        time->sec  = now.tm_sec;
        time->msec = (ts.tv_nsec / 1000000) % 1000;
    }
}

unsigned long long os_monotonic_usec()
{
    struct timespec ts;
    unsigned long long cputime;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    cputime = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return cputime;
}

unsigned long long os_realtime_usec()
{
    struct timespec ts;
    unsigned long long cputime;
    clock_gettime(CLOCK_REALTIME, &ts);
    cputime = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return cputime;
}
