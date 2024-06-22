cmake_minimum_required(VERSION 2.8)
project(liteplayer_core)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -O2 -fPIC -std=gnu99 -Wall -DOS_APPLE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC -std=c++11 -Wall -DOS_APPLE")

# include files
include_directories(${TOP_DIR}/thirdparty/sysutils/include)
include_directories(${TOP_DIR}/include)

# liteplayer files
file(GLOB CODECS_SRC src
    ${TOP_DIR}/thirdparty/codecs/pvmp3/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/pvaac/*.cpp
)
set(LITEPLAYER_SRC
    ${CODECS_SRC}
    ${TOP_DIR}/src/esp_adf/audio_element.c
    ${TOP_DIR}/src/esp_adf/audio_event_iface.c
    ${TOP_DIR}/src/audio_decoder/mp3_pvmp3_wrapper.c
    ${TOP_DIR}/src/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/src/audio_decoder/aac_pvaac_wrapper.c
    ${TOP_DIR}/src/audio_decoder/aac_decoder.c
    ${TOP_DIR}/src/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/src/audio_decoder/wav_decoder.c
    ${TOP_DIR}/src/audio_extractor/mp3_extractor.c
    ${TOP_DIR}/src/audio_extractor/aac_extractor.c
    ${TOP_DIR}/src/audio_extractor/m4a_extractor.c
    ${TOP_DIR}/src/audio_extractor/wav_extractor.c
    ${TOP_DIR}/src/liteplayer_adapter.c
    ${TOP_DIR}/src/liteplayer_source.c
    ${TOP_DIR}/src/liteplayer_parser.c
    ${TOP_DIR}/src/liteplayer_main.c
    ${TOP_DIR}/src/liteplayer_listplayer.c
    ${TOP_DIR}/src/liteplayer_ttsplayer.c
)
add_library(liteplayer_core STATIC ${LITEPLAYER_SRC})
target_compile_options(liteplayer_core PRIVATE
    -Wno-error=narrowing
    -D__amd64__
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE
    -DLITEPLAYER_CONFIG_AAC_SBR
    -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=\(void\)
)
target_include_directories(liteplayer_core PRIVATE
    ${TOP_DIR}/thirdparty/codecs
    ${TOP_DIR}/thirdparty/codecs/pvmp3/include
    ${TOP_DIR}/thirdparty/codecs/pvmp3/src
    ${TOP_DIR}/thirdparty/codecs/pvaac
    ${TOP_DIR}/src
)

# sysutils files
set(SYSUTILS_SRC
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c
)
add_library(sysutils STATIC ${SYSUTILS_SRC})
