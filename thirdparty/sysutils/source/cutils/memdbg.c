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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osal/os_thread.h"
#include "osal/os_time.h"
#include "osal/os_memory.h"
#include "cutils/list.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"

#define LOG_TAG "memdbg"

#define MEMORY_BOUNDARY_SIZE  8
#define MEMORY_BOUNDARY_FLAG '*'

struct memnode {
    void *ptr;
    unsigned int size;
    const char *name;
    const char *file;
    const char *func;
    int line;
    struct os_wall_time when;
    struct listnode listnode;
};

struct memlist {
    unsigned long malloc_count;
    unsigned long free_count;
    unsigned long cur_used;
    unsigned long max_used;
    os_mutex mutex;
    struct listnode list;
};

static struct memlist *g_memlist = NULL;

void *memdbg_malloc(unsigned int size, const char *file, const char *func, int line);
void *memdbg_calloc(unsigned int n, unsigned int size, const char *file, const char *func, int line);
void *memdbg_realloc(void *ptr, unsigned int size, const char *file, const char *func, int line);
void memdbg_free(void *ptr, const char *file, const char *func, int line);
char *memdbg_strdup(const char *str, const char *file, const char *func, int line);
void memdbg_dump_info();

static char *file_name(const char *filepath)
{
    char *filename = (char *)filepath;
    if (filename != NULL) {
        unsigned int len = strlen(filepath);
        if (len > 0) {
            filename += len - 1;
            while (filename > filepath) {
                if ((*filename == '\\') || *filename == '/') {
                    filename++;
                    break;
                }
                filename--;
            }
        }
    }
    return filename;
}

static void memnode_print(struct memnode *node, const char *info)
{
    OS_LOGW(LOG_TAG, "> %s: ptr=[%p], size=[%lu], "
           "created by [%s:%s:%d], at [%04d%02d%02d-%02d%02d%02d:%03d]",
           info, node->ptr, (unsigned long)node->size,
           file_name(node->file), node->func, node->line,
           node->when.year, node->when.mon, node->when.day,
           node->when.hour, node->when.min, node->when.sec, node->when.msec);
}

#if defined(SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED)
static void *memory_boundary_malloc(unsigned int size)
{
    unsigned int real_size = size + 2*MEMORY_BOUNDARY_SIZE;
    void *ptr = os_malloc(real_size);
    if (ptr != NULL) {
        // fill upper boundary with specific flag
        memset(ptr, MEMORY_BOUNDARY_FLAG, MEMORY_BOUNDARY_SIZE);
        // fill lower boundary with specific flag
        memset((char *)ptr + real_size - MEMORY_BOUNDARY_SIZE,
               MEMORY_BOUNDARY_FLAG, MEMORY_BOUNDARY_SIZE);
        return (char *)ptr + MEMORY_BOUNDARY_SIZE;
    }
    return NULL;
}

static void memory_boundary_free(void *ptr)
{
    if (ptr != NULL) {
        void *head = (void *) ((char *)ptr - MEMORY_BOUNDARY_SIZE);
        os_free(head);
    }
}

static void memory_boundary_verify(struct memnode *node)
{
    int i = 0;
    char *ptr;
    // check lower boundary
    ptr = (char *)node->ptr - MEMORY_BOUNDARY_SIZE;
    for (i = 0; i < MEMORY_BOUNDARY_SIZE; i++) {
        if (((char *)ptr)[i] != MEMORY_BOUNDARY_FLAG) {
            memnode_print(node, "Overflow at lower boundary");
            break;
        }
    }
    // check upper boundary
    ptr = (char *)node->ptr + node->size;
    for (i = 0; i < MEMORY_BOUNDARY_SIZE; i++) {
        if (((char *)ptr)[i] != MEMORY_BOUNDARY_FLAG) {
            memnode_print(node, "Overflow at upper boundary");
            break;
        }
    }
}
#endif

static struct memlist *memlist_init()
{
    if (g_memlist == NULL) {
        struct memlist *memlist = os_calloc(1, sizeof(struct memlist));
        if (memlist == NULL) {
            OS_LOGE(LOG_TAG, "Failed to alloc memlist, abort memory debug");
            return NULL;
        }

        memlist->mutex = os_mutex_create();
        if (memlist->mutex == NULL) {
            OS_LOGE(LOG_TAG, "Failed to alloc mem_mutex, abort memory debug");
            os_free(memlist);
            return NULL;
        }

        memlist->malloc_count = 0;
        memlist->free_count = 0;
        memlist->max_used = 0;
        list_init(&memlist->list);

        g_memlist = memlist;
    }
    return g_memlist;
}

void *memdbg_malloc(unsigned int size, const char *file, const char *func, int line)
{
    void *ptr;
#if defined(SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED)
        ptr = memory_boundary_malloc(size);
#else
        ptr = os_malloc(size);
#endif
    if (ptr == NULL) {
        OS_LOGF(LOG_TAG, "%s:%s:%d: failed to alloc memory", file_name(file), func, line);
        return NULL;
    }

    struct memlist *list = memlist_init();
    if (list != NULL) {
        struct memnode *node = os_malloc(sizeof(struct memnode));
        if (node != NULL) {
            node->ptr = ptr;
            node->size = size;
            node->file = file;
            node->func = func;
            node->line = line;
            os_realtime_to_walltime(&node->when);

            os_mutex_lock(list->mutex);
            list_add_tail(&list->list, &node->listnode);
            list->malloc_count++;
            list->cur_used += size;
            if (list->cur_used > list->max_used)
                list->max_used = list->cur_used;
            os_mutex_unlock(list->mutex);
        }
    }
    return ptr;
}

void *memdbg_calloc(unsigned int n, unsigned int size, const char *file, const char *func, int line)
{
    void *ptr = memdbg_malloc(size * n, file, func, line);
    if (ptr != NULL)
        memset(ptr, 0x0, size * n);
    return ptr;
}

void *memdbg_realloc(void *ptr, unsigned int size, const char *file, const char *func, int line)
{
    if (ptr == NULL) {
        if (size > 0)
            return memdbg_malloc(size, file, func, line);
        else
            return NULL;
    }
    if (size == 0) {
        memdbg_free(ptr, file, func, line);
        return NULL;
    }

    struct memlist *list = memlist_init();
    if (list != NULL) {
        struct listnode *item;
        void *prev_ptr = NULL;
        unsigned int prev_size = 0;

        os_mutex_lock(list->mutex);
        list_for_each_reverse(item, &list->list) {
            struct memnode *node = listnode_to_item(item, struct memnode, listnode);
            if (node->ptr == ptr) {
                prev_ptr = node->ptr;
                prev_size = node->size;
                break;
            }
        }
        if (prev_ptr == NULL) {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list, abort realloc",
                   file_name(file), func, line, ptr);
            os_mutex_unlock(list->mutex);
            return NULL;
        }
        os_mutex_unlock(list->mutex);

        if (size > prev_size) {
            void *new_ptr = memdbg_malloc(size, file, func, line);
            if (new_ptr != NULL)
                memcpy(new_ptr, prev_ptr, prev_size);
            memdbg_free(prev_ptr, file, func, line);
            return new_ptr;
        } else {
            return prev_ptr;
        }
    } else {
#if defined(SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED)
        OS_LOGF(LOG_TAG, "%s:%s:%d: failed to create memlist instance, abort realloc",
                file_name(file), func, line);
        return NULL;

#else
        return os_realloc(ptr, size);
#endif
    }

    return NULL;
}

void memdbg_free(void *ptr, const char *file, const char *func, int line)
{
    struct memlist *list = memlist_init();
    if (list != NULL) {
        struct listnode *item;
        struct memnode *node;
        bool found = false;

        os_mutex_lock(list->mutex);
        list_for_each_reverse(item, &list->list) {
            node = listnode_to_item(item, struct memnode, listnode);
            if (node->ptr == ptr) {
                found = true;
                break;
            }
        }
        if (found) {
#if defined(SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED)
                memory_boundary_verify(node);
#endif
            list->free_count++;
            list->cur_used -= node->size;
            list_remove(item);
            os_free(node);
        } else {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list, double free?",
                   file_name(file), func, line, ptr);
        }
        os_mutex_unlock(list->mutex);
    }

#if defined(SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED)
    memory_boundary_free(ptr);
#else
    os_free(ptr);
#endif
}

char *memdbg_strdup(const char *str, const char *file, const char *func, int line)
{
    if (str == NULL)
        return NULL;
    unsigned int len = strlen(str);
    char *ptr = memdbg_malloc(len+1, file, func, line);
    if (ptr != NULL) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    }
    return ptr;
}

void memdbg_dump_info()
{
    struct memlist *list  = memlist_init();
    if (list != NULL) {
        struct memnode *node;
        struct listnode *item;

        OS_LOGW(LOG_TAG, ">>");
        OS_LOGW(LOG_TAG, "++++++++++++++++++++ MEMORY DUMP ++++++++++++++++++++");

        os_mutex_lock(list->mutex);
        list_for_each(item, &list->list) {
            node = listnode_to_item(item, struct memnode, listnode);
            memnode_print(node, "Dump");
#if defined(SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED)
            memory_boundary_verify(node);
#endif
        }
        OS_LOGW(LOG_TAG, "Summary: malloc [%lu] blocks, free [%lu] blocks, current use [%lu] Bytes, max use [%lu] Bytes",
                list->malloc_count, list->free_count, list->cur_used, list->max_used);
        os_mutex_unlock(list->mutex);

        OS_LOGW(LOG_TAG, "-------------------- MEMORY DUMP --------------------");
        OS_LOGW(LOG_TAG, "<<");
    }
}

// ---------------------------------------------------------------------------

static struct memlist *g_clzlist = NULL;

static void clznode_print(struct memnode *node, const char *info)
{
    OS_LOGW(LOG_TAG, "> %s: ptr=[%p], name=[%s], "
           "created by [%s:%s:%d], at [%04d%02d%02d-%02d%02d%02d:%03d]",
           info, node->ptr, node->name,
           file_name(node->file), node->func, node->line,
           node->when.year, node->when.mon, node->when.day,
           node->when.hour, node->when.min, node->when.sec, node->when.msec);
}

static struct memlist *clzlist_init()
{
    if (g_clzlist == NULL) {
        struct memlist *clzlist = os_calloc(1, sizeof(struct memlist));
        if (clzlist == NULL) {
            OS_LOGE(LOG_TAG, "Failed to alloc clzlist, abort class debug");
            return NULL;
        }

        clzlist->mutex = os_mutex_create();
        if (clzlist->mutex == NULL) {
            OS_LOGE(LOG_TAG, "Failed to alloc mem_mutex, abort class debug");
            os_free(clzlist);
            return NULL;
        }

        clzlist->malloc_count = 0;
        clzlist->free_count = 0;
        list_init(&clzlist->list);

        g_clzlist = clzlist;
    }
    return g_clzlist;
}

void clzdbg_new(void *ptr, const char *name, const char *file, const char *func, int line)
{
    struct memlist *list = clzlist_init();
    if (list != NULL) {
        struct memnode *node = (struct memnode *)OS_MALLOC(sizeof(struct memnode));
        if (node != NULL) {
            node->ptr = ptr;
            node->name = name;
            node->file = file;
            node->func = func;
            node->line = line;
            os_realtime_to_walltime(&node->when);

            //clznode_print(node, "new");

            os_mutex_lock(list->mutex);
            list_add_tail(&list->list, &node->listnode);
            list->malloc_count++;
            os_mutex_unlock(list->mutex);
        }
    }
}

void clzdbg_delete(void *ptr, const char *file, const char *func, int line)
{
    struct memlist *list = clzlist_init();
    if (list != NULL) {
        struct memnode *node;
        struct listnode *item;
        bool found = false;

        os_mutex_lock(list->mutex);

        list_for_each_reverse(item, &list->list) {
            node = listnode_to_item(item, struct memnode, listnode);
            if (node->ptr == ptr) {
                found = true;
                break;
            }
        }

        if (found) {
            //clznode_print(node, "delete");
            list->free_count++;
            list_remove(item);
            OS_FREE(node);
        } else {
            OS_LOGF(LOG_TAG, "%s:%s:%d: failed to find ptr[%p] in list", file_name(file), func, line, ptr);
        }

        os_mutex_unlock(list->mutex);
    }
}

void clzdbg_dump()
{
    struct memlist *list = clzlist_init();
    if (list != NULL) {
        OS_LOGW(LOG_TAG, ">>");
        OS_LOGW(LOG_TAG, "++++++++++++++++++++ CLASS DEBUG ++++++++++++++++++++");

        os_mutex_lock(list->mutex);
        struct listnode *item;
        list_for_each(item, &list->list) {
            struct memnode * node = listnode_to_item(item, struct memnode, listnode);
            clznode_print(node, "Dump");
        }
        OS_LOGW(LOG_TAG, "Summary: new [%ld] objects, delete [%ld] objects", list->malloc_count, list->free_count);
        os_mutex_unlock(list->mutex);

        OS_LOGW(LOG_TAG, "-------------------- CLASS DEBUG --------------------");
        OS_LOGW(LOG_TAG, "<<");
    }
}
