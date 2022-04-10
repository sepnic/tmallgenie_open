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

#ifndef __SYSUTILS_BASE64_H_
#define __SYSUTILS_BASE64_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cipher_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BASE64_ENCODE_EXPECTED_OUTLEN(inlen) ((unsigned int)((((inlen) + 2) / 3) * 4 + 1))
#define BASE64_DECODE_EXPECTED_OUTLEN(inlen) ((unsigned int)(((inlen) / 4) * 3 + 1))

/** out is null-terminated encode string. */
bool base64_encode(const unsigned char *in, unsigned int inlen, char *out, unsigned int *outlen);

/** out is null-terminated decode string. */
bool base64_decode(const char *in, unsigned int inlen, unsigned char *out, unsigned int *outlen);

#ifdef __cplusplus
}
#endif

#endif // __SYSUTILS_BASE64_H_
