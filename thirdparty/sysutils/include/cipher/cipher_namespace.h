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

#ifndef __SYSUTILS_CIPHER_NAMESPACE_H__
#define __SYSUTILS_CIPHER_NAMESPACE_H__

#define SYSUTILS_CIPHER_PREFIX           sysutils

#define SYSUTILS_CIPHER_STATCC1(x,y,z)   SYSUTILS_CIPHER_STATCC2(x,y,z)
#define SYSUTILS_CIPHER_STATCC2(x,y,z)   x##y##z

#ifdef SYSUTILS_CIPHER_PREFIX
#define SYSUTILS_CIPHER_NAMESPACE(func)  SYSUTILS_CIPHER_STATCC1(SYSUTILS_CIPHER_PREFIX, _, func)
#else
#define SYSUTILS_CIPHER_NAMESPACE(func)  func
#endif

// base64.h
#define base64_encode                  SYSUTILS_CIPHER_NAMESPACE(base64_encode)
#define base64_decode                  SYSUTILS_CIPHER_NAMESPACE(base64_decode)

// md5.h
#define  MD5Init                       SYSUTILS_CIPHER_NAMESPACE(MD5Init)
#define  MD5Update                     SYSUTILS_CIPHER_NAMESPACE(MD5Update)
#define  MD5Update                     SYSUTILS_CIPHER_NAMESPACE(MD5Update)

// hmac_sha2.h
#define hmac_sha224_init               SYSUTILS_CIPHER_NAMESPACE(hmac_sha224_init)
#define hmac_sha224_reinit             SYSUTILS_CIPHER_NAMESPACE(hmac_sha224_reinit)
#define hmac_sha224_update             SYSUTILS_CIPHER_NAMESPACE(hmac_sha224_update)
#define hmac_sha224_final              SYSUTILS_CIPHER_NAMESPACE(hmac_sha224_final)
#define hmac_sha224                    SYSUTILS_CIPHER_NAMESPACE(hmac_sha224)
#define hmac_sha256_init               SYSUTILS_CIPHER_NAMESPACE(hmac_sha256_init)
#define hmac_sha256_reinit             SYSUTILS_CIPHER_NAMESPACE(hmac_sha256_reinit)
#define hmac_sha256_update             SYSUTILS_CIPHER_NAMESPACE(hmac_sha256_update)
#define hmac_sha256_final              SYSUTILS_CIPHER_NAMESPACE(hmac_sha256_final)
#define hmac_sha256                    SYSUTILS_CIPHER_NAMESPACE(hmac_sha256)
#define hmac_sha384_init               SYSUTILS_CIPHER_NAMESPACE(hmac_sha384_init)
#define hmac_sha384_reinit             SYSUTILS_CIPHER_NAMESPACE(hmac_sha384_reinit)
#define hmac_sha384_update             SYSUTILS_CIPHER_NAMESPACE(hmac_sha384_update)
#define hmac_sha384_final              SYSUTILS_CIPHER_NAMESPACE(hmac_sha384_final)
#define hmac_sha384                    SYSUTILS_CIPHER_NAMESPACE(hmac_sha384)
#define hmac_sha512_init               SYSUTILS_CIPHER_NAMESPACE(hmac_sha512_init)
#define hmac_sha512_reinit             SYSUTILS_CIPHER_NAMESPACE(hmac_sha512_reinit)
#define hmac_sha512_update             SYSUTILS_CIPHER_NAMESPACE(hmac_sha512_update)
#define hmac_sha512_final              SYSUTILS_CIPHER_NAMESPACE(hmac_sha512_final)
#define hmac_sha512                    SYSUTILS_CIPHER_NAMESPACE(hmac_sha512)

// sha2.h
#define sha224_init                    SYSUTILS_CIPHER_NAMESPACE(sha224_init)
#define sha224_update                  SYSUTILS_CIPHER_NAMESPACE(sha224_update)
#define sha224_final                   SYSUTILS_CIPHER_NAMESPACE(sha224_final)
#define sha224                         SYSUTILS_CIPHER_NAMESPACE(sha224)
#define sha256_init                    SYSUTILS_CIPHER_NAMESPACE(sha256_init)
#define sha256_update                  SYSUTILS_CIPHER_NAMESPACE(sha256_update)
#define sha256_final                   SYSUTILS_CIPHER_NAMESPACE(sha256_final)
#define sha256                         SYSUTILS_CIPHER_NAMESPACE(sha256)
#define sha384_init                    SYSUTILS_CIPHER_NAMESPACE(sha384_init)
#define sha384_update                  SYSUTILS_CIPHER_NAMESPACE(sha384_update)
#define sha384_final                   SYSUTILS_CIPHER_NAMESPACE(sha384_final)
#define sha384                         SYSUTILS_CIPHER_NAMESPACE(sha384)
#define sha512_init                    SYSUTILS_CIPHER_NAMESPACE(sha512_init)
#define sha512_update                  SYSUTILS_CIPHER_NAMESPACE(sha512_update)
#define sha512_final                   SYSUTILS_CIPHER_NAMESPACE(sha512_final)
#define sha512                         SYSUTILS_CIPHER_NAMESPACE(sha512)

// aes.h
#define AES_init_ctx                   SYSUTILS_CIPHER_NAMESPACE(AES_init_ctx)
#define AES_init_ctx_iv                SYSUTILS_CIPHER_NAMESPACE(AES_init_ctx_iv)
#define AES_ctx_set_iv                 SYSUTILS_CIPHER_NAMESPACE(AES_ctx_set_iv)
#define AES_ECB_encrypt                SYSUTILS_CIPHER_NAMESPACE(AES_ECB_encrypt)
#define AES_ECB_decrypt                SYSUTILS_CIPHER_NAMESPACE(AES_ECB_decrypt)
#define AES_CBC_encrypt_buffer         SYSUTILS_CIPHER_NAMESPACE(AES_CBC_encrypt_buffer)
#define AES_CBC_decrypt_buffer         SYSUTILS_CIPHER_NAMESPACE(AES_CBC_decrypt_buffer)
#define AES_CTR_xcrypt_buffer          SYSUTILS_CIPHER_NAMESPACE(AES_CTR_xcrypt_buffer)

#endif /* __SYSUTILS_CIPHER_NAMESPACE_H__ */
