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

#ifndef __TMALLGENIE_VENDOR_ADAPTER_H__
#define __TMALLGENIE_VENDOR_ADAPTER_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // system & account info
    const char *(*bizType)();
    const char *(*bizGroup)();
    const char *(*bizSecret)();
    const char *(*caCert)();
    const char *(*macAddr)();
    const char *(*uuid)();       // if unauthorized, return null
    const char *(*accessToken)();// if unauthorized, return null

    // audio pcm out
    void *(*pcmOutOpen)(int sampleRate, int channelCount, int bitsPerSample);
    int   (*pcmOutWrite)(void *handle, void *buffer, unsigned int size); // return bytes written, <0 means fail
    void  (*pcmOutClose)(void *handle);

    // audio pcm in
    void *(*pcmInOpen)(int sampleRate, int channelCount, int bitsPerSample);
    int   (*pcmInRead)(void *handle, void *buffer, unsigned int size); // return bytes written, <0 means fail
    void  (*pcmInClose)(void *handle);

    // audio system
    bool  (*setSpeakerVolume)(int volume);
    int   (*getSpeakerVolume)();
    bool  (*setSpeakerMuted)(bool muted);
    bool  (*getSpeakerMuted)();
} GnVendor_Wrapper_t;

#ifdef __cplusplus
}
#endif

#endif // __TMALLGENIE_VENDOR_ADAPTER_H__
