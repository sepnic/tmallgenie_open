cmake_minimum_required(VERSION 3.4.1)

# cflags: compile paramters
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu99 -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -UNDEBUG")

set(LITEVAD_DIR "${CMAKE_SOURCE_DIR}/../../../../../..")

include_directories(${LITEVAD_DIR}/include ${LITEVAD_DIR}/thirdparty/webrtc/inc)

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

add_library(litevad-jni SHARED litevad-jni.cpp ${LITEVAD_SRC})

target_link_libraries(litevad-jni android log)
