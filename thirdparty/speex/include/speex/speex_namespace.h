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

#ifndef __SPEEX_NAMESPACE_H__
#define __SPEEX_NAMESPACE_H__

#define SPEEX_PREFIX                            GENIE

#define SPEEX_STATCC1(x,y,z)                    SPEEX_STATCC2(x,y,z)
#define SPEEX_STATCC2(x,y,z)                    x##y##z

#ifdef SPEEX_PREFIX
#define SPEEX_NAMESPACE(func)                   SPEEX_STATCC1(SPEEX_PREFIX, _, func)
#else
#define SPEEX_NAMESPACE(func)                   func
#endif

#define speex_bits_init                         SPEEX_NAMESPACE(speex_bits_init)
#define speex_bits_init_buffer                  SPEEX_NAMESPACE(speex_bits_init_buffer)
#define speex_bits_set_bit_buffer               SPEEX_NAMESPACE(speex_bits_set_bit_buffer)
#define speex_bits_destroy                      SPEEX_NAMESPACE(speex_bits_destroy)
#define speex_bits_reset                        SPEEX_NAMESPACE(speex_bits_reset)
#define speex_bits_rewind                       SPEEX_NAMESPACE(speex_bits_rewind)
#define speex_bits_read_from                    SPEEX_NAMESPACE(speex_bits_read_from)
#define speex_bits_read_whole_bytes             SPEEX_NAMESPACE(speex_bits_read_whole_bytes)
#define speex_bits_write                        SPEEX_NAMESPACE(speex_bits_write)
#define speex_bits_write_whole_bytes            SPEEX_NAMESPACE(speex_bits_write_whole_bytes)
#define speex_bits_pack                         SPEEX_NAMESPACE(speex_bits_pack)
#define speex_bits_unpack_signed                SPEEX_NAMESPACE(speex_bits_unpack_signed)
#define speex_bits_unpack_unsigned              SPEEX_NAMESPACE(speex_bits_unpack_unsigned)
#define speex_bits_nbytes                       SPEEX_NAMESPACE(speex_bits_nbytes)
#define speex_bits_peek_unsigned                SPEEX_NAMESPACE(speex_bits_peek_unsigned)
#define speex_bits_peek                         SPEEX_NAMESPACE(speex_bits_peek)
#define speex_bits_advance                      SPEEX_NAMESPACE(speex_bits_advance)
#define speex_bits_remaining                    SPEEX_NAMESPACE(speex_bits_remaining)
#define speex_bits_insert_terminator            SPEEX_NAMESPACE(speex_bits_insert_terminator)

#define speex_inband_handler                    SPEEX_NAMESPACE(speex_inband_handler)
#define speex_std_mode_request_handler          SPEEX_NAMESPACE(speex_std_mode_request_handler)
#define speex_std_high_mode_request_handler     SPEEX_NAMESPACE(speex_std_high_mode_request_handler)
#define speex_std_char_handler                  SPEEX_NAMESPACE(speex_std_char_handler)
#define speex_default_user_handler              SPEEX_NAMESPACE(speex_default_user_handler)
#define speex_std_low_mode_request_handler      SPEEX_NAMESPACE(speex_std_low_mode_request_handler)
#define speex_std_vbr_request_handler           SPEEX_NAMESPACE(speex_std_vbr_request_handler)
#define speex_std_enh_request_handler           SPEEX_NAMESPACE(speex_std_enh_request_handler)
#define speex_std_vbr_quality_request_handler   SPEEX_NAMESPACE(speex_std_vbr_quality_request_handler)

#define speex_init_header                       SPEEX_NAMESPACE(speex_init_header)
#define speex_header_to_packet                  SPEEX_NAMESPACE(speex_header_to_packet)
#define speex_packet_to_header                  SPEEX_NAMESPACE(speex_packet_to_header)
#define speex_header_free                       SPEEX_NAMESPACE(speex_header_free)

#define speex_stereo_state_init                 SPEEX_NAMESPACE(speex_stereo_state_init)
#define speex_stereo_state_reset                SPEEX_NAMESPACE(speex_stereo_state_reset)
#define speex_stereo_state_destroy              SPEEX_NAMESPACE(speex_stereo_state_destroy)
#define speex_encode_stereo                     SPEEX_NAMESPACE(speex_encode_stereo)
#define speex_encode_stereo_int                 SPEEX_NAMESPACE(speex_encode_stereo_int)
#define speex_decode_stereo                     SPEEX_NAMESPACE(speex_decode_stereo)
#define speex_decode_stereo_int                 SPEEX_NAMESPACE(speex_decode_stereo_int)
#define speex_std_stereo_request_handler        SPEEX_NAMESPACE(speex_std_stereo_request_handler)

#define speex_encoder_init                      SPEEX_NAMESPACE(speex_encoder_init)
#define speex_encoder_destroy                   SPEEX_NAMESPACE(speex_encoder_destroy)
#define speex_encode                            SPEEX_NAMESPACE(speex_encode)
#define speex_encode_int                        SPEEX_NAMESPACE(speex_encode_int)
#define speex_encoder_ctl                       SPEEX_NAMESPACE(speex_encoder_ctl)
#define speex_decoder_init                      SPEEX_NAMESPACE(speex_decoder_init)
#define speex_decoder_destroy                   SPEEX_NAMESPACE(speex_decoder_destroy)
#define speex_decode                            SPEEX_NAMESPACE(speex_decode)
#define speex_decode_int                        SPEEX_NAMESPACE(speex_decode_int)
#define speex_decoder_ctl                       SPEEX_NAMESPACE(speex_decoder_ctl)
#define speex_mode_query                        SPEEX_NAMESPACE(speex_mode_query)
#define speex_lib_ctl                           SPEEX_NAMESPACE(speex_lib_ctl)
//#define speex_lib_get_mode                      SPEEX_NAMESPACE(speex_lib_get_mode)
#define speex_nb_mode                           SPEEX_NAMESPACE(speex_nb_mode)
#define speex_wb_mode                           SPEEX_NAMESPACE(speex_wb_mode)
#define speex_uwb_mode                          SPEEX_NAMESPACE(speex_uwb_mode)
#define speex_mode_list                         SPEEX_NAMESPACE(speex_mode_list)

#define speex_encode_native                     SPEEX_NAMESPACE(speex_encode_native)
#define speex_decode_native                     SPEEX_NAMESPACE(speex_decode_native)
#define nb_mode_query                           SPEEX_NAMESPACE(nb_mode_query)
#define wb_mode_query                           SPEEX_NAMESPACE(wb_mode_query)

#endif /* __SPEEX_NAMESPACE_H__ */
