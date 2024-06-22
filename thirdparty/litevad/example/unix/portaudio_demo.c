// Copyright (c) 2019-2023 Qinglong<sysu.zqlong@gmail.com>
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
#include <stdlib.h>

#include "portaudio.h"
#include "litevad.h"

#define RECORD_SAMPLE_RATE      16000
#define RECORD_SAMPLE_BIT       16
#define RECORD_CHANNEL_COUNT    1
#define RECORD_BUFFER_TIME      30 // in ms

static int PortAudio_InStreamCallback(const void *input, void *output,
    unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags, void *userData)
{
    litevad_handle_t vadHandle = (litevad_handle_t)userData;
    unsigned long nbytes = frameCount*RECORD_CHANNEL_COUNT*RECORD_SAMPLE_BIT/8;
    litevad_result_t vad_state = litevad_process(vadHandle, input, nbytes);
    switch (vad_state) {
    case LITEVAD_RESULT_SPEECH_BEGIN:
        fprintf(stdout, "speech begin\n");
        break;
    case LITEVAD_RESULT_SPEECH_END:
        fprintf(stdout, "speech end\n");
        break;
    case LITEVAD_RESULT_SPEECH_BEGIN_AND_END:
        fprintf(stdout, "speech begin and end\n");
        break;
    case LITEVAD_RESULT_FRAME_SILENCE:
        //fprintf(stdout, "frame silence\n");
        break;
    case LITEVAD_RESULT_FRAME_ACTIVE:
        //fprintf(stdout, "frame active\n");
        break;
    case LITEVAD_RESULT_ERROR:
        fprintf(stdout, "vad error\n");
        break;
    default:
        break;
    }
    return paContinue;
}

int main(int argc, char *argv[])
{
    // litevad init
    litevad_handle_t vadHandle =
        litevad_create(RECORD_SAMPLE_RATE, RECORD_CHANNEL_COUNT, RECORD_SAMPLE_BIT);
    if (vadHandle == NULL) {
        fprintf(stderr, "Failed to litevad_create\n");
        return 0;
    }

    // PortAudio init
    if (Pa_Initialize() != paNoError) {
        fprintf(stderr, "Failed to Pa_Initialize\n");
        goto __out;
    }
    unsigned int bytesPerMs = (RECORD_SAMPLE_RATE/1000)*(RECORD_SAMPLE_BIT/8)*RECORD_CHANNEL_COUNT;
    unsigned int readSize = bytesPerMs * RECORD_BUFFER_TIME;
    unsigned long framesNeed = readSize/(RECORD_CHANNEL_COUNT*RECORD_SAMPLE_BIT/8);
    PaStream *inStream = NULL;
    PaStreamParameters inParameters;
    inParameters.device = Pa_GetDefaultInputDevice();
    if (inParameters.device == paNoDevice) {
        fprintf(stderr, "Failed to Pa_GetDefaultInputDevice\n");
        goto __out;
    }
    inParameters.channelCount = RECORD_CHANNEL_COUNT;
    inParameters.sampleFormat = paInt16;
    inParameters.suggestedLatency = Pa_GetDeviceInfo(inParameters.device)->defaultLowInputLatency;
    inParameters.hostApiSpecificStreamInfo = NULL;
    if (Pa_OpenStream(&inStream, &inParameters, NULL,
            RECORD_SAMPLE_RATE, framesNeed, paFramesPerBufferUnspecified,
            PortAudio_InStreamCallback, vadHandle) != paNoError) {
        fprintf(stderr, "Failed to Pa_OpenStream\n");
        goto __out;
    }
    if (Pa_StartStream(inStream) != paNoError) {
        fprintf(stderr, "Failed to Pa_StartStream\n");
        goto __out;
    }

    while (1) {
        fprintf(stdout, "Waiting key event:\n");
        fprintf(stdout, "  Q|q   : quit\n");
        char keyevent = getc(stdin);
        if (keyevent == 'Q' || keyevent == 'q') {
            fprintf(stdout, "Quit\n");
            break;
        }
    }

__out:
    Pa_Terminate();
    litevad_destroy(vadHandle);
    return 0;
}
