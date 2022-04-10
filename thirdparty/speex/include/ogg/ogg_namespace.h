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

#ifndef __OGG_NAMESPACE_H__
#define __OGG_NAMESPACE_H__

#define OGG_PREFIX                  GENIE

#define OGG_STATCC1(x,y,z)          OGG_STATCC2(x,y,z)
#define OGG_STATCC2(x,y,z)          x##y##z

#ifdef OGG_PREFIX
#define OGG_NAMESPACE(func)         OGG_STATCC1(OGG_PREFIX, _, func)
#else
#define OGG_NAMESPACE(func)         func
#endif

#define oggpack_writeinit           OGG_NAMESPACE(oggpack_writeinit)
#define oggpack_writecheck          OGG_NAMESPACE(oggpack_writecheck)
#define oggpack_writetrunc          OGG_NAMESPACE(oggpack_writetrunc)
#define oggpack_writealign          OGG_NAMESPACE(oggpack_writealign)
#define oggpack_writecopy           OGG_NAMESPACE(oggpack_writecopy)
#define oggpack_reset               OGG_NAMESPACE(oggpack_reset)
#define oggpack_writeclear          OGG_NAMESPACE(oggpack_writeclear)
#define oggpack_readinit            OGG_NAMESPACE(oggpack_readinit)
#define oggpack_write               OGG_NAMESPACE(oggpack_write)
#define oggpack_look                OGG_NAMESPACE(oggpack_look)
#define oggpack_look1               OGG_NAMESPACE(oggpack_look1)
#define oggpack_adv                 OGG_NAMESPACE(oggpack_adv)
#define oggpack_adv1                OGG_NAMESPACE(oggpack_adv1)
#define oggpack_read                OGG_NAMESPACE(oggpack_read)
#define oggpack_read1               OGG_NAMESPACE(oggpack_read1)
#define oggpack_bytes               OGG_NAMESPACE(oggpack_bytes)
#define oggpack_bits                OGG_NAMESPACE(oggpack_bits)
#define oggpack_get_buffer          OGG_NAMESPACE(oggpack_get_buffer)
#define oggpackB_writeinit          OGG_NAMESPACE(oggpackB_writeinit)
#define oggpackB_writecheck         OGG_NAMESPACE(oggpackB_writecheck)
#define oggpackB_writetrunc         OGG_NAMESPACE(oggpackB_writetrunc)
#define oggpackB_writealign         OGG_NAMESPACE(oggpackB_writealign)
#define oggpackB_writecopy          OGG_NAMESPACE(oggpackB_writecopy)
#define oggpackB_reset              OGG_NAMESPACE(oggpackB_reset)
#define oggpackB_writeclear         OGG_NAMESPACE(oggpackB_writeclear)
#define oggpackB_readinit           OGG_NAMESPACE(oggpackB_readinit)
#define oggpackB_write              OGG_NAMESPACE(oggpackB_write)
#define oggpackB_look               OGG_NAMESPACE(oggpackB_look)
#define oggpackB_look1              OGG_NAMESPACE(oggpackB_look1)
#define oggpackB_adv                OGG_NAMESPACE(oggpackB_adv)
#define oggpackB_adv1               OGG_NAMESPACE(oggpackB_adv1)
#define oggpackB_read               OGG_NAMESPACE(oggpackB_read)
#define oggpackB_read1              OGG_NAMESPACE(oggpackB_read1)
#define oggpackB_bytes              OGG_NAMESPACE(oggpackB_bytes)
#define oggpackB_bits               OGG_NAMESPACE(oggpackB_bits)
#define oggpackB_get_buffer         OGG_NAMESPACE(oggpackB_get_buffer)
#define ogg_stream_packetin         OGG_NAMESPACE(ogg_stream_packetin)
#define ogg_stream_iovecin          OGG_NAMESPACE(ogg_stream_iovecin)
#define ogg_stream_pageout          OGG_NAMESPACE(ogg_stream_pageout)
#define ogg_stream_pageout_fill     OGG_NAMESPACE(ogg_stream_pageout_fill)
#define ogg_stream_flush            OGG_NAMESPACE(ogg_stream_flush)
#define ogg_stream_flush_fill       OGG_NAMESPACE(ogg_stream_flush_fill)
#define ogg_sync_init               OGG_NAMESPACE(ogg_sync_init)
#define ogg_sync_clear              OGG_NAMESPACE(ogg_sync_clear)
#define ogg_sync_reset              OGG_NAMESPACE(ogg_sync_reset)
#define ogg_sync_destroy            OGG_NAMESPACE(ogg_sync_destroy)
#define ogg_sync_check              OGG_NAMESPACE(ogg_sync_check)
#define ogg_sync_buffer             OGG_NAMESPACE(ogg_sync_buffer)
#define ogg_sync_wrote              OGG_NAMESPACE(ogg_sync_wrote)
#define ogg_sync_pageseek           OGG_NAMESPACE(ogg_sync_pageseek)
#define ogg_sync_pageout            OGG_NAMESPACE(ogg_sync_pageout)
#define ogg_stream_pagein           OGG_NAMESPACE(ogg_stream_pagein)
#define ogg_stream_packetout        OGG_NAMESPACE(ogg_stream_packetout)
#define ogg_stream_packetpeek       OGG_NAMESPACE(ogg_stream_packetpeek)
#define ogg_stream_init             OGG_NAMESPACE(ogg_stream_init)
#define ogg_stream_clear            OGG_NAMESPACE(ogg_stream_clear)
#define ogg_stream_reset            OGG_NAMESPACE(ogg_stream_reset)
#define ogg_stream_reset_serialno   OGG_NAMESPACE(ogg_stream_reset_serialno)
#define ogg_stream_destroy          OGG_NAMESPACE(ogg_stream_destroy)
#define ogg_stream_check            OGG_NAMESPACE(ogg_stream_check)
#define ogg_stream_eos              OGG_NAMESPACE(ogg_stream_eos)
#define ogg_page_checksum_set       OGG_NAMESPACE(ogg_page_checksum_set)
#define ogg_page_version            OGG_NAMESPACE(ogg_page_version)
#define ogg_page_continued          OGG_NAMESPACE(ogg_page_continued)
#define ogg_page_bos                OGG_NAMESPACE(ogg_page_bos)
#define ogg_page_eos                OGG_NAMESPACE(ogg_page_eos)
#define ogg_page_granulepos         OGG_NAMESPACE(ogg_page_granulepos)
#define ogg_page_serialno           OGG_NAMESPACE(ogg_page_serialno)
#define ogg_page_pageno             OGG_NAMESPACE(ogg_page_pageno)
#define ogg_page_packets            OGG_NAMESPACE(ogg_page_packets)
#define ogg_packet_clear            OGG_NAMESPACE(ogg_packet_clear)

#endif /* __OGG_NAMESPACE_H__ */
