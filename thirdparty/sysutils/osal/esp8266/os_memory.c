/*
 * Copyright (C) 2020-2022 Qinglong<sysu.zqlong@gmail.com>
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
#include "osal/os_memory.h"

void *os_malloc(unsigned int size)
{
    return malloc(size);
}

void *os_calloc(unsigned int n, unsigned int size)
{
    return calloc(n, size);
}

void *os_realloc(void *ptr, unsigned int size)
{
    return realloc(ptr, size);
}

void os_free(void *ptr)
{
    free(ptr);
}

char *os_strdup(const char *str)
{
    return strdup(str);
}
