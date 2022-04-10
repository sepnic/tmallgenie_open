/*
 * Copyright (C) 2018-2020 luoyun <sysu.zqlong@gmail.com>
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

#include <jni.h>
#include <stdio.h>
#include <string>
#include <android/log.h>
#include "litevad.h"

#define TAG "NativeLiteVad"

#define pr_dbg(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, fmt, ##__VA_ARGS__)

#define JAVA_CLASS_NAME "com/example/litevad_demo/MainActivity"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

struct litevad_priv {
    litevad_handle_t mVadHandle;
};

static void jniThrowException(JNIEnv *env, const char *className, const char *msg) {
    jclass clazz = env->FindClass(className);
    if (!clazz) {
        pr_err("Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        return;
    }

    if (env->ThrowNew(clazz, msg) != JNI_OK) {
        pr_err("Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
    }
    env->DeleteLocalRef(clazz);
}

static jlong Litevad_native_create(JNIEnv* env, jobject thiz, jint sample_rate, jint channel_count)
{
    pr_dbg("@@@ Litevad_native_create");

    struct litevad_priv *priv = (struct litevad_priv *)calloc(1, sizeof(struct litevad_priv));
    if (priv == nullptr) return (jlong)nullptr;

    priv->mVadHandle = litevad_create(sample_rate, channel_count);
    if (priv->mVadHandle == nullptr) {
        free(priv);
        return (jlong)nullptr;
    }
    return (jlong)priv;
}

static jint Litevad_native_process(JNIEnv *env, jobject thiz, jlong handle, jbyteArray buff, jint size)
{
    //pr_dbg("@@@ Litevad_native_process");

    auto priv = reinterpret_cast<struct litevad_priv *>(handle);
    if (priv == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }

    jbyte *audio_buff = env->GetByteArrayElements(buff, nullptr);
    jint ret = litevad_process(priv->mVadHandle, (void *)audio_buff, size);
    env->ReleaseByteArrayElements(buff, audio_buff, 0);
    return ret;
}

static void Litevad_native_reset(JNIEnv *env, jobject thiz, jlong handle)
{
    pr_dbg("@@@ Litevad_native_reset");

    auto priv = reinterpret_cast<struct litevad_priv *>(handle);
    if (priv == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return;
    }

    litevad_reset(priv->mVadHandle);
}

static void Litevad_native_destroy(JNIEnv *env, jobject thiz, jlong handle)
{
    pr_dbg("@@@ Litevad_native_destroy");

    auto priv = reinterpret_cast<struct litevad_priv *>(handle);
    if (priv == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return;
    }

    litevad_destroy(priv->mVadHandle);
    free(priv);
}

static JNINativeMethod gMethods[] = {
        {"native_create", "(III)J", (void *)Litevad_native_create},
        {"native_process", "(J[BI)I", (void *)Litevad_native_process}, 
        {"native_reset", "(J)V", (void *)Litevad_native_reset},
        {"native_destroy", "(J)V", (void *)Litevad_native_destroy},
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
        pr_err("Failed to get env");
        goto bail;
    }
    assert(env != nullptr);

    if (registerNativeMethods(env, JAVA_CLASS_NAME, gMethods, NELEM(gMethods)) != JNI_TRUE) {
        pr_err("Failed to register native methods");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

bail:
    return result;
}
