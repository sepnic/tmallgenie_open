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

#ifndef __SYSUTILS_LIST_H__
#define __SYSUTILS_LIST_H__

#include "cutil_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

struct listnode
{
    struct listnode *next;
    struct listnode *prev;
};

#define listnode_to_item(node, container, member) \
    (container *) (((char*)(node)) - ((char*)(&((container *)0)->member)))

#define list_declare(name) \
    struct listnode name = { \
        .next = &name, \
        .prev = &name, \
    }

#define list_for_each(node, list) \
    for (node = (list)->next; node != (list); node = node->next)

#define list_for_each_reverse(node, list) \
    for (node = (list)->prev; node != (list); node = node->prev)

#define list_for_each_safe(node, n, list) \
    for (node = (list)->next, n = node->next; \
         node != (list); \
         node = n, n = node->next)

#define list_for_each_reverse_safe(node, n, list) \
    for (node = (list)->prev, n = node->prev; \
         node != (list); \
         node = n, n = node->prev)

static inline void list_init(struct listnode *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_add_tail(struct listnode *list, struct listnode *newnode)
{
    newnode->next = list;
    newnode->prev = list->prev;
    list->prev->next = newnode;
    list->prev = newnode;
}

static inline void list_add_head(struct listnode *list, struct listnode *newnode)
{
    newnode->next = list->next;
    newnode->prev = list;
    list->next->prev = newnode;
    list->next = newnode;
}

static inline void list_add_before(struct listnode *position, struct listnode *newnode)
{
    newnode->next = position;
    newnode->prev = position->prev;
    position->prev->next = newnode;
    position->prev = newnode;
}

static inline void list_add_after(struct listnode *position, struct listnode *newnode)
{
    newnode->next = position->next;
    newnode->prev = position;
    position->next->prev = newnode;
    position->next = newnode;
}

static inline void list_remove(struct listnode *delnode)
{
    delnode->next->prev = delnode->prev;
    delnode->prev->next = delnode->next;
}

#define list_empty(list) ((list) == (list)->next)
#define list_head(list) ((list)->next)
#define list_tail(list) ((list)->prev)

#ifdef __cplusplus
};
#endif

#endif // __SYSUTILS_LIST_H__
