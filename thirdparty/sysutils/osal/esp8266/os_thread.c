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

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "osal/os_thread.h"

#if defined(OS_FREERTOS_ESP8266) || defined(OS_FREERTOS_ESP32)
#include "esp_pthread.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <sys/prctl.h>
#endif

// todo: following functions are not implemented yet, need to update freertos and freertos-plus-posix
//  pthread_attr_setschedparam
//  pthread_self
//  pthread_condattr_setclock

#define DEFAULT_THREAD_PRIORITY   (31)      // default priority for unix-like system
#define DEFAULT_THREAD_STACKSIZE  (32*1024) // 32KB

struct os_thread_priv {
    void *(*cb)(void *arg);
    void *arg;
    const char *name;
};

static void *os_thread_common_entry(void *arg)
{
    struct os_thread_priv *priv = (struct os_thread_priv *)arg;

    if (priv->name != NULL) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
        prctl(PR_SET_NAME, priv->name);
#elif defined(OS_APPLE)
        // TODO: set thread name for macosx/ios
#elif defined(OS_RTOS)
        // TODO: set thread name for rtos
#endif
        free((void *)(priv->name));
    }

    void *ret = NULL;
    if (priv->cb != NULL)
        ret = priv->cb(priv->arg);

    free(priv);
    return ret;
}

int os_thread_sched_priority(enum os_thread_prio prio_type)
{
#if defined(OS_FREERTOS_ESP8266) || defined(OS_FREERTOS_ESP32)
    switch (prio_type) {
    case OS_THREAD_PRIO_REALTIME:
        return 23;
    case OS_THREAD_PRIO_HIGH:
        return 8;
    case OS_THREAD_PRIO_NORMAL:
        return 5;
    case OS_THREAD_PRIO_LOW:
        return 3;
    case OS_THREAD_PRIO_IDLE:
        return 1;
    default:
        return 5;
    }
#else
    // todo: translate prio_type to sched_priority if OS_RTOS
    pthread_attr_t attr;
    struct sched_param param;
    int sched_priority = DEFAULT_THREAD_PRIORITY;
    pthread_attr_init(&attr);
    if (pthread_attr_getschedparam(&attr, &param) != 0)
        sched_priority = DEFAULT_THREAD_PRIORITY;
    else
        sched_priority = param.sched_priority;
    pthread_attr_destroy(&attr);
    return sched_priority;
#endif
}

os_thread os_thread_create(struct os_thread_attr *attr, void *(*cb)(void *arg), void *arg)
{
    pthread_attr_t tattr;
    int detachstate = PTHREAD_CREATE_JOINABLE;
    if (attr && !attr->joinable)
        detachstate = PTHREAD_CREATE_DETACHED;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, detachstate);

#if defined(OS_RTOS)
    if (attr != NULL) {
#if defined(OS_FREERTOS_ESP8266) || defined(OS_FREERTOS_ESP32)
        esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
        cfg.prio = os_thread_sched_priority(attr->priority);
        cfg.thread_name = attr->name;
        esp_pthread_set_cfg(&cfg);
#else
        struct sched_param tsched;
        tsched.sched_priority = os_thread_sched_priority(attr->priority);
        pthread_attr_setschedparam(&tattr, &tsched);
#endif
        pthread_attr_setstacksize(&tattr, attr->stacksize);
    }
#endif

    int ret = -1;
    struct os_thread_priv *priv = (struct os_thread_priv *)malloc(sizeof(struct os_thread_priv));
    if (priv == NULL) {
        goto create_out;
    }
    priv->cb = cb;
    priv->arg = arg;
    priv->name = (attr && attr->name) ? strdup(attr->name) : strdup("sysutils");
    pthread_t tid;
    ret = pthread_create(&tid, &tattr, os_thread_common_entry, priv);

create_out:
    pthread_attr_destroy(&tattr);
    if (ret != 0) {
        if (priv != NULL) {
            if (priv->name != NULL)
                free((void *)(priv->name));
            free((void *)priv);
        }
        return NULL;
    }
    return (os_thread)tid;
}

os_thread os_thread_self()
{
#if defined(OS_FREERTOS_ESP8266) || defined(OS_FREERTOS_ESP32)
    // todo: pthread_self NOT supported yet
    return NULL;
#else
    return (os_thread)pthread_self();
#endif
}

unsigned long os_thread_default_stacksize()
{
    pthread_attr_t attr;
    size_t stacksize = DEFAULT_THREAD_STACKSIZE;
    pthread_attr_init(&attr);
    if (pthread_attr_getstacksize(&attr, &stacksize) != 0)
        stacksize = DEFAULT_THREAD_STACKSIZE;
    pthread_attr_destroy(&attr);
    return (unsigned long)stacksize;
}

int os_thread_join(os_thread thread, void **retval)
{
    return pthread_join((pthread_t)thread, retval);
}

int os_thread_detach(os_thread thread)
{
    return pthread_detach((pthread_t)thread);
}

os_mutex os_mutex_create()
{
    pthread_mutex_t *mutex = calloc(1, sizeof(pthread_mutex_t));
    if (mutex == NULL)
        return NULL;
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    return (os_mutex)mutex;
}

int os_mutex_lock(os_mutex mutex)
{
    return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

int os_mutex_trylock(os_mutex mutex)
{
    return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

int os_mutex_unlock(os_mutex mutex)
{
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

void os_mutex_destroy(os_mutex mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    free(mutex);
}

os_cond os_cond_create()
{
    pthread_cond_t *cond = calloc(1, sizeof(pthread_cond_t));
    if (cond == NULL)
        return NULL;
#if defined(OS_FREERTOS_ESP8266) || defined(OS_FREERTOS_ESP32)
    // todo: pthread_condattr_setclock NOT supported yet
    int ret = pthread_cond_init(cond, NULL);
#else
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    #if !defined(OS_APPLE)
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    #endif
    int ret = pthread_cond_init(cond, &attr);
    pthread_condattr_destroy(&attr);
#endif

    if (ret != 0) {
        free(cond);
        return NULL;
    }
    return (os_cond)cond;
}

int os_cond_wait(os_cond cond, os_mutex mutex)
{
    return pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

int os_cond_timedwait(os_cond cond, os_mutex mutex, unsigned long usec)
{
    struct timespec ts;
#if !defined(OS_APPLE)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    ts.tv_sec += usec / 1000000;
    ts.tv_nsec += (usec % 1000000) * 1000;
    while (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec %= 1000000000L;
    }
    return pthread_cond_timedwait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex, &ts);
}

int os_cond_signal(os_cond cond)
{
    return pthread_cond_signal((pthread_cond_t *)cond);
}

int os_cond_broadcast(os_cond cond)
{
    return pthread_cond_broadcast((pthread_cond_t *)cond);
}

void os_cond_destroy(os_cond cond)
{
    pthread_cond_destroy((pthread_cond_t *)cond);
    free(cond);
}

void os_thread_sleep_usec(unsigned long usec)
{
    usleep(usec);
}

void os_thread_sleep_msec(unsigned long msec)
{
    usleep(msec * 1000);
}
