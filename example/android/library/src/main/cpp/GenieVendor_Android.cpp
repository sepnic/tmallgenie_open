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
#include <string>
#include "cutils/log_helper.h"
#include "GenieVendor_Android.h"

#define TAG "GenieVendorAndroid"

static std::string sGnAccountUuid;
static std::string sGnAccountAccessToken;

static std::string sGnDeviceWifiMac;
static std::string sGnDeviceBizType;
static std::string sGnDeviceBizGroup;
static std::string sGnDeviceBizSecret;
static std::string sGnDeviceCaCert;

static int  sGnSpeakerVolume = 50;
static bool sGnSpeakerMuted  = false;

// vendor init
bool GnVendor_init(const char *wifiMac,
                   const char *bizType, const char *bizGroup, const char *bizSecret, const char *caCert,
                   const char *uuid, const char *accessToken)
{
    if (wifiMac != nullptr) {
        sGnDeviceWifiMac = wifiMac;
        OS_LOGI(TAG, "WifiMac: %s", sGnDeviceWifiMac.c_str());
    } else {
        OS_LOGE(TAG, "Invalid WifiMac address, abort...");
        return false;
    }

    if (bizType != nullptr && bizGroup != nullptr && bizSecret != nullptr && caCert != nullptr) {
        // IMPORTANT: DO NOT EXPOSE THESE CONFIDENTIAL DATA
        sGnDeviceBizType = bizType;
        sGnDeviceBizGroup = bizGroup;
        sGnDeviceBizSecret = bizSecret;
        sGnDeviceCaCert = caCert;
    } else {
        OS_LOGE(TAG, "Invalid biz information, but will go ahead with default biz");
    }

    if (uuid != nullptr && accessToken != nullptr) {
        sGnAccountUuid = uuid;
        sGnAccountAccessToken = accessToken;
        // uuid and accessToken are also confidential, but they have been encrypted in GenieInteractionService
        OS_LOGI(TAG, "ExistedAccount: uuid=%s, accessToken=%s", sGnAccountUuid.c_str(), sGnAccountAccessToken.c_str());
    }

    sGnSpeakerMuted = false;
    return true;
}

// system & account info
const char *GnVendor_bizType()
{
    if (sGnDeviceBizType.empty())
        return nullptr;
    else
        return sGnDeviceBizType.c_str();
}

const char *GnVendor_bizGroup()
{
    if (sGnDeviceBizGroup.empty())
        return nullptr;
    else
        return sGnDeviceBizGroup.c_str();
}

const char *GnVendor_bizSecret()
{
    if (sGnDeviceBizSecret.empty())
        return nullptr;
    else
        return sGnDeviceBizSecret.c_str();
}

const char *GnVendor_caCert()
{
    if (sGnDeviceCaCert.empty())
        return nullptr;
    else
        return sGnDeviceCaCert.c_str();
}

const char *GnVendor_macAddr()
{
    return sGnDeviceWifiMac.c_str();
}

const char *GnVendor_uuid()
{
    if (sGnAccountUuid.empty())
        return nullptr;
    else
        return sGnAccountUuid.c_str();
}

const char *GnVendor_accessToken()
{
    if (sGnAccountAccessToken.empty())
        return nullptr;
    else
        return sGnAccountAccessToken.c_str();
}

bool GnVendor_setSpeakerVolume(int volume)
{
    if (TmallGenie_onSetVolume(volume)) {
        sGnSpeakerVolume = volume;
        return true;
    }
    return  false;
}

int GnVendor_getSpeakerVolume()
{
    int volume = TmallGenie_onGetVolume();
    if (volume >= 0)
        sGnSpeakerVolume = volume;
    return sGnSpeakerVolume;
}

bool GnVendor_setSpeakerMuted(bool muted)
{
    sGnSpeakerMuted = muted;
    return true;
}

bool GnVendor_getSpeakerMuted()
{
    return sGnSpeakerMuted;
}
