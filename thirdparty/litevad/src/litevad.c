/*
 * Copyright (C) 2018-2023 Qinglong<sysu.zqlong@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "vad/webrtc_vad.h"
#include "litevad.h"

#if defined(ANDROID)
#include <android/log.h>
#define TAG "litevad"
#define pr_dbg(fmt, ...) //__android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, fmt, ##__VA_ARGS__)

#elif defined(LITEVAD_HAVE_SYSUTILS_ENABLED)
#include "cutils/log_helper.h"
#define TAG "litevad"
#define pr_dbg(fmt, ...) //OS_LOGD(TAG, fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) OS_LOGE(TAG, fmt, ##__VA_ARGS__)

#else
#define pr_dbg(fmt, ...) //fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#define pr_err(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#endif

// BOS: begin of speech
// EOS:   end of speech

// 持续 300ms 检测到语音，认为出现有效语音数据，出现该条件之前，不会进行 VAD 判停
// 防止用户点击录音后，几秒内没说话，就直接判停了
#define DEFAULT_BOS_ACTIVE_TIME  300

// 持续 700ms 检测到静音，将触发 VAD 判停
#define DEFAULT_EOS_SILENCE_TIME 700

// 语音权重阈值（最大值 100，最小值 0）：
// 语音权重的引入主要为了解决“片段时间内，由于语音停顿，一直达不到‘持续 300 ms
// 检测到语音’的条件”。片段时间内，语音活动间中出现停顿，我们认为这些还是属于连续
// 的语音活动
#define DEFAULT_BOS_ACTIVE_WEIGHT 30

// 静音权重阈值（最大值 100，最小值 0）：
//  1. 持续 400ms 检测到语音，认为出现有效语音数据，此时权重为 100
//  2. 检测到 1 帧数据有语音，权重 +1；检测到 1 帧数据静音，权重 -1；值范围：0-100
//  3. 当权重值低于 30，将触发 VAD 判停
// 静音权重的引入主要为了解决“片段时间内，误检测为语音而无法判停”的问题。比如说：
// 片段时间内，出现了大量的静音数据，但又会检测到零碎的语音，我们认为这些零碎的语
// 音是误检测的
#define DEFAULT_EOS_SILENCE_WEIGHT 30

// VAD 模式（合法值：0/1/2/3），值越大对语音的判断越严格（越准确？）
#define DEFAULT_VAD_MODE           3

// 每帧的长度（单位 ms，合法值：10ms/20ms/30ms），建议设置为 10ms
#define DEFAULT_SPEECH_FRAME_TIME  10

struct litevad_priv {
    VadInst *vad_inst;
    int      vad_mode;
    int      rate_idx;
    int      sample_rate;
    int      channel_count;
    int      active_time;
    int      silence_time;
    int      speech_weight;
    bool     speech_detected;
};

// valid vad operating mode, A more aggressive (higher mode) VAD is more
// restrictive in reporting speech
static const int valid_vad_modes[] = { 0, 1, 2, 3 };
// valid sample rates in kHz
static const int valid_sample_rates[] = { 8, 16, 32, 48 };
// valid frame lengths in ms
static const int valid_frame_times[] = { 10, 20, 30 };

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static bool valid_vad_mode(int vad_mode)
{
    for (int i = 0; i < ARRAY_SIZE(valid_vad_modes); i++) {
        if (vad_mode == valid_vad_modes[i])
            return true;
    }
    return false;
}

static bool valid_sample_rate(int sample_rate, int *rate_idx)
{
    for (int i = 0; i < ARRAY_SIZE(valid_sample_rates); i++) {
        if (sample_rate == (valid_sample_rates[i]*1000)) {
            *rate_idx = i;
            return true;
        }
    }
    return false;
}

static bool valid_frame_size(int rate_idx, int frame_size)
{
    int samples_per_ms = valid_sample_rates[rate_idx];
    for (int i = 0; i < ARRAY_SIZE(valid_frame_times); i++) {
        if (frame_size == (valid_frame_times[i]*samples_per_ms))
            return true;
    }
    return false;
}

litevad_handle_t litevad_create(int sample_rate, int channel_count, int sample_bits)
{
    if (!valid_vad_mode(DEFAULT_VAD_MODE)) {
        pr_err("Invalid vad mode, valid value: 0/1/2/3");
        return NULL;
    }

    int rate_idx = 0;
    if (!valid_sample_rate(sample_rate, &rate_idx)) {
        pr_err("Invalid sampling frequency, valid value: 8000/16000/32000/48000");
        return NULL;
    }

    if (channel_count != 1) {
        // todo: support stereo2mono
        pr_err("Invalid channel count, valid value: 1");
        return NULL;
    }

    if (sample_bits != 16) {
        pr_err("Invalid sample bits, valid value: 16");
        return NULL;
    }

    int ret = 0;
    struct litevad_priv *priv =
            (struct litevad_priv *)calloc(1, sizeof(struct litevad_priv));
    if (priv == NULL)
        return NULL;

    priv->vad_inst = WebRtcVad_Create();
    if (priv->vad_inst == NULL) {
        pr_err("Failed to create vad instance");
        goto bail;
    }

    ret = WebRtcVad_Init(priv->vad_inst);
    if (ret != 0) {
        pr_err("Failed to init vad instance");
        goto bail;
    }

    ret = WebRtcVad_set_mode(priv->vad_inst, DEFAULT_VAD_MODE);
    if (ret != 0) {
        pr_err("Failed to set vad mode");
        goto bail;
    }

    priv->vad_mode         = DEFAULT_VAD_MODE;
    priv->rate_idx         = rate_idx;
    priv->sample_rate      = sample_rate;
    priv->channel_count    = channel_count;
    return (litevad_handle_t)priv;

bail:
    if (priv->vad_inst != NULL)
        WebRtcVad_Free(priv->vad_inst);
    free(priv);
    return NULL;
}

static int litevad_process_frame(litevad_handle_t handle, const short *frame_buff, int frame_size)
{
    struct litevad_priv *priv = (struct litevad_priv *)handle;
    if (!valid_frame_size(priv->rate_idx, frame_size)) {
        pr_err("Invalid frame size, valid frame time: 10ms/20ms/30ms");
        return LITEVAD_RESULT_ERROR;
    }

    int frame_time = frame_size / valid_sample_rates[priv->rate_idx];
    int ret = WebRtcVad_Process(priv->vad_inst, priv->sample_rate, frame_buff, frame_size);
    if (ret == 1) {
        priv->silence_time = 0;
        priv->active_time += frame_time;
        if (priv->speech_weight < 100)
            priv->speech_weight++;
        ret = LITEVAD_RESULT_FRAME_ACTIVE;
    } else if (ret == 0) {
        priv->silence_time += frame_time;
        priv->active_time = 0;
        if (priv->speech_weight > 0)
           priv->speech_weight--;
        ret = LITEVAD_RESULT_FRAME_SILENCE;
    } else {
        pr_err("Failed to process vad instance");
        ret = LITEVAD_RESULT_ERROR;
    }

    pr_dbg("Process: ret=%d, active_time=%d(ms), silence_time=%d(ms), speech_weight=%d",
            ret, priv->active_time, priv->silence_time, priv->speech_weight);
    return ret;
}

litevad_result_t litevad_process(litevad_handle_t handle, const void *buff, int size)
{
    struct litevad_priv *priv = (struct litevad_priv *)handle;
    litevad_result_t result = LITEVAD_RESULT_ERROR;
    short *frame_buff = (short *)buff;
    int nsamples = size / sizeof(short);
    int frame_size = DEFAULT_SPEECH_FRAME_TIME * valid_sample_rates[priv->rate_idx];
    int i = 0, ret = 0;

    if ((nsamples % frame_size) != 0) {
        pr_err("Invalid frame length");
        return LITEVAD_RESULT_ERROR;
    }

    while (i < nsamples) {
        ret = litevad_process_frame(priv, &frame_buff[i], frame_size);
        if (ret == LITEVAD_RESULT_ERROR) {
            result = LITEVAD_RESULT_ERROR;
            break;
        }

        if (!priv->speech_detected &&
            priv->active_time < DEFAULT_BOS_ACTIVE_TIME &&
            priv->speech_weight < DEFAULT_BOS_ACTIVE_WEIGHT) {
            result = LITEVAD_RESULT_FRAME_SILENCE;
        } else {
            if (!priv->speech_detected) {
                pr_dbg("speech begin");
                priv->speech_detected = true;
                priv->speech_weight = 100;
                priv->silence_time = 0;
                result = LITEVAD_RESULT_SPEECH_BEGIN;
            }

            if (priv->silence_time < DEFAULT_EOS_SILENCE_TIME &&
                priv->speech_weight > DEFAULT_EOS_SILENCE_WEIGHT) {
                if (result != LITEVAD_RESULT_SPEECH_BEGIN)
                    result = LITEVAD_RESULT_FRAME_ACTIVE;
            } else if (ret == LITEVAD_RESULT_FRAME_SILENCE) {
                pr_dbg("speech end");
                priv->active_time = 0;
                priv->silence_time = 0;
                priv->speech_weight = 0;
                priv->speech_detected = false;
                if (result != LITEVAD_RESULT_SPEECH_BEGIN)
                    result = LITEVAD_RESULT_SPEECH_END;
                else
                    result = LITEVAD_RESULT_SPEECH_BEGIN_AND_END;
                break;
            } else {
                if (result != LITEVAD_RESULT_SPEECH_BEGIN)
                    result = ret;
            }
        }
        i += frame_size;
    }

    return result;
}

void litevad_reset(litevad_handle_t handle)
{
    struct litevad_priv *priv = (struct litevad_priv *)handle;
    priv->active_time = 0;
    priv->silence_time = 0;
    priv->speech_weight = 0;
    priv->speech_detected = false;
    WebRtcVad_Init(priv->vad_inst);
    WebRtcVad_set_mode(priv->vad_inst, priv->vad_mode);
}

void litevad_destroy(litevad_handle_t handle)
{
    struct litevad_priv *priv = (struct litevad_priv *)handle;
    WebRtcVad_Free(priv->vad_inst);
    free(priv);
}
