litevad 是一个基于 webrtc 的轻量级 VAD 模块，采用简单的算法可以检测语句的开始与结束，这在某些场景下非常实用。比如语音助手启动录音后，说完“今天天气怎么样”后，litevad 可以发出语句结束的事件让程序自动结束录音，用户无须手动点击按钮结束录音。

使用者可以调整 litevad.c 里面的参数，以取得更好的表现效果：
``` c
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
```

android/demo 是一个 android 上的使用范例，AudioReecorder 的录音数据送到 litevad 处理，返回如下事件：
``` c
LITEVAD_RESULT_ERROR = -1,         // 错误，一般是传入的音频帧数不是 10ms/20ms/30ms 的整数倍导致的
LITEVAD_RESULT_FRAME_SILENCE = 0,  // 当前数据判断是静音帧
LITEVAD_RESULT_FRAME_ACTIVE = 1,   // 当前数据判断是语音帧
LITEVAD_RESULT_SPEECH_BEGIN = 2,   // 满足语句开始条件
LITEVAD_RESULT_SPEECH_END = 3,     // 满足语句结束条件
```

确保帧数合法，请参考如下代码：
``` c
int sampleRate = 16000;
int channelConfig = AudioFormat.CHANNEL_IN_MONO;
int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
int bytesPer10Ms = sampleRate/100*2;
int bufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
bufferSize = (bufferSize/bytesPer10Ms + 1) * bytesPer10Ms; // 确保读取的音频帧数为 10ms 的整数倍
mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, channelConfig, audioFormat, bufferSize);
```

TODO LIST：
1. 噪音环境下体验一般，容易误判，可能数据在 VAD 前先进行降噪会比较好，之前用 rnnoise (https://github.com/xiph/rnnoise) 降噪效果很好，以后可考虑集成
