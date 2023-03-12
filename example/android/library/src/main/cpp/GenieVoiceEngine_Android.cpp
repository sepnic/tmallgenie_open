// Copyright (c) 2023- Qinglong<sysu.zqlong@gmail.com>
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
#include <list>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "GenieVendor_Android.h"

#define TAG "GenieVendorAndroid"

// todo: optimize queue-size to reduce latency and fix xrun
#define MIN_PLAYER_BUFFER_QUEUE_LEN     2
#define MIN_RECORDER_BUFFER_QUEUE_LEN   4

class GnVendor_PcmBuffer {
public:
    GnVendor_PcmBuffer(char *buf, int len) {
        data = new char[len];
        size = len;
        memcpy(data, buf, len);
    }
    ~GnVendor_PcmBuffer() {
        delete [] data;
    }
    char *data;
    int size;
};

struct GnVendor_PcmOut {
    SLObjectItf engineObj;
    SLEngineItf engineItf;
    SLObjectItf outmixObj;
    SLObjectItf playerObj;
    SLPlayItf   playerItf;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;
    SLuint32 queueSize;

    std::list<GnVendor_PcmBuffer *> *bufferList;
    os_mutex bufferLock;
    os_cond bufferCond;
    bool isStarted;
};

struct GnVendor_PcmIn {
    SLObjectItf engineObj;
    SLEngineItf engineItf;
    SLObjectItf recorderObj;
    SLRecordItf   recorderItf;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;
    SLuint32 queueSize;

    char *bufferPtr;
    int bufferSize;
    std::list<GnVendor_PcmBuffer *> *bufferList;
    os_mutex bufferLock;
    os_cond bufferCond;
    bool isStarted;
    bool forceStop;
};

// this callback handler is called every time a buffer finishes playing
static void GnVendor_pcmOutBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    auto *priv = reinterpret_cast<struct GnVendor_PcmOut *>(context);
    os_mutex_lock(priv->bufferLock);
    // free the buffer that finishes playing
    if (!priv->bufferList->empty()) {
        delete priv->bufferList->front();
        priv->bufferList->pop_front();
        // notify writing-thread that the list has space to store new buffer
        os_cond_signal(priv->bufferCond);
    }
    os_mutex_unlock(priv->bufferLock);
}

// this callback handler is called every time a buffer finishes recording
static void GnVendor_pcmInBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    auto *priv = reinterpret_cast<struct GnVendor_PcmIn *>(context);
    auto *inbuf = new GnVendor_PcmBuffer(priv->bufferPtr, priv->bufferSize);
    os_mutex_lock(priv->bufferLock);
    if (!priv->forceStop) {
        if (priv->bufferList->size() < priv->queueSize) {
            // enqueue the buffer that finishes recording
            priv->bufferList->push_back(inbuf);
            // notify reading-thread that the list has available buffer
            os_cond_signal(priv->bufferCond);
        } else {
            // todo: replace the old data with the new one
            OS_LOGE(TAG, "No free buffer to save new recording data, maybe reading thread is blocked");
        }
        SLresult result = (*priv->recorderBufferQueue)->Enqueue(priv->recorderBufferQueue, priv->bufferPtr, priv->bufferSize);
        if (SL_RESULT_SUCCESS != result) {
            OS_LOGE(TAG, "Failed to enqueue buffer to recorderBufferQueue");
        }
    } else {
        SLresult result = (*priv->recorderItf)->SetRecordState(priv->recorderItf, SL_RECORDSTATE_STOPPED);
        if (SL_RESULT_SUCCESS == result) {
            // notify stop-thread that the recorder has been stopped
            priv->isStarted = false;
            os_cond_signal(priv->bufferCond);
        } else {
            OS_LOGE(TAG, "Failed to stop recorder");
        }
    }
    os_mutex_unlock(priv->bufferLock);
}

void *GnVendor_pcmOutOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGD(TAG, "GnVendor_pcmOutOpen: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    SLuint32 pcmformat;
    switch (bitsPerSample) {
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
            OS_LOGE(TAG, "Unsupported sample bits(%d) for OpenSLES", bitsPerSample);
            return nullptr;
    }

    auto *priv = reinterpret_cast<struct GnVendor_PcmOut *>(OS_CALLOC(1, sizeof(struct GnVendor_PcmOut)));
    if (priv == nullptr)
        return nullptr;

    priv->queueSize = MIN_PLAYER_BUFFER_QUEUE_LEN * channelCount;
    if (sampleRate > 16000)
        priv->queueSize *= (sampleRate/16000);

    SLresult result = SL_RESULT_SUCCESS;
    do {
        priv->bufferList = new std::list<GnVendor_PcmBuffer *>();
        priv->bufferLock = os_mutex_create();
        if (priv->bufferLock == nullptr) break;
        priv->bufferCond = os_cond_create();
        if (priv->bufferCond == nullptr) break;

        // create engine
        result = slCreateEngine(&priv->engineObj, 0, nullptr, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the engine
        result = (*priv->engineObj)->Realize(priv->engineObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the engine interface, which is needed in order to create other objects
        result = (*priv->engineObj)->GetInterface(priv->engineObj, SL_IID_ENGINE, &priv->engineItf);
        if (SL_RESULT_SUCCESS != result) break;
        // create output mix
        result = (*priv->engineItf)->CreateOutputMix(priv->engineItf, &priv->outmixObj,  0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the output mix
        result = (*priv->outmixObj)->Realize(priv->outmixObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;

        // configure audio sink
        SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX, priv->outmixObj};
        SLDataSink audioSnk = {&outmix, nullptr};
        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, priv->queueSize};
        SLDataFormat_PCM format_pcm = {
                SL_DATAFORMAT_PCM,
                static_cast<SLuint32>(channelCount),                 // channel count
                static_cast<SLuint32>(sampleRate*1000),          // sample rate in mili second
                pcmformat,                                       // bitsPerSample
                pcmformat,                                       // containerSize
                channelCount == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                        // endianness
        };
        SLDataSource audioSrc = {&bufq, &format_pcm};

        // create audio player
        const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        result = (*priv->engineItf)->CreateAudioPlayer(priv->engineItf, &priv->playerObj, &audioSrc, &audioSnk,
                                                       sizeof(ids)/sizeof(SLInterfaceID), ids, req);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the player
        result = (*priv->playerObj)->Realize(priv->playerObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the play interface
        result = (*priv->playerObj)->GetInterface(priv->playerObj, SL_IID_PLAY, &priv->playerItf);
        if (SL_RESULT_SUCCESS != result) break;
        // get the buffer queue interface
        result = (*priv->playerObj)->GetInterface(priv->playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &priv->playerBufferQueue);
        if (SL_RESULT_SUCCESS != result) break;
        // register callback on the buffer queue
        result = (*priv->playerBufferQueue)->RegisterCallback(priv->playerBufferQueue, GnVendor_pcmOutBufferQueueCallback, priv);
        if (SL_RESULT_SUCCESS != result) break;
    } while (false);

    if (SL_RESULT_SUCCESS == result) {
        return priv;
    } else {
        GnVendor_pcmOutClose(priv);
        return nullptr;
    }
}

int GnVendor_pcmOutWrite(void *handle, void *buffer, unsigned int size)
{
    auto *priv = reinterpret_cast<struct GnVendor_PcmOut *>(handle);
    auto *outbuf = new GnVendor_PcmBuffer((char *)buffer, (int)size);

    os_mutex_lock(priv->bufferLock);

    // waiting the list is available
    while (priv->bufferList->size() >= priv->queueSize)
        os_cond_wait(priv->bufferCond, priv->bufferLock);
    priv->bufferList->push_back(outbuf);

    SLresult result = (*priv->playerBufferQueue)->Enqueue(priv->playerBufferQueue, outbuf->data, outbuf->size);
    if (SL_RESULT_SUCCESS != result) {
        os_mutex_unlock(priv->bufferLock);
        return -1;
    }

    if (!priv->isStarted) {
        if (priv->bufferList->size() >= priv->queueSize) {
            priv->isStarted = true;
            // set the player's state to playing
            result = (*priv->playerItf)->SetPlayState(priv->playerItf, SL_PLAYSTATE_PLAYING);
            if (SL_RESULT_SUCCESS != result) {
                os_mutex_unlock(priv->bufferLock);
                return -1;
            }
        }
    }

    os_mutex_unlock(priv->bufferLock);
    return (int)size;
}

void GnVendor_pcmOutClose(void *handle)
{
    OS_LOGD(TAG, "GnVendor_pcmOutClose");
    auto *priv = reinterpret_cast<struct GnVendor_PcmOut *>(handle);
    // waiting all buffers in the list finished playing
    if (priv->bufferLock != nullptr && priv->bufferCond != nullptr && priv->bufferList != nullptr) {
        os_mutex_lock(priv->bufferLock);
        while (!priv->bufferList->empty())
            os_cond_wait(priv->bufferCond, priv->bufferLock);
        os_mutex_unlock(priv->bufferLock);
    }

    if (priv->bufferList != nullptr) {
        for (auto & iter : *priv->bufferList) {
            delete iter;
        }
        delete priv->bufferList;
    }

    if (priv->bufferCond != nullptr)
        os_cond_destroy(priv->bufferCond);
    if (priv->bufferLock != nullptr)
        os_mutex_destroy(priv->bufferLock);

    if (priv->playerObj != nullptr)
        (*priv->playerObj)->Destroy(priv->playerObj);
    if (priv->outmixObj != nullptr)
        (*priv->outmixObj)->Destroy(priv->outmixObj);
    if (priv->engineObj != nullptr)
        (*priv->engineObj)->Destroy(priv->engineObj);

    OS_FREE(priv);
}

void *GnVendor_pcmInOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGD(TAG, "GnVendor_pcmInOpen: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    SLuint32 pcmformat;
    switch (bitsPerSample) {
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
            OS_LOGE(TAG, "Unsupported sample bits(%d) for OpenSLES", bitsPerSample);
            return nullptr;
    }

    auto *priv = reinterpret_cast<struct GnVendor_PcmIn *>(OS_CALLOC(1, sizeof(struct GnVendor_PcmIn)));
    if (priv == nullptr)
        return nullptr;

    priv->queueSize = MIN_RECORDER_BUFFER_QUEUE_LEN;

    int frameMs = 30; // valid value for vad engine: { 10ms, 20ms, 30ms }
    int frameSize = sampleRate/1000*frameMs;
    int multiple = 2;
    priv->bufferSize = (frameSize*channelCount*bitsPerSample/8)*multiple;
    priv->bufferPtr = new char[priv->bufferSize];

    SLresult result = SL_RESULT_SUCCESS;
    do {
        priv->bufferList = new std::list<GnVendor_PcmBuffer *>();
        priv->bufferLock = os_mutex_create();
        if (priv->bufferLock == nullptr) break;
        priv->bufferCond = os_cond_create();
        if (priv->bufferCond == nullptr) break;

        // create engine
        result = slCreateEngine(&priv->engineObj, 0, nullptr, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the engine
        result = (*priv->engineObj)->Realize(priv->engineObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the engine interface, which is needed in order to create other objects
        result = (*priv->engineObj)->GetInterface(priv->engineObj, SL_IID_ENGINE, &priv->engineItf);
        if (SL_RESULT_SUCCESS != result) break;

        // configure audio source
        SLDataLocator_IODevice iodevice = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                          SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
        SLDataSource audioSrc = {&iodevice, nullptr};
        // configure audio sink
        SLDataLocator_AndroidSimpleBufferQueue bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, priv->queueSize};
        SLDataFormat_PCM format_pcm = {
                SL_DATAFORMAT_PCM,
                static_cast<SLuint32>(channelCount),                 // channel count
                static_cast<SLuint32>(sampleRate*1000),          // sample rate in mili second
                pcmformat,                                       // bitsPerSample
                pcmformat,                                       // containerSize
                channelCount == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                        // endianness
        };
        SLDataSink audioSnk = {&bufq, &format_pcm};

        // create audio recorder
        const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        result = (*priv->engineItf)->CreateAudioRecorder(priv->engineItf, &priv->recorderObj, &audioSrc, &audioSnk,
                                                       sizeof(ids)/sizeof(SLInterfaceID), ids, req);

        if (SL_RESULT_SUCCESS != result) break;
        // realize the recorder
        result = (*priv->recorderObj)->Realize(priv->recorderObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the record interface
        result = (*priv->recorderObj)->GetInterface(priv->recorderObj, SL_IID_RECORD, &priv->recorderItf);
        if (SL_RESULT_SUCCESS != result) break;
        // get the buffer queue interface
        result = (*priv->recorderObj)->GetInterface(priv->recorderObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &priv->recorderBufferQueue);
        if (SL_RESULT_SUCCESS != result) break;
        result = (*priv->recorderBufferQueue)->Enqueue(priv->recorderBufferQueue, priv->bufferPtr, priv->bufferSize);
        if (SL_RESULT_SUCCESS != result) break;
        // register callback on the buffer queue
        result = (*priv->recorderBufferQueue)->RegisterCallback(priv->recorderBufferQueue, GnVendor_pcmInBufferQueueCallback, priv);
        if (SL_RESULT_SUCCESS != result) break;

        // set the recorder's state to recording
        result = (*priv->recorderItf)->SetRecordState(priv->recorderItf, SL_RECORDSTATE_RECORDING);
        if (SL_RESULT_SUCCESS != result) break;
        priv->isStarted = true;
    } while (false);

    if (SL_RESULT_SUCCESS == result) {
        return priv;
    } else {
        GnVendor_pcmInClose(priv);
        return nullptr;
    }
}

int GnVendor_pcmInRead(void *handle, void *buffer, unsigned int size)
{
    auto *priv = reinterpret_cast<struct GnVendor_PcmIn *>(handle);

    os_mutex_lock(priv->bufferLock);

    // waiting the list is available
    while (priv->bufferList->empty())
        os_cond_wait(priv->bufferCond, priv->bufferLock);

    auto *inBuffer = priv->bufferList->front();
    size = size <= inBuffer->size ? size : inBuffer->size;
    memcpy(buffer, inBuffer->data, size);

    delete inBuffer;
    priv->bufferList->pop_front();

    os_mutex_unlock(priv->bufferLock);
    return (int)size;
}

void GnVendor_pcmInClose(void *handle)
{
    OS_LOGD(TAG, "GnVendor_pcmInClose");
    auto *priv = reinterpret_cast<struct GnVendor_PcmIn *>(handle);
    priv->forceStop = true;
    if (priv->bufferLock != nullptr && priv->bufferCond != nullptr) {
        os_mutex_lock(priv->bufferLock);
        while (priv->isStarted)
            os_cond_wait(priv->bufferCond, priv->bufferLock);
        os_mutex_unlock(priv->bufferLock);
    }

    if (priv->bufferList != nullptr) {
        for (auto & iter : *priv->bufferList) {
            delete iter;
        }
        delete priv->bufferList;
    }
    if (priv->bufferPtr != nullptr)
        delete [] priv->bufferPtr;

    if (priv->bufferCond != nullptr)
        os_cond_destroy(priv->bufferCond);
    if (priv->bufferLock != nullptr)
        os_mutex_destroy(priv->bufferLock);

    if (priv->recorderObj != nullptr)
        (*priv->recorderObj)->Destroy(priv->recorderObj);
    if (priv->engineObj != nullptr)
        (*priv->engineObj)->Destroy(priv->engineObj);

    OS_FREE(priv);
}
