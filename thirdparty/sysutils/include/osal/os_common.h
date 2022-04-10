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

#ifndef __SYSUTILS_OS_COMMON_H__
#define __SYSUTILS_OS_COMMON_H__

#include <stdio.h>
#include "osal_namespace.h"

/* check os */

/* check Apple */
#if !defined(OS_APPLE)
#if defined(__APPLE__)
#define OS_APPLE 1
#endif
#endif

/* check Unix */
#if !defined(OS_UNIX)
#if defined(__unix) || defined(unix) || defined(__unix__)
#define OS_UNIX 1
#endif
#endif

/* check Linux */
#if !defined(OS_LINUX)
#if defined(__linux__) || defined(__linux)
#define OS_LINUX 1
#endif
#endif

/* check Android */
#if !defined(OS_ANDROID)
#if defined(__ANDROID__) || defined(ANDROID)
#define OS_ANDROID 1
#endif
#endif

/* check RTOS */
#if !defined(OS_RTOS)
#if defined(OS_FREERTOS_ESP8266) || defined(OS_FREERTOS_ESP32)
#define OS_RTOS 1
#endif
#endif

#endif // __SYSUTILS_OS_COMMON_H__
