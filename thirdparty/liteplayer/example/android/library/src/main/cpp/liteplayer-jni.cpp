/*
 * Copyright (C) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
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

/* This is a JNI example where we use native methods to play music
 * using the native liteplayer* APIs.
 * See the corresponding Java source file located at:
 *
 *   src/com/example/liteplayerdemo/Liteplayer.java
 *
 * In this example we use assert() for "impossible" error conditions,
 * and explicit handling and recovery for more likely error conditions.
 */

#include <jni.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <android/log.h>

#include "liteplayer_main.h"
#include "liteplayer_adapter.h"
#include "source_file_wrapper.h"
#include "source_httpclient_wrapper.h"
#include "sink_opensles_wrapper.h"

#define TAG "LiteplayerJNI"
#define JAVA_CLASS_NAME "com/sepnic/liteplayer/Liteplayer"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

//#define ENABLE_OPENSLES

#define OS_LOGF(tag, ...) __android_log_print(ANDROID_LOG_FATAL,tag, __VA_ARGS__)
#define OS_LOGE(tag, ...) __android_log_print(ANDROID_LOG_ERROR, tag, __VA_ARGS__)
#define OS_LOGW(tag, ...) __android_log_print(ANDROID_LOG_WARN, tag, __VA_ARGS__)
#define OS_LOGI(tag, ...) __android_log_print(ANDROID_LOG_INFO, tag, __VA_ARGS__)
#define OS_LOGD(tag, ...) __android_log_print(ANDROID_LOG_DEBUG, tag, __VA_ARGS__)
#define OS_LOGV(tag, ...) //__android_log_print(ANDROID_LOG_VERBOSE, tag, __VA_ARGS__)

struct liteplayer_priv {
    liteplayer_handle_t mPlayer;
    jmethodID   mPostEvent;
#if !defined(ENABLE_OPENSLES)
    jmethodID   mOpenTrack;
    jmethodID   mWriteTrack;
    jmethodID   mCloseTrack;
#endif
    jclass      mClass;
    jobject     mObject;
};

static JavaVM *sJavaVM = nullptr;

static void jniThrowException(JNIEnv *env, const char *className, const char *msg) {
    jclass clazz = env->FindClass(className);
    if (!clazz) {
        OS_LOGE(TAG, "Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        return;
    }

    if (env->ThrowNew(clazz, msg) != JNI_OK) {
        OS_LOGE(TAG, "Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
    }
    env->DeleteLocalRef(clazz);
}

#if !defined(ENABLE_OPENSLES)
static const char *audiotrack_wrapper_name()
{
    return "iotrack";
}

static sink_handle_t audiotrack_wrapper_open(int samplerate, int channels, int bits, void *sink_priv)
{
    OS_LOGD(TAG, "Opening AudioTrack: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    JNIEnv *env;
    JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerAudioTrack", nullptr };
    jint res = sJavaVM->AttachCurrentThread(&env, &args);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
        return nullptr;
    }

    auto priv = reinterpret_cast<struct liteplayer_priv *>(sink_priv);
    res = env->CallStaticIntMethod(priv->mClass, priv->mOpenTrack, priv->mObject, samplerate, channels, bits);

    sJavaVM->DetachCurrentThread();
    return res == 0 ? (sink_handle_t)priv : nullptr;
}

static int audiotrack_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGV(TAG, "Writing AudioTrack: buffer=%p, size=%d", buffer, size);
    JNIEnv *env;
    JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerAudioTrack", nullptr };
    jint res = sJavaVM->AttachCurrentThread(&env, &args);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
        return -1;
    }

    jbyteArray sampleArray = env->NewByteArray(size);
    if (sampleArray == nullptr) {
        sJavaVM->DetachCurrentThread();
        return -1;
    }
    jbyte *sampleByte = env->GetByteArrayElements(sampleArray, nullptr);
    memcpy(sampleByte, buffer, size);
    env->ReleaseByteArrayElements(sampleArray, sampleByte, 0);

    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    res = env->CallStaticIntMethod(priv->mClass, priv->mWriteTrack, priv->mObject, sampleArray, size);

    env->DeleteLocalRef(sampleArray);
    sJavaVM->DetachCurrentThread();
    return size;
}

static void audiotrack_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "Closing AudioTrack");
    JNIEnv *env;
    JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerAudioTrack", nullptr };
    jint res = sJavaVM->AttachCurrentThread(&env, &args);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
        return;
    }

    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    env->CallStaticVoidMethod(priv->mClass, priv->mCloseTrack, priv->mObject);

    sJavaVM->DetachCurrentThread();
}
#endif

static int Liteplayer_native_stateCallback(enum liteplayer_state state, int errcode, void *callback_priv)
{
    OS_LOGD(TAG, "Liteplayer_native_stateCallback: state=%d, errcode=%d", state, errcode);
    JNIEnv *env;
    jint res = sJavaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    jboolean attached = JNI_FALSE;
    if (res != JNI_OK) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerStateCallback", nullptr };
        res = sJavaVM->AttachCurrentThread(&env, &args);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return -1;
        }
        attached = true;
    }

    auto priv = reinterpret_cast<struct liteplayer_priv *>(callback_priv);
    env->CallStaticVoidMethod(priv->mClass, priv->mPostEvent, priv->mObject, state, errcode);

    if (attached)
        sJavaVM->DetachCurrentThread();
    return 0;
}

static jlong Liteplayer_native_create(JNIEnv* env, jobject thiz, jobject weak_this)
{
    OS_LOGD(TAG, "Liteplayer_native_create");

    struct liteplayer_priv *priv = (struct liteplayer_priv *)calloc(1, sizeof(struct liteplayer_priv));
    if (priv == nullptr) return (jlong)nullptr;

    jclass clazz;
    clazz = env->FindClass(JAVA_CLASS_NAME);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        free(priv);
        return (jlong)nullptr;
    }
    priv->mPostEvent = env->GetStaticMethodID(clazz, "postEventFromNative", "(Ljava/lang/Object;II)V");
    if (priv->mPostEvent == nullptr) {
        OS_LOGE(TAG, "Failed to get postEventFromNative mothod");
        free(priv);
        return (jlong)nullptr;
    }
#if !defined(ENABLE_OPENSLES)
    priv->mOpenTrack = env->GetStaticMethodID(clazz, "openAudioTrackFromNative", "(Ljava/lang/Object;III)I");
    if (priv->mOpenTrack == nullptr) {
        OS_LOGE(TAG, "Failed to get openAudioTrackFromNative mothod");
        free(priv);
        return (jlong)nullptr;
    }
    priv->mWriteTrack = env->GetStaticMethodID(clazz, "writeAudioTrackFromNative", "(Ljava/lang/Object;[BI)I");
    if (priv->mWriteTrack == nullptr) {
        OS_LOGE(TAG, "Failed to get writeAudioTrackFromNative mothod");
        free(priv);
        return (jlong)nullptr;
    }
    priv->mCloseTrack = env->GetStaticMethodID(clazz, "closeAudioTrackFromNative", "(Ljava/lang/Object;)V");
    if (priv->mCloseTrack == nullptr) {
        OS_LOGE(TAG, "Failed to get closeAudioTrackFromNative mothod");
        free(priv);
        return (jlong)nullptr;
    }
#endif
    env->DeleteLocalRef(clazz);

    // Hold onto Liteplayer class for use in calling the static method that posts events to the application thread.
    clazz = env->GetObjectClass(thiz);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        free(priv);
        jniThrowException(env, "java/lang/Exception", nullptr);
        return (jlong)nullptr;
    }
    priv->mClass = (jclass)env->NewGlobalRef(clazz);
    // We use a weak reference so the Liteplayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    priv->mObject  = env->NewGlobalRef(weak_this);

    priv->mPlayer = liteplayer_create();
    if (priv->mPlayer == nullptr) {
        free(priv);
        return (jlong)nullptr;
    }
    // Register state listener, todo: notify java objest
    liteplayer_register_state_listener(priv->mPlayer, Liteplayer_native_stateCallback, priv);
    // Register sink adapter
    struct sink_wrapper sink_ops = {
#if !defined(ENABLE_OPENSLES)
            .priv_data = priv,
            .name = audiotrack_wrapper_name,
            .open = audiotrack_wrapper_open,
            .write = audiotrack_wrapper_write,
            .close = audiotrack_wrapper_close,
#else
            .priv_data = NULL,
            .name = opensles_wrapper_name,
            .open = opensles_wrapper_open,
            .write = opensles_wrapper_write,
            .close = opensles_wrapper_close,
#endif
    };
    liteplayer_register_sink_wrapper(priv->mPlayer, &sink_ops);
    // Register file adapter
    struct source_wrapper file_ops = {
            .async_mode = false,
            .buffer_size = 2*1024,
            .priv_data = NULL,
            .url_protocol = file_wrapper_url_protocol,
            .open = file_wrapper_open,
            .read = file_wrapper_read,
            .content_pos = file_wrapper_content_pos,
            .content_len = file_wrapper_content_len,
            .seek = file_wrapper_seek,
            .close = file_wrapper_close,
    };
    liteplayer_register_source_wrapper(priv->mPlayer, &file_ops);
    // Register http adapter
    struct source_wrapper http_ops = {
            .async_mode = true,
            .buffer_size = 256*1024,
            .priv_data = NULL,
            .url_protocol = httpclient_wrapper_url_protocol,
            .open = httpclient_wrapper_open,
            .read = httpclient_wrapper_read,
            .content_pos = httpclient_wrapper_content_pos,
            .content_len = httpclient_wrapper_content_len,
            .seek = httpclient_wrapper_seek,
            .close = httpclient_wrapper_close,
    };
    liteplayer_register_source_wrapper(priv->mPlayer, &http_ops);

    return (jlong)priv;
}

static jint Liteplayer_native_setDataSource(JNIEnv *env, jobject thiz, jlong handle, jstring path)
{
    OS_LOGD(TAG, "Liteplayer_native_setDataSource");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    if (path == nullptr) {
        jniThrowException(env, "java/lang/IllegalArgumentException", nullptr);
        return -1;
    }
    const char *tmp = env->GetStringUTFChars(path, nullptr);
    if (tmp == nullptr) {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return -1;
    }
    std::string url = tmp;
    env->ReleaseStringUTFChars(path, tmp);
    return (jint) liteplayer_set_data_source(priv->mPlayer, url.c_str());
}

static jint Liteplayer_native_prepareAsync(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_prepareAsync");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_prepare_async(priv->mPlayer);
}

static jint Liteplayer_native_start(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_start");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_start(priv->mPlayer);
}

static jint Liteplayer_native_pause(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_pause");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_pause(priv->mPlayer);
}

static jint Liteplayer_native_resume(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_resume");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_resume(priv->mPlayer);
}

static jint Liteplayer_native_seekTo(JNIEnv *env, jobject thiz, jlong handle, jint msec)
{
    OS_LOGD(TAG, "Liteplayer_native_seekTo");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_seek(priv->mPlayer, msec);
}

static jint Liteplayer_native_stop(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_stop");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_stop(priv->mPlayer);
}

static jint Liteplayer_native_reset(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_reset");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_reset(priv->mPlayer);
}

static jint Liteplayer_native_getCurrentPosition(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_getCurrentPosition");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return 0;
    }
    int msec = 0;
    liteplayer_get_position(priv->mPlayer, &msec);
    return (jint)msec;
}

static jint Liteplayer_native_getDuration(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_getCurrentPosition");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return 0;
    }
    int msec = 0;
    liteplayer_get_duration(priv->mPlayer, &msec);
    return (jint)msec;
}

static void Liteplayer_native_destroy(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_destroy");
    auto priv = reinterpret_cast<struct liteplayer_priv *>(handle);
    if (priv == nullptr || priv->mPlayer == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return;
    }
    liteplayer_destroy(priv->mPlayer);
    priv->mPlayer = nullptr;
    // remove global references
    env->DeleteGlobalRef(priv->mObject);
    env->DeleteGlobalRef(priv->mClass);
    priv->mObject = nullptr;
    priv->mClass = nullptr;
    free(priv);
}

static JNINativeMethod gMethods[] = {
        {"native_create", "(Ljava/lang/Object;)J", (void *)Liteplayer_native_create},
        {"native_destroy", "(J)V", (void *)Liteplayer_native_destroy},
        {"native_setDataSource", "(JLjava/lang/String;)I", (void *)Liteplayer_native_setDataSource},
        {"native_prepareAsync", "(J)I", (void *)Liteplayer_native_prepareAsync},
        {"native_start", "(J)I", (void *)Liteplayer_native_start},
        {"native_pause", "(J)I", (void *)Liteplayer_native_pause},
        {"native_resume", "(J)I", (void *)Liteplayer_native_resume},
        {"native_seekTo", "(JI)I", (void *)Liteplayer_native_seekTo},
        {"native_stop", "(J)I", (void *)Liteplayer_native_stop},
        {"native_reset", "(J)I", (void *)Liteplayer_native_reset},
        {"native_getCurrentPosition", "(J)I", (void *)Liteplayer_native_getCurrentPosition},
        {"native_getDuration", "(J)I", (void *)Liteplayer_native_getDuration},
};

static int registerNativeMethods(JNIEnv *env, const char *className,JNINativeMethod *getMethods, int methodsNum)
{
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == nullptr) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz,getMethods,methodsNum) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = nullptr;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        OS_LOGE(TAG, "Failed to get env");
        goto bail;
    }
    assert(env != nullptr);

    if (registerNativeMethods(env, JAVA_CLASS_NAME, gMethods, NELEM(gMethods)) != JNI_TRUE) {
        OS_LOGE(TAG, "Failed to register native methods");
        goto bail;
    }

    sJavaVM = vm;
    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

bail:
    return result;
}
