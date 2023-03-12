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
#include <string>
#include "json/cJSON.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "GenieVendor_Android.h"
#include "GenieSdk.h"

#define TAG "TmallGenieJNI"
#define JAVA_CLASS_NAME "com/sepnic/tmallgenie/TmallGenie"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

class TmallGenieJni {
public:
    GenieSdk_Callback_t *mSdkCallback;
    jboolean    mSdkInited;
    jboolean    mSdkStarted;
    jmethodID   mOnCommand;
    jmethodID   mOnStatus;
    jmethodID   mOnAsrResult;
    jmethodID   mOnNluResult;
    jmethodID   mOnMemberQrCode;
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

static void TmallGenie_onCommand(Genie_Domain_t domain, Genie_Command_t command, const char *payload)
{
    OS_LOGD(TAG, "TmallGenie_onCommand: domain:%d, command:%d, payload:%s", domain, command, payload);
    JNIEnv *env;
    jint res = sJavaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    jboolean attached = JNI_FALSE;
    if (res != JNI_OK) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "TmallGenieCommandListener", nullptr };
        res = sJavaVM->AttachCurrentThread(&env, &args);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return;
        }
        attached = JNI_TRUE;
    }

    switch (command) {
        case GENIE_COMMAND_GuestDeviceActivateResp:
        case GENIE_COMMAND_MemberDeviceActivateResp: {
            cJSON *payloadJson = cJSON_Parse(payload);
            if (payloadJson == nullptr) return;
            cJSON *uuidJson = cJSON_GetObjectItem(payloadJson, "uuid");
            cJSON *accessTokenJson = cJSON_GetObjectItem(payloadJson, "accessToken");
            char *uuid = nullptr, *accessToken = nullptr;
            if (uuidJson != nullptr)
                uuid = cJSON_GetStringValue(uuidJson);
            if (accessTokenJson != nullptr)
                accessToken = cJSON_GetStringValue(accessTokenJson);
            if (uuid != nullptr && accessToken != nullptr) {
                OS_LOGI(TAG, "Account already authorized: uuid=%s, accessToken=%s", uuid, accessToken);
                GnVendor_updateAccount(uuid, accessToken);
            }
            cJSON_Delete(payloadJson);
        }
            break;
        case GENIE_COMMAND_UserInfoResp: {
            cJSON *payloadJson = cJSON_Parse(payload);
            if (payloadJson == nullptr) return;
            OS_LOGI(TAG, "UserInfoResp: payload=%s", payload);
            cJSON *userTypeJson = cJSON_GetObjectItem(payloadJson, "userType");
            char *userType = nullptr;
            if (userTypeJson != nullptr)
                userType = cJSON_GetStringValue(userTypeJson);
            if (userType != nullptr && strcmp(userType, "guest") == 0) {
                cJSON *qrCodeJson = cJSON_GetObjectItem(payloadJson, "qrCode");
                char *qrCode = nullptr;
                if (qrCodeJson != nullptr)
                    qrCode = cJSON_GetStringValue(qrCodeJson);
                if (qrCode != nullptr) {
                    OS_LOGW(TAG, "User type is guest, please scan the qrCode with tmallgenie app to"
                                 " bind the device as a member: qrCode=%s", qrCode);
                    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnMemberQrCode, sTmallGenieJni.mObject,
                                              env->NewStringUTF(qrCode));
                }
            }
            cJSON_Delete(payloadJson);
        }
            break;
        default:
            break;
    }

    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnCommand, sTmallGenieJni.mObject,
                              (jint)domain, (jint)command, env->NewStringUTF(payload));
    if (attached)
        sJavaVM->DetachCurrentThread();
}

void TmallGenie_onStatus(Genie_Status_t status)
{
    JNIEnv *env;
    jint res = sJavaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    jboolean attached = JNI_FALSE;
    if (res != JNI_OK) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "TmallGenieStatusListener", nullptr };
        res = sJavaVM->AttachCurrentThread(&env, &args);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return;
        }
        attached = JNI_TRUE;
    }
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnStatus, sTmallGenieJni.mObject,
                              (jint)status);
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static void TmallGenie_onAsrResult(const char *result)
{
    OS_LOGD(TAG, "TmallGenie_onAsrResult: %s", result);
    JNIEnv *env;
    jint res = sJavaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    jboolean attached = JNI_FALSE;
    if (res != JNI_OK) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "TmallGenieAsrResultListener", nullptr };
        res = sJavaVM->AttachCurrentThread(&env, &args);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return;
        }
        attached = JNI_TRUE;
    }
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnAsrResult, sTmallGenieJni.mObject,
                              env->NewStringUTF(result));
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static void TmallGenie_onNluResult(const char *result)
{
    OS_LOGD(TAG, "TmallGenie_onNluResult: %s", result);
    JNIEnv *env;
    jint res = sJavaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    jboolean attached = JNI_FALSE;
    if (res != JNI_OK) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "TmallGenieNluResultListener", nullptr };
        res = sJavaVM->AttachCurrentThread(&env, &args);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return;
        }
        attached = JNI_TRUE;
    }
    env->CallStaticVoidMethod(sTmallGenieJni.mClass, sTmallGenieJni.mOnNluResult, sTmallGenieJni.mObject,
                              env->NewStringUTF(result));
    if (attached)
        sJavaVM->DetachCurrentThread();
}

static jboolean TmallGenie_nativeCreate(JNIEnv* env, jobject thiz, jobject weak_this, jstring userinfoFile, jstring wifiMac)
{
    OS_LOGD(TAG, "TmallGenie_nativeCreate");

    if (userinfoFile == nullptr || wifiMac == nullptr)
        jniThrowException(env, "java/lang/IllegalArgumentException", "Received NULL jstring");

    if (sTmallGenieJni.mSdkInited) {
        OS_LOGE(TAG, "TmallGenie already inited");
        return JNI_FALSE;
    }

    jclass clazz;
    clazz = env->FindClass(JAVA_CLASS_NAME);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
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
    sTmallGenieJni.mOnMemberQrCode = env->GetStaticMethodID(clazz, "onMemberQrCodeFromNative", "(Ljava/lang/Object;Ljava/lang/String;)V");
    if (sTmallGenieJni.mOnMemberQrCode == nullptr) {
        OS_LOGE(TAG, "Failed to get onMemberQrCodeFromNative mothod");
        return JNI_FALSE;
    }

    env->DeleteLocalRef(clazz);

    clazz = env->GetObjectClass(thiz);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        jniThrowException(env, "java/lang/Exception", nullptr);
        return JNI_FALSE;
    }
    sTmallGenieJni.mClass = (jclass)env->NewGlobalRef(clazz);
    sTmallGenieJni.mObject  = env->NewGlobalRef(weak_this);

    const char *userinfoChar = env->GetStringUTFChars(userinfoFile, nullptr);
    const char *wifiMacChar = env->GetStringUTFChars(wifiMac, nullptr);
    if (!GnVendor_init(userinfoChar, wifiMacChar)) {
        OS_LOGE(TAG, "Failed to GnVendor_init");
        env->ReleaseStringUTFChars(userinfoFile,userinfoChar);
        env->ReleaseStringUTFChars(wifiMac,wifiMacChar);
        return JNI_FALSE;
    }
    env->ReleaseStringUTFChars(userinfoFile,userinfoChar);
    env->ReleaseStringUTFChars(wifiMac,wifiMacChar);

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
    OS_LOGD(TAG, "TmallGenie_nativeStart");
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
    OS_LOGD(TAG, "TmallGenie_nativeStop");
    if (sTmallGenieJni.mSdkStarted) {
        GenieSdk_Stop();
        sTmallGenieJni.mSdkStarted = JNI_FALSE;
    }
}

static void TmallGenie_nativeDestroy(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "TmallGenie_nativeDestroy");
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
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onNetworkConnected();
}

static void TmallGenie_onNetworkDisconnected(JNIEnv *env, jobject thiz)
{
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onNetworkDisconnected();
}

static void TmallGenie_onMicphoneWakeup(JNIEnv *env, jobject thiz, jstring wakeupWord, jint doa, jdouble confidence)
{
    if (wakeupWord == nullptr)
        jniThrowException(env, "java/lang/IllegalArgumentException", "Received NULL jstring");
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onMicphoneWakeup("tianmaojingling", 0, 0.618);
}

static void TmallGenie_onMicphoneSilence(JNIEnv *env, jobject thiz)
{
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onMicphoneSilence();
}

static void TmallGenie_onSpeakerVolumeChanged(JNIEnv *env, jobject thiz, jint volume)
{
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onSpeakerVolumeChanged(volume);
}

static void TmallGenie_onSpeakerMutedChanged(JNIEnv *env, jobject thiz, jboolean muted)
{
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onSpeakerMutedChanged(muted);
}
static void TmallGenie_onQueryUserInfo(JNIEnv *env, jobject thiz)
{
    if (sTmallGenieJni.mSdkInited)
        sTmallGenieJni.mSdkCallback->onQueryUserInfo();
}

static void TmallGenie_onTextRecognize(JNIEnv *env, jobject thiz, jstring inputText)
{
    if (inputText == nullptr)
        jniThrowException(env, "java/lang/IllegalArgumentException", "Received NULL jstring");
    if (sTmallGenieJni.mSdkInited) {
        const char *text = env->GetStringUTFChars(inputText, nullptr);
        sTmallGenieJni.mSdkCallback->onTextRecognize(text);
        env->ReleaseStringUTFChars(inputText,text);
    }
}

static JNINativeMethod gMethods[] = {
        {"native_create", "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)Z", (void *)TmallGenie_nativeCreate},
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
