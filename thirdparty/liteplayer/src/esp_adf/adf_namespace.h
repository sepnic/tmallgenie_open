// Copyright (c) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __LITEPLAYER_ESP_ADF_NAMESPACE_H__
#define __LITEPLAYER_ESP_ADF_NAMESPACE_H__

#define ADF_PREFIX                     liteplayer
#define ADF_STATCC1(x,y,z)             ADF_STATCC2(x,y,z)
#define ADF_STATCC2(x,y,z)             x##y##z

#ifdef ADF_PREFIX
#define ADF_NAMESPACE(func)            ADF_STATCC1(ADF_PREFIX, _, func)
#else
#define ADF_NAMESPACE(func)            func
#endif

// audio_element.h
#define audio_element_init                          ADF_NAMESPACE(audio_element_init)
#define audio_element_deinit                        ADF_NAMESPACE(audio_element_deinit)
#define audio_element_setdata                       ADF_NAMESPACE(audio_element_setdata)
#define audio_element_getdata                       ADF_NAMESPACE(audio_element_getdata)
#define audio_element_set_tag                       ADF_NAMESPACE(audio_element_set_tag)
#define audio_element_get_tag                       ADF_NAMESPACE(audio_element_get_tag)
#define audio_element_setinfo                       ADF_NAMESPACE(audio_element_setinfo)
#define audio_element_getinfo                       ADF_NAMESPACE(audio_element_getinfo)
#define audio_element_set_uri                       ADF_NAMESPACE(audio_element_set_uri)
#define audio_element_get_uri                       ADF_NAMESPACE(audio_element_get_uri)
#define audio_element_run                           ADF_NAMESPACE(audio_element_run)
#define audio_element_terminate                     ADF_NAMESPACE(audio_element_terminate)
#define audio_element_stop                          ADF_NAMESPACE(audio_element_stop)
#define audio_element_wait_for_stop_ms              ADF_NAMESPACE(audio_element_wait_for_stop_ms)
#define audio_element_pause                         ADF_NAMESPACE(audio_element_pause)
#define audio_element_resume                        ADF_NAMESPACE(audio_element_resume)
#define audio_element_seek                          ADF_NAMESPACE(audio_element_seek)
#define audio_element_msg_set_listener              ADF_NAMESPACE(audio_element_msg_set_listener)
#define audio_element_set_event_callback            ADF_NAMESPACE(audio_element_set_event_callback)
#define audio_element_msg_remove_listener           ADF_NAMESPACE(audio_element_msg_remove_listener)
#define audio_element_set_input_ringbuf             ADF_NAMESPACE(audio_element_set_input_ringbuf)
#define audio_element_get_input_ringbuf             ADF_NAMESPACE(audio_element_get_input_ringbuf)
#define audio_element_set_output_ringbuf            ADF_NAMESPACE(audio_element_set_output_ringbuf)
#define audio_element_get_output_ringbuf            ADF_NAMESPACE(audio_element_get_output_ringbuf)
#define audio_element_get_state                     ADF_NAMESPACE(audio_element_get_state)
#define audio_element_abort_input_ringbuf           ADF_NAMESPACE(audio_element_abort_input_ringbuf)
#define audio_element_abort_output_ringbuf          ADF_NAMESPACE(audio_element_abort_output_ringbuf)
#define audio_element_wait_for_buffer               ADF_NAMESPACE(audio_element_wait_for_buffer)
#define audio_element_report_status                 ADF_NAMESPACE(audio_element_report_status)
#define audio_element_report_info                   ADF_NAMESPACE(audio_element_report_info)
#define audio_element_report_codec_fmt              ADF_NAMESPACE(audio_element_report_codec_fmt)
#define audio_element_report_pos                    ADF_NAMESPACE(audio_element_report_pos)
#define audio_element_set_input_timeout             ADF_NAMESPACE(audio_element_set_input_timeout)
#define audio_element_set_output_timeout            ADF_NAMESPACE(audio_element_set_output_timeout)
#define audio_element_reset_input_ringbuf           ADF_NAMESPACE(audio_element_reset_input_ringbuf)
#define audio_element_change_cmd                    ADF_NAMESPACE(audio_element_change_cmd)
#define audio_element_reset_output_ringbuf          ADF_NAMESPACE(audio_element_reset_output_ringbuf)
#define audio_element_input                         ADF_NAMESPACE(audio_element_input)
#define audio_element_output                        ADF_NAMESPACE(audio_element_output)
#define audio_element_input_chunk                   ADF_NAMESPACE(audio_element_input_chunk)
#define audio_element_output_chunk                  ADF_NAMESPACE(audio_element_output_chunk)
#define audio_element_set_read_cb                   ADF_NAMESPACE(audio_element_set_read_cb)
#define audio_element_set_write_cb                  ADF_NAMESPACE(audio_element_set_write_cb)
#define audio_element_get_event_queue               ADF_NAMESPACE(audio_element_get_event_queue)
#define audio_element_set_ringbuf_done              ADF_NAMESPACE(audio_element_set_ringbuf_done)
#define audio_element_reset_state                   ADF_NAMESPACE(audio_element_reset_state)
#define audio_element_get_output_ringbuf_size       ADF_NAMESPACE(audio_element_get_output_ringbuf_size)
#define audio_element_set_output_ringbuf_size       ADF_NAMESPACE(audio_element_set_output_ringbuf_size)
#define audio_element_multi_input                   ADF_NAMESPACE(audio_element_multi_input)
#define audio_element_multi_output                  ADF_NAMESPACE(audio_element_multi_output)
#define audio_element_set_multi_input_ringbuf       ADF_NAMESPACE(audio_element_set_multi_input_ringbuf)
#define audio_element_set_multi_output_ringbuf      ADF_NAMESPACE(audio_element_set_multi_output_ringbuf)
#define audio_element_get_multi_input_ringbuf       ADF_NAMESPACE(audio_element_get_multi_input_ringbuf)
#define audio_element_get_multi_output_ringbuf      ADF_NAMESPACE(audio_element_get_multi_output_ringbuf)

// audio_event_iface.h
#define audio_event_iface_init                      ADF_NAMESPACE(audio_event_iface_init)
#define audio_event_iface_destroy                   ADF_NAMESPACE(audio_event_iface_destroy)
#define audio_event_iface_set_listener              ADF_NAMESPACE(audio_event_iface_set_listener)
#define audio_event_iface_remove_listener           ADF_NAMESPACE(audio_event_iface_remove_listener)
#define audio_event_iface_set_cmd_waiting_timeout   ADF_NAMESPACE(audio_event_iface_set_cmd_waiting_timeout)
#define audio_event_iface_waiting_cmd_msg           ADF_NAMESPACE(audio_event_iface_waiting_cmd_msg)
#define audio_event_iface_cmd                       ADF_NAMESPACE(audio_event_iface_cmd)
#define audio_event_iface_sendout                   ADF_NAMESPACE(audio_event_iface_sendout)
#define audio_event_iface_discard                   ADF_NAMESPACE(audio_event_iface_discard)
#define audio_event_iface_listen                    ADF_NAMESPACE(audio_event_iface_listen)
#define audio_event_iface_get_queue_handle          ADF_NAMESPACE(audio_event_iface_get_queue_handle)
#define audio_event_iface_read                      ADF_NAMESPACE(audio_event_iface_read)
#define audio_event_iface_get_msg_queue_handle      ADF_NAMESPACE(audio_event_iface_get_msg_queue_handle)
#define audio_event_iface_set_msg_listener          ADF_NAMESPACE(audio_event_iface_set_msg_listener)
#define audio_element_get_multi_output_ringbuf      ADF_NAMESPACE(audio_element_get_multi_output_ringbuf)
#define audio_element_get_multi_output_ringbuf      ADF_NAMESPACE(audio_element_get_multi_output_ringbuf)

#endif /* __LITEPLAYER_ESP_ADF_NAMESPACE_H__ */
