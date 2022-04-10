Liteplayer 是一个为嵌入式平台设计的低开销低延时的音频播放器，已运行在千万级别的设备上，包括 Android、iOS、Linux、RTOS 多种终端平台

Liteplayer 具有如下特点：
1. 支持 MP3、AAC、M4A、WAV、FLAC、OPUS 格式，支持本地文件、本地播放列表、HTTP/HTTPS/HLS 和 TTS 数据流，接口和状态机与 Android MediaPlayer 一致
2. 极低的系统开销，1-2 个线程（建议网络流使用双线程模式，文件流使用单线程模式），最低至 48KB 堆内存占用，已集成在 主频192MHz + 内存448KB 的系统上并产品量产；高配置平台上可配置更大的缓冲区以取得更好的播放体验
3. 高度的移植性，纯 C 语言 C99 标准，已运行在 Linux、Android、iOS、MacOS、FreeRTOS、AliOS-Things 上；如果其平台不支持 POSIX 接口规范，则实现 Thread、Memory、Time 相关的少量 OSAL 接口也可接入
4. 抽象流数据输入、音频设备输出的接口，使用者可自由添加各种流协议如 rtsp、rtmp、sdcardfs、flash 等等
5. 适配多个解码器，包括 libmad、pv-mp3、helix-aac、pv-aac 等等，也可适配芯片原厂提供的解码器
6. 提供丰富的调试手段，可以收集及分析播放链路各节点的音频流数据；提供内存检测手段，能直观查看内存分配细节、分析内存泄漏和内存越界

**核心播放接口**：
- 提供播放器基本服务，包括 set_data_source、prepare、start、pause、resume、seek、stop、reset 等操作
- https://github.com/sepnic/liteplayer_open/blob/master/include/liteplayer_main.h

**列表播放接口**：
- 除了播放器基本功能外，还支持 m3u8 协议列表、本地播放列表、切换上下首、单曲循环等操作
- https://github.com/sepnic/liteplayer_open/blob/master/include/liteplayer_listplayer.h

**TTS播放接口**：
- 提供 TTS 流播放功能，支持 prepare、start、stop、reset 等操作
- https://github.com/sepnic/liteplayer_open/blob/master/include/liteplayer_ttsplayer.h

**播放器适配层**：
- 数据源输入、音频设备输出的抽象接口，默认适配了 "文件流-标准文件系统"、 "网络流-httpclient"、"音频设备输出-alsa/OpenSLES/AudioTrack"
- https://github.com/sepnic/liteplayer_open/blob/master/include/liteplayer_adapter.h

**OSAL 适配层**：
- Thread、Memory、Time 等操作系统相关的抽象接口，如果系统已支持 POSIX 接口规范，则不用修改直接使用即可
- https://github.com/sepnic/sysutils/tree/cutils_c99/osal

![LiteplayerArchitecture](https://github.com/sepnic/liteplayer_open/blob/master/Liteplayer.png)
