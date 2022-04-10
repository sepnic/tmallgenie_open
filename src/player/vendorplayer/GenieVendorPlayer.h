// Copyright (c) 2021-2022 Qinglong<sysu.zqlong@gmail.com>
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

#ifndef __TMALLGENIE_VENDOR_PLAYER_ADAPTER_H__
#define __TMALLGENIE_VENDOR_PLAYER_ADAPTER_H__

#include "GenieVendorAdapter.h"
#include "player/GenieUtpManager.h"
#include "player/GeniePlayer.h"

#ifdef __cplusplus
extern "C" {
#endif

GnPlayer_Adapter_t *GnVendorPlayer_GetInstance(GnVendor_PcmOut_t *pcmOut);

#ifdef __cplusplus
}
#endif

#endif /* __TMALLGENIE_VENDOR_PLAYER_ADAPTER_H__ */
