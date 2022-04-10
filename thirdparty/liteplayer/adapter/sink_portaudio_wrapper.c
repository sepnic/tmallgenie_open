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
#include <stdint.h>
#include <string.h>

#include "osal/os_time.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "portaudio.h"
#include "sink_portaudio_wrapper.h"

#define TAG "[liteplayer]portaudio"

struct portaudio_priv {
    PaStream *out_stream;
    int samplerate;
    int channels;
    int bits;
};

const char *portaudio_wrapper_name()
{
    return "portaudio";
}

sink_handle_t portaudio_wrapper_open(int samplerate, int channels, int bits, void *priv_data)
{
    OS_LOGD(TAG, "Opening portaudio: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);
    struct portaudio_priv *portaudio = OS_CALLOC(1, sizeof(struct portaudio_priv));
    if (portaudio == NULL)
        return NULL;
    if (Pa_Initialize() != paNoError) {
        OS_FREE(portaudio);
        return NULL;
    }

    portaudio->samplerate = samplerate;
    portaudio->channels = channels;
    portaudio->bits = bits;

    PaStreamParameters ouputParameters;
	ouputParameters.device = Pa_GetDefaultOutputDevice();
	ouputParameters.channelCount = channels;
    switch (bits) {
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
	if (Pa_OpenStream(&portaudio->out_stream, NULL, &ouputParameters,
            samplerate, 512, paFramesPerBufferUnspecified, NULL, NULL) != paNoError)
        goto __error_open;
	if (Pa_StartStream(portaudio->out_stream) != paNoError)
		goto __error_open;
    return portaudio;

__error_open:
    if (portaudio->out_stream != NULL)
        Pa_CloseStream(portaudio->out_stream);
    Pa_Terminate();
    OS_FREE(portaudio);
    return NULL;
}

int portaudio_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    struct portaudio_priv *portaudio = (struct portaudio_priv *)handle;
    unsigned long frames = size/(portaudio->channels*portaudio->bits/8);
    PaErrorCode ret = Pa_WriteStream(portaudio->out_stream, buffer, frames);
    if (ret == paNoError || ret == paOutputUnderflowed)
        return size;
    return -1;
}

void portaudio_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing portaudio");
    struct portaudio_priv *portaudio = (struct portaudio_priv *)handle;
    Pa_CloseStream(portaudio->out_stream);
    Pa_Terminate();
    OS_FREE(portaudio);
}
