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
#include <stdbool.h>
#include <string.h>
#include <list>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "sink_opensles_wrapper.h"

#define TAG "[liteplayer]opensles"

#define MIN_BUFFER_QUEUE_LEN 2

class OutBuffer {
public:
    OutBuffer(char *buf, int len) {
        data = new char[len];
        size = len;
        memcpy(data, buf, len);
    }

    ~OutBuffer() {
        delete [] data;
    }

    char *data;
    int size;
};

struct opensles_priv {
    SLObjectItf engineObj;
    SLEngineItf engineItf;
    SLObjectItf outmixObj;
    SLObjectItf playerObj;
    SLPlayItf   playerItf;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;

    SLuint32 queueSize;
    std::list<OutBuffer *> *bufferList;
    os_mutex bufferLock;
    os_cond bufferCond;
    bool hasStarted;
};

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    struct opensles_priv *priv = (struct opensles_priv *)context;
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
        OS_LOGE(TAG, "Unsupported sample bits(%d) for OpenSLES", bits);
        return NULL;
    }

    struct opensles_priv *priv = (struct opensles_priv *)OS_CALLOC(1, sizeof(struct opensles_priv));
    if (priv == NULL)
        return NULL;

    // todo: optimize queue-size to reduce latency and fix underrun
    priv->queueSize = MIN_BUFFER_QUEUE_LEN * channels;
    if (samplerate > 16000)
        priv->queueSize *= (samplerate/16000);

    SLresult result = SL_RESULT_SUCCESS;
    do {
        priv->bufferList = new std::list<OutBuffer *>();
        priv->bufferLock = os_mutex_create();
        if (priv->bufferLock == NULL) break;
        priv->bufferCond = os_cond_create();
        if (priv->bufferCond == NULL) break;

        // create engine
        result = slCreateEngine(&priv->engineObj, 0, NULL, 0, NULL, NULL);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the engine
        result = (*priv->engineObj)->Realize(priv->engineObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the engine interface, which is needed in order to create other objects
        result = (*priv->engineObj)->GetInterface(priv->engineObj, SL_IID_ENGINE, &priv->engineItf);
        if (SL_RESULT_SUCCESS != result) break;
        // create output mix
        result = (*priv->engineItf)->CreateOutputMix(priv->engineItf, &priv->outmixObj,  0, NULL, NULL);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the output mix
        result = (*priv->outmixObj)->Realize(priv->outmixObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;

        // configure audio sink
        SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX, priv->outmixObj};
        SLDataSink audioSnk = {&outmix, NULL};
        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, priv->queueSize};
        SLDataFormat_PCM format_pcm = {
                SL_DATAFORMAT_PCM,
                static_cast<SLuint32>(channels),                 // channel count
                static_cast<SLuint32>(samplerate*1000),          // sample rate in mili second
                pcmformat,                                       // bitsPerSample
                pcmformat,                                       // containerSize
                channels == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                        // endianness
        };
        SLDataSource audioSrc = {&bufq, &format_pcm};

        // create audio player
        const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
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
        result = (*priv->playerObj)->GetInterface(priv->playerObj, SL_IID_BUFFERQUEUE, &priv->playerBufferQueue);
        if (SL_RESULT_SUCCESS != result) break;
        // register callback on the buffer queue
        result = (*priv->playerBufferQueue)->RegisterCallback(priv->playerBufferQueue, bqPlayerCallback, priv);
        if (SL_RESULT_SUCCESS != result) break;
    } while (0);

    if (SL_RESULT_SUCCESS == result) {
        return priv;
    } else {
        opensles_wrapper_close(priv);
        return NULL;
    }
}

int opensles_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGV(TAG, "Writing OpenSLES: buffer=%p, size=%d", buffer, size);
    struct opensles_priv *priv = (struct opensles_priv *)handle;
    OutBuffer *outbuf = new OutBuffer(buffer, size);

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

    if (!priv->hasStarted) {
        if (priv->bufferList->size() >= priv->queueSize) {
            priv->hasStarted = true;
            // set the player's state to playing
            result = (*priv->playerItf)->SetPlayState(priv->playerItf, SL_PLAYSTATE_PLAYING);
            if (SL_RESULT_SUCCESS != result) {
                os_mutex_unlock(priv->bufferLock);
                return -1;
            }
        }
    }

    os_mutex_unlock(priv->bufferLock);
    return size;
}

void opensles_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing OpenSLES");
    struct opensles_priv *priv = (struct opensles_priv *)handle;

    // waiting all buffers in the list finished playing
    if (priv->bufferLock != NULL && priv->bufferCond != NULL && priv->bufferList != NULL) {
        os_mutex_lock(priv->bufferLock);
        while (!priv->bufferList->empty())
            os_cond_wait(priv->bufferCond, priv->bufferLock);
        os_mutex_unlock(priv->bufferLock);
    }

    if (priv->bufferList != NULL) {
        for (auto iter =priv->bufferList->begin(); iter != priv->bufferList->end(); iter++) {
            delete *iter;
        }
        delete priv->bufferList;
    }

    if (priv->bufferCond != NULL)
        os_cond_destroy(priv->bufferCond);
    if (priv->bufferLock != NULL)
        os_mutex_destroy(priv->bufferLock);

    if (priv->playerObj != NULL)
        (*priv->playerObj)->Destroy(priv->playerObj);
    if (priv->outmixObj != NULL)
        (*priv->outmixObj)->Destroy(priv->outmixObj);
    if (priv->engineObj != NULL)
        (*priv->engineObj)->Destroy(priv->engineObj);

    OS_FREE(priv);
}
