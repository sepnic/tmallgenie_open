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
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "json/cJSON.h"
#include "GenieVoiceEngine_Alsa.h"
#include "GenieVendor.h"

#define TAG "GenieVendorAlsa"

#if 0
#define GENIE_LINUX_ALSA_DEVICE     "default"
#else
#define GENIE_LINUX_ALSA_DEVICE     "sysdefault"
#endif
#define GENIE_LINUX_ACCOUNT_FILE    "GenieAccount.json"

static int  sGnSpeakerVolume = -1;
static bool sGnSpeakerMuted = false;

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
    FILE *file = fopen(GENIE_LINUX_ACCOUNT_FILE, "rb");
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
    static char mac[INET6_ADDRSTRLEN] = {0};
    struct ifaddrs *if_addr = NULL, *if_addrs = NULL;
    if (getifaddrs(&if_addrs) == 0) {
        for (if_addr = if_addrs; if_addr != NULL; if_addr = if_addr->ifa_next) {
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
    FILE *file = fopen(GENIE_LINUX_ACCOUNT_FILE, "wb+");
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
void *GnVendor_pcmOutOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGD(TAG, "Opening alsa-out: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
            sampleRate, channelCount, bitsPerSample);
    return GnLinux_alsaOpen(SND_PCM_STREAM_PLAYBACK, sampleRate, channelCount, bitsPerSample);
}

int GnVendor_pcmOutWrite(void *handle, void *buffer, unsigned int size)
{
    GnLinux_Alsa_t *alsa = (GnLinux_Alsa_t *)handle;
    size_t frame_count = (size_t)(size * 8 / alsa->bits_per_frame);
    unsigned char *data = (unsigned char *)buffer;
    while (frame_count > 0) {
        ssize_t ret = snd_pcm_writei(alsa->pcm, data, frame_count);
        if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < frame_count)) {
            OS_LOGW(TAG, "snd_pcm_wait");
            snd_pcm_wait(alsa->pcm, 1000);
        } else if (ret == -EPIPE) {
            OS_LOGW(TAG, "Underrun");
            snd_pcm_prepare(alsa->pcm);
        } else if (ret == -ESTRPIPE) {
            OS_LOGW(TAG, "Need suspend");
        } else if (ret < 0) {
            OS_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(ret));
            return -1;
        }
        if (ret > 0) {
            frame_count -= ret;
            data += ret * alsa->bits_per_frame / 8;
        }
    }
    return size;
}

void GnVendor_pcmOutClose(void *handle)
{
    OS_LOGD(TAG, "closing alsa-out");
    GnLinux_Alsa_t *alsa = (GnLinux_Alsa_t *)handle;
    snd_pcm_drain(alsa->pcm);
    snd_pcm_close(alsa->pcm);
    OS_FREE(alsa);
}

// audio pcm in
void *GnVendor_pcmInOpen(int sampleRate, int channelCount, int bitsPerSample)
{
    OS_LOGD(TAG, "Opening alsa-in: sampleRate=%d, channelCount=%d, bitsPerSample=%d",
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
    OS_LOGD(TAG, "closing alsa-in");
    GnVoiceEngine_recorderStop();
}

// audio system
bool GnVendor_setSpeakerVolume(int volume)
{
    if (volume < 0 || volume > 100)
        return false;

    if (volume == sGnSpeakerVolume)
        return true;

    long min = 0, max = 0, vol = 0;
    snd_mixer_t *handle = NULL;
    snd_mixer_selem_id_t *sid = NULL;
    snd_mixer_open(&handle, 0);
    if (handle == NULL) {
        OS_LOGE(TAG, "Failed to snd_mixer_open");
        goto __error_setvolume;
    }
    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_alloca(&sid);
    if (sid == NULL) {
        OS_LOGE(TAG, "Failed to snd_mixer_selem_id_alloca");
        goto __error_setvolume;
    }
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Master");
    snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);
    if (elem == NULL) {
        OS_LOGE(TAG, "Failed to snd_mixer_find_selem");
        goto __error_setvolume;
    }
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    vol = (volume * (max-min)/(100-1)) + min;
    snd_mixer_selem_set_playback_volume_all(elem, vol);
    snd_mixer_close(handle);

    sGnSpeakerVolume = volume;
    return true;

__error_setvolume:
    if (handle != NULL)
        snd_mixer_close(handle);
    return false;
}

int GnVendor_getSpeakerVolume()
{
    if (sGnSpeakerVolume < 0) {
        long min = 0, max = 0, vol = 0;
        snd_mixer_t *handle = NULL;
        snd_mixer_selem_id_t *sid = NULL;
        snd_mixer_open(&handle, 0);
        if (handle == NULL) {
            OS_LOGE(TAG, "Failed to snd_mixer_open");
            goto __error_getvolume;
        }
        snd_mixer_attach(handle, "default");
        snd_mixer_selem_register(handle, NULL, NULL);
        snd_mixer_load(handle);
        snd_mixer_selem_id_alloca(&sid);
        if (sid == NULL) {
            OS_LOGE(TAG, "Failed to snd_mixer_selem_id_alloca");
            goto __error_getvolume;
        }
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, "Master");
        snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);
        if (elem == NULL) {
            OS_LOGE(TAG, "Failed to snd_mixer_find_selem");
            goto __error_getvolume;
        }
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_get_playback_volume(elem, 0, &vol);
        snd_mixer_close(handle);
        sGnSpeakerVolume = 100 * (vol-min) / (max-min);
        return sGnSpeakerVolume;
    __error_getvolume:
        if (handle != NULL)
            snd_mixer_close(handle);
        sGnSpeakerVolume = 50;
        return sGnSpeakerVolume;
    }
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
