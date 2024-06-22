cmake_minimum_required(VERSION 3.4.1)
project(tmallgenie_demo)

option(ENABLE_LITEVAD_SPEECH_DETECT  "Enable litevad speech detect"  "ON")

set(TOP_DIR             "${CMAKE_SOURCE_DIR}/../../../../../..")
set(SYSUTILS_DIR        "${TOP_DIR}/thirdparty/sysutils")
set(NOPOLL_DIR          "${TOP_DIR}/thirdparty/nopoll")
set(MBEDTLS_DIR         "${TOP_DIR}/thirdparty/mbedtls")
set(LITEPLAYER_DIR      "${TOP_DIR}/thirdparty/liteplayer")
set(SPEEX_DIR           "${TOP_DIR}/thirdparty/speex")
set(TMALLGENIE_DIR      "${TOP_DIR}")

# cflags: compile paramters
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu99 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

# include files
include_directories(${TMALLGENIE_DIR}/include)
include_directories(${SYSUTILS_DIR}/include)
include_directories(${MBEDTLS_DIR}/include)
include_directories(${NOPOLL_DIR}/src)
include_directories(${LITEPLAYER_DIR}/include)
include_directories(${SPEEX_DIR}/include)

# liteplayer_core files
file(GLOB LITEPLAYER_CODEC_SRC src
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvmp3/src/*.cpp
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvaac/*.cpp)
set(LITEPLAYER_SRC
    ${LITEPLAYER_CODEC_SRC}
    ${LITEPLAYER_DIR}/src/esp_adf/audio_element.c
    ${LITEPLAYER_DIR}/src/esp_adf/audio_event_iface.c
    ${LITEPLAYER_DIR}/src/audio_decoder/mp3_pvmp3_wrapper.c
    ${LITEPLAYER_DIR}/src/audio_decoder/mp3_decoder.c
    ${LITEPLAYER_DIR}/src/audio_decoder/aac_pvaac_wrapper.c
    ${LITEPLAYER_DIR}/src/audio_decoder/aac_decoder.c
    ${LITEPLAYER_DIR}/src/audio_decoder/m4a_decoder.c
    ${LITEPLAYER_DIR}/src/audio_decoder/wav_decoder.c
    ${LITEPLAYER_DIR}/src/audio_extractor/mp3_extractor.c
    ${LITEPLAYER_DIR}/src/audio_extractor/aac_extractor.c
    ${LITEPLAYER_DIR}/src/audio_extractor/m4a_extractor.c
    ${LITEPLAYER_DIR}/src/audio_extractor/wav_extractor.c
    ${LITEPLAYER_DIR}/src/liteplayer_adapter.c
    ${LITEPLAYER_DIR}/src/liteplayer_source.c
    ${LITEPLAYER_DIR}/src/liteplayer_parser.c
    ${LITEPLAYER_DIR}/src/liteplayer_main.c
    ${LITEPLAYER_DIR}/src/liteplayer_listplayer.c
    ${LITEPLAYER_DIR}/src/liteplayer_ttsplayer.c)
add_library(liteplayer_core STATIC ${LITEPLAYER_SRC})
target_compile_options(liteplayer_core PRIVATE
    -Wno-error=narrowing
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE -DLITEPLAYER_CONFIG_AAC_SBR
    -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=\(void\))
target_include_directories(liteplayer_core PRIVATE
    ${LITEPLAYER_DIR}/thirdparty/codecs
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvmp3/include
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvmp3/src
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvaac
    ${LITEPLAYER_DIR}/src)

# tmallgenie_open files
set(TMALLGENIE_OPEN_SRC
    ${LITEPLAYER_DIR}/adapter/source_httpclient_wrapper.c
    ${LITEPLAYER_DIR}/adapter/source_file_wrapper.c
    ${TMALLGENIE_DIR}/src/player/GenieUtpManager.c
    ${TMALLGENIE_DIR}/src/player/GeniePlayer.c
    ${TMALLGENIE_DIR}/src/player/vendorplayer/GenieVendorPlayer.c
    ${TMALLGENIE_DIR}/src/recorder/GenieRecorder.c
    ${TMALLGENIE_DIR}/src/GenieSdk.c)
add_library(tmallgenie_open STATIC ${TMALLGENIE_OPEN_SRC})
target_include_directories(tmallgenie_open PRIVATE
    ${TMALLGENIE_DIR}/src ${LITEPLAYER_DIR}/adapter)
target_compile_options(tmallgenie_open PRIVATE -DGENIE_HAVE_SPEEXOGG_ENABLED)

# mbedtls files
file(GLOB MBEDTLS_SRC src ${MBEDTLS_DIR}/library/*.c)
add_library(mbedtls STATIC ${MBEDTLS_SRC})
target_compile_options(mbedtls PRIVATE -D_SOCKLEN_T)

# sysutils files
set(SYSUTILS_SRC
    ${SYSUTILS_DIR}/osal/unix/os_log.c
    ${SYSUTILS_DIR}/osal/unix/os_memory.c
    ${SYSUTILS_DIR}/osal/unix/os_thread.c
    ${SYSUTILS_DIR}/osal/unix/os_time.c
    ${SYSUTILS_DIR}/osal/unix/os_timer.c
    ${SYSUTILS_DIR}/osal/unix/os_misc.c
    ${SYSUTILS_DIR}/source/cutils/memdbg.c
    ${SYSUTILS_DIR}/source/cutils/mlooper.c
    ${SYSUTILS_DIR}/source/cutils/mqueue.c
    ${SYSUTILS_DIR}/source/cutils/ringbuf.c
    ${SYSUTILS_DIR}/source/cutils/lockfree_ringbuf.c
    ${SYSUTILS_DIR}/source/httpclient/httpclient.c
    ${SYSUTILS_DIR}/source/json/cJSON.c
    ${SYSUTILS_DIR}/source/json/cJSON_Utils.c)
add_library(sysutils STATIC ${SYSUTILS_SRC})
target_compile_options(sysutils PRIVATE -DOS_ANDROID -DSYSUTILS_HAVE_MBEDTLS_ENABLED)

# nopoll files
set(NOPOLL_SRC
    ${NOPOLL_DIR}/src/nopoll.c
    ${NOPOLL_DIR}/src/nopoll_conn_opts.c
    ${NOPOLL_DIR}/src/nopoll_decl.c
    ${NOPOLL_DIR}/src/nopoll_listener.c
    ${NOPOLL_DIR}/src/nopoll_loop.c
    ${NOPOLL_DIR}/src/nopoll_conn.c
    ${NOPOLL_DIR}/src/nopoll_ctx.c
    ${NOPOLL_DIR}/src/nopoll_io.c
    ${NOPOLL_DIR}/src/nopoll_log.c
    ${NOPOLL_DIR}/src/nopoll_msg.c
    ${TMALLGENIE_DIR}/src/base/websocket_client.c)
add_library(nopoll STATIC ${NOPOLL_SRC})
target_compile_options(nopoll PRIVATE
    -DNOPOLL_HAVE_SYSUTILS_ENABLED -DNOPOLL_HAVE_MBEDTLS_ENABLED)

# speex files
file(GLOB SPEEX_SRC src ${SPEEX_DIR}/libspeex/*.c ${SPEEX_DIR}/libogg/*.c)
add_library(speex STATIC ${SPEEX_SRC})
target_compile_options(speex PRIVATE
    -DFIXED_POINT -DUSE_KISS_FFT -DEXPORT= -DSPEEX_HAVE_SYSUTILS_ENABLED)

# tmallgenie_core
add_library(tmallgenie_core STATIC IMPORTED)
set_target_properties(tmallgenie_core PROPERTIES IMPORTED_LOCATION "${TMALLGENIE_DIR}/lib/android/${ANDROID_ABI}/libtmallgenie_core.a")

# tmallgenie-jni
set(JNI_SRC
    tmallgenie-jni.cpp
    GenieVendor_Android.cpp
    GenieVoiceEngine_Android.cpp)
add_library(tmallgenie-jni SHARED ${JNI_SRC})
target_link_libraries(tmallgenie-jni
    tmallgenie_open tmallgenie_core liteplayer_core nopoll speex sysutils mbedtls
    OpenSLES android log)

if(ENABLE_LITEVAD_SPEECH_DETECT)
    set(LITEVAD_DIR "${TOP_DIR}/thirdparty/litevad")
    include_directories(${LITEVAD_DIR}/include)

    # litevad
    set(LITEVAD_SRC
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/energy.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/division_operations.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/get_scaling_square.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/resample.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/resample_48khz.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/resample_by_2.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/resample_by_2_internal.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/signal_processing/resample_fractional.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/vad/vad_core.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/vad/vad_filterbank.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/vad/vad_gmm.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/vad/vad_sp.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/vad/webrtc_vad.c
        ${LITEVAD_DIR}/thirdparty/webrtc/src/resampler/resampler.cc
        ${LITEVAD_DIR}/src/litevad.c)
    add_library(litevad STATIC ${LITEVAD_SRC})
    target_include_directories(litevad PRIVATE ${LITEVAD_DIR}/thirdparty/webrtc/inc)
    target_compile_options(litevad PRIVATE -DLITEVAD_HAVE_SYSUTILS_ENABLED)

    target_compile_options(tmallgenie-jni PRIVATE -DGENIE_HAVE_LITEVAD_SPEECH_DETECT_ENABLED)
    target_link_libraries(tmallgenie-jni litevad)
endif()
