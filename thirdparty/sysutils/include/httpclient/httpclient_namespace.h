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

#ifndef __SYSUTILS_HTTPCLIENT_NAMESPACE_H__
#define __SYSUTILS_HTTPCLIENT_NAMESPACE_H__

#define SYSUTILS_HTTPCLIENT_PREFIX           sysutils
#define SYSUTILS_HTTPCLIENT_STATCC1(x,y,z)   SYSUTILS_HTTPCLIENT_STATCC2(x,y,z)
#define SYSUTILS_HTTPCLIENT_STATCC2(x,y,z)   x##y##z

#ifdef SYSUTILS_HTTPCLIENT_PREFIX
#define SYSUTILS_HTTPCLIENT_NAMESPACE(func)  SYSUTILS_HTTPCLIENT_STATCC1(SYSUTILS_HTTPCLIENT_PREFIX, _, func)
#else
#define SYSUTILS_HTTPCLIENT_NAMESPACE(func)  func
#endif

// httpclient.h
#define httpclient_get                         SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_get)
#define httpclient_post                        SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_post)
#define httpclient_put                         SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_put)
#define httpclient_delete                      SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_delete)
#define httpclient_connect                     SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_connect)
#define httpclient_send_request                SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_send_request)
#define httpclient_recv_response               SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_recv_response)
#define httpclient_close                       SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_close)
#define httpclient_get_response_code           SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_get_response_code)
#define httpclient_get_response_header_value   SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_get_response_header_value)
#define httpclient_set_custom_header           SYSUTILS_HTTPCLIENT_NAMESPACE(httpclient_set_custom_header)

#endif /* __SYSUTILS_HTTPCLIENT_NAMESPACE_H__ */
