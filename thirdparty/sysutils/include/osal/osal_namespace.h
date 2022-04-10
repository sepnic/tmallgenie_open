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

#ifndef __SYSUTILS_OSAL_NAMESPACE_H__
#define __SYSUTILS_OSAL_NAMESPACE_H__

#define SYSUTILS_OSAL_PREFIX           sysutils
#define SYSUTILS_OSAL_STATCC1(x,y,z)   SYSUTILS_OSAL_STATCC2(x,y,z)
#define SYSUTILS_OSAL_STATCC2(x,y,z)   x##y##z

#ifdef SYSUTILS_OSAL_PREFIX
#define SYSUTILS_OSAL_NAMESPACE(func)  SYSUTILS_OSAL_STATCC1(SYSUTILS_OSAL_PREFIX, _, func)
#else
#define SYSUTILS_OSAL_NAMESPACE(func)  func
#endif

// os_log.h
#define os_fatal                       SYSUTILS_OSAL_NAMESPACE(os_fatal)
#define os_error                       SYSUTILS_OSAL_NAMESPACE(os_error)
#define os_warning                     SYSUTILS_OSAL_NAMESPACE(os_warning)
#define os_info                        SYSUTILS_OSAL_NAMESPACE(os_info)
#define os_debug                       SYSUTILS_OSAL_NAMESPACE(os_debug)
#define os_verbose                     SYSUTILS_OSAL_NAMESPACE(os_verbose)

// os_memory.h
#define os_malloc                      SYSUTILS_OSAL_NAMESPACE(os_malloc)
#define os_calloc                      SYSUTILS_OSAL_NAMESPACE(os_calloc)
#define os_realloc                     SYSUTILS_OSAL_NAMESPACE(os_realloc)
#define os_free                        SYSUTILS_OSAL_NAMESPACE(os_free)
#define os_strdup                      SYSUTILS_OSAL_NAMESPACE(os_strdup)

// os_misc.h
#define os_random                      SYSUTILS_OSAL_NAMESPACE(os_random)

// os_thread.h
#define os_thread_create               SYSUTILS_OSAL_NAMESPACE(os_thread_create)
#define os_thread_self                 SYSUTILS_OSAL_NAMESPACE(os_thread_self)
#define os_thread_default_stacksize    SYSUTILS_OSAL_NAMESPACE(os_thread_default_stacksize)
#define os_thread_join                 SYSUTILS_OSAL_NAMESPACE(os_thread_join)
#define os_thread_detach               SYSUTILS_OSAL_NAMESPACE(os_thread_detach)
#define os_mutex_create                SYSUTILS_OSAL_NAMESPACE(os_mutex_create)
#define os_mutex_lock                  SYSUTILS_OSAL_NAMESPACE(os_mutex_lock)
#define os_mutex_trylock               SYSUTILS_OSAL_NAMESPACE(os_mutex_trylock)
#define os_mutex_unlock                SYSUTILS_OSAL_NAMESPACE(os_mutex_unlock)
#define os_mutex_destroy               SYSUTILS_OSAL_NAMESPACE(os_mutex_destroy)
#define os_cond_create                 SYSUTILS_OSAL_NAMESPACE(os_cond_create)
#define os_cond_wait                   SYSUTILS_OSAL_NAMESPACE(os_cond_wait)
#define os_cond_timedwait              SYSUTILS_OSAL_NAMESPACE(os_cond_timedwait)
#define os_cond_signal                 SYSUTILS_OSAL_NAMESPACE(os_cond_signal)
#define os_cond_broadcast              SYSUTILS_OSAL_NAMESPACE(os_cond_broadcast)
#define os_cond_destroy                SYSUTILS_OSAL_NAMESPACE(os_cond_destroy)
#define os_thread_sleep_usec           SYSUTILS_OSAL_NAMESPACE(os_thread_sleep_usec)
#define os_thread_sleep_msec           SYSUTILS_OSAL_NAMESPACE(os_thread_sleep_msec)

// os_time.h
#define os_realtime_to_walltime        SYSUTILS_OSAL_NAMESPACE(os_realtime_to_walltime)
#define os_monotonic_usec              SYSUTILS_OSAL_NAMESPACE(os_monotonic_usec)
#define os_realtime_usec               SYSUTILS_OSAL_NAMESPACE(os_realtime_usec)

// os_timer.h
#define os_timer_create                SYSUTILS_OSAL_NAMESPACE(os_timer_create)
#define os_timer_start                 SYSUTILS_OSAL_NAMESPACE(os_timer_start)
#define os_timer_stop                  SYSUTILS_OSAL_NAMESPACE(os_timer_stop)
#define os_timer_is_active             SYSUTILS_OSAL_NAMESPACE(os_timer_is_active)
#define os_timer_destroy               SYSUTILS_OSAL_NAMESPACE(os_timer_destroy)

#endif /* __SYSUTILS_OSAL_NAMESPACE_H__ */
