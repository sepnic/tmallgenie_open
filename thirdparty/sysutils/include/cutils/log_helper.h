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

#ifndef __SYSUTILS_LOG_HELPER_H__
#define __SYSUTILS_LOG_HELPER_H__

#include "osal/os_log.h"
#include "cutil_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(OS_ANDROID)
    #include <android/log.h>
    #define OS_LOGF(tag, format, ...) __android_log_print(ANDROID_LOG_FATAL, tag, format, ##__VA_ARGS__)
    #define OS_LOGE(tag, format, ...) __android_log_print(ANDROID_LOG_ERROR, tag, format, ##__VA_ARGS__)
    #define OS_LOGW(tag, format, ...) __android_log_print(ANDROID_LOG_WARN, tag, format, ##__VA_ARGS__)
    #define OS_LOGI(tag, format, ...) __android_log_print(ANDROID_LOG_INFO, tag, format, ##__VA_ARGS__)
    #define OS_LOGD(tag, format, ...) __android_log_print(ANDROID_LOG_DEBUG, tag, format, ##__VA_ARGS__)
    #if defined(SYSUTILS_HAVE_VERBOSE_LOG_ENABLED)
    #define OS_LOGV(tag, format, ...) __android_log_print(ANDROID_LOG_VERBOSE, tag, format, ##__VA_ARGS__)
    #else
    #define OS_LOGV(tag, format, ...)
    #endif

#else
    #define OS_LOGF(tag, format, ...) os_fatal(tag, format, ##__VA_ARGS__)
    #define OS_LOGE(tag, format, ...) os_error(tag, format, ##__VA_ARGS__)
    #define OS_LOGW(tag, format, ...) os_warning(tag, format, ##__VA_ARGS__)
    #define OS_LOGI(tag, format, ...) os_info(tag, format, ##__VA_ARGS__)
    #define OS_LOGD(tag, format, ...) os_debug(tag, format, ##__VA_ARGS__)
    #if defined(SYSUTILS_HAVE_VERBOSE_LOG_ENABLED)
    #define OS_LOGV(tag, format, ...) os_verbose(tag, format, ##__VA_ARGS__)
    #else
    #define OS_LOGV(tag, format, ...)
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYSUTILS_LOG_HELPER_H__ */
