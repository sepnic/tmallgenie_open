**tmallgenie_open** 是一个简单易用的、低延时的、低开销的、跨平台的语音助手/智能音箱项目：
- 简单易用：平台层接入简单，适配 [GenieVendorAdapter](https://github.com/sepnic/tmallgenie_open/blob/master/include/GenieVendorAdapter.h) 接口即可；应用层使用简单，接口见 [GenieSdk](https://github.com/sepnic/tmallgenie_open/blob/master/include/GenieSdk.h)，使用范例见 [GenieMain](https://github.com/sepnic/tmallgenie_open/blob/master/example/unix/GenieMain.c)
- 低延时：从说话结束到服务端响应的时间少于1秒，达到秒回的效果，与阿里官方智能音箱一样的交互体验
- 低开销：性能和内存开销极低，可以在各类 RTOS 系统上落地，如 ESP32 demo 为例，CPU 主频为 160MHz，堆内存开销 400KB 以内（还包括了 256KB 的音乐播放器缓冲区和 32KB 的 TTS 播放器缓冲区）
- 跨平台：已适配 MacOSX、Ubuntu、ESP32 平台，后续看需求支持更多的平台

Demo 视频：
 - [Ubuntu 精灵语音助手](https://www.bilibili.com/video/BV1Tu411R7y4)
 - [ESP32 精灵智能音箱](https://www.bilibili.com/video/BV1q34y1C7CA)

## 精灵整体框架图
![GenieArchitecture](https://github.com/sepnic/tmallgenie_open/blob/master/GenieArchitecture.png)

左边是阿里精灵服务端，右边是设备端 SDK，两端通过 websocket 协议进行账户激活和鉴权、语音交互，另外设备端需要通过 http 协议下载服务端内容中心的音乐资源。

## 精灵语音交互逻辑
```
+---------------------------------------------------------------------------------------+
|                                Genie Interaction Service                              |
+-------^-- ^------+----------------^----------------+-----------^------------+---------+
        |   |      |                |                |           |            |
        |   |  9.send stop command: |     5.send start command:  |            |
        |   |    ExpectSpeechStop   |       ExpectSpeechStart    |            |
        |   |      |                |                |           |            |
        |   |      |        upload pcm data:         |           | 2.broadcast wakeup event:
        |   |      |        onMicphoneStreaming()    |           |   GENIE_STATUS_MicphoneWakeup
        |   |      |                |                |           |            |
        |   |   +--v----------------+----------------v--+        |            |
        |   |   |             Genie Recorder            |  4.request record:  |
        |   |   +--+----------------^----------------+--+    onExpectSpeech() |
        |   |      |                |                |           |            |
        |   |      |                +----------------+      +----+------------v----+
        |   |      |            6.start recording loop:     |     Genie Player     |
        |   |      |              read pcm data and upload  +----^------------+----+
        |   |      |              to interaction service         |            |
        |   |      |                                             +------------+
        |   |      +----------->10:stop recording                3.play WAKEUP_REMIND prompt:
        |   |                                                      onCommandNewPromptWakeup()
        |   |                                                      and wait prompt played done
        |   +---------------------+
        |                         |
1.keyword detected:        8.silence detected:
  onMicphoneWakeup()         onMicphoneSilence()
        |                         |
+-------+-------------------------+-------+
|          Vendor Voice Engine            |
+-------^------+-----------+------^-------+
        |      |           |      |
        +------+           +------+
0.start keyword detect,    7.start vad detect,
  waiting sound trigger      waiting micphone silence
```
- Genie Interaction Service：端云交互中心，包括服务端对接、账户激活鉴权、网关协议解析、设备状态机，这部分代码不公开
- Genie Recorder：录音器，负责录音和语音压缩上传，见 https://github.com/sepnic/tmallgenie_open/tree/master/src/recorder
- Genie Player：播放控制及播放器，见 https://github.com/sepnic/tmallgenie_open/tree/master/src/player
- Vendor Voice Engine：平台层的语音引擎，负责回音消除、噪音抑制、语音唤醒、VAD 语句判停等处理，MacOSX/Ubuntu 平台这方面的实现见 https://github.com/sepnic/tmallgenie_open/tree/master/adapter/portaudio

Genie Interaction Service 是大管家，接收服务端下发的指令（比如音乐和TTS播放、闹钟和提醒设置、灯效设置、屏幕显示、智能家居控制），接收设备端各个子组件上传的状态、事件、请求（比如网络连接断开事件、语音唤醒事件、语音静音事件、音量调整事件、播放状态、NLU 请求），指令和事件都在这里进行仲裁并决定下一步行动。底下的子组件是相互独立的，各自只与 Genie Interaction Service 交互，这样做的好处是子组件都是独立解耦的，相互之间没有依赖，各自处理好自己分内之事即可。一般来说子组件需要注册一个指令监听者以处理下发的指令，还有调用相关回调函数上传自身的状态和事件。比如 player 需要处理所有与播放相关的指令并上传播放器状态，Vendor Voice Engine 需要上传语音唤醒事件和语音静音事件。

值得一提的是 Genie 播放内容分为三类：
- U：音乐、新闻类内容，这类内容的特点是支持暂停和恢复播放
- T：TTS 内容，比如实时天气的语音回复，这类内容的特点是不能暂停或恢复播放，只能中止
- P：Prompt 内容，比如唤醒提示音、闹钟提示音、网络中断提示音，这类内容的特点和 T 一样不能暂停或恢复播放，只能中止

播放优先级：T=P>U，具体来说，T 和 P 按照先进先出策略排队播放，而 U 播放过程中，如果有 T/P 到来，则需要先挂起 U，优先播放 T/P，待所有的 T/P 播放完成或中止后，才恢复 U 播放。此外语音唤醒事件会中止 T/P 或挂起 U，待录音结束后才恢复 U。

player 包含两部分，分别是播控系统（UTPManager）和播放器（Liteplayer）。
- 播控系统：按照所述的优先级实现播放器控制策略
- 播放器：目前使用的是本人实现的 Liteplayer，也可以使用其他播放器接入到播控系统

## 编译运行
- MacOSX/Ubuntu：[How to build tmallgenie for macosx/ubuntu](https://github.com/sepnic/tmallgenie_open/blob/master/example/unix/README.md)
- ESP32：[How to build tmallgenie for esp32](https://github.com/sepnic/tmallgenie_open/blob/master/example/esp32/README.md)

## 依赖的第三方库
- [sysutils](https://github.com/sepnic/sysutils)：系统基本组件，包括 osal/looper/ringbuf/cipher/json/httpclient。作者：本人(Qinglong)，许可：Apache-2.0
- [mbedtls](https://github.com/Mbed-TLS/mbedtls/tree/mbedtls-2.16)：通讯加密。作者：MbedTLS，许可：Apache-2.0
- [nopoll](https://github.com/ASPLes/nopoll)：websocket 协议通讯。作者：ASPLes，许可：LGPL-2.1
- [speex](https://github.com/xiph/speex) & [ogg](https://github.com/xiph/ogg)：语音压缩和编码。作者：xiph，许可：FreeBSD
- [pvmp3](http://androidxref.com/4.4_r1/xref/frameworks/av/media/libstagefright/codecs/mp3dec/src/) & [pvaac](http://androidxref.com/2.2.3/xref/frameworks/base/media/libstagefright/codecs/aacdec/)：mp3 & aac 音频解码库。作者：PacketVideo，许可：Apache-2.0
- [liteplayer](https://github.com/sepnic/liteplayer_open)：音频播放器。作者：本人(Qinglong)，许可：Apache-2.0
- [litevad](https://github.com/sepnic/litevad)：语句结束检测，依赖 webrtc VAD 模块。作者：本人(Qinglong)，许可：Apache-2.0
- [snowboy](https://github.com/Kitt-AI/snowboy)：语音热词检测，即语音唤醒。作者：KITT.AI，许可：Apache-2.0
- [portaudio](https://github.com/PortAudio/portaudio)：跨平台的音频 I/O 库。作者：PortAudio，许可：FreeBSD

## 限制说明
tmallgenie_open 仅用作个人研究学习，不能用于商业用途。本项目现在有两个限制：
1. 在线时间超过 10 分钟，自动中止服务
2. 交互次数超过 20 次，自动中止服务

拟在 MacOSX/Ubuntu 平台取消这些限制，针对 ESP32 平台会一直保留这些限制。

现在 example 代码中的 bizType/bizGroup/bizSecret 均为空，事实上这些客户信息需要到 [天猫精灵AI平台](https://product.aligenie.com/) 申请（注意需要企业认证后才能操作）。如果已有私有的 biz，且确保该 biz 不会用于正式产品，则建议填入这些信息；为空的话，SDK 会使用默认的一组 biz，但过多的人共用一组 biz，可能会被服务端检测到异常并封杀

## TODO
1. ESP32 demo 支持语音唤醒，参考 [esp-sr](https://github.com/espressif/esp-sr)
2. MacOSX/Ubuntu demo 监听网络状态
3. Android demo
4. 语音前处理，如回音消除、噪音抑制，只有前端语音干净了，KWD/VAD/ASR 的处理结果才更准确
5. Genie 支持会员激活（目前都是走访客激活），以便支持 IoT 等服务
6. 设备智能配网 SmartConfig
