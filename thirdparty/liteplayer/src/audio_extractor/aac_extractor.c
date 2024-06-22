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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/aac_extractor.h"

#define TAG "[liteplayer]aac_extractor"

#define DEFAULT_AAC_PARSER_BUFFER_SIZE 2048

enum {
    ERR_AAC_NONE                          =   0,
    ERR_AAC_INDATA_UNDERFLOW              =  -1,
    ERR_AAC_NULL_POINTER                  =  -2,
    ERR_AAC_INVALID_ADTS_HEADER           =  -3,
    ERR_AAC_INVALID_ADIF_HEADER           =  -4,
    ERR_AAC_INVALID_FRAME                 =  -5,
    ERR_AAC_MPEG4_UNSUPPORTED             =  -6,
    ERR_AAC_CHANNEL_MAP                   =  -7,
    ERR_AAC_SYNTAX_ELEMENT                =  -8,

    ERR_AAC_DEQUANT                       =  -9,
    ERR_AAC_STEREO_PROCESS                = -10,
    ERR_AAC_PNS                           = -11,
    ERR_AAC_SHORT_BLOCK_DEINT             = -12,
    ERR_AAC_TNS                           = -13,
    ERR_AAC_IMDCT                         = -14,
    ERR_AAC_NCHANS_TOO_HIGH               = -15,

    ERR_AAC_SBR_INIT                      = -16,
    ERR_AAC_SBR_BITSTREAM                 = -17,
    ERR_AAC_SBR_DATA                      = -18,
    ERR_AAC_SBR_PCM_FORMAT                = -19,
    ERR_AAC_SBR_NCHANS_TOO_HIGH           = -20,
    ERR_AAC_SBR_SINGLERATE_UNSUPPORTED    = -21,

    ERR_AAC_RAWBLOCK_PARAMS               = -22,

    ERR_AAC_UNKNOWN                       = -9999,
};

enum {
    AAC_PROFILE_MP                        = 0,
    AAC_PROFILE_LC                        = 1,
    AAC_PROFILE_SSR                       = 2,
    AAC_NUM_PROFILES                      = 3,
};

#define ADTS_HEADER_BYTES   7
#define NUM_SAMPLE_RATES    12
#define NUM_DEF_CHAN_MAPS   8
#define NUM_ELEMENTS        8
#define MAX_NUM_PCE_ADIF    16

/* channel mapping (table 1.6.3.4) (-1 = unknown, so need to determine mapping based on rules in 8.5.1) */
static const int channelMapTab[NUM_DEF_CHAN_MAPS] = {
    -1, 1, 2, 3, 4, 5, 6, 8
};

/* sample rates (table 4.5.1) */
static const int sampRateTab[NUM_SAMPLE_RATES] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025,  8000
};

struct ADTSHeader {
    /* fixed */
    unsigned char id;                             /* MPEG bit - should be 1 */
    unsigned char layer;                          /* MPEG layer - should be 0 */
    unsigned char protectBit;                     /* 0 = CRC word follows, 1 = no CRC word */
    unsigned char profile;                        /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    unsigned char sampRateIdx;                    /* sample rate index range = [0, 11] */
    unsigned char privateBit;                     /* ignore */
    unsigned char channelConfig;                  /* 0 = implicit, >0 = use default table */
    unsigned char origCopy;                       /* 0 = copy, 1 = original */
    unsigned char home;                           /* ignore */

    /* variable */
    unsigned char copyBit;                        /* 1 bit of the 72-bit copyright ID (transmitted as 1 bit per frame) */
    unsigned char copyStart;                      /* 1 = this bit starts the 72-bit ID, 0 = it does not */
    int           frameLength;                    /* length of frame */
    int           bufferFull;                     /* number of 32-bit words left in enc buffer, 0x7FF = VBR */
    unsigned char numRawDataBlocks;               /* number of raw data blocks in frame */

    /* CRC */
    int           crcCheckWord;                   /* 16-bit CRC check word (present if protectBit == 0) */
};

struct BitStreamInfo {
    unsigned char *bytePtr;
    unsigned int iCache;
    int cachedBits;
    int nBytes;
};

/**************************************************************************************
 * Function:    SetBitstreamPointer
 *
 * Description: initialize bitstream reader
 *
 * Inputs:      pointer to BitStreamInfo struct
 *              number of bytes in bitstream
 *              pointer to byte-aligned buffer of data to read from
 *
 * Outputs:     initialized bitstream info struct
 *
 * Return:      none
 **************************************************************************************/
static void SetBitstreamPointer(struct BitStreamInfo *bsi, int nBytes, unsigned char *buf)
{
    /* init bitstream */
    bsi->bytePtr = buf;
    bsi->iCache = 0;        /* 4-byte unsigned int */
    bsi->cachedBits = 0;    /* i.e. zero bits in cache */
    bsi->nBytes = nBytes;
}

/**************************************************************************************
 * Function:    RefillBitstreamCache
 *
 * Description: read new data from bitstream buffer into 32-bit cache
 *
 * Inputs:      pointer to initialized BitStreamInfo struct
 *
 * Outputs:     updated bitstream info struct
 *
 * Return:      none
 *
 * Notes:       only call when iCache is completely drained (resets bitOffset to 0)
 *              always loads 4 new bytes except when bsi->nBytes < 4 (end of buffer)
 *              stores data as big-endian in cache, regardless of machine endian-ness
 **************************************************************************************/
//Optimized for REV16, REV32 (FB)
static __inline void RefillBitstreamCache(struct BitStreamInfo *bsi)
{
    int nBytes = bsi->nBytes;
    if (nBytes >= 4) {
        /* optimize for common case, independent of machine endian-ness */
        bsi->iCache  = (*bsi->bytePtr++) << 24;
        bsi->iCache |= (*bsi->bytePtr++) << 16;
        bsi->iCache |= (*bsi->bytePtr++) <<  8;
        bsi->iCache |= (*bsi->bytePtr++);

        bsi->cachedBits = 32;
        bsi->nBytes -= 4;
    } else {
        bsi->iCache = 0;
        while (nBytes--) {
            bsi->iCache |= (*bsi->bytePtr++);
            bsi->iCache <<= 8;
        }
        bsi->iCache <<= ((3 - bsi->nBytes)*8);
        bsi->cachedBits = 8*bsi->nBytes;
        bsi->nBytes = 0;
    }
}

/**************************************************************************************
 * Function:    GetBits
 *
 * Description: get bits from bitstream, advance bitstream pointer
 *
 * Inputs:      pointer to initialized BitStreamInfo struct
 *              number of bits to get from bitstream
 *
 * Outputs:     updated bitstream info struct
 *
 * Return:      the next nBits bits of data from bitstream buffer
 *
 * Notes:       nBits must be in range [0, 31], nBits outside this range masked by 0x1f
 *              for speed, does not indicate error if you overrun bit buffer 
 *              if nBits == 0, returns 0
 **************************************************************************************/
static unsigned int GetBits(struct BitStreamInfo *bsi, int nBits)
{
    unsigned int data, lowBits;

    nBits &= 0x1f;                          /* nBits mod 32 to avoid unpredictable results like >> by negative amount */
    data = bsi->iCache >> (31 - nBits);     /* unsigned >> so zero-extend */
    data >>= 1;                             /* do as >> 31, >> 1 so that nBits = 0 works okay (returns 0) */
    bsi->iCache <<= nBits;                  /* left-justify cache */
    bsi->cachedBits -= nBits;               /* how many bits have we drawn from the cache so far */

    /* if we cross an int boundary, refill the cache */
    if (bsi->cachedBits < 0) {
        lowBits = -bsi->cachedBits;
        RefillBitstreamCache(bsi);
        data |= bsi->iCache >> (32 - lowBits);      /* get the low-order bits */

        bsi->cachedBits -= lowBits;         /* how many bits have we drawn from the cache so far */
        bsi->iCache <<= lowBits;            /* left-justify cache */
    }

    return data;
}

static int aac_find_adts_syncword(char *buf, int size)
{
    if (size < 2)
        return -1;

    /* find byte-aligned syncword - need 12 (MPEG 1,2) or 11 (MPEG 2.5) matching bits */
    for (int i = 0; i < size - 1; i++) {
        if ((buf[i] & 0xFF) == 0xFF && (buf[i+1] & 0xF0) == 0xF0)
            return i;
    }
    return -1;
}

int aac_parse_adts_frame(char *buf, int buf_size, struct aac_info *info)
{
    struct BitStreamInfo bsi;
    struct ADTSHeader fhADTS = {0};

    if (buf_size < 9) {
        OS_LOGE(TAG, "Not enough data to parse");
        return -1;
    }

    /* init bitstream reader */
    SetBitstreamPointer(&bsi, 9, (unsigned char *)buf);

    /* verify that first 12 bits of header are syncword */
    if (GetBits(&bsi, 12) != 0x0FFF) {
        OS_LOGE(TAG, "Not a valid AAC header[0x%X]", GetBits(&bsi, 12));
        return ERR_AAC_INVALID_ADTS_HEADER;
    }

    /* fixed fields - should not change from frame to frame */
    fhADTS.id =               GetBits(&bsi, 1);
    fhADTS.layer =            GetBits(&bsi, 2);
    fhADTS.protectBit =       GetBits(&bsi, 1);
    fhADTS.profile =          GetBits(&bsi, 2);
    fhADTS.sampRateIdx =      GetBits(&bsi, 4);
    fhADTS.privateBit =       GetBits(&bsi, 1);
    fhADTS.channelConfig =    GetBits(&bsi, 3);
    fhADTS.origCopy =         GetBits(&bsi, 1);
    fhADTS.home =             GetBits(&bsi, 1);

    /* variable fields - can change from frame to frame */
    fhADTS.copyBit =          GetBits(&bsi, 1);
    fhADTS.copyStart =        GetBits(&bsi, 1);
    fhADTS.frameLength =      GetBits(&bsi, 13);
    fhADTS.bufferFull =       GetBits(&bsi, 11);
    fhADTS.numRawDataBlocks = GetBits(&bsi, 2) + 1;

    /* note - MPEG4 spec, correction 1 changes how CRC is handled when protectBit == 0 and numRawDataBlocks > 1 */
    if (fhADTS.protectBit == 0)
        fhADTS.crcCheckWord = GetBits(&bsi, 16);

    /* check validity of header */
    if (fhADTS.layer != 0 ||
        fhADTS.profile != AAC_PROFILE_LC ||
        fhADTS.sampRateIdx >= NUM_SAMPLE_RATES ||
        fhADTS.channelConfig >= NUM_DEF_CHAN_MAPS) {
        OS_LOGE(TAG, "Validity check fail for AAC header");
        return ERR_AAC_INVALID_ADTS_HEADER;
    }

    //if (fhADTS.id != 0) {
    //    OS_LOGE(TAG, "AAC not MPEG4 supported");
    //    return ERR_AAC_MPEG4_UNSUPPORTED;
    //}

    /* update codec info */
    info->channels = channelMapTab[fhADTS.channelConfig];
    info->sample_rate = sampRateTab[fhADTS.sampRateIdx];
    info->frame_size = fhADTS.frameLength;
    return 0;
}

static void aac_dump_info(struct aac_info *info)
{
    OS_LOGD(TAG, "AAC INFO:");
    OS_LOGD(TAG, "  >channels          : %d", info->channels);
    OS_LOGD(TAG, "  >sample_rate       : %d", info->sample_rate);
    OS_LOGD(TAG, "  >frame_start_offset: %d", info->frame_start_offset);
}

int aac_extractor(aac_fetch_cb fetch_cb, void *fetch_priv, struct aac_info *info)
{
    int frame_start_offset = 0;
    int id3v2_len = 0;
    bool found = false;
    char buf[DEFAULT_AAC_PARSER_BUFFER_SIZE];
    int buf_size = sizeof(buf);

    buf_size = fetch_cb(buf, buf_size, 0, fetch_priv);
    if (buf_size < 9) {
        OS_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
        goto finish;
    }

    if (strncmp((const char *)buf, "ID3", 3) == 0) {
        id3v2_len =
                ((((int)(buf[6])) & 0x7F) << 21) +
                ((((int)(buf[7])) & 0x7F) << 14) +
                ((((int)(buf[8])) & 0x7F) <<  7) +
                 (((int)(buf[9])) & 0x7F);

        frame_start_offset = id3v2_len + 10;
        OS_LOGV(TAG, "ID3 tag find with length[%d]", id3v2_len);
    }

    if (frame_start_offset + 9 <= buf_size) {
        int ret = aac_parse_adts_frame(&buf[frame_start_offset], 9, info);
        if (ret == 0) {
            found = true;
            goto finish;
        }
    }

    if (frame_start_offset != 0) {
        OS_LOGV(TAG, "Request more data to parse frame header");
        buf_size = sizeof(buf);
        buf_size = fetch_cb(buf, buf_size, frame_start_offset, fetch_priv);
        if (buf_size < 9) {
            OS_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
            goto finish;
        }
    }

    int last_position = 0;
    int sync_offset = 0;

find_syncword:
    if (last_position + 9 > buf_size) {
        OS_LOGE(TAG, "Not enough data[%d] to parse", buf_size);
        goto finish;
    }

    sync_offset = aac_find_adts_syncword(&buf[last_position], buf_size - last_position);
    if (sync_offset >= 0) {
        last_position += sync_offset;
        int ret = aac_parse_adts_frame(&buf[last_position], 9, info);
        if (ret == 0) {
            found = true;
            goto finish;
        } else {
            OS_LOGV(TAG, "Retry to find sync word");
            last_position++;
            goto find_syncword;
        }
    } else {
        OS_LOGE(TAG, "Can't find aac sync word");
        goto finish;
    }

finish:
    if (found) {
        info->frame_start_offset = frame_start_offset;
        aac_dump_info(info);
    }
    return found ? 0 : -1;
}
