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
set(MBEDTLS_DIR         "${TOP_DIR}/thirdparty/mbedtls")
set(LITEPLAYER_DIR      "${TOP_DIR}/thirdparty/liteplayer")
set(LITEVAD_DIR         "${TOP_DIR}/thirdparty/litevad")
set(SPEEX_DIR           "${TOP_DIR}/thirdparty/speex")
set(SNOWBOY_DIR         "${TOP_DIR}/thirdparty/snowboy")

MESSAGE(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DOS_LINUX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_LINUX")
    link_directories(${SNOWBOY_DIR}/lib/ubuntu64)
    link_directories(${TOP_DIR}/lib/ubuntu64)
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
include_directories(${MBEDTLS_DIR}/include)
include_directories(${LITEPLAYER_DIR}/include)
include_directories(${LITEVAD_DIR}/include)
include_directories(${SPEEX_DIR}/include)
include_directories(${SNOWBOY_DIR}/include)

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Werror -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -std=c++11")

# genie open files
set(TMALLGENIE_OPEN_SRC
    ${LITEPLAYER_DIR}/adapter/source_httpclient_wrapper.c
    ${LITEPLAYER_DIR}/adapter/source_file_wrapper.c
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
    ${SYSUTILS_DIR}/source/cutils/swtimer.c
    ${SYSUTILS_DIR}/source/httpclient/httpclient.c
    ${SYSUTILS_DIR}/source/json/cJSON.c
    ${SYSUTILS_DIR}/source/json/cJSON_Utils.c)
add_library(sysutils STATIC ${SYSUTILS_SRC})
target_compile_options(sysutils PRIVATE -DSYSUTILS_HAVE_MBEDTLS_ENABLED)

# mbedtls files
file(GLOB MBEDTLS_SRC src ${MBEDTLS_DIR}/library/*.c)
add_library(mbedtls STATIC ${MBEDTLS_SRC})

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

# genie adapter
if(ENABLE_GENIE_ADAPTER_PORTAUDIO)
    # portaudio
    set(PORTAUDIO_DIR "${TOP_DIR}/thirdparty/portaudio")
    include_directories(${PORTAUDIO_DIR}/include)
    include_directories(${PORTAUDIO_DIR}/src/common)
    include_directories(${PORTAUDIO_DIR}/src/os/unix)
    set(PORTAUDIO_SRC
        ${PORTAUDIO_DIR}/src/common/pa_allocation.c
        ${PORTAUDIO_DIR}/src/common/pa_converters.c
        ${PORTAUDIO_DIR}/src/common/pa_cpuload.c
        ${PORTAUDIO_DIR}/src/common/pa_debugprint.c
        ${PORTAUDIO_DIR}/src/common/pa_dither.c
        ${PORTAUDIO_DIR}/src/common/pa_front.c
        ${PORTAUDIO_DIR}/src/common/pa_process.c
        ${PORTAUDIO_DIR}/src/common/pa_ringbuffer.c
        ${PORTAUDIO_DIR}/src/common/pa_stream.c
        ${PORTAUDIO_DIR}/src/common/pa_trace.c
        ${PORTAUDIO_DIR}/src/hostapi/skeleton/pa_hostapi_skeleton.c
        ${PORTAUDIO_DIR}/src/os/unix/pa_unix_hostapis.c
        ${PORTAUDIO_DIR}/src/os/unix/pa_unix_util.c)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_USE_ALSA=1")
        set(PLATFORM_LIB "asound -lf77blas -lcblas -llapack_atlas -latlas")
        set(PORTAUDIO_SRC ${PORTAUDIO_SRC} ${PORTAUDIO_DIR}/src/hostapi/alsa/pa_linux_alsa.c)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_USE_COREAUDIO=1")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Accelerate")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreAudio")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioToolbox")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioUnit")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreServices")
        set(PORTAUDIO_SRC ${PORTAUDIO_SRC}
            ${PORTAUDIO_DIR}/src/hostapi/coreaudio/pa_mac_core.c
            ${PORTAUDIO_DIR}/src/hostapi/coreaudio/pa_mac_core_blocking.c
            ${PORTAUDIO_DIR}/src/hostapi/coreaudio/pa_mac_core_utilities.c)
    endif()
    include(TestBigEndian)
    TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
    if(IS_BIG_ENDIAN)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_BIG_ENDIAN")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_LITTLE_ENDIAN")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=deprecated-declarations")

    set(TMALLGENIE_ADAPTER_SRC
        ${PORTAUDIO_SRC}
        ${TOP_DIR}/adapter/portaudio/GenieVoiceEngine_PortAudio.c
        ${TOP_DIR}/adapter/portaudio/GenieVendor_PortAudio.c)
else()
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        # linux alsa
        set(TMALLGENIE_ADAPTER_SRC
            ${TOP_DIR}/adapter/alsa/GenieVoiceEngine_Alsa.c
            ${TOP_DIR}/adapter/alsa/GenieVendor_Alsa.c)
        set(PLATFORM_LIB "asound -lf77blas -lcblas -llapack_atlas -latlas")
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
    set(PLATFORM_LIB snowboy-detect ${PLATFORM_LIB})
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
    tmallgenie_open tmallgenie_core liteplayer_core litevad speex sysutils mbedtls pthread
    ${PLATFORM_LIB})
