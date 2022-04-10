/*
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef __SYSUTILS_MD5_H__
#define __SYSUTILS_MD5_H__

#include <stdint.h>
#include "cipher_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MD5Context {
  uint32_t buf[4];
  uint32_t bits[2];
  uint8_t in[64];
};

void MD5Init(struct MD5Context *ctx);

void MD5Update(struct MD5Context *ctx, unsigned char const *buf, size_t len);

void MD5Final(struct MD5Context *ctx, unsigned char digest[16]);

#ifdef __cplusplus
}
#endif

#endif  // __SYSUTILS_MD5_H__
