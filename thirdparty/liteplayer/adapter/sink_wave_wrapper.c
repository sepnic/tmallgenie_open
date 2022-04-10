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
#include <stdint.h>
#include <string.h>

#include "osal/os_time.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "sink_wave_wrapper.h"

#define TAG "[liteplayer]wave"

//RIFF block
typedef struct {
    uint32_t ChunkID;           //chunk id;"RIFF",0X46464952
    uint32_t ChunkSize ;        //file length - 8
    uint32_t Format;            //WAVE,0X45564157
} __attribute__((packed)) ChunkRIFF;

//fmt block
typedef struct {
    uint32_t ChunkID;           //chunk id;"fmt ",0X20746D66
    uint32_t ChunkSize;         //Size of this fmt block (Not include ID and Size);16 or 18 or 40 bytes.
    uint16_t AudioFormat;       //Format;0X01:linear PCM;0X11:IMA ADPCM
    uint16_t NumOfChannels;     //Number of channel;1: 1 channel;2: 2 channels;
    uint32_t SampleRate;        //sample rate;0X1F40 = 8Khz
    uint32_t ByteRate;          //Byte rate;
    uint16_t BlockAlign;        //align with byte;
    uint16_t BitsPerSample;     //Bit lenght per sample point,4 ADPCM
//  uint16_t ByteExtraData;     //Exclude in linear PCM format(0~22)
} __attribute__((packed)) ChunkFMT;

//fact block
typedef struct {
    uint32_t ChunkID;           //chunk id;"fact",0X74636166;
    uint32_t ChunkSize;         //Size(Not include ID and Size);4 byte
    uint32_t NumOfSamples;      //number of sample
} __attribute__((packed)) ChunkFACT;

//LIST block
typedef struct {
    uint32_t ChunkID;            //chunk id 0X5453494C;
    uint32_t ChunkSize;          //Size
} __attribute__((packed)) ChunkLIST;

//PEAK block
typedef struct {
    uint32_t ChunkID;            //chunk id; 0X4B414550
    uint32_t ChunkSize;          //Size
} __attribute__((packed)) ChunkPeak;

//data block
typedef struct {
    uint32_t ChunkID;           //chunk id;"data",0X5453494C
    uint32_t ChunkSize;         //Size of data block(Not include ID and Size)
} __attribute__((packed)) ChunkDATA;

//wav block
typedef struct {
    ChunkRIFF riff;             //riff
    ChunkFMT fmt;               //fmt
    //ChunkFACT fact;             //fact,Exclude in linear PCM format
    ChunkDATA data;             //data
} __attribute__((packed)) wav_header_t;

enum wav_format {
    WAV_FMT_PCM = 0x0001,
    WAV_FMT_ADPCM = 0x0002,
    WAV_FMT_IEEE_FLOAT = 0x0003,
    WAV_FMT_DVI_ADPCM = 0x0011,
};

#define BSWAP_16(x) (uint_16)( (((uint_16)(x) & 0x00ff) << 8) | (((uint_16)(x) & 0xff00) >> 8) )

#define BSWAP_32(x) (uint_32)( (((uint_32)(x) & 0xff000000) >> 24) | \
                               (((uint_32)(x) & 0x00ff0000) >> 8)  | \
                               (((uint_32)(x) & 0x0000ff00) << 8)  | \
                               (((uint_32)(x) & 0x000000ff) << 24) )

#if !defined(BYTE_ORDER_BIG_ENDIAN)
#define COMPOSE_INT(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define COMPOSE_SHORT(a,b)   ((a) | ((b)<<8))
#define LE_SHORT(v)          (v)
#define LE_INT(v)            (v)
#define BE_SHORT(v)          BSWAP_16(v)
#define BE_INT(v)            BSWAP_32(v)
#else
#define COMPOSE_INT(a,b,c,d) ((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define COMPOSE_SHORT(a,b)   ((b) | ((a)<<8))
#define LE_SHORT(v)          BSWAP_16(v)
#define LE_INT(v)            BSWAP_32(v)
#define BE_SHORT(v)          (v)
#define BE_INT(v)            (v)
#endif

#define WAV_CHUNK_RIFF        COMPOSE_INT('R','I','F','F')
#define WAV_CHUNK_WAVE        COMPOSE_INT('W','A','V','E')
#define WAV_CHUNK_FMT         COMPOSE_INT('f','m','t',' ')
#define WAV_CHUNK_LIST        COMPOSE_INT('L','I','S','T')
#define WAV_CHUNK_FACT        COMPOSE_INT('f','a','c','t')
#define WAV_CHUNK_PEAK        COMPOSE_INT('P','E','A','K')
#define WAV_CHUNK_DATA        COMPOSE_INT('d','a','t','a')

struct wave_priv {
    FILE *file;
    int samplerate;
    int channels;
    int bits;
    long offset;
};

static void wav_build_header(wav_header_t *header, int samplerate, int bits, int channels, enum wav_format format, long datasize)
{
    header->riff.ChunkID = WAV_CHUNK_RIFF;
    header->riff.Format = WAV_CHUNK_WAVE;
    header->riff.ChunkSize = LE_INT(datasize+sizeof(wav_header_t)-8);
    header->fmt.ChunkID = WAV_CHUNK_FMT;
    header->fmt.ChunkSize = LE_INT(16);
    header->fmt.AudioFormat = LE_SHORT(format);
    header->fmt.NumOfChannels = LE_SHORT(channels);
    header->fmt.SampleRate = LE_INT(samplerate);
    header->fmt.BitsPerSample = LE_SHORT(bits);
    header->fmt.BlockAlign = LE_SHORT(bits*channels/8);
    header->fmt.ByteRate = LE_INT(header->fmt.BlockAlign*samplerate);
    header->data.ChunkID = WAV_CHUNK_DATA;
    header->data.ChunkSize = LE_INT(datasize);
}

const char *wave_wrapper_name()
{
    return "wave";
}

sink_handle_t wave_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening wave: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    struct wave_priv *priv = OS_CALLOC(1, sizeof(struct wave_priv));
    if (priv == NULL)
        return NULL;

    char filename[64];
    struct os_wall_time ts;
    os_realtime_to_walltime(&ts);
    memset(filename, 0x0, sizeof(filename));
    snprintf(filename, sizeof(filename),
             "pcm_out-%04d%02d%02d-%02d%02d%02d-%03d.wav",
             ts.year, ts.mon, ts.day, ts.hour, ts.min, ts.sec, ts.msec
    );

    priv->file = fopen(filename, "wb+");
    if (priv->file == NULL) {
        OS_FREE(priv);
        return NULL;
    }

    priv->samplerate = samplerate;
    priv->channels = channels;
    priv->bits = bits;
    priv->offset = 0;

    wav_header_t header;
    memset(&header, 0x0, sizeof(wav_header_t));
    fwrite(&header, 1, sizeof(header), priv->file);

    return priv;
}

int wave_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    struct wave_priv *priv = (struct wave_priv *)handle;
    size_t bytes_written = fwrite(buffer, 1, size, priv->file);
    if (bytes_written > 0)
        priv->offset += bytes_written;
    return bytes_written;
}

void wave_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing wave");
    struct wave_priv *priv = (struct wave_priv *)handle;

    wav_header_t header;
    memset(&header, 0x0, sizeof(wav_header_t));
    wav_build_header(&header, priv->samplerate, priv->bits, priv->channels, WAV_FMT_PCM, priv->offset);
    fseek(priv->file, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(header), priv->file);

    fflush(priv->file);
    fclose(priv->file);
    OS_FREE(priv);
}
