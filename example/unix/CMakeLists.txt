cmake_minimum_required(VERSION 2.8)
project(tmallgenie_demo)

option(ENABLE_SNOWBOY_KEYWORD_DETECT  "Enable snowboy keyword detect" "ON")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
option(ENABLE_GENIE_ADAPTER_PORTAUDIO "Enable portaudio adapter"      "OFF")
else()
option(ENABLE_GENIE_ADAPTER_PORTAUDIO "Enable portaudio adapter"      "ON")
endif()

set(TOP_DIR             "${CMAKE_SOURCE_DIR}/../..")
set(SYSUTILS_DIR        "${TOP_DIR}/thirdparty/sysutils")
set(NOPOLL_DIR          "${TOP_DIR}/thirdparty/nopoll")
set(LITEPLAYER_DIR      "${TOP_DIR}/thirdparty/liteplayer")
set(LITEVAD_DIR         "${TOP_DIR}/thirdparty/litevad")
set(SPEEX_DIR           "${TOP_DIR}/thirdparty/speex")
set(SNOWBOY_DIR         "${TOP_DIR}/thirdparty/snowboy")

MESSAGE(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DOS_LINUX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_LINUX")
    execute_process(COMMAND uname -a OUTPUT_VARIABLE UNAME_OUTPUT)
    if(${UNAME_OUTPUT} MATCHES "raspberrypi")
        if(${UNAME_OUTPUT} MATCHES "aarch64")
            link_directories(${SNOWBOY_DIR}/lib/aarch64-ubuntu1604)
            link_directories(${TOP_DIR}/lib/aarch64-ubuntu)
        else()
            link_directories(${SNOWBOY_DIR}/lib/rpi)
            link_directories(${TOP_DIR}/lib/rpi)
        endif()
    elseif(${UNAME_OUTPUT} MATCHES "x86_64")
        link_directories(${SNOWBOY_DIR}/lib/ubuntu64)
        link_directories(${TOP_DIR}/lib/ubuntu64)
    else()
        message(FATAL_ERROR "Neither raspberrypi nor x86_64, abort....")
        return()
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DOS_APPLE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_APPLE")
    link_directories(${SNOWBOY_DIR}/lib/osx)
    link_directories(${TOP_DIR}/lib/macosx)
else()
    message(FATAL_ERROR "Neither Linux nor Macos, abort....")
    return()
endif()

# include files
include_directories(${TOP_DIR}/include)
include_directories(${SYSUTILS_DIR}/include)
include_directories(${NOPOLL_DIR}/src)
include_directories(${LITEPLAYER_DIR}/include)
include_directories(${LITEVAD_DIR}/include)
include_directories(${SPEEX_DIR}/include)
include_directories(${SNOWBOY_DIR}/include)

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -O3 -fPIC -Wall -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fPIC -Wall -std=c++11")

# cflags: check memory leak
#set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DSYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED")

# cflags: enable verbose log
#set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DSYSUTILS_HAVE_VERBOSE_LOG_ENABLED")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSUTILS_HAVE_VERBOSE_LOG_ENABLED")

# genie open files
set(TMALLGENIE_OPEN_SRC
    ${LITEPLAYER_DIR}/adapter/source_httpclient_wrapper.c
    ${LITEPLAYER_DIR}/adapter/source_file_wrapper.c
    ${TOP_DIR}/src/core/GenieService.c
    ${TOP_DIR}/src/player/GenieUtpManager.c
    ${TOP_DIR}/src/player/GeniePlayer.c
    ${TOP_DIR}/src/player/vendorplayer/GenieVendorPlayer.c
    ${TOP_DIR}/src/recorder/GenieRecorder.c
    ${TOP_DIR}/src/GenieSdk.c)
add_library(tmallgenie_open STATIC ${TMALLGENIE_OPEN_SRC})
target_include_directories(tmallgenie_open PRIVATE ${TOP_DIR}/src ${LITEPLAYER_DIR}/adapter)
target_compile_options(tmallgenie_open PRIVATE -DGENIE_HAVE_SPEEXOGG_ENABLED)

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
target_compile_options(sysutils PRIVATE -DSYSUTILS_HAVE_MBEDTLS_ENABLED)

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
    ${TOP_DIR}/src/base/websocket_client.c)
add_library(nopoll STATIC ${NOPOLL_SRC})
target_compile_options(nopoll PRIVATE
    -DNOPOLL_HAVE_SYSUTILS_ENABLED -DNOPOLL_HAVE_MBEDTLS_ENABLED)

# liteplayer files
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
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE
    -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=)
target_include_directories(liteplayer_core PRIVATE
    ${LITEPLAYER_DIR}/thirdparty/codecs
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvmp3/include
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvmp3/src
    ${LITEPLAYER_DIR}/thirdparty/codecs/pvaac
    ${LITEPLAYER_DIR}/src)

# litevad files
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

# speex files
file(GLOB SPEEX_SRC src ${SPEEX_DIR}/libspeex/*.c ${SPEEX_DIR}/libogg/*.c)
add_library(speex STATIC ${SPEEX_SRC})
target_compile_options(speex PRIVATE
    -DFIXED_POINT -DUSE_KISS_FFT -DEXPORT= -DSPEEX_HAVE_SYSUTILS_ENABLED)

# mbedtls
if(1)
    set(MBEDTLS_DIR "${TOP_DIR}/thirdparty/mbedtls")
    include_directories(${MBEDTLS_DIR}/include)
    file(GLOB MBEDTLS_SRC src ${MBEDTLS_DIR}/library/*.c)
    add_library(mbedtls STATIC ${MBEDTLS_SRC})
    set(MBEDTLS_LIBS "mbedtls")
else()
    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/install_mbedtls.sh
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${CMAKE_CURRENT_BINARY_DIR}/mbedtls/include)
    link_directories(${CMAKE_CURRENT_BINARY_DIR}/mbedtls/install/library)
    set(MBEDTLS_LIBS "mbedtls -lmbedx509 -lmbedcrypto")
endif()

# genie adapter
if(ENABLE_GENIE_ADAPTER_PORTAUDIO)
    execute_process(COMMAND ${CMAKE_SOURCE_DIR}/install_portaudio.sh
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${CMAKE_CURRENT_BINARY_DIR}/portaudio/install/include)
    link_directories(${CMAKE_CURRENT_BINARY_DIR}/portaudio/install/lib)

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(PLATFORM_LIBS "asound -lf77blas -lcblas -llapack_atlas -latlas")
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Accelerate")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreAudio")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioToolbox")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioUnit")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreServices")
    endif()

    set(PLATFORM_LIBS portaudio ${PLATFORM_LIBS})

    set(TMALLGENIE_ADAPTER_SRC
        ${TOP_DIR}/adapter/portaudio/GenieVoiceEngine_PortAudio.c
        ${TOP_DIR}/adapter/portaudio/GenieVendor_PortAudio.c)
else()
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        # linux alsa
        set(TMALLGENIE_ADAPTER_SRC
            ${TOP_DIR}/adapter/alsa/GenieVoiceEngine_Alsa.c
            ${TOP_DIR}/adapter/alsa/GenieVendor_Alsa.c)
        set(PLATFORM_LIBS "asound -lf77blas -lcblas -llapack_atlas -latlas")
    else()
        message(FATAL_ERROR "None genie adapter, abort....")
        return()
    endif()
endif()

# snowboy keyword detect
if(ENABLE_SNOWBOY_KEYWORD_DETECT)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -DGENIE_HAVE_SNOWBOY_KEYWORD_DETECT_ENABLED")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
    set(TMALLGENIE_ADAPTER_SRC ${TMALLGENIE_ADAPTER_SRC}
        ${SNOWBOY_DIR}/wrapper/snowboy-detect-c-wrapper.cc)
    set(PLATFORM_LIBS snowboy-detect ${PLATFORM_LIBS})
    file(COPY
        ${SNOWBOY_DIR}/resources/common.res
        ${SNOWBOY_DIR}/resources/models/snowboy.umdl
        ${SNOWBOY_DIR}/resources/models/alexa.umdl
        ${SNOWBOY_DIR}/resources/models/jarvis.umdl
        ${SNOWBOY_DIR}/resources/models/computer.umdl
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# GenieMain
add_executable(GenieMain ${CMAKE_SOURCE_DIR}/GenieMain.c ${TMALLGENIE_ADAPTER_SRC})
target_include_directories(GenieMain PRIVATE ${TOP_DIR}/adapter)
target_link_libraries(GenieMain
    tmallgenie_open tmallgenie_protocol
    liteplayer_core nopoll litevad speex sysutils pthread
    ${MBEDTLS_LIBS} ${PLATFORM_LIBS})
