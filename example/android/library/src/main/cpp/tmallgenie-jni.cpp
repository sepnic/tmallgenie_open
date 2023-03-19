/*
 * Copyright (C) 2023- Qinglong<sysu.zqlong@gmail.com>
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
#include <cstdio>
#include <cassert>
#include <string>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "GenieVendor_Android.h"
#include "GenieSdk.h"

#define TAG "TmallGenieJNI"
#define JAVA_CLASS_NAME "com/sepnic/tmallgenie/TmallGenie"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

class TmallGenieJni {
public:
    TmallGenieJni() : mSdkInited(JNI_FALSE), mSdkStarted(JNI_FALSE) {}
    ~TmallGenieJni() = default;
    GenieSdk_Callback_t *mSdkCallback;
    jboolean    mSdkInited;
    jboolean    mSdkStarted;
    jmethodID   mOnGetVolume;
    jmethodID   mOnSetVolume;
    jmethodID   mOnFeedVoiceEngine;
    jmethodID   mOnCommand;
    jmethodID   mOnStatus;
    jmethodID   mOnAsrResult;
    jmethodID   mOnNluResult;
    jclass      mClass;
    jobject     mObject;
};

static JavaVM *sJavaVM = nullptr;
static TmallGenieJni sTmallGenieJni;

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

static bool jniGetEnv(JNIEnv **env, jboolean *attached)
{
    jint res = sJavaVM->GetEnv((void**) env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = sJavaVM->AttachCurrentThread(env, nullptr);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return false;
        }
        *attached = JNI_TRUE;
    }
    return true;
}

int TmallGenie_onGetVolume()
{
    OS_LOGI(TAG, "TmallGenie_onGetVolume");
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return false;
    jint volume = env->CallStaticIntMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnGetVolume, sTmallGenieJni.mObject);
    if (attached)
        sJavaVM->DetachCurrentThread();
    return volume;
}

bool TmallGenie_onSetVolume(int volume)
{
    OS_LOGI(TAG, "TmallGenie_onSetVolume: volume:%d", volume);
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return false;
    jboolean res = env->CallStaticBooleanMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnSetVolume, sTmallGenieJni.mObject, (jint)volume);
    if (attached)
        sJavaVM->DetachCurrentThread();
    return res == JNI_TRUE;
}

void TmallGenie_onFeedVoiceEngine(char *buffer, int size)
{
    OS_LOGV(TAG, "TmallGenie_onFeedVoiceEngine");
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return;
    jbyteArray byteArray = env->NewByteArray(size);
    env->SetByteArrayRegion(byteArray, 0, size, reinterpret_cast<const jbyte *>(buffer));
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnFeedVoiceEngine, sTmallGenieJni.mObject,
                              byteArray, size);
    env->DeleteLocalRef(byteArray);
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static void TmallGenie_onCommand(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    OS_LOGI(TAG, "TmallGenie_onCommand: domain:%d, command:%d, payload:%s", domain, command, payload);
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return;
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnCommand, sTmallGenieJni.mObject,
                              (jint)domain, (jint)command, env->NewStringUTF(payload));
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static void TmallGenie_onStatus(Genie_Status_t status)
{
    OS_LOGI(TAG, "TmallGenie_onStatus: %d", status);
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return;
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnStatus, sTmallGenieJni.mObject,
                              (jint)status);
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static void TmallGenie_onAsrResult(const char *result)
{
    OS_LOGI(TAG, "TmallGenie_onAsrResult: %s", result);
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return;
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnAsrResult, sTmallGenieJni.mObject,
                              env->NewStringUTF(result));
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static void TmallGenie_onNluResult(const char *result)
{
    OS_LOGI(TAG, "TmallGenie_onNluResult: %s", result);
    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return;
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnNluResult, sTmallGenieJni.mObject,
                              env->NewStringUTF(result));
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static jboolean TmallGenie_nativeCreate(JNIEnv* env, jobject thiz, jobject weak_this, jstring wifiMac,
                                        jstring bizType, jstring bizGroup, jstring bizSecret, jstring caCert,
                                        jstring uuid, jstring accessToken)
{
    OS_LOGI(TAG, "TmallGenie_nativeCreate");

    if (wifiMac == nullptr)
        jniThrowException(env, "java/lang/IllegalArgumentException", "Received NULL wifiMac");

    if (sTmallGenieJni.mSdkInited) {
        OS_LOGE(TAG, "TmallGenie already inited");
        return JNI_FALSE;
    }

    jclass clazz = env->GetObjectClass(thiz);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        jniThrowException(env, "java/lang/Exception", nullptr);
        return JNI_FALSE;
    }
    sTmallGenieJni.mClass = (jclass)env->NewGlobalRef(clazz);
    sTmallGenieJni.mObject  = env->NewGlobalRef(weak_this);

    const char *bizTypeChar = nullptr, *bizGroupChar = nullptr, *bizSecretChar = nullptr, *caCertChar = nullptr;
    const char *uuidChar = nullptr, *accessTokenChar = nullptr;
    const char *wifiMacChar = env->GetStringUTFChars(wifiMac, nullptr);
    if (bizType != nullptr)
        bizTypeChar = env->GetStringUTFChars(bizType, nullptr);
    if (bizGroup != nullptr)
        bizGroupChar = env->GetStringUTFChars(bizGroup, nullptr);
    if (bizSecret != nullptr)
        bizSecretChar = env->GetStringUTFChars(bizSecret, nullptr);
    if (caCert != nullptr)
        caCertChar = env->GetStringUTFChars(caCert, nullptr);
    if (uuid != nullptr)
        uuidChar = env->GetStringUTFChars(uuid, nullptr);
    if (accessToken != nullptr)
        accessTokenChar = env->GetStringUTFChars(accessToken, nullptr);

    bool result = GnVendor_init(wifiMacChar, bizTypeChar, bizGroupChar, bizSecretChar, caCertChar,
                                uuidChar, accessTokenChar);

    env->ReleaseStringUTFChars(wifiMac, wifiMacChar);
    if (bizType != nullptr)
        env->ReleaseStringUTFChars(bizType, bizTypeChar);
    if (bizGroup != nullptr)
        env->ReleaseStringUTFChars(bizGroup, bizGroupChar);
    if (bizSecret != nullptr)
        env->ReleaseStringUTFChars(bizSecret, bizSecretChar);
    if (caCert != nullptr)
        env->ReleaseStringUTFChars(caCert, caCertChar);
    if (uuid != nullptr)
        env->ReleaseStringUTFChars(uuid, uuidChar);
    if (accessToken != nullptr)
        env->ReleaseStringUTFChars(accessToken, accessTokenChar);
    if (!result) {
        OS_LOGE(TAG, "Failed to GnVendor_init");
        return JNI_FALSE;
    }

    GnVendor_Wrapper_t adapter = {
            .bizType = GnVendor_bizType,
            .bizGroup = GnVendor_bizGroup,
            .bizSecret = GnVendor_bizSecret,
            .caCert = GnVendor_caCert,
            .macAddr = GnVendor_macAddr,
            .uuid = GnVendor_uuid,
            .accessToken = GnVendor_accessToken,
            .pcmOutOpen = GnVendor_pcmOutOpen,
            .pcmOutWrite = GnVendor_pcmOutWrite,
            .pcmOutClose = GnVendor_pcmOutClose,
            .pcmInOpen = GnVendor_pcmInOpen,
            .pcmInRead = GnVendor_pcmInRead,
            .pcmInClose = GnVendor_pcmInClose,
            .setSpeakerVolume = GnVendor_setSpeakerVolume,
            .getSpeakerVolume = GnVendor_getSpeakerVolume,
            .setSpeakerMuted = GnVendor_setSpeakerMuted,
            .getSpeakerMuted = GnVendor_getSpeakerMuted,
    };
    if (!GenieSdk_Init(&adapter)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Init");
        return JNI_FALSE;
    }
    if (!GenieSdk_Get_Callback(&sTmallGenieJni.mSdkCallback)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Get_Callback");
        return JNI_FALSE;
    }
    if (!GenieSdk_Register_CommandListener(TmallGenie_onCommand)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Register_CommandListener");
        JNI_FALSE;
    }
    if (!GenieSdk_Register_StatusListener(TmallGenie_onStatus)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Register_StatusListener");
        JNI_FALSE;
    }
    if (!GenieSdk_Register_AsrResultListener(TmallGenie_onAsrResult)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Register_AsrResultListener");
        JNI_FALSE;
    }
    if (!GenieSdk_Register_NluResultListener(TmallGenie_onNluResult)) {
        OS_LOGE(TAG, "Failed to GenieSdk_Register_NluResultListener");
        JNI_FALSE;
    }

    sTmallGenieJni.mSdkInited = JNI_TRUE;
    return JNI_TRUE;
}

static jboolean TmallGenie_nativeStart(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_nativeStart");
    if (!sTmallGenieJni.mSdkInited)
        return JNI_FALSE;
    if (!sTmallGenieJni.mSdkStarted) {
        if (!GenieSdk_Start()) {
            OS_LOGE(TAG, "Failed to GenieSdk_Start");
            return JNI_FALSE;
        }
        sTmallGenieJni.mSdkStarted = JNI_TRUE;
    }
    return sTmallGenieJni.mSdkStarted;
}

static void TmallGenie_nativeStop(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_nativeStop");
    if (sTmallGenieJni.mSdkStarted) {
        GenieSdk_Stop();
        sTmallGenieJni.mSdkStarted = JNI_FALSE;
    }
}

static void TmallGenie_nativeDestroy(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_nativeDestroy");
    if (sTmallGenieJni.mSdkStarted) {
        GenieSdk_Stop();
        sTmallGenieJni.mSdkStarted = JNI_FALSE;
    }
    if (sTmallGenieJni.mSdkInited) {
        GenieSdk_Unregister_CommandListener(TmallGenie_onCommand);
        GenieSdk_Unregister_StatusListener(TmallGenie_onStatus);
        GenieSdk_Unregister_AsrResultListener(TmallGenie_onAsrResult);
        GenieSdk_Unregister_NluResultListener(TmallGenie_onNluResult);
        sTmallGenieJni.mSdkInited = JNI_FALSE;
    }
    // remove global references
    env->DeleteGlobalRef(sTmallGenieJni.mObject);
    env->DeleteGlobalRef(sTmallGenieJni.mClass);
    sTmallGenieJni.mObject = nullptr;
    sTmallGenieJni.mClass = nullptr;
}

static void TmallGenie_onNetworkConnected(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_onNetworkConnected");
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onNetworkConnected();
}

static void TmallGenie_onNetworkDisconnected(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_onNetworkDisconnected");
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onNetworkDisconnected();
}

static void TmallGenie_onMicphoneWakeup(JNIEnv *env, jobject thiz, jstring wakeupWord, jint doa, jdouble confidence)
{
    OS_LOGI(TAG, "TmallGenie_onMicphoneWakeup");
    if (wakeupWord == nullptr)
        jniThrowException(env, "java/lang/IllegalArgumentException", "Received NULL wakeupWord");
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onMicphoneWakeup("tianmaojingling", 0, 0.618);
}

static void TmallGenie_onMicphoneSilence(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_onMicphoneSilence");
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onMicphoneSilence();
}

static void TmallGenie_onSpeakerVolumeChanged(JNIEnv *env, jobject thiz, jint volume)
{
    OS_LOGI(TAG, "TmallGenie_onSpeakerVolumeChanged: volume=%d", volume);
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onSpeakerVolumeChanged(volume);
}

static void TmallGenie_onSpeakerMutedChanged(JNIEnv *env, jobject thiz, jboolean muted)
{
    OS_LOGI(TAG, "TmallGenie_onSpeakerMutedChanged: muted=%d", muted);
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onSpeakerMutedChanged(muted);
}
static void TmallGenie_onQueryUserInfo(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_onQueryUserInfo");
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onQueryUserInfo();
}

static void TmallGenie_onTextRecognize(JNIEnv *env, jobject thiz, jstring inputText)
{
    OS_LOGI(TAG, "TmallGenie_onTextRecognize");
    if (inputText == nullptr)
        jniThrowException(env, "java/lang/IllegalArgumentException", "Received NULL inputText");
    if (sTmallGenieJni.mSdkInited) {
        const char *text = env->GetStringUTFChars(inputText, nullptr);
        sTmallGenieJni.mSdkCallback->onTextRecognize(text);
        env->ReleaseStringUTFChars(inputText, text);
    }
}

static jboolean TmallGenie_startVoiceEngine(JNIEnv *env, jobject thiz, jint sampleRate, jint ChannelCount, jint bitsPerSample)
{
    OS_LOGI(TAG, "TmallGenie_startVoiceEngine");
    return GnVendor_startVoiceEngine(sampleRate, ChannelCount, bitsPerSample);
}

static void TmallGenie_stopVoiceEngine(JNIEnv *env, jobject thiz)
{
    OS_LOGI(TAG, "TmallGenie_stopVoiceEngine");
    GnVendor_stopVoiceEngine();
}

static JNINativeMethod gMethods[] = {
        {"native_create",
         "(Ljava/lang/Object;Ljava/lang/String;"
         "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
         "Ljava/lang/String;Ljava/lang/String;)Z",
         (void *)TmallGenie_nativeCreate},
        {"native_start", "()Z", (void *)TmallGenie_nativeStart},
        {"native_stop", "()V", (void *)TmallGenie_nativeStop},
        {"native_destroy", "()V", (void *)TmallGenie_nativeDestroy},
        {"native_onNetworkConnected", "()V", (void *)TmallGenie_onNetworkConnected},
        {"native_onNetworkDisconnected", "()V", (void *)TmallGenie_onNetworkDisconnected},
        {"native_onMicphoneWakeup", "(Ljava/lang/String;ID)V", (void *)TmallGenie_onMicphoneWakeup},
        {"native_onMicphoneSilence", "()V", (void *)TmallGenie_onMicphoneSilence},
        {"native_onSpeakerVolumeChanged", "(I)V", (void *)TmallGenie_onSpeakerVolumeChanged},
        {"native_onSpeakerMutedChanged", "(Z)V", (void *)TmallGenie_onSpeakerMutedChanged},
        {"native_onQueryUserInfo", "()V", (void *)TmallGenie_onQueryUserInfo},
        {"native_onTextRecognize", "(Ljava/lang/String;)V", (void *)TmallGenie_onTextRecognize},
        {"native_startVoiceEngine", "(III)Z", (void *)TmallGenie_startVoiceEngine},
        {"native_stopVoiceEngine", "()V", (void *)TmallGenie_stopVoiceEngine},
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

    sTmallGenieJni.mOnGetVolume = env->GetStaticMethodID(clazz, "onGetVolumeFromNative", "(Ljava/lang/Object;)I");
    if (sTmallGenieJni.mOnGetVolume == nullptr) {
        OS_LOGE(TAG, "Failed to get onGetVolumeFromNative mothod");
        return JNI_FALSE;
    }
    sTmallGenieJni.mOnSetVolume = env->GetStaticMethodID(clazz, "onSetVolumeFromNative", "(Ljava/lang/Object;I)Z");
    if (sTmallGenieJni.mOnSetVolume == nullptr) {
        OS_LOGE(TAG, "Failed to get onSetVolumeFromNative mothod");
        return JNI_FALSE;
    }
    sTmallGenieJni.mOnFeedVoiceEngine = env->GetStaticMethodID(clazz, "onFeedVoiceEngineFromNative", "(Ljava/lang/Object;[BI)V");
    if (sTmallGenieJni.mOnFeedVoiceEngine == nullptr) {
        OS_LOGE(TAG, "Failed to get onFeedVoiceEngineFromNative mothod");
        return JNI_FALSE;
    }
    sTmallGenieJni.mOnCommand = env->GetStaticMethodID(clazz, "onCommandFromNative", "(Ljava/lang/Object;IILjava/lang/String;)V");
    if (sTmallGenieJni.mOnCommand == nullptr) {
        OS_LOGE(TAG, "Failed to get onCommandFromNative mothod");
        return JNI_FALSE;
    }
    sTmallGenieJni.mOnStatus = env->GetStaticMethodID(clazz, "onStatusFromNative", "(Ljava/lang/Object;I)V");
    if (sTmallGenieJni.mOnStatus == nullptr) {
        OS_LOGE(TAG, "Failed to get onStatusFromNative mothod");
        return JNI_FALSE;
    }
    sTmallGenieJni.mOnAsrResult = env->GetStaticMethodID(clazz, "onAsrResultFromNative", "(Ljava/lang/Object;Ljava/lang/String;)V");
    if (sTmallGenieJni.mOnAsrResult == nullptr) {
        OS_LOGE(TAG, "Failed to get onAsrResultFromNative mothod");
        return JNI_FALSE;
    }
    sTmallGenieJni.mOnNluResult = env->GetStaticMethodID(clazz, "onNluResultFromNative", "(Ljava/lang/Object;Ljava/lang/String;)V");
    if (sTmallGenieJni.mOnNluResult == nullptr) {
        OS_LOGE(TAG, "Failed to get onNluResultFromNative mothod");
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
