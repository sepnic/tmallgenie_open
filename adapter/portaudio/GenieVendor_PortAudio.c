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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "osal/os_time.h"
#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "json/cJSON.h"
#include "portaudio.h"
#include "GenieVoiceEngine_PortAudio.h"
#include "GenieVendor.h"
#if defined(__linux__) || defined(__ANDROID__)
#include <arpa/inet.h>
#include <netpacket/packet.h>
#elif defined(__APPLE__)
#include <net/if.h>
#include <net/if_dl.h>
#endif

#define TAG "GenieVendorPortAudio"

#define GENIE_ACCOUNT_FILE "GenieAccount.json"

static int  sGnSpeakerVolume = 50;
static bool sGnSpeakerMuted  = false;

static bool sGnVoiceEngineInited = false;

static bool sGnAccountAuthorized = false;
static char sGnAccountUuid[512];
static char sGnAccountAccessToken[512];

// vendor init
bool GnVendor_init()
{
    char buffer[1024] = {0};
    cJSON *rootJson = NULL, *uuidJson = NULL, *accessTokenJson = NULL;
    char *uuidStr = NULL, *accessTokenStr = NULL;
    FILE *file = fopen(GENIE_ACCOUNT_FILE, "rb");
    if (file == NULL)
        goto __done_account;
    if (fread(buffer, 1, sizeof(buffer), file) <= 0)
        goto __done_account;
    if ((rootJson = cJSON_Parse(buffer)) == NULL ||
        (uuidJson = cJSON_GetObjectItem(rootJson, "uuid")) == NULL ||
        (accessTokenJson = cJSON_GetObjectItem(rootJson, "accessToken")) == NULL)
        goto __done_account;
    if ((uuidStr = cJSON_GetStringValue(uuidJson)) == NULL ||
        (accessTokenStr = cJSON_GetStringValue(accessTokenJson)) == NULL)
        goto __done_account;
    snprintf(sGnAccountUuid, sizeof(sGnAccountUuid), "%s", uuidStr);
    snprintf(sGnAccountAccessToken, sizeof(sGnAccountAccessToken), "%s", accessTokenStr);
    sGnAccountAuthorized = true;

__done_account:
    if (rootJson != NULL)
        cJSON_Delete(rootJson);
    if (file != NULL)
        fclose(file);
    sGnVoiceEngineInited = GnVoiceEngine_init();
    if (!sGnVoiceEngineInited)
        OS_LOGE(TAG, "GnVoiceEngine_init failed");
    return true;
}

// system & account info
const char *GnVendor_bizType()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_bizGroup()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_bizSecret()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_caCert()
{
    // FIXME: apply for your device key from https://product.aligenie.com/
    return NULL;
}

const char *GnVendor_macAddr()
{
    static char mac[32] = {0};
    struct ifaddrs *if_addr = NULL, *if_addrs = NULL;
    if (getifaddrs(&if_addrs) == 0) {
        for (if_addr = if_addrs; if_addr != NULL; if_addr = if_addr->ifa_next) {
#if defined(__linux__) || defined(__ANDROID__)
            if (if_addr->ifa_addr != NULL && if_addr->ifa_addr->sa_family == AF_PACKET) {
                struct sockaddr_ll *sll = (struct sockaddr_ll *)(if_addr->ifa_addr);
                if (sll->sll_halen == 6) {
                    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                        sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2],
                        sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]);
                    if (strncmp(mac, "00:00:00:00:00:00", strlen(mac)) != 0) {
                        OS_LOGD(TAG, "ifa_name: %s, mac_addr: %s", if_addr->ifa_name, mac);
                        freeifaddrs(if_addrs);
                        return mac;
                    }
                }
            }
#elif defined(__APPLE__)
            if (if_addr->ifa_addr->sa_family == AF_LINK) {
                struct sockaddr_dl *sdl = (struct sockaddr_dl *)if_addr->ifa_addr;
                unsigned char *ptr = (unsigned char *)LLADDR(sdl);
                if (sdl->sdl_alen == 6) {
                    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                        *(ptr+0), *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
                    if (strncmp(mac, "00:00:00:00:00:00", strlen(mac)) != 0) {
                        OS_LOGD(TAG, "ifa_name: %s, mac_addr: %s", if_addr->ifa_name, mac);
                        freeifaddrs(if_addrs);
                        return mac;
                    }
                }
            }
#endif
        }
        freeifaddrs(if_addrs);
    }
    return NULL;
}

const char *GnVendor_uuid()
{
    if (sGnAccountAuthorized)
        return sGnAccountUuid;
    else
        return NULL;
}

const char *GnVendor_accessToken()
{
    if (sGnAccountAuthorized)
        return sGnAccountAccessToken;
    else
        return NULL;
}

void GnVendor_updateAccount(const char *uuid, const char *accessToken)
{
    char buffer[1024] = {0};
    cJSON *rootJson = cJSON_CreateObject();
    cJSON_AddItemToObject(rootJson, "uuid", cJSON_CreateString(uuid));
    cJSON_AddItemToObject(rootJson, "accessToken", cJSON_CreateString(accessToken));
    memset(buffer, 0x0, sizeof(buffer));
    cJSON_PrintPreallocated(rootJson, buffer, sizeof(buffer), false);
    FILE *file = fopen(GENIE_ACCOUNT_FILE, "wb+");
    if (file != NULL) {
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

// audio pcm out
typedef struct GeniePortAudio {
    PaStream *outStream;
    int sampleRate;
    int channelCount;
    int bitsPerSample;
} GeniePortAudio;

void *GnVendor_pcmOutOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGD(TAG, "Opening portaudio-out: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    GeniePortAudio *portaudio = OS_CALLOC(1, sizeof(GeniePortAudio));
    if (portaudio == NULL)
        return NULL;
    if (Pa_Initialize() != paNoError) {
        OS_FREE(portaudio);
        return NULL;
    }

    portaudio->sampleRate = sampleRate;
    portaudio->channelCount = channelCount;
    portaudio->bitsPerSample = bitsPerSample;

    PaStreamParameters ouputParameters;
	ouputParameters.device = Pa_GetDefaultOutputDevice();
	ouputParameters.channelCount = channelCount;
    switch (bitsPerSample) {
    case 32:
        ouputParameters.sampleFormat = paInt32;
        break;
    case 24:
        ouputParameters.sampleFormat = paInt24;
        break;
    case 16:
    default:
        ouputParameters.sampleFormat = paInt16;
        break;
    }
	ouputParameters.suggestedLatency =
            Pa_GetDeviceInfo(ouputParameters.device)->defaultLowOutputLatency;
	ouputParameters.hostApiSpecificStreamInfo = NULL;
	if (Pa_OpenStream(&portaudio->outStream, NULL, &ouputParameters,
            sampleRate, 512, paFramesPerBufferUnspecified, NULL, NULL) != paNoError)
        goto __error_open;
	if (Pa_StartStream(portaudio->outStream) != paNoError)
		goto __error_open;
    return portaudio;

__error_open:
    if (portaudio->outStream != NULL)
        Pa_CloseStream(portaudio->outStream);
    Pa_Terminate();
    OS_FREE(portaudio);
    return NULL;
}

int GnVendor_pcmOutWrite(void *handle, void *buffer, unsigned int size)
{
    GeniePortAudio *portaudio = (GeniePortAudio *)handle;
    unsigned long frames = size/(portaudio->channelCount*portaudio->bitsPerSample/8);
    PaErrorCode ret = Pa_WriteStream(portaudio->outStream, buffer, frames);
    if (ret == paNoError || ret == paOutputUnderflowed)
        return size;
    return -1;
}

void GnVendor_pcmOutClose(void *handle)
{
    OS_LOGD(TAG, "closing portaudio-out");
    GeniePortAudio *portaudio = (GeniePortAudio *)handle;
    Pa_CloseStream(portaudio->outStream);
    Pa_Terminate();
    OS_FREE(portaudio);
}

// audio pcm in
void *GnVendor_pcmInOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGD(TAG, "Opening portaudio-in: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    if (sGnVoiceEngineInited &&
        GnVoiceEngine_recorderStart(sampleRate, channelCount, bitsPerSample))
        return (void *)0xffff;
    return NULL;
}

int GnVendor_pcmInRead(void *handle, void *buffer, unsigned int size)
{
    return GnVoiceEngine_recorderRead(buffer, size);
}

void GnVendor_pcmInClose(void *handle)
{
    OS_LOGD(TAG, "closing portaudio-in");
    GnVoiceEngine_recorderStop();
}

// audio system
bool GnVendor_setSpeakerVolume(int volume)
{
    sGnSpeakerVolume = volume;
    return true;
}

int GnVendor_getSpeakerVolume()
{
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
