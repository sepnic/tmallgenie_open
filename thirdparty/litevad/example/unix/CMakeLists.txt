cmake_minimum_required(VERSION 2.8)
project(alsa_demo)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu99 -Wall -Werror")

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")

include_directories(${TOP_DIR}/include)

# litevad
set(LITEVAD_SRC
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/energy.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/division_operations.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/get_scaling_square.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_48khz.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_by_2.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_by_2_internal.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_fractional.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_core.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_filterbank.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_gmm.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_sp.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/webrtc_vad.c
    ${TOP_DIR}/src/litevad.c)
add_library(litevad STATIC ${LITEVAD_SRC})
target_include_directories(litevad PRIVATE ${TOP_DIR}/thirdparty/webrtc/inc)

# portaudio
execute_process(COMMAND ${CMAKE_SOURCE_DIR}/install_portaudio.sh
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/portaudio/install/include)
set(PORTAUDIO_LIB "${CMAKE_CURRENT_BINARY_DIR}/portaudio/install/lib/libportaudio.a")
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Accelerate")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreAudio")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioToolbox")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioUnit")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreServices")
endif()

# portaudio_demo
add_executable(portaudio_demo ${CMAKE_SOURCE_DIR}/portaudio_demo.c)
target_link_libraries(portaudio_demo litevad ${PORTAUDIO_LIB} pthread m)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(portaudio_demo asound)
endif()

# alsa_demo
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_executable(alsa_demo ${CMAKE_SOURCE_DIR}/alsa_demo.c)
    target_link_libraries(alsa_demo litevad asound)
endif()
