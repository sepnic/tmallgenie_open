/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This header file includes all of the fix point signal processing library (SPL) function
 * descriptions and declarations.
 * For specific function calls, see bottom of file.
 */

#ifndef WEBRTC_SPL_SIGNAL_PROCESSING_LIBRARY_H_
#define WEBRTC_SPL_SIGNAL_PROCESSING_LIBRARY_H_

#include <string.h>
#include "typedefs.h"

// Macros specific for the fixed point implementation
#define WEBRTC_SPL_WORD16_MAX       32767
#define WEBRTC_SPL_WORD16_MIN       -32768
#define WEBRTC_SPL_WORD32_MAX       (int32_t)0x7fffffff
#define WEBRTC_SPL_WORD32_MIN       (int32_t)0x80000000
#define WEBRTC_SPL_MAX_LPC_ORDER    14
#define WEBRTC_SPL_MIN(A, B)        (A < B ? A : B)  // Get min value
#define WEBRTC_SPL_MAX(A, B)        (A > B ? A : B)  // Get max value
// TODO(kma/bjorn): For the next two macros, investigate how to correct the code
// for inputs of a = WEBRTC_SPL_WORD16_MIN or WEBRTC_SPL_WORD32_MIN.
#define WEBRTC_SPL_ABS_W16(a) \
    (((int16_t)a >= 0) ? ((int16_t)a) : -((int16_t)a))
#define WEBRTC_SPL_ABS_W32(a) \
    (((int32_t)a >= 0) ? ((int32_t)a) : -((int32_t)a))

#define WEBRTC_SPL_MUL(a, b) \
    ((int32_t) ((int32_t)(a) * (int32_t)(b)))
#define WEBRTC_SPL_UMUL(a, b) \
    ((uint32_t) ((uint32_t)(a) * (uint32_t)(b)))
#define WEBRTC_SPL_UMUL_32_16(a, b) \
    ((uint32_t) ((uint32_t)(a) * (uint16_t)(b)))
#define WEBRTC_SPL_MUL_16_U16(a, b) \
    ((int32_t)(int16_t)(a) * (uint16_t)(b))

#ifndef WEBRTC_ARCH_ARM_V7
// For ARMv7 platforms, these are inline functions in spl_inl_armv7.h
#ifndef MIPS32_LE
// For MIPS platforms, these are inline functions in spl_inl_mips.h
#define WEBRTC_SPL_MUL_16_16(a, b) \
    ((int32_t) (((int16_t)(a)) * ((int16_t)(b))))
#define WEBRTC_SPL_MUL_16_32_RSFT16(a, b) \
    (WEBRTC_SPL_MUL_16_16(a, b >> 16) \
     + ((WEBRTC_SPL_MUL_16_16(a, (b & 0xffff) >> 1) + 0x4000) >> 15))
#endif
#endif

#define WEBRTC_SPL_MUL_16_32_RSFT11(a, b) \
    ((WEBRTC_SPL_MUL_16_16(a, (b) >> 16) << 5) \
    + (((WEBRTC_SPL_MUL_16_U16(a, (uint16_t)(b)) >> 1) + 0x0200) >> 10))
#define WEBRTC_SPL_MUL_16_32_RSFT14(a, b) \
    ((WEBRTC_SPL_MUL_16_16(a, (b) >> 16) << 2) \
    + (((WEBRTC_SPL_MUL_16_U16(a, (uint16_t)(b)) >> 1) + 0x1000) >> 13))
#define WEBRTC_SPL_MUL_16_32_RSFT15(a, b) \
    ((WEBRTC_SPL_MUL_16_16(a, (b) >> 16) << 1) \
    + (((WEBRTC_SPL_MUL_16_U16(a, (uint16_t)(b)) >> 1) + 0x2000) >> 14))

#define WEBRTC_SPL_MUL_16_16_RSFT(a, b, c) \
    (WEBRTC_SPL_MUL_16_16(a, b) >> (c))

#define WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(a, b, c) \
    ((WEBRTC_SPL_MUL_16_16(a, b) + ((int32_t) \
                                  (((int32_t)1) << ((c) - 1)))) >> (c))

// C + the 32 most significant bits of A * B
#define WEBRTC_SPL_SCALEDIFF32(A, B, C) \
    (C + (B >> 16) * A + (((uint32_t)(0x0000FFFF & B) * A) >> 16))

#define WEBRTC_SPL_SAT(a, b, c)         (b > a ? a : b < c ? c : b)

// Shifting with negative numbers allowed
// Positive means left shift
#define WEBRTC_SPL_SHIFT_W32(x, c) \
    (((c) >= 0) ? ((x) << (c)) : ((x) >> (-(c))))

// Shifting with negative numbers not allowed
// We cannot do casting here due to signed/unsigned problem
#define WEBRTC_SPL_LSHIFT_W32(x, c)     ((x) << (c))

#define WEBRTC_SPL_RSHIFT_U32(x, c)     ((uint32_t)(x) >> (c))

#define WEBRTC_SPL_RAND(a) \
    ((int16_t)((((int16_t)a * 18816) >> 7) & 0x00007fff))

#ifdef __cplusplus
extern "C" {
#endif

// inline functions:
#include "spl_inl.h"

int16_t WebRtcSpl_GetScalingSquare(int16_t* in_vector,
                                   size_t in_vector_length,
                                   size_t times);

// Divisions. Implementations collected in division_operations.c and
// descriptions at bottom of this file.
uint32_t WebRtcSpl_DivU32U16(uint32_t num, uint16_t den);
int32_t WebRtcSpl_DivW32W16(int32_t num, int16_t den);
int16_t WebRtcSpl_DivW32W16ResW16(int32_t num, int16_t den);
int32_t WebRtcSpl_DivResultInQ31(int32_t num, int32_t den);
int32_t WebRtcSpl_DivW32HiLow(int32_t num, int16_t den_hi, int16_t den_low);
// End: Divisions.

int32_t WebRtcSpl_Energy(int16_t* vector,
                         size_t vector_length,
                         int* scale_factor);

/************************************************************
 *
 * RESAMPLING FUNCTIONS AND THEIR STRUCTS ARE DEFINED BELOW
 *
 ************************************************************/

/*******************************************************************
 * resample.c
 *
 * Includes the following resampling combinations
 * 22 kHz -> 16 kHz
 * 16 kHz -> 22 kHz
 * 22 kHz ->  8 kHz
 *  8 kHz -> 22 kHz
 *
 ******************************************************************/

// state structure for 22 -> 16 resampler
typedef struct {
  int32_t S_22_44[8];
  int32_t S_44_32[8];
  int32_t S_32_16[8];
} WebRtcSpl_State22khzTo16khz;

void WebRtcSpl_Resample22khzTo16khz(const int16_t* in,
                                    int16_t* out,
                                    WebRtcSpl_State22khzTo16khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample22khzTo16khz(WebRtcSpl_State22khzTo16khz* state);

// state structure for 16 -> 22 resampler
typedef struct {
  int32_t S_16_32[8];
  int32_t S_32_22[8];
} WebRtcSpl_State16khzTo22khz;

void WebRtcSpl_Resample16khzTo22khz(const int16_t* in,
                                    int16_t* out,
                                    WebRtcSpl_State16khzTo22khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample16khzTo22khz(WebRtcSpl_State16khzTo22khz* state);

// state structure for 22 -> 8 resampler
typedef struct {
  int32_t S_22_22[16];
  int32_t S_22_16[8];
  int32_t S_16_8[8];
} WebRtcSpl_State22khzTo8khz;

void WebRtcSpl_Resample22khzTo8khz(const int16_t* in, int16_t* out,
                                   WebRtcSpl_State22khzTo8khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample22khzTo8khz(WebRtcSpl_State22khzTo8khz* state);

// state structure for 8 -> 22 resampler
typedef struct {
  int32_t S_8_16[8];
  int32_t S_16_11[8];
  int32_t S_11_22[8];
} WebRtcSpl_State8khzTo22khz;

void WebRtcSpl_Resample8khzTo22khz(const int16_t* in, int16_t* out,
                                   WebRtcSpl_State8khzTo22khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample8khzTo22khz(WebRtcSpl_State8khzTo22khz* state);

/*******************************************************************
 * resample_fractional.c
 * Functions for internal use in the other resample functions
 *
 * Includes the following resampling combinations
 * 48 kHz -> 32 kHz
 * 32 kHz -> 24 kHz
 * 44 kHz -> 32 kHz
 *
 ******************************************************************/

void WebRtcSpl_Resample48khzTo32khz(const int32_t* In, int32_t* Out, size_t K);

void WebRtcSpl_Resample32khzTo24khz(const int32_t* In, int32_t* Out, size_t K);

void WebRtcSpl_Resample44khzTo32khz(const int32_t* In, int32_t* Out, size_t K);

/*******************************************************************
 * resample_48khz.c
 *
 * Includes the following resampling combinations
 * 48 kHz -> 16 kHz
 * 16 kHz -> 48 kHz
 * 48 kHz ->  8 kHz
 *  8 kHz -> 48 kHz
 *
 ******************************************************************/

typedef struct {
  int32_t S_48_48[16];
  int32_t S_48_32[8];
  int32_t S_32_16[8];
} WebRtcSpl_State48khzTo16khz;

void WebRtcSpl_Resample48khzTo16khz(const int16_t* in, int16_t* out,
                                    WebRtcSpl_State48khzTo16khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample48khzTo16khz(WebRtcSpl_State48khzTo16khz* state);

typedef struct {
  int32_t S_16_32[8];
  int32_t S_32_24[8];
  int32_t S_24_48[8];
} WebRtcSpl_State16khzTo48khz;

void WebRtcSpl_Resample16khzTo48khz(const int16_t* in, int16_t* out,
                                    WebRtcSpl_State16khzTo48khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample16khzTo48khz(WebRtcSpl_State16khzTo48khz* state);

typedef struct {
  int32_t S_48_24[8];
  int32_t S_24_24[16];
  int32_t S_24_16[8];
  int32_t S_16_8[8];
} WebRtcSpl_State48khzTo8khz;

void WebRtcSpl_Resample48khzTo8khz(const int16_t* in, int16_t* out,
                                   WebRtcSpl_State48khzTo8khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample48khzTo8khz(WebRtcSpl_State48khzTo8khz* state);

typedef struct {
  int32_t S_8_16[8];
  int32_t S_16_12[8];
  int32_t S_12_24[8];
  int32_t S_24_48[8];
} WebRtcSpl_State8khzTo48khz;

void WebRtcSpl_Resample8khzTo48khz(const int16_t* in, int16_t* out,
                                   WebRtcSpl_State8khzTo48khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample8khzTo48khz(WebRtcSpl_State8khzTo48khz* state);

/*******************************************************************
 * resample_by_2.c
 *
 * Includes down and up sampling by a factor of two.
 *
 ******************************************************************/

void WebRtcSpl_DownsampleBy2(const int16_t* in, size_t len,
                             int16_t* out, int32_t* filtState);

void WebRtcSpl_UpsampleBy2(const int16_t* in, size_t len,
                           int16_t* out, int32_t* filtState);

/************************************************************
 * END OF RESAMPLING FUNCTIONS
 ************************************************************/

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // WEBRTC_SPL_SIGNAL_PROCESSING_LIBRARY_H_
