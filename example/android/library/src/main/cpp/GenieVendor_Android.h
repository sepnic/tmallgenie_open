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

#ifndef __TMALLGENIE_VENDOR_H__
#define __TMALLGENIE_VENDOR_H__

#include <stdbool.h>

// jni callback
int  TmallGenie_onGetVolume();
bool TmallGenie_onSetVolume(int volume);

// vendor init
bool GnVendor_init(const char *wifiMac,
                   const char *bizType = nullptr, const char *bizGroup = nullptr,
                   const char *bizSecret = nullptr, const char *caCert = nullptr,
                   const char *uuid = nullptr, const char *accessToken = nullptr);
bool GnVendor_enableKeywordDetect();
void GnVendor_disableKeywordDetect();

// system & account info
const char *GnVendor_bizType();
const char *GnVendor_bizGroup();
const char *GnVendor_bizSecret();
const char *GnVendor_caCert();
const char *GnVendor_macAddr();
const char *GnVendor_uuid();       // if unauthorized, return null
const char *GnVendor_accessToken();// if unauthorized, return null

// audio pcm out
void *GnVendor_pcmOutOpen(int sampleRate, int channelCount, int bitsPerSample);
int GnVendor_pcmOutWrite(void *handle, void *buffer, unsigned int size); // return bytes written, <0 means fail
void GnVendor_pcmOutClose(void *handle);

// audio pcm in
void *GnVendor_pcmInOpen(int sampleRate, int channelCount, int bitsPerSample);
int GnVendor_pcmInRead(void *handle, void *buffer, unsigned int size); // return bytes written, <0 means fail
void GnVendor_pcmInClose(void *handle);

// audio system
bool GnVendor_setSpeakerVolume(int volume);
int GnVendor_getSpeakerVolume();
bool GnVendor_setSpeakerMuted(bool muted);
bool GnVendor_getSpeakerMuted();

#endif // __TMALLGENIE_VENDOR_H__