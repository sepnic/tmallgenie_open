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
#include "json/cJSON.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "GenieVendor_Android.h"

#define TAG "GenieVendorAndroid"

static char sGnAccountFile[256];
static bool sGnAccountAuthorized = false;
static char sGnAccountUuid[512];
static char sGnAccountAccessToken[512];
static char sGnWifiMac[32];
static int  sGnSpeakerVolume = 50;
static bool sGnSpeakerMuted  = false;

// vendor init
bool GnVendor_init(const char *userinfoFile, const char *wifiMac)
{
    // todo: get biz and account info from java layer, don't fix biz info in jni
    if (userinfoFile != nullptr) {
        snprintf(sGnAccountFile, sizeof(sGnAccountFile), "%s", userinfoFile);
        cJSON *rootJson = nullptr;
        FILE *file = nullptr;
        do {
            char buffer[1024] = {0};
            cJSON *uuidJson = nullptr, *accessTokenJson = nullptr;
            char *uuidStr = nullptr, *accessTokenStr = nullptr;
            file = fopen(userinfoFile, "rb");
            if (file == nullptr)
                break;
            if (fread(buffer, 1, sizeof(buffer), file) <= 0)
                break;
            if ((rootJson = cJSON_Parse(buffer)) == nullptr ||
                (uuidJson = cJSON_GetObjectItem(rootJson, "uuid")) == nullptr ||
                (accessTokenJson = cJSON_GetObjectItem(rootJson, "accessToken")) == nullptr)
                break;
            if ((uuidStr = cJSON_GetStringValue(uuidJson)) == nullptr ||
                (accessTokenStr = cJSON_GetStringValue(accessTokenJson)) == nullptr)
                break;
            snprintf(sGnAccountUuid, sizeof(sGnAccountUuid), "%s", uuidStr);
            snprintf(sGnAccountAccessToken, sizeof(sGnAccountAccessToken), "%s", accessTokenStr);
            sGnAccountAuthorized = true;
            OS_LOGI(TAG, "Successfully read account info from TmallGenieUserinfo.txt: uuid=%s, accessToken=%s", uuidStr, accessTokenStr);
        } while (false);
        if (rootJson != nullptr)
            cJSON_Delete(rootJson);
        if (file != nullptr)
            fclose(file);
    } else {
        snprintf(sGnAccountFile, sizeof(sGnAccountFile), "%s", "/storage/emulated/0/TmallGenieUserinfo.txt");
    }
    if (wifiMac != nullptr) {
        snprintf(sGnWifiMac, sizeof(sGnWifiMac), "%s", wifiMac);
        OS_LOGI(TAG, "Successfully read wifi mac address: mac=%s", wifiMac);
    } else {
        OS_LOGE(TAG, "Invalid wifi mac address, abort...");
        return false;
    }
    return true;
}

// system & account info
const char *GnVendor_bizType()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return nullptr;
}

const char *GnVendor_bizGroup()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return nullptr;
}

const char *GnVendor_bizSecret()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return nullptr;
}

const char *GnVendor_caCert()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return nullptr;
}

const char *GnVendor_macAddr()
{
    return sGnWifiMac;
}

const char *GnVendor_uuid()
{
    if (sGnAccountAuthorized)
        return sGnAccountUuid;
    else
        return nullptr;
}

const char *GnVendor_accessToken()
{
    if (sGnAccountAuthorized)
        return sGnAccountAccessToken;
    else
        return nullptr;
}

void GnVendor_updateAccount(const char *uuid, const char *accessToken)
{
    if (uuid == nullptr || accessToken == nullptr)
        return;
    char buffer[1024];
    cJSON *rootJson = cJSON_CreateObject();
    cJSON_AddItemToObject(rootJson, "uuid", cJSON_CreateString(uuid));
    cJSON_AddItemToObject(rootJson, "accessToken", cJSON_CreateString(accessToken));
    memset(buffer, 0x0, sizeof(buffer));
    cJSON_PrintPreallocated(rootJson, buffer, sizeof(buffer), false);
    FILE *file = fopen(sGnAccountFile, "wb+");
    if (file != nullptr) {
        fwrite(buffer, 1, strlen(buffer)+1, file);
        fclose(file);
    }
    cJSON_Delete(rootJson);

    if (strlen(uuid) < sizeof(sGnAccountUuid) &&
        strlen(accessToken) < sizeof(sGnAccountAccessToken)) {
        snprintf(sGnAccountUuid, sizeof(sGnAccountUuid), "%s", uuid);
        snprintf(sGnAccountAccessToken, sizeof(sGnAccountAccessToken), "%s", accessToken);
        sGnAccountAuthorized = true;
    }
}

bool GnVendor_setSpeakerVolume(int volume)
{
    // TODO: set stream volume
    sGnSpeakerVolume = volume;
    return true;
}

int GnVendor_getSpeakerVolume()
{
    return sGnSpeakerVolume;
}

bool GnVendor_setSpeakerMuted(bool muted)
{
    // TODO: set stream mute
    sGnSpeakerMuted = muted;
    return true;
}

bool GnVendor_getSpeakerMuted()
{
    return sGnSpeakerMuted;
}
