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

#include "osal/os_thread.h"
#include "cutils/log_helper.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"

#define TAG "[liteplayer]m4a_extractor"

// FIXME: If low memory, please reduce STSZ_MAX_BUFFER
//#define STSZ_MAX_BUFFER       (256*1024)

#define STREAM_BUFFER_SIZE    (2048)

#define M4A_PARSER_TASK_PRIO  (OS_THREAD_PRIO_HIGH)
#define M4A_PARSER_TASK_STACK (6144)

typedef enum aac_error {
    AAC_ERR_NONE          = -0x00,    /* no error */
    AAC_ERR_FAIL          = -0x01,    /* input buffer too small */
    AAC_ERR_UNSUPPORTED   = -0x02,    /* invalid (null) buffer pointer */
    AAC_ERR_NOMEM         = -0x03,    /* not enough memory */
    AAC_ERR_OPCODE        = -0x04,    /* opcode error */
    AAC_ERR_STARVE_0      = -0x05,    /* no data remaining, need more data */
    AAC_ERR_STARVE_1      = -0x06,    /* still have data left but not enough for continue handling */
    AAC_ERR_STARVE_2      = -0x07,    /* ATOM_DATA finish, no data remaining, need more data to process ATOM_NAME. */
    AAC_ERR_LOSTSYNC      = -0x08,    /* lost synchronization */
    AAC_ERR_AGAIN         = -0x09,    /* try again */
    AAC_ERR_EOF           = -0x0A,    /* EOF */
} AAC_ERR_T;

enum ATOM_TYPE {
    ATOM_STOP = 0,      /* end of atoms */
    ATOM_NAME,          /* plain atom */
    ATOM_DESCENT,       /* starts group of children */
    ATOM_ASCENT,        /* ends group */
    ATOM_DATA,
};

struct atom_box {
    int opcode;
    void *data;
};

struct atom_parser {
    ringbuf_handle      rb;
    uint8_t             data[STREAM_BUFFER_SIZE];
    uint32_t            offset;
    struct atom_box    *atom;
    struct m4a_info    *m4a_info;
};
typedef struct atom_parser *atom_parser_handle_t;

static inline uint32_t u32in(uint8_t *buf)
{
    return ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0));
}

static inline uint16_t u16in(uint8_t *buf)
{
    return ((buf[0] << 8) | (buf[1] << 0));
}

static inline uint8_t u8in(uint8_t *buf)
{
    return buf[0];
}

static inline void datain(uint8_t *buf_out, uint8_t *buf_in, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        buf_out[i] = u8in(buf_in); buf_in += 1;
    }
}

static int32_t atom_rb_read(atom_parser_handle_t handle, int32_t wanted_size)
{
    int32_t byte_read = 0;

    while (wanted_size > STREAM_BUFFER_SIZE) {
        byte_read = rb_read(handle->rb, (char *)handle->data, STREAM_BUFFER_SIZE, AUDIO_MAX_DELAY);
        if (byte_read < 0) {
            OS_LOGE(TAG, "Failed to read rb, ret=%d", byte_read);
            return byte_read;
        } else {
            handle->offset += byte_read;
            wanted_size -= byte_read;
        }
    }

    if (wanted_size != 0) {
        byte_read = rb_read_chunk(handle->rb, (char *)handle->data, wanted_size, AUDIO_MAX_DELAY);
        if (byte_read < 0) {
            OS_LOGE(TAG, "Failed to read rb, ret=%d", byte_read);
            return byte_read;
        } else {
            handle->offset += byte_read;
        }
    }

    return 0;
}

static AAC_ERR_T dummyin(atom_parser_handle_t handle, uint32_t atom_size)
{
    return atom_rb_read(handle, atom_size);
}

static AAC_ERR_T mdhdin(atom_parser_handle_t handle, uint32_t atom_size)
{
    uint8_t *buf = handle->data;
    struct m4a_info *m4a_info = handle->m4a_info;

    uint16_t wanted_byte = 6*sizeof(uint32_t);
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Creation time
    u32in(buf); buf += 4;
    // Modification time
    u32in(buf); buf += 4;
    // Time scale
    m4a_info->time_scale = u32in(buf); buf += 4;
    m4a_info->samplerate = m4a_info->time_scale; // fixup after AudioSpecificConfig
    // Duration
    m4a_info->duration = u32in(buf); buf += 4;
    // Language
    u16in(buf); buf += 2;
    // pre_defined
    u16in(buf); buf += 2;

    if (atom_size > wanted_byte)
        return atom_rb_read(handle, atom_size-wanted_byte);
    else
        return AAC_ERR_NONE;
}

static AAC_ERR_T hdlr1in(atom_parser_handle_t handle, uint32_t atom_size)
{
    uint8_t *buf = handle->data;
    uint16_t wanted_byte = 6*sizeof(uint32_t);

    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Component type
    u32in(buf); buf += 4;

    // Component subtype
    uint8_t subtype[4] = {0};
    datain(subtype, buf, 4); buf += 4;
    if (memcmp("soun", subtype, 4) != 0) {
        OS_LOGE(TAG, "hdlr error, expect subtype is soun, subtype=%s", subtype);
        return AAC_ERR_UNSUPPORTED;
    }

    // reserved
    u32in(buf); buf += 4;
    u32in(buf); buf += 4;
    u32in(buf); buf += 4;

    if (atom_size > wanted_byte)
        return atom_rb_read(handle, atom_size-wanted_byte);
    else
        return AAC_ERR_NONE;
}

static AAC_ERR_T stsdin(atom_parser_handle_t handle, uint32_t atom_size)
{
    uint8_t *buf = handle->data;
    uint16_t wanted_byte = 2*sizeof(uint32_t);

    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;

    uint32_t entries = u32in(buf); buf += 4;
    if (entries != 1) {
        OS_LOGE(TAG, "stsd error, number of entries should be 1, entries=%d", entries);
        return AAC_ERR_UNSUPPORTED;
    }
    return AAC_ERR_NONE;
};

static AAC_ERR_T mp4ain(atom_parser_handle_t handle, uint32_t atom_size)
{
    uint8_t *buf = handle->data;
    struct m4a_info *m4a_info = handle->m4a_info;

    uint16_t wanted_byte = 7*sizeof(uint32_t);
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // Reserved (6 bytes)
    u32in(buf); buf += 4;
    u16in(buf); buf += 2;
    // Data reference index
    u16in(buf); buf += 2;
    // Version
    u16in(buf); buf += 2;
    // Revision level
    u16in(buf); buf += 2;
    // Vendor
    u32in(buf); buf += 4;
    // Number of channels
    m4a_info->channels = u16in(buf); buf += 2; // fixup after AudioSpecificConfig
    // Sample size (bits)
    m4a_info->bits = u16in(buf); buf += 2;
    // Compression ID
    u16in(buf); buf += 2;
    // Packet size
    u16in(buf); buf += 2;
    // Sample rate (16.16)
    // fractional framerate, probably not for audio
    // rate integer part
    u16in(buf); buf += 2;
    // rate reminder part
    u16in(buf); buf += 2;

    return AAC_ERR_NONE;
}

static uint32_t getsize(uint8_t *buf, uint8_t *read)
{
    uint8_t cnt = 0;
    uint32_t size = 0;
    for (cnt = 0; cnt < 4; cnt++) {
        uint8_t tmp = u8in(buf); buf += 1;

        size <<= 7;
        size |= (tmp & 0x7F);
        if (!(tmp & 0x80)) {
            break;
        }
    }
    *read = cnt + 1;
    return size;
}

static AAC_ERR_T esdsin(atom_parser_handle_t handle, uint32_t atom_size)
{
    // descriptor tree:
    // MP4ES_Descriptor
    //   MP4DecoderConfigDescriptor
    //      MP4DecSpecificInfoDescriptor
    //   MP4SLConfigDescriptor
    enum {
        MP4ESDescrTag = 3,
        MP4DecConfigDescrTag = 4,
        MP4DecSpecificDescrTag = 5,
        MP4SLConfigDescrTag = 6,
    };

    uint8_t read = 0;
    uint8_t *buf = handle->data;
    struct m4a_info *m4a_info = handle->m4a_info;

    int32_t ret = atom_rb_read(handle, atom_size);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    
    if (u8in(buf) != MP4ESDescrTag) {  // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    getsize(buf, &read); buf += read;

    // ESID
    u16in(buf); buf += 2;
    // flags(url(bit 6); ocr(5); streamPriority (0-4)):
    u8in(buf); buf += 1;

    if (u8in(buf) != MP4DecConfigDescrTag) { // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    getsize(buf, &read); buf += read;
    if (u8in(buf) != 0x40) { /* not MPEG-4 audio */ // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    // stream type
    u8in(buf); buf += 1;
    // buffer size db (24 bits)
    u16in(buf); buf += 2;
    u8in(buf); buf += 1;
    // bitrate
    m4a_info->bitrate_max = u32in(buf); buf += 4;
    m4a_info->bitrate_avg = u32in(buf); buf += 4;

    if (u8in(buf) != MP4DecSpecificDescrTag) {// 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    m4a_info->asc.size = getsize(buf, &read); buf += read;
    if (m4a_info->asc.size > sizeof(m4a_info->asc.buf)) {
        return AAC_ERR_FAIL;
    }

    // get AudioSpecificConfig
    datain(m4a_info->asc.buf, buf, m4a_info->asc.size); // max 16
    buf += m4a_info->asc.size;

    if (u8in(buf) != MP4SLConfigDescrTag) { // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    getsize(buf, &read); buf += read;

    // "predefined" (no idea)
    u8in(buf); buf += 1;

    return AAC_ERR_NONE;
}

static AAC_ERR_T sttsin(atom_parser_handle_t handle, uint32_t atom_size)
{
    struct m4a_info *m4a_info = handle->m4a_info;
    uint16_t wanted_byte = 2*sizeof(uint32_t);
    uint32_t remain_byte = atom_size - wanted_byte;
    uint8_t *buf = handle->data;
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;

    m4a_info->stts_time2sample_entries = u32in(buf); buf += 4;
    m4a_info->stts_time2sample =
        audio_calloc(m4a_info->stts_time2sample_entries, sizeof(struct time2sample));
    if (m4a_info->stts_time2sample == NULL) {
        return AAC_ERR_NOMEM;
    }

    for (uint32_t cnt = 0; cnt < m4a_info->stts_time2sample_entries; cnt++) {
        wanted_byte = 2*sizeof(uint32_t);
        remain_byte -= wanted_byte;
        ret = atom_rb_read(handle, wanted_byte);
        AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

        buf = handle->data;
        m4a_info->stts_time2sample[cnt].sample_count = u32in(buf); buf += 4;
        m4a_info->stts_time2sample[cnt].sample_duration = u32in(buf); buf += 4;

        OS_LOGV(TAG, "stts_time2sample[%d]: sample_count/sample_duration: %u:%u",
                cnt,
                m4a_info->stts_time2sample[cnt].sample_count,
                m4a_info->stts_time2sample[cnt].sample_duration);
    }
    return atom_rb_read(handle, remain_byte);
}

static AAC_ERR_T stscin(atom_parser_handle_t handle, uint32_t atom_size)
{
    struct m4a_info *m4a_info = handle->m4a_info;
    uint16_t wanted_byte = 2*sizeof(uint32_t);
    uint32_t remain_byte = atom_size - wanted_byte;
    uint8_t *buf = handle->data;
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;

    m4a_info->stsc_sample2chunk_entries = u32in(buf); buf += 4;
    m4a_info->stsc_sample2chunk =
        audio_calloc(m4a_info->stsc_sample2chunk_entries, sizeof(struct sample2chunk));
    if (m4a_info->stsc_sample2chunk == NULL) {
        return AAC_ERR_NOMEM;
    }

    for (uint32_t cnt = 0; cnt < m4a_info->stsc_sample2chunk_entries; cnt++) {
        wanted_byte = 3*sizeof(uint32_t);
        remain_byte -= wanted_byte;
        ret = atom_rb_read(handle, wanted_byte);
        AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

        buf = handle->data;
        m4a_info->stsc_sample2chunk[cnt].first_chunk = u32in(buf); buf += 4;
        m4a_info->stsc_sample2chunk[cnt].samples_per_chunk = u32in(buf); buf += 4;
        m4a_info->stsc_sample2chunk[cnt].sample_description_index = u32in(buf); buf += 4;

        OS_LOGV(TAG, "stsc_sample2chunk[%d]: first_chunk/samples_per_chunk/sample_description_index: %u:%u:%u",
                cnt,
                m4a_info->stsc_sample2chunk[cnt].first_chunk,
                m4a_info->stsc_sample2chunk[cnt].samples_per_chunk,
                m4a_info->stsc_sample2chunk[cnt].sample_description_index);
    }
    return atom_rb_read(handle, remain_byte);
}

static AAC_ERR_T stszin(atom_parser_handle_t handle, uint32_t atom_size)
{
    struct m4a_info *m4a_info = handle->m4a_info;
    uint16_t wanted_byte = 3*sizeof(uint32_t);
    uint32_t remain_byte = atom_size - wanted_byte;
    uint8_t *buf = handle->data;
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Sample size
    u32in(buf); buf += 4;
    // Number of entries
    m4a_info->stsz_samplesize_entries = u32in(buf);  buf += 4;

    /**
    * To save memeory, we assuem all frame size is 16bit width(not bigger than 0xFFFF)
    * So the moment we got frame count(stszsize), we'll check how many memory is needed
    * to store stsz header. And return fail if bigger than default buffer.
    */
#if defined(STSZ_MAX_BUFFER) && (STSZ_MAX_BUFFER > 0)
    if (m4a_info->stsz_samplesize_entries*sizeof(int16_t) > STSZ_MAX_BUFFER) {
        OS_LOGE(TAG, "Large STSZ(%u), out of memory", (uint32_t)(m4a_info->stsz_samplesize_entries*sizeof(int16_t)));
        return AAC_ERR_NOMEM;
    }
#endif
    m4a_info->stsz_samplesize = audio_calloc(m4a_info->stsz_samplesize_entries, sizeof(uint16_t));
    if (m4a_info->stsz_samplesize == NULL) {
        return AAC_ERR_NOMEM;
    }

    uint32_t sample_size = 0;
    for (int32_t cnt = 0; cnt < m4a_info->stsz_samplesize_entries; cnt++) {
        remain_byte -= 4;
        ret = atom_rb_read(handle, sizeof(uint32_t));
        AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

        buf = handle->data;
        sample_size = u32in(buf);
        if (m4a_info->stsz_samplesize_max < sample_size) {
            if (sample_size > 0xFFFF) {
                OS_LOGE(TAG, "Large samplesize(%u), out of max size of short", sample_size);
                return AAC_ERR_FAIL;
            }
            m4a_info->stsz_samplesize_max = sample_size;
        }
        m4a_info->stsz_samplesize[cnt] = sample_size & 0xFFFF;
    }

    OS_LOGV(TAG, "STSZ max sample size: %u", m4a_info->stsz_samplesize_max);
    if (m4a_info->stsz_samplesize_max > 0xFFFF) {
        return AAC_ERR_UNSUPPORTED;
    }

    return atom_rb_read(handle, remain_byte);;
}

static AAC_ERR_T stcoin(atom_parser_handle_t handle, uint32_t atom_size)
{
    uint8_t *buf = handle->data;
    struct m4a_info *m4a_info = handle->m4a_info;
    uint16_t wanted_byte = 3*sizeof(uint32_t);
    uint32_t remain_byte = atom_size - wanted_byte;

    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;

    // Number of entries
    m4a_info->stco_chunk2offset_entries = u32in(buf); buf += 4;
    m4a_info->stco_chunk2offset =
        audio_calloc(m4a_info->stco_chunk2offset_entries, sizeof(struct chunk2offset));
    if (m4a_info->stco_chunk2offset == NULL) {
        return AAC_ERR_NOMEM;
    }

    // first chunk offset
    uint32_t offset = u32in(buf); buf += 4;
    uint32_t new_chunk = 1, old_chunk = 1;
    uint32_t old_first = m4a_info->stsc_sample2chunk[0].first_chunk;
    uint32_t new_first = m4a_info->stsc_sample2chunk[1].first_chunk;
    uint32_t old_samples = m4a_info->stsc_sample2chunk[0].samples_per_chunk;
    uint32_t new_samples = 0;
    uint32_t idx = 0;
    for (int32_t cnt = 1; cnt < m4a_info->stco_chunk2offset_entries; cnt++) {
        for (; new_chunk < m4a_info->stsc_sample2chunk_entries; new_chunk++) {
            if (new_chunk > old_chunk) {
                old_first = m4a_info->stsc_sample2chunk[new_chunk-1].first_chunk;
                old_samples = m4a_info->stsc_sample2chunk[new_chunk-1].samples_per_chunk;
                new_first = m4a_info->stsc_sample2chunk[new_chunk].first_chunk;
                old_chunk = new_chunk;
            }
            if (new_first > cnt)
                break;
            new_samples = (new_first - old_first) * old_samples;
        }
        new_samples += (cnt - old_first) * old_samples;

        m4a_info->stco_chunk2offset[idx].sample_index = new_samples;
        m4a_info->stco_chunk2offset[idx].chunk_offset = offset;
        idx++;
        //OS_LOGV(TAG, "STCO: ChunkOffset[%u]: index/offset: %u/%u", idx, new_samples, offset);

        new_samples -= (cnt - old_first) * old_samples;

        remain_byte -= 4;
        ret = atom_rb_read(handle, sizeof(uint32_t));
        AUDIO_ERR_CHECK(TAG, ret == 0, return ret);
        offset = u32in(handle->data);
    }
    m4a_info->stco_chunk2offset[idx].sample_index = 0;
    m4a_info->stco_chunk2offset[idx].chunk_offset = 0;

    m4a_info->mdat_offset = m4a_info->stco_chunk2offset[0].chunk_offset;

    return atom_rb_read(handle, remain_byte);
}

static AAC_ERR_T atom_parse(atom_parser_handle_t handle)
{
    uint8_t *buf = NULL;
    int32_t ret = AAC_ERR_NONE;

    if (handle->atom->opcode == ATOM_DESCENT) {
        OS_LOGV(TAG, "Atom is descent");
        return AAC_ERR_NONE;
    } else if (handle->atom->opcode == ATOM_ASCENT) {
        OS_LOGV(TAG, "Atom is ascent");
        return AAC_ERR_NONE;
    }

    if (handle->atom->opcode != ATOM_NAME) {
        OS_LOGE(TAG, "Invalid opcode, expect ATOM_NAME");
        return AAC_ERR_OPCODE;
    } else {
        OS_LOGV(TAG, "Looking for '%s' at offset[%u]", (char *)handle->atom->data, handle->offset);
    }

_next_atom:
    buf = handle->data;
    ret = atom_rb_read(handle, 8);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    uint8_t atom_name[4] = {0};
    uint32_t atom_size = 0;

    atom_size = u32in(buf); buf += 4;
    datain(atom_name, buf, 4); buf += 4;

    OS_LOGV(TAG, "atom[%s], size[%u], offset[%u]", atom_name, atom_size, handle->offset);
    if (memcmp(atom_name, handle->atom->data, sizeof(atom_name)) == 0) {
        OS_LOGV(TAG, "----OK----");
        goto atom_found;
    } else {
        if (atom_size > 8) {
            ret = atom_rb_read(handle, atom_size-8);
            AUDIO_ERR_CHECK(TAG, ret == 0, return ret);
        }
        goto _next_atom;
    }

atom_found:
    handle->atom++;
    if (handle->atom->opcode == ATOM_DESCENT) {
        OS_LOGV(TAG, "Atom is descent");
        return AAC_ERR_NONE;
    }

    if (handle->atom->opcode != ATOM_DATA) {
        OS_LOGE(TAG, "Invalid opcode, expect ATOM_DATA");
        return AAC_ERR_OPCODE;
    }

    int32_t err = ((AAC_ERR_T(*)(atom_parser_handle_t, uint32_t))handle->atom->data)(handle, atom_size-8);
    return err;
}

static AAC_ERR_T moovin(atom_parser_handle_t handle, uint32_t atom_size)
{
    AAC_ERR_T err;

    static struct atom_box mvhd[] = {
        {ATOM_NAME, "mvhd"},
        {ATOM_DATA, dummyin},
    };
    static struct atom_box trak[] = {
        {ATOM_NAME, "trak"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "tkhd"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "mdia"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "mdhd"},
        {ATOM_DATA, mdhdin},
        {ATOM_NAME, "hdlr"},
        {ATOM_DATA, hdlr1in},
        {ATOM_NAME, "minf"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "smhd"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "dinf"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "stbl"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "stsd"},
        {ATOM_DATA, stsdin},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "mp4a"},
        {ATOM_DATA, mp4ain},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "esds"},
        {ATOM_DATA, esdsin},
        {ATOM_ASCENT, NULL},
        {ATOM_ASCENT, NULL},
        {ATOM_NAME, "stts"},
        {ATOM_DATA, sttsin},
        {ATOM_NAME, "stsc"},
        {ATOM_DATA, stscin},
        {ATOM_NAME, "stsz"},
        {ATOM_DATA, stszin},
        {ATOM_NAME, "stco"},
        {ATOM_DATA, stcoin},
        {0}
    };

    handle->atom = mvhd;
    err = atom_parse(handle);
    if (err != AAC_ERR_NONE) {
        OS_LOGE(TAG, "Failed to parse mvhd atom");
        return err;
    }

    handle->atom = trak;
    while (1) {
        if (handle->atom->opcode == 0){
            OS_LOGV(TAG, "Finisehd to parse trak atom");
            break;
        }

        err = atom_parse(handle);
        if (err != AAC_ERR_NONE) {
            OS_LOGE(TAG, "Failed to parse trak atom");
            return err;
        }

        handle->atom++;
    }

    return AAC_ERR_NONE;
}

static int m4a_parse_asc(struct m4a_info *m4a_info)
{
    if (m4a_info->asc.size >= 2) {
        static const uint32_t sample_rates[] = {
            96000, 88200, 64000, 48000, 44100, 32000,
            24000, 22050, 16000, 12000, 11025, 8000
        };
        uint16_t config = (m4a_info->asc.buf[0] << 8 | m4a_info->asc.buf[1]);
        uint8_t sample_rate_index = (config >> 7) & 0x0f;
        uint8_t channels_num = (config >> 3) & 0x07;
        if (sample_rate_index < 12) {
            m4a_info->asc.samplerate = sample_rates[sample_rate_index];
            m4a_info->asc.channels = channels_num;
            return AAC_ERR_NONE;
        }
    }
    return AAC_ERR_FAIL;
}

static void m4a_dump_info(struct m4a_info *m4a_info)
{
    OS_LOGD(TAG, "M4A INFO:");
    OS_LOGD(TAG, "  >Channels             : %u", m4a_info->channels);
    OS_LOGD(TAG, "  >Sampling rate        : %u", m4a_info->samplerate);
    OS_LOGD(TAG, "  >Bits per sample      : %u", m4a_info->bits);
    OS_LOGD(TAG, "  >Max sample size      : %u", m4a_info->stsz_samplesize_max);
    OS_LOGD(TAG, "  >Max bitrate          : %u", m4a_info->bitrate_max);
    OS_LOGD(TAG, "  >Average bitrate      : %u", m4a_info->bitrate_avg);
    OS_LOGD(TAG, "  >ASC buff             : %x:%x:%x:%x:%x:%x:%x",
            m4a_info->asc.buf[0], m4a_info->asc.buf[1], m4a_info->asc.buf[2],
            m4a_info->asc.buf[3], m4a_info->asc.buf[4], m4a_info->asc.buf[5],
            m4a_info->asc.buf[6]);
    OS_LOGD(TAG, "  >ASC size             : %u", m4a_info->asc.size);
    OS_LOGD(TAG, "  >ASC sampling rate    : %u", m4a_info->asc.samplerate);
    OS_LOGD(TAG, "  >ASC channels         : %u", m4a_info->asc.channels);
    OS_LOGD(TAG, "  >Sample timescale     : %u", m4a_info->stts_time2sample[0].sample_duration);
    OS_LOGD(TAG, "  >Duration             : %.1f sec", (float)m4a_info->duration/m4a_info->time_scale);
    OS_LOGD(TAG, "  >MDAT offset/size     : %u/%u", m4a_info->mdat_offset, m4a_info->mdat_size);
    OS_LOGD(TAG, "  >STSZ entries         : %u", m4a_info->stsz_samplesize_entries);
    OS_LOGD(TAG, "  >STTS entries         : %u", m4a_info->stts_time2sample_entries);
    OS_LOGD(TAG, "  >STSC entries         : %u", m4a_info->stsc_sample2chunk_entries);
    OS_LOGD(TAG, "  >STCO entries         : %u", m4a_info->stco_chunk2offset_entries);
}

static AAC_ERR_T m4a_check_header(atom_parser_handle_t handle)
{
    uint8_t *buf = handle->data;
    uint32_t atom_size = 0;
    uint8_t atom_name[4] = {0};
    uint16_t wanted_byte = 2*sizeof(uint32_t);
    uint32_t offset = 0;
    int32_t ret = 0;

    handle->m4a_info->parsed_once = true;

    ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return AAC_ERR_FAIL);

    atom_size = u32in(buf); buf += 4;
    datain(atom_name, buf, 4); buf += 4;

    OS_LOGV(TAG, "atom[%s], size[%u], offset[%u]", atom_name, atom_size, handle->offset);
    if (memcmp(atom_name, "ftyp", 4) != 0) {
        OS_LOGE(TAG, "Not M4A audio");
        return AAC_ERR_UNSUPPORTED;
    }

next_atom:
    offset += atom_size;

    if (atom_size > wanted_byte) {
        ret = atom_rb_read(handle, atom_size-wanted_byte);
        AUDIO_ERR_CHECK(TAG, ret == 0, return AAC_ERR_FAIL);
    }

    buf = handle->data;
    ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return AAC_ERR_FAIL);

    atom_size = u32in(buf); buf += 4;
    datain(atom_name, buf, 4); buf += 4;

    OS_LOGV(TAG, "atom[%s], size[%u], offset[%u]", atom_name, atom_size, handle->offset);
    if (memcmp(atom_name, "mdat", 4) == 0) {
        OS_LOGV(TAG, "moov behide of mdat: mdat_offset=%u, mdat_size=%u", offset, atom_size);
        handle->m4a_info->mdat_size = atom_size;
        handle->m4a_info->mdat_offset = offset;
        handle->m4a_info->moov_offset = handle->m4a_info->mdat_offset + handle->m4a_info->mdat_size;
        handle->m4a_info->moov_tail = true;
        rb_reset(handle->rb);
        return AAC_ERR_AGAIN;
    } else if (memcmp(atom_name, "moov", 4) == 0) {
        OS_LOGV(TAG, "moov ahead of mdat");
        handle->m4a_info->moov_tail = false;
        return moovin(handle, 8);
    } else {
        goto next_atom;
    }

    return AAC_ERR_FAIL;
}

int m4a_parse_header(ringbuf_handle rb, struct m4a_info *info)
{
    static struct atom_box moov[] = {
        {ATOM_NAME, "moov"},
        {ATOM_DATA, moovin},
        {0}
    };

    struct atom_parser parser = {
        .rb = rb,
        .offset = 0,
        .atom = NULL,
        .m4a_info = info,
    };
    AAC_ERR_T err = AAC_ERR_FAIL;

    if (info->parsed_once) {
        if (info->moov_tail) {
            parser.offset = info->moov_offset;
        } else {
            OS_LOGE(TAG, "Failed to check moov");
            goto finish;
        }
    } else {
        err = m4a_check_header(&parser);
        if (err == AAC_ERR_AGAIN) {
            OS_LOGV(TAG, "moov behide of mdat, please check again with new offset(%u)", info->moov_offset);
        }
        goto finish;
    }

    /* Start to parse moov header */
    parser.atom = moov;
    err = atom_parse(&parser);
    AUDIO_ERR_CHECK(TAG, err == AAC_ERR_NONE, goto finish);

finish:
    if (err == AAC_ERR_NONE) {
        err = m4a_parse_asc(info);
        m4a_dump_info(info);
    }

    rb_done_read(rb);
    return err;
}

struct m4a_reader_priv {
    struct m4a_info *info;
    ringbuf_handle rb;
    int ret;
};

static void *m4a_reader_parse_thread(void *arg)
{
    struct m4a_reader_priv *priv = (struct m4a_reader_priv *)arg;
    priv->ret = m4a_parse_header(priv->rb, priv->info);
    return NULL;
}

int m4a_extractor(m4a_fetch_cb fetch_cb, void *fetch_priv, struct m4a_info *info)
{
    ringbuf_handle rb_atom = NULL;
    os_thread tid = NULL;
    char buffer[STREAM_BUFFER_SIZE];
    int bytes_writen, bytes_read, offset = 0;
    bool double_check = false;

    rb_atom = rb_create(STREAM_BUFFER_SIZE);
    if (rb_atom == NULL)
        return AAC_ERR_FAIL;

    struct m4a_reader_priv priv = {
        .info = info,
        .rb = rb_atom,
        .ret = AAC_ERR_FAIL,
    };

    struct os_thread_attr tattr = {
        .name = "ael-m4a_extractor",
        .priority = M4A_PARSER_TASK_PRIO,
        .stacksize = M4A_PARSER_TASK_STACK,
        .joinable = true,
    };

m4a_parse:
    tid = os_thread_create(&tattr, m4a_reader_parse_thread, &priv);
    if (tid == NULL) {
        OS_LOGE(TAG, "Failed to create task to parse m4a");
        goto m4a_finish;
    }

    do {
        bytes_read = fetch_cb(buffer, sizeof(buffer), offset, fetch_priv);
        if (bytes_read <= 0)
            break;

        offset += bytes_read;
        bytes_writen = 0;

        do {
            bytes_writen = rb_write(rb_atom, &buffer[bytes_writen], bytes_read, AUDIO_MAX_DELAY);
            if (bytes_writen > 0) {
                bytes_read -= bytes_writen;
            } else {
                if (bytes_writen == RB_DONE) {
                    OS_LOGV(TAG, "RB done write");
                } else if(bytes_writen == RB_ABORT) {
                    OS_LOGV(TAG, "RB abort write");
                } else {
                    OS_LOGW(TAG, "RB write fail, ret=%d", bytes_writen);
                }
                goto m4a_writen;
            }
        } while(bytes_read > 0);
    } while (1);

m4a_writen:
    rb_done_write(rb_atom);
    os_thread_join(tid, NULL);

    if (!double_check) {
        double_check = true;
        if (priv.ret == AAC_ERR_AGAIN && info->parsed_once && info->moov_tail) {
            rb_reset(rb_atom);
            offset = info->moov_offset;
            goto m4a_parse;
        }
    }

m4a_finish:
    if (priv.ret != AAC_ERR_NONE) {
        if (info->stsz_samplesize != NULL) {
            audio_free(info->stsz_samplesize);
            info->stsz_samplesize = NULL;
        }
        if (info->stts_time2sample != NULL) {
            audio_free(info->stts_time2sample);
            info->stts_time2sample = NULL;
        }
        if (info->stsc_sample2chunk != NULL) {
            audio_free(info->stsc_sample2chunk);
            info->stsc_sample2chunk = NULL;
        }
        if (info->stco_chunk2offset != NULL) {
            audio_free(info->stco_chunk2offset);
            info->stco_chunk2offset = NULL;
        }
    }
    rb_destroy(rb_atom);
    return priv.ret;
}

int m4a_get_seek_offset(int seek_ms, struct m4a_info *info, uint32_t *sample_index, uint32_t *sample_offset)
{
    if (seek_ms < 0 || info == NULL || sample_index == NULL || sample_offset == NULL)
        return -1;

    bool seek_done = false;
    uint32_t seek_offset = 0;
    uint32_t seek_index = 0;
    uint32_t samples = (seek_ms*(info->time_scale/1000))/info->stts_time2sample[0].sample_duration;

    for (int32_t cnt = 0; cnt < info->stco_chunk2offset_entries-1; cnt++) {
        if (samples >= info->stco_chunk2offset[cnt].sample_index &&
            samples <  info->stco_chunk2offset[cnt+1].sample_index) {
            seek_offset = info->stco_chunk2offset[cnt].chunk_offset;
            seek_index = info->stco_chunk2offset[cnt].sample_index;
            seek_done = true;
            OS_LOGD(TAG, "Found seek index/offset: %u/%u", seek_index, seek_offset);
            break;
        }
    }

    if (!seek_done || seek_offset < info->stco_chunk2offset[0].chunk_offset) {
        OS_LOGE(TAG, "Failed to find seek offset");
        return -1;
    }

    *sample_index = seek_index;
    *sample_offset = seek_offset;
    return 0;
}

int m4a_build_adts_header(uint8_t *adts_buf, uint32_t adts_size, uint8_t *asc_buf, uint32_t asc_size, uint32_t frame_size)
{
    if (adts_buf == NULL || adts_size != 7 || asc_buf == NULL || asc_size < 2) {
        OS_LOGE(TAG, "Invalid arguments, adts_size=%u, asc_size=%u", adts_size, asc_size);
        return -1;
    }

    uint8_t object_type = asc_buf[0] >> 3;
    uint8_t sample_rate_index = ((asc_buf[0] & 0x07) << 1) | ((asc_buf[1] & 0x80) >> 7);
    uint8_t channel_conf = (asc_buf[1] & 0x78) >> 3;
    uint16_t aac_frame_length = (uint16_t)(frame_size + 7);

    adts_buf[0] = 0xFF;
    adts_buf[1] = 0xF1;
    adts_buf[2] = (uint8_t)(((object_type - 1) << 6) + (sample_rate_index << 2) + (channel_conf >> 2));
    adts_buf[3] = (uint8_t)(((channel_conf & 3) << 6) + (aac_frame_length >> 11));
    adts_buf[4] = (uint8_t)((aac_frame_length & 0x7FF) >> 3);
    adts_buf[5] = (uint8_t)(((aac_frame_length & 0x07) << 5) + 0x1F);
    adts_buf[6] = 0xFC;
    return 0;
}
