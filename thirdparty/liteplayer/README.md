Liteplayer 是一个为嵌入式平台设计的低开销低延时的音频播放器，支持 Android、iOS、Linux、RTOS 等平台

Liteplayer 具有如下特点：
1. 支持 MP3、AAC、M4A、WAV、格式，支持本地文件、本地播放列表、HTTP/HTTPS/HLS 和 TTS 数据流，接口和状态机与 Android MediaPlayer 一致
2. 极低的系统开销，1-2 个线程（建议网络流使用双线程模式，文件流使用单线程模式），最低至 48KB 堆内存占用，已集成在 主频192MHz + 内存448KB 的系统上并产品量产；高配置平台上可配置更大的缓冲区以取得更好的播放体验
3. 高度的移植性，纯 C 语言 C99 标准，已运行在 Linux、Android、iOS、MacOS、FreeRTOS、AliOS-Things 上；如果其平台不支持 POSIX 接口规范，则实现 Thread、Memory、Time 相关的少量 OSAL 接口也可接入
4. 抽象流数据输入、音频设备输出的接口，使用者可自由添加各种流协议如 rtsp、rtmp、sdcardfs、flash 等等
5. 适配多个解码器，包括 pv-mp3、pv-aac、wave 等等，也可适配芯片原厂提供的解码器

编译运行：
- MacOSX/Ubuntu：[How to build liteplayer for macosx/ubuntu](https://github.com/sepnic/liteplayer/blob/main/example/unix/README.md)
- ESP32：[How to build liteplayer for esp32](https://github.com/sepnic/liteplayer/blob/main/example/esp32/README.md)
- Android：Android Studio 打开 [example/android](https://github.com/sepnic/liteplayer/blob/main/example/android)，编译运行

![LiteplayerArchitecture](https://github.com/sepnic/liteplayer/blob/main/Liteplayer.png)
