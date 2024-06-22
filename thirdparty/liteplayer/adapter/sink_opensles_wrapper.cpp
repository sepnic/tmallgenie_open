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

#include <cstdio>
#include <cstring>
#include <mutex>
#include <condition_variable>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>

#include "cutils/lockfree_ringbuf.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "sink_opensles_wrapper.h"

#define TAG "[liteplayer]opensles"

static const SLuint32 kPlayerBufferQueueSize= 2;
static const int kPlayerFrameTime           = 30;// in ms
static const int kPlayerFrameCount          = 2; // writeBufferTime = kPlayerFrameTime*kPlayerFrameCount
static const int kPlayerBufferCount         = 4; // playerRingbufTime = writeBufferTime*kPlayerBufferCount

class OpenSLES_PcmOut {
public:
    OpenSLES_PcmOut()
            : engineObj(nullptr),
              engineItf(nullptr),
              outmixObj(nullptr),
              playerObj(nullptr),
              playerItf(nullptr),
              playerBufferQueue(nullptr),
              queueSize(kPlayerBufferQueueSize),
              enqueueBuffer(nullptr),
              ringbuf(nullptr),
              isStarted(false),
              needStop(false)
    {}

    ~OpenSLES_PcmOut() {
        if (ringbuf != nullptr)
            lockfree_ringbuf_destroy(ringbuf);
        if (enqueueBuffer != nullptr)
            delete enqueueBuffer;
    }

    SLObjectItf engineObj;
    SLEngineItf engineItf;
    SLObjectItf outmixObj;
    SLObjectItf playerObj;
    SLPlayItf   playerItf;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;
    SLuint32 queueSize;

    char *enqueueBuffer;
    int enqueueSize;
    void *ringbuf;
    std::mutex bufferLock;
    std::condition_variable bufferCanRead;
    std::condition_variable bufferCanWrite;
    bool isStarted;
    bool needStop;
};

// this callback handler is called every time a buffer finishes playing
static void opensles_playerBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    auto *out = reinterpret_cast<OpenSLES_PcmOut *>(context);
    std::unique_lock<std::mutex> lk(out->bufferLock);

    if (out->needStop && lockfree_ringbuf_bytes_filled(out->ringbuf) < out->enqueueSize) {
        lk.unlock();
        out->bufferCanWrite.notify_one();
        return;
    }

    while (lockfree_ringbuf_bytes_filled(out->ringbuf) < out->enqueueSize)
        out->bufferCanRead.wait(lk);
    lockfree_ringbuf_read(out->ringbuf, out->enqueueBuffer, out->enqueueSize);
    SLresult result = (*out->playerBufferQueue)->Enqueue(out->playerBufferQueue, out->enqueueBuffer, out->enqueueSize);
    if (SL_RESULT_SUCCESS != result)
        OS_LOGE(TAG, "Failed to enqueue buffer to playerBufferQueue");
    // notify writing-thread that the list has space to store new buffer
    lk.unlock();
    out->bufferCanWrite.notify_one();
}

const char *opensles_wrapper_name()
{
    return "opensles";
}

sink_handle_t opensles_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening OpenSLES: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    SLuint32 pcmformat;
    switch (bits) {
        case 16:
            pcmformat = SL_PCMSAMPLEFORMAT_FIXED_16;
            break;
        case 24:
            pcmformat = SL_PCMSAMPLEFORMAT_FIXED_24;
            break;
        case 32:
            pcmformat = SL_PCMSAMPLEFORMAT_FIXED_32;
            break;
        default:
            OS_LOGE(TAG, "Unsupported sample bits(%d)", bits);
            return nullptr;
    }

    int frameSize = samplerate/1000*kPlayerFrameTime;
    int enqueueSize = (frameSize*channels*bits/8)*kPlayerFrameCount;
    void *ringbuf = lockfree_ringbuf_create(enqueueSize*kPlayerBufferCount);
    if (ringbuf == nullptr)
        return nullptr;

    auto *out = new OpenSLES_PcmOut();
    out->queueSize *= channels;
    if (samplerate > 16000)
        out->queueSize *= (samplerate/16000);
    out->ringbuf = ringbuf;
    out->enqueueSize = enqueueSize;
    out->enqueueBuffer = new char[out->enqueueSize];

    SLresult result = SL_RESULT_SUCCESS;
    do {
        // create engine
        result = slCreateEngine(&out->engineObj, 0, nullptr, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the engine
        result = (*out->engineObj)->Realize(out->engineObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the engine interface, which is needed in order to create other objects
        result = (*out->engineObj)->GetInterface(out->engineObj, SL_IID_ENGINE, &out->engineItf);
        if (SL_RESULT_SUCCESS != result) break;
        // create output mix
        result = (*out->engineItf)->CreateOutputMix(out->engineItf, &out->outmixObj,  0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the output mix
        result = (*out->outmixObj)->Realize(out->outmixObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;

        // configure audio sink
        SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX, out->outmixObj};
        SLDataSink audioSnk = {&outmix, nullptr};
        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, out->queueSize};
        SLDataFormat_PCM format_pcm = {
                SL_DATAFORMAT_PCM,
                static_cast<SLuint32>(channels),         // channel count
                static_cast<SLuint32>(samplerate*1000), // sample rate in milli second
                pcmformat,                              // bitsPerSample
                pcmformat,                               // containerSize
                channels == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                            // endianness
        };
        SLDataSource audioSrc = {&bufq, &format_pcm};

        // create audio player
        const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        result = (*out->engineItf)->CreateAudioPlayer(out->engineItf, &out->playerObj, &audioSrc, &audioSnk,
                                                      sizeof(ids)/sizeof(SLInterfaceID), ids, req);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the player
        result = (*out->playerObj)->Realize(out->playerObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the play interface
        result = (*out->playerObj)->GetInterface(out->playerObj, SL_IID_PLAY, &out->playerItf);
        if (SL_RESULT_SUCCESS != result) break;
        // get the buffer queue interface
        result = (*out->playerObj)->GetInterface(out->playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &out->playerBufferQueue);
        if (SL_RESULT_SUCCESS != result) break;
        // register callback on the buffer queue
        result = (*out->playerBufferQueue)->RegisterCallback(out->playerBufferQueue, opensles_playerBufferQueueCallback, out);
        if (SL_RESULT_SUCCESS != result) break;
    } while (false);

    if (SL_RESULT_SUCCESS == result) {
        return out;
    } else {
        opensles_wrapper_close(out);
        return nullptr;
    }
}

int opensles_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    auto *out = reinterpret_cast<OpenSLES_PcmOut *>(handle);

    if (size > lockfree_ringbuf_get_size(out->ringbuf)) {
        OS_LOGE(TAG, "Insufficient ringbuf size, write size too big");
        return -1;
    }

    std::unique_lock<std::mutex> lk(out->bufferLock);
    // waiting the list is available
    while (lockfree_ringbuf_bytes_available(out->ringbuf) < size)
        out->bufferCanWrite.wait(lk);

    lockfree_ringbuf_write(out->ringbuf, (char *)buffer, size);
    if (lockfree_ringbuf_bytes_filled(out->ringbuf) >= out->enqueueSize) {
        lk.unlock();
        out->bufferCanRead.notify_one();
    }

    if (!out->isStarted) {
        if (lockfree_ringbuf_bytes_filled(out->ringbuf) >= lockfree_ringbuf_get_size(out->ringbuf)/2) {
            out->isStarted = true;
            memset(out->enqueueBuffer, 0x0, out->enqueueSize);
            SLresult result = (*out->playerBufferQueue)->Enqueue(out->playerBufferQueue, out->enqueueBuffer, out->enqueueSize);
            if (SL_RESULT_SUCCESS != result)
                return -1;
            // set the player's state to playing
            result = (*out->playerItf)->SetPlayState(out->playerItf, SL_PLAYSTATE_PLAYING);
            if (SL_RESULT_SUCCESS != result)
                return -1;
        }
    }
    return size;
}

void opensles_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing OpenSLES");
    auto *out = reinterpret_cast<OpenSLES_PcmOut *>(handle);

    out->needStop = true;
    {
        // waiting all buffers in the list finished playing
        std::unique_lock<std::mutex> lk(out->bufferLock);
        while (lockfree_ringbuf_bytes_filled(out->ringbuf) >= out->enqueueSize)
            out->bufferCanWrite.wait(lk);
    }

    if (out->playerObj != nullptr)
        (*out->playerObj)->Destroy(out->playerObj);
    if (out->outmixObj != nullptr)
        (*out->outmixObj)->Destroy(out->outmixObj);
    if (out->engineObj != nullptr)
        (*out->engineObj)->Destroy(out->engineObj);

    delete out;
}
