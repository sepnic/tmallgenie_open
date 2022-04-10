cmake_minimum_required(VERSION 2.8)
project(speexenc_demo)

set(SPEEX_DIR "${CMAKE_SOURCE_DIR}/..")

# include files
include_directories(${SPEEX_DIR}/include)

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -std=gnu99")

# speex files
file(GLOB SPEEX_SRC src
    ${SPEEX_DIR}/libspeex/*.c
    ${SPEEX_DIR}/libogg/*.c
)
add_library(speex STATIC ${SPEEX_SRC})
target_compile_options(speex PRIVATE
    -DFIXED_POINT -DUSE_KISS_FFT -DEXPORT=
    -Wno-error=literal-conversion
)

# speexenc_demo
add_executable(speexenc_demo ${CMAKE_SOURCE_DIR}/speexenc_demo.c)
target_link_libraries(speexenc_demo speex)
