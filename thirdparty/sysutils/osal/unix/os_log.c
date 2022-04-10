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

#include "osal/os_log.h"

#if defined(OS_ANDROID)
#include <android/log.h>
void os_fatal(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    __android_log_print(ANDROID_LOG_FATAL, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_error(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    __android_log_print(ANDROID_LOG_ERROR, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_warning(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    __android_log_print(ANDROID_LOG_WARN, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_info(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    __android_log_print(ANDROID_LOG_INFO, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_debug(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    __android_log_print(ANDROID_LOG_DEBUG, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_verbose(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    __android_log_print(ANDROID_LOG_VERBOSE, tag, format, arg_ptr);
    va_end(arg_ptr);
}

#else
#include <string.h>
#include <stdarg.h>
#include "osal/os_time.h"

#if defined(OS_RTOS)
#define LOG_BUFFER_SIZE  512
#else
#define LOG_BUFFER_SIZE  2048
#endif

enum log_level {
    LOG_FATAL = 0,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_VERBOSE,
};

static const char *log_level_strings[] = {
    [LOG_FATAL]   = "F",
    [LOG_ERROR]   = "E",
    [LOG_WARN]    = "W",
    [LOG_INFO]    = "I",
    [LOG_DEBUG]   = "D",
    [LOG_VERBOSE] = "V",
};

#define OS_LOG_BLACK         "\033[0;30m"
#define OS_LOG_RED           "\033[0;31m"
#define OS_LOG_GREEN         "\033[0;32m"
#define OS_LOG_BROWN         "\033[0;33m"
#define OS_LOG_BLUE          "\033[0;34m"
#define OS_LOG_PURPLE        "\033[0;35m"
#define OS_LOG_CYAN          "\033[0;36m"
#define OS_LOG_GRAY          "\033[1;30m"
#define OS_LOG_COLOR_RESET   "\033[0m"

#define OS_LOG_COLOR_F       OS_LOG_RED
#define OS_LOG_COLOR_E       OS_LOG_RED
#define OS_LOG_COLOR_W       OS_LOG_BROWN
#define OS_LOG_COLOR_I       OS_LOG_GREEN
#define OS_LOG_COLOR_D       OS_LOG_BLUE
#define OS_LOG_COLOR_V       OS_LOG_GRAY

// [date] [time] [prio] [tag]: [log]
static void log_print(enum log_level prio, const char *tag, const char *format, va_list arg_ptr);

void os_fatal(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log_print(LOG_FATAL, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_error(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log_print(LOG_ERROR, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_warning(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log_print(LOG_WARN, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_info(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log_print(LOG_INFO, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_debug(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log_print(LOG_DEBUG, tag, format, arg_ptr);
    va_end(arg_ptr);
}
void os_verbose(const char *tag, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log_print(LOG_VERBOSE, tag, format, arg_ptr);
    va_end(arg_ptr);
}

static const char *os_log_color(enum log_level prio)
{
    switch (prio) {
    case LOG_FATAL:
        return OS_LOG_COLOR_F;
    case LOG_ERROR:
        return OS_LOG_COLOR_E;
    case LOG_WARN:
        return OS_LOG_COLOR_W;
    case LOG_INFO:
        return OS_LOG_COLOR_I;
    case LOG_DEBUG:
        return OS_LOG_COLOR_D;
    case LOG_VERBOSE:
        return OS_LOG_COLOR_V;
    default:
        return NULL;
    }
}

static void log_print(enum log_level prio, const char *tag, const char *format, va_list arg_ptr)
{
    size_t offset = 0;
    int arg_size = 0;
    char log_entry[LOG_BUFFER_SIZE];
    size_t valid_size = LOG_BUFFER_SIZE - 2;

#if defined(OS_RTOS)
    unsigned long long cputime = os_monotonic_usec();
    if ((int)(valid_size - offset) > 0)
        offset += snprintf(log_entry + offset, valid_size - offset,
                           "%lu", (unsigned long)(cputime/1000));
#else
    struct os_wall_time ts;
    // add data & time to header
    os_realtime_to_walltime(&ts);
    if ((int)(valid_size - offset) > 0)
        offset += snprintf(log_entry + offset, valid_size - offset,
                           "%4d-%02d-%02d %02d:%02d:%02d:%03d",
                           ts.year, ts.mon, ts.day, ts.hour, ts.min, ts.sec, ts.msec);
#endif

    // add priority to header
    if ((int)(valid_size - offset) > 0)
        offset += snprintf(log_entry + offset, valid_size - offset,
                           " %s", log_level_strings[prio]);

    // add tag to header
    if ((int)(valid_size - offset) > 0)
        offset += snprintf(log_entry + offset, valid_size - offset, " %s: ", tag);

    if ((int)(valid_size - offset) > 0) {
        arg_size = vsnprintf(log_entry + offset, valid_size - offset, format, arg_ptr);
        if (arg_size > 0) {
            offset += arg_size;
            if (offset > valid_size)
                offset = valid_size - 1;
        }
        log_entry[offset++] = '\n';
        log_entry[offset] = '\0';
    } else {
        offset = valid_size - 1;
        log_entry[offset++] = '\n';
        log_entry[offset] = '\0';
    }

    // print log to console
    fprintf(stdout, "%s" "%s" OS_LOG_COLOR_RESET, os_log_color(prio), log_entry);
}
#endif // !OS_ANDROID
