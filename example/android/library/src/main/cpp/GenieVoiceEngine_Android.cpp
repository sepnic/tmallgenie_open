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
#include <mutex>
#include <condition_variable>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>

#include "lockfree_ringbuf.h"
#include "GenieSdk.h"
#include "GenieVendor_Android.h"

#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
#include "litevad.h"
#endif

#define TAG "GenieVendorAndroid"

#define OS_LOGF(tag, format, ...) __android_log_print(ANDROID_LOG_FATAL,   tag, format, ##__VA_ARGS__)
#define OS_LOGE(tag, format, ...) __android_log_print(ANDROID_LOG_ERROR,   tag, format, ##__VA_ARGS__)
#define OS_LOGW(tag, format, ...) __android_log_print(ANDROID_LOG_WARN,    tag, format, ##__VA_ARGS__)
#define OS_LOGI(tag, format, ...) __android_log_print(ANDROID_LOG_INFO,    tag, format, ##__VA_ARGS__)
#define OS_LOGD(tag, format, ...) __android_log_print(ANDROID_LOG_DEBUG,   tag, format, ##__VA_ARGS__)
#define OS_LOGV(tag, format, ...) __android_log_print(ANDROID_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

// See build options in CMakeLists.txt
//#define GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED

// DO NOT MODIFY RECORD SETTINGS FOR VOICE ENGINE
#define VOICE_ENGINE_SAMPLE_RATE        16000
#define VOICE_ENGINE_SAMPLE_BIT         16
#define VOICE_ENGINE_CHANNEL_COUNT      1

static const SLuint32 kPlayerBufferQueueSize    = 2;
static const SLuint32 kRecorderBufferQueueSize  = 2;

static const int kPlayerFrameTime       = 30;// in ms
static const int kPlayerFrameCount      = 2; // writeBufferTime = kPlayerFrameTime*kPlayerFrameCount
static const int kPlayerBufferCount     = 4; // playerRingbufTime = writeBufferTime*kPlayerBufferCount

static const int kRecorderFrameTime     = 30;// valid value for vad engine: { 10ms, 20ms, 30ms }
static const int kRecorderFrameCount    = 2; // readBufferTime = kRecorderFrameTime*kRecorderFrameCount
static const int kRecorderBufferCount   = 4; // recorderRingbufTime = readBufferTime*kRecorderBufferCount

class GnVendor_PcmOut {
public:
    GnVendor_PcmOut()
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

    ~GnVendor_PcmOut() {
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

class GnVendor_PcmIn {
public:
    GnVendor_PcmIn()
      : engineObj(nullptr),
        engineItf(nullptr),
        recorderObj(nullptr),
        recorderItf(nullptr),
        recorderBufferQueue(nullptr),
        queueSize(kRecorderBufferQueueSize),
#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
        vadHandle(nullptr),
        vadActive(false),
#endif
        enqueueBuffer(nullptr),
        ringbuf(nullptr),
        isStarted(false),
        isRecording(false),
        enableVoiceEngine(false),
        needStop(false)
    {}

    ~GnVendor_PcmIn() {
#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
        if (vadHandle != nullptr)
            litevad_destroy(vadHandle);
#endif
        if (ringbuf != nullptr)
            lockfree_ringbuf_destroy(ringbuf);
        if (enqueueBuffer != nullptr)
            delete enqueueBuffer;
    }

    SLObjectItf engineObj;
    SLEngineItf engineItf;
    SLObjectItf recorderObj;
    SLRecordItf recorderItf;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;
    SLuint32 queueSize;

    int sampleRate;
    int channelCount;
    int bitsPerSample;

#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
    litevad_handle_t vadHandle;
    bool vadActive;
#endif

    char *enqueueBuffer;
    int enqueueSize;
    void *ringbuf;
    std::mutex bufferLock;
    std::condition_variable bufferCanRead;
    bool isStarted;
    bool isRecording;
    bool enableVoiceEngine;
    bool needStop;
};

static std::mutex      sVoiceEngineLock;
static GnVendor_PcmIn *sVoiceEngineInst = nullptr;

// this callback handler is called every time a buffer finishes playing
static void GnVendor_playerBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    auto *out = reinterpret_cast<GnVendor_PcmOut *>(context);
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

// this callback handler is called every time a buffer finishes recording
static void GnVendor_recorderBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    auto *in = reinterpret_cast<GnVendor_PcmIn *>(context);
    if (!in->needStop) {
        static GenieSdk_Callback_t *sSdkCallback = nullptr;
        if (in->isRecording) {
#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
            litevad_result_t vadState = litevad_process(in->vadHandle, in->enqueueBuffer, in->enqueueSize);
            if (in->vadActive && vadState == LITEVAD_RESULT_SPEECH_END) {
                OS_LOGI(TAG, "VAD state changed (speech >> silence), onMicphoneSilence");
                if (sSdkCallback == nullptr)
                    GenieSdk_Get_Callback(&sSdkCallback);
                if (sSdkCallback != nullptr)
                    sSdkCallback->onMicphoneSilence();
            }
            if (!in->vadActive && vadState == LITEVAD_RESULT_SPEECH_BEGIN)
                in->vadActive = true;
#endif

            std::unique_lock<std::mutex> lk(in->bufferLock);
            if (lockfree_ringbuf_bytes_available(in->ringbuf) < in->enqueueSize)
                OS_LOGE(TAG, "Insufficient free buffer (free:%d < read:%d), maybe read-thread is blocked",
                        lockfree_ringbuf_bytes_available(in->ringbuf), in->enqueueSize);
            // enqueue the buffer that finishes recording
            lockfree_ringbuf_unsafe_overwrite(in->ringbuf, in->enqueueBuffer, in->enqueueSize);
            // notify reading-thread that the list has available buffer
            lk.unlock();
            in->bufferCanRead.notify_one();
        }

        if (in->enableVoiceEngine)
            TmallGenie_onFeedVoiceEngine(in->enqueueBuffer, in->enqueueSize);

        SLresult result = (*in->recorderBufferQueue)->Enqueue(in->recorderBufferQueue, in->enqueueBuffer, in->enqueueSize);
        if (SL_RESULT_SUCCESS != result)
            OS_LOGE(TAG, "Failed to enqueue buffer to recorderBufferQueue");
    } else {
        SLresult result = (*in->recorderItf)->SetRecordState(in->recorderItf, SL_RECORDSTATE_STOPPED);
        if (SL_RESULT_SUCCESS != result)
            OS_LOGE(TAG, "Failed to stop recorder");
        // notify stop-thread that the recorder has been stopped
        std::unique_lock<std::mutex> lk(in->bufferLock);
        in->isStarted = false;
        lk.unlock();
        in->bufferCanRead.notify_one();
    }
}

void *GnVendor_pcmOutOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGI(TAG, "GnVendor_pcmOutOpen: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
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
            OS_LOGE(TAG, "Unsupported sample bits(%d)", bitsPerSample);
            return nullptr;
    }

    int frameSize = sampleRate/1000*kPlayerFrameTime;
    int enqueueSize = (frameSize*channelCount*bitsPerSample/8)*kPlayerFrameCount;
    void *ringbuf = lockfree_ringbuf_create(enqueueSize*kPlayerBufferCount);
    if (ringbuf == nullptr)
        return nullptr;

    auto *out = new GnVendor_PcmOut();
    out->queueSize *= channelCount;
    if (sampleRate > 16000)
        out->queueSize *= (sampleRate/16000);
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
                static_cast<SLuint32>(channelCount),             // channel count
                static_cast<SLuint32>(sampleRate*1000),          // sample rate in milli second
                pcmformat,                                       // bitsPerSample
                pcmformat,                                       // containerSize
                channelCount == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                        // endianness
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
        result = (*out->playerBufferQueue)->RegisterCallback(out->playerBufferQueue, GnVendor_playerBufferQueueCallback, out);
        if (SL_RESULT_SUCCESS != result) break;
    } while (false);

    if (SL_RESULT_SUCCESS == result) {
        return out;
    } else {
        GnVendor_pcmOutClose(out);
        return nullptr;
    }
}

int GnVendor_pcmOutWrite(void *handle, void *buffer, unsigned int size)
{
    auto *out = reinterpret_cast<GnVendor_PcmOut *>(handle);

    if (size > lockfree_ringbuf_get_size(out->ringbuf)) {
        OS_LOGE(TAG, "Insufficient ringbuf size, write size too big");
        return -1;
    }

    std::unique_lock<std::mutex> lk(out->bufferLock);
    // waiting the list is available
    while (lockfree_ringbuf_bytes_available(out->ringbuf) < size)
        out->bufferCanWrite.wait(lk);

    lockfree_ringbuf_write(out->ringbuf, (char *)buffer, (int)size);
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
    return (int)size;
}

void GnVendor_pcmOutClose(void *handle)
{
    OS_LOGI(TAG, "GnVendor_pcmOutClose");
    auto *out = reinterpret_cast<GnVendor_PcmOut *>(handle);

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

static void *GnVendor_pcmInOpenHw(int sampleRate, int channelCount, int bitsPerSample)
{
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
            OS_LOGE(TAG, "Unsupported sample bits(%d)", bitsPerSample);
            return nullptr;
    }

    int frameSize = sampleRate/1000*kRecorderFrameTime;
    int enqueueSize = (frameSize*channelCount*bitsPerSample/8)*kRecorderFrameCount;
    void *ringbuf = lockfree_ringbuf_create(enqueueSize*kRecorderBufferCount);
    if (ringbuf == nullptr)
        return nullptr;

    auto *in = new GnVendor_PcmIn();
    in->ringbuf = ringbuf;
    in->enqueueSize = enqueueSize;
    in->enqueueBuffer = new char[in->enqueueSize];
    in->sampleRate = sampleRate;
    in->channelCount = channelCount;
    in->bitsPerSample = bitsPerSample;

#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
    if (sampleRate != VOICE_ENGINE_SAMPLE_RATE ||
        channelCount != VOICE_ENGINE_CHANNEL_COUNT ||
        bitsPerSample != VOICE_ENGINE_SAMPLE_BIT) {
        OS_LOGE(TAG, "Invalid record parameters for litevad");
        delete in;
        return nullptr;
    }
    in->vadHandle = litevad_create(sampleRate, channelCount, bitsPerSample);
    if (in->vadHandle == nullptr) {
        OS_LOGE(TAG, "Failed to litevad_create");
        delete in;
        return nullptr;
    }
#endif

    SLresult result = SL_RESULT_SUCCESS;
    do {
        // create engine
        result = slCreateEngine(&in->engineObj, 0, nullptr, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the engine
        result = (*in->engineObj)->Realize(in->engineObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the engine interface, which is needed in order to create other objects
        result = (*in->engineObj)->GetInterface(in->engineObj, SL_IID_ENGINE, &in->engineItf);
        if (SL_RESULT_SUCCESS != result) break;

        // configure audio source
        SLDataLocator_IODevice iodevice = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                           SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
        SLDataSource audioSrc = {&iodevice, nullptr};
        // configure audio sink
        SLDataLocator_AndroidSimpleBufferQueue bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, in->queueSize};
        SLDataFormat_PCM format_pcm = {
                SL_DATAFORMAT_PCM,
                static_cast<SLuint32>(channelCount),             // channel count
                static_cast<SLuint32>(sampleRate*1000),          // sample rate in milli second
                pcmformat,                                       // bitsPerSample
                pcmformat,                                       // containerSize
                channelCount == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                        // endianness
        };
        SLDataSink audioSnk = {&bufq, &format_pcm};

        // create audio recorder
        const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        result = (*in->engineItf)->CreateAudioRecorder(in->engineItf, &in->recorderObj, &audioSrc, &audioSnk,
                                                       sizeof(ids)/sizeof(SLInterfaceID), ids, req);

        if (SL_RESULT_SUCCESS != result) break;
        // realize the recorder
        result = (*in->recorderObj)->Realize(in->recorderObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the record interface
        result = (*in->recorderObj)->GetInterface(in->recorderObj, SL_IID_RECORD, &in->recorderItf);
        if (SL_RESULT_SUCCESS != result) break;
        // get the buffer queue interface
        result = (*in->recorderObj)->GetInterface(in->recorderObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &in->recorderBufferQueue);
        if (SL_RESULT_SUCCESS != result) break;
        result = (*in->recorderBufferQueue)->Enqueue(in->recorderBufferQueue, in->enqueueBuffer, in->enqueueSize);
        if (SL_RESULT_SUCCESS != result) break;
        // register callback on the buffer queue
        result = (*in->recorderBufferQueue)->RegisterCallback(in->recorderBufferQueue, GnVendor_recorderBufferQueueCallback, in);
        if (SL_RESULT_SUCCESS != result) break;

        // set the recorder's state to recording
        result = (*in->recorderItf)->SetRecordState(in->recorderItf, SL_RECORDSTATE_RECORDING);
        if (SL_RESULT_SUCCESS != result) break;
        in->isStarted = true;
    } while (false);

    if (SL_RESULT_SUCCESS == result) {
        return in;
    } else {
        GnVendor_pcmInClose(in);
        return nullptr;
    }
}

int GnVendor_pcmInRead(void *handle, void *buffer, unsigned int size)
{
    auto *in = reinterpret_cast<GnVendor_PcmIn *>(handle);

    std::unique_lock<std::mutex> lk(in->bufferLock);
    // waiting the list is available
    while (lockfree_ringbuf_bytes_filled(in->ringbuf) <= 0)
        in->bufferCanRead.wait(lk);

    return lockfree_ringbuf_read(in->ringbuf, (char *)buffer, (int)size);
}

static void GnVendor_pcmInCloseHW(void *handle)
{
    auto *in = reinterpret_cast<GnVendor_PcmIn *>(handle);
    in->needStop = true;
    {
        std::unique_lock<std::mutex> lk(in->bufferLock);
        while (in->isStarted)
            in->bufferCanRead.wait(lk);
    }

    if (in->recorderObj != nullptr)
        (*in->recorderObj)->Destroy(in->recorderObj);
    if (in->engineObj != nullptr)
        (*in->engineObj)->Destroy(in->engineObj);

    delete in;
}

void *GnVendor_pcmInOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGI(TAG, "GnVendor_pcmInOpen: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);

    std::lock_guard<std::mutex> lg(sVoiceEngineLock);

    if (sVoiceEngineInst != nullptr) {
        if (sampleRate != sVoiceEngineInst->sampleRate ||
            channelCount != sVoiceEngineInst->channelCount ||
            bitsPerSample != sVoiceEngineInst->bitsPerSample) {
            OS_LOGE(TAG, "Keyword-detect enabled, can't get voice-engine instance with incompatible parameters");
            return nullptr;
        }
#if defined(GENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
        litevad_reset(sVoiceEngineInst->vadHandle);
        sVoiceEngineInst->vadActive = false;
#endif
        {
            std::unique_lock<std::mutex> ul(sVoiceEngineInst->bufferLock);
            lockfree_ringbuf_unsafe_reset(sVoiceEngineInst->ringbuf);
        }

        OS_LOGD(TAG, "Keyword-detect enabled, get voice-engine instance instead of opening new one");
        sVoiceEngineInst->isRecording = true;
        return sVoiceEngineInst;
    }

    sVoiceEngineInst = (GnVendor_PcmIn *)GnVendor_pcmInOpenHw(sampleRate, channelCount, bitsPerSample);
    if (sVoiceEngineInst != nullptr)
        sVoiceEngineInst->isRecording = true;
    return sVoiceEngineInst;
}

void GnVendor_pcmInClose(void *handle)
{
    OS_LOGI(TAG, "GnVendor_pcmInClose");
    auto *in = reinterpret_cast<GnVendor_PcmIn *>(handle);

    std::lock_guard<std::mutex> lk(sVoiceEngineLock);

    if (in != sVoiceEngineInst) {
        GnVendor_pcmInCloseHW(in);
        return;
    }

    if (sVoiceEngineInst->enableVoiceEngine) {
        OS_LOGD(TAG, "Keyword-detect enabled, don't actually close voice-engine instance");
        sVoiceEngineInst->isRecording = false;
        return;
    }

    GnVendor_pcmInCloseHW(sVoiceEngineInst);
    sVoiceEngineInst = nullptr;
}

bool GnVendor_startVoiceEngine(int sampleRate, int ChannelCount, int bitsPerSample)
{
    OS_LOGI(TAG, "GnVendor_enableVoiceEngine");

    std::lock_guard<std::mutex> lk(sVoiceEngineLock);

    if (sVoiceEngineInst == nullptr)
        sVoiceEngineInst = (GnVendor_PcmIn *)GnVendor_pcmInOpenHw(sampleRate, ChannelCount, bitsPerSample);
    if (sVoiceEngineInst == nullptr || (sVoiceEngineInst->sampleRate != sampleRate ||
                                        sVoiceEngineInst->channelCount != ChannelCount ||
                                        sVoiceEngineInst->bitsPerSample != bitsPerSample)) {
        OS_LOGE(TAG, "Failed to GnVendor_enableVoiceEngine");
        return false;
    }
    sVoiceEngineInst->enableVoiceEngine = true;

    return true;
}

void GnVendor_stopVoiceEngine()
{
    OS_LOGI(TAG, "GnVendor_disableVoiceEngine");

    std::lock_guard<std::mutex> lk(sVoiceEngineLock);

    if (sVoiceEngineInst != nullptr) {
        sVoiceEngineInst->enableVoiceEngine = false;
        if (sVoiceEngineInst->isRecording)
            return;
        GnVendor_pcmInCloseHW(sVoiceEngineInst);
        sVoiceEngineInst = nullptr;
    }
}
