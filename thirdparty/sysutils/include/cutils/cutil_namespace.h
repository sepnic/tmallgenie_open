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

#ifndef __SYSUTILS_CUTILS_NAMESPACE_H__
#define __SYSUTILS_CUTILS_NAMESPACE_H__

#define SYSUTILS_CUTILS_PREFIX           sysutils
#define SYSUTILS_CUTILS_STATCC1(x,y,z)   SYSUTILS_CUTILS_STATCC2(x,y,z)
#define SYSUTILS_CUTILS_STATCC2(x,y,z)   x##y##z

#ifdef SYSUTILS_CUTILS_PREFIX
#define SYSUTILS_CUTILS_NAMESPACE(func)  SYSUTILS_CUTILS_STATCC1(SYSUTILS_CUTILS_PREFIX, _, func)
#else
#define SYSUTILS_CUTILS_NAMESPACE(func)  func
#endif

// mlooper.h
#define message_obtain                 SYSUTILS_CUTILS_NAMESPACE(message_obtain)
#define message_obtain_buffer_obtain   SYSUTILS_CUTILS_NAMESPACE(message_obtain_buffer_obtain)
#define message_set_handle_cb          SYSUTILS_CUTILS_NAMESPACE(message_set_handle_cb)
#define message_set_free_cb            SYSUTILS_CUTILS_NAMESPACE(message_set_free_cb)
#define message_set_discard_cb         SYSUTILS_CUTILS_NAMESPACE(message_set_discard_cb)
#define message_set_timeout_cb         SYSUTILS_CUTILS_NAMESPACE(message_set_timeout_cb)
#define mlooper_create                 SYSUTILS_CUTILS_NAMESPACE(mlooper_create)
#define mlooper_destroy                SYSUTILS_CUTILS_NAMESPACE(mlooper_destroy)
#define mlooper_start                  SYSUTILS_CUTILS_NAMESPACE(mlooper_start)
#define mlooper_stop                   SYSUTILS_CUTILS_NAMESPACE(mlooper_stop)
#define mlooper_message_count          SYSUTILS_CUTILS_NAMESPACE(mlooper_message_count)
#define mlooper_dump                   SYSUTILS_CUTILS_NAMESPACE(mlooper_dump)
#define mlooper_post_message           SYSUTILS_CUTILS_NAMESPACE(mlooper_post_message)
#define mlooper_post_message_front     SYSUTILS_CUTILS_NAMESPACE(mlooper_post_message_front)
#define mlooper_post_message_delay     SYSUTILS_CUTILS_NAMESPACE(mlooper_post_message_delay)
#define mlooper_remove_self_message    SYSUTILS_CUTILS_NAMESPACE(mlooper_remove_self_message)
#define mlooper_remove_self_message_if SYSUTILS_CUTILS_NAMESPACE(mlooper_remove_self_message_if)
#define mlooper_clear_self_message     SYSUTILS_CUTILS_NAMESPACE(mlooper_clear_self_message)
#define mlooper_remove_message         SYSUTILS_CUTILS_NAMESPACE(mlooper_remove_message)
#define mlooper_remove_message_if      SYSUTILS_CUTILS_NAMESPACE(mlooper_remove_message_if)
#define mlooper_clear_message          SYSUTILS_CUTILS_NAMESPACE(mlooper_clear_message)

// mqueue.h
#define mqueue_create                  SYSUTILS_CUTILS_NAMESPACE(mqueue_create)
#define mqueue_destroy                 SYSUTILS_CUTILS_NAMESPACE(mqueue_destroy)
#define mqueue_reset                   SYSUTILS_CUTILS_NAMESPACE(mqueue_reset)
#define mqueue_send                    SYSUTILS_CUTILS_NAMESPACE(mqueue_send)
#define mqueue_receive                 SYSUTILS_CUTILS_NAMESPACE(mqueue_receive)
#define mqueue_count_available         SYSUTILS_CUTILS_NAMESPACE(mqueue_count_available)
#define mqueue_count_filled            SYSUTILS_CUTILS_NAMESPACE(mqueue_count_filled)
#define mqueueset_create               SYSUTILS_CUTILS_NAMESPACE(mqueueset_create)
#define mqueueset_destroy              SYSUTILS_CUTILS_NAMESPACE(mqueueset_destroy)
#define mqueueset_add_queue            SYSUTILS_CUTILS_NAMESPACE(mqueueset_add_queue)
#define mqueueset_remove_queue         SYSUTILS_CUTILS_NAMESPACE(mqueueset_remove_queue)
#define mqueueset_select_queue         SYSUTILS_CUTILS_NAMESPACE(mqueueset_select_queue)

// ringbuf.h
#define rb_create                      SYSUTILS_CUTILS_NAMESPACE(rb_create)
#define rb_destroy                     SYSUTILS_CUTILS_NAMESPACE(rb_destroy)
#define rb_abort                       SYSUTILS_CUTILS_NAMESPACE(rb_abort)
#define rb_reset                       SYSUTILS_CUTILS_NAMESPACE(rb_reset)
#define rb_bytes_available             SYSUTILS_CUTILS_NAMESPACE(rb_bytes_available)
#define rb_bytes_filled                SYSUTILS_CUTILS_NAMESPACE(rb_bytes_filled)
#define rb_get_size                    SYSUTILS_CUTILS_NAMESPACE(rb_get_size)
#define rb_read                        SYSUTILS_CUTILS_NAMESPACE(rb_read)
#define rb_write                       SYSUTILS_CUTILS_NAMESPACE(rb_write)
#define rb_read_chunk                  SYSUTILS_CUTILS_NAMESPACE(rb_read_chunk)
#define rb_write_chunk                 SYSUTILS_CUTILS_NAMESPACE(rb_write_chunk)
#define rb_done_write                  SYSUTILS_CUTILS_NAMESPACE(rb_done_write)
#define rb_done_read                   SYSUTILS_CUTILS_NAMESPACE(rb_done_read)
#define rb_unblock_reader              SYSUTILS_CUTILS_NAMESPACE(rb_unblock_reader)
#define rb_set_threshold               SYSUTILS_CUTILS_NAMESPACE(rb_set_threshold)
#define rb_get_threshold               SYSUTILS_CUTILS_NAMESPACE(rb_get_threshold)
#define rb_reach_threshold             SYSUTILS_CUTILS_NAMESPACE(rb_reach_threshold)
#define rb_is_full                     SYSUTILS_CUTILS_NAMESPACE(rb_is_full)
#define rb_is_done_write               SYSUTILS_CUTILS_NAMESPACE(rb_is_done_write)

// swtimer.h
#define swtimer_create                 SYSUTILS_CUTILS_NAMESPACE(swtimer_create)
#define swtimer_start                  SYSUTILS_CUTILS_NAMESPACE(swtimer_start)
#define swtimer_stop                   SYSUTILS_CUTILS_NAMESPACE(swtimer_stop)
#define swtimer_is_active              SYSUTILS_CUTILS_NAMESPACE(swtimer_is_active)
#define swtimer_destroy                SYSUTILS_CUTILS_NAMESPACE(swtimer_destroy)

#endif /* __SYSUTILS_CUTILS_NAMESPACE_H__ */
