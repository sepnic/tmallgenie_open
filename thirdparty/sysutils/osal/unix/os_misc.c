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

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include "osal/os_misc.h"

#define OS_RANDOM_DEVICE "/dev/urandom"

int os_random(void *buffer, unsigned int size)
{
    static pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;
    static int rand_fd = -1;

    if (rand_fd < 0) {
        pthread_mutex_lock(&rand_mutex);
        if (rand_fd < 0)
            rand_fd = open(OS_RANDOM_DEVICE, O_RDONLY);
        pthread_mutex_unlock(&rand_mutex);
    }

    if (rand_fd >= 0)
        return (int)read(rand_fd, (char *)buffer, (size_t)(size));
    else
        return -1;
}
