cmake_minimum_required(VERSION 2.8)
project(tmallgenie_unittest)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")
set(SYSUTILS_DIR "${TOP_DIR}/thirdparty/sysutils")
set(NOPOLL_DIR "${TOP_DIR}/thirdparty/nopoll")

MESSAGE(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DOS_LINUX")
    execute_process(COMMAND uname -a OUTPUT_VARIABLE UNAME_OUTPUT)
    if(${UNAME_OUTPUT} MATCHES "raspberrypi")
        if(${UNAME_OUTPUT} MATCHES "aarch64")
            link_directories(${TOP_DIR}/lib/aarch64-ubuntu)
        else()
            link_directories(${TOP_DIR}/lib/rpi)
        endif()
    elseif(${UNAME_OUTPUT} MATCHES "x86_64")
        link_directories(${TOP_DIR}/lib/ubuntu64)
    else()
        message(FATAL_ERROR "Neither raspberrypi nor x86_64, abort....")
        return()
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DOS_APPLE")
    link_directories(${TOP_DIR}/lib/macosx)
else()
    message(FATAL_ERROR "Neither Linux nor Macos, abort....")
    return()
endif()

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fPIC -Wall -std=gnu99")

# include files
include_directories(${SYSUTILS_DIR}/include)
include_directories(${NOPOLL_DIR}/src)
include_directories(${TOP_DIR}/include ${TOP_DIR}/src)

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

# sysutils
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
    ${SYSUTILS_DIR}/source/json/cJSON_Utils.c
)
add_library(sysutils STATIC ${SYSUTILS_SRC})
target_compile_options(sysutils PRIVATE -DSYSUTILS_HAVE_MBEDTLS_ENABLED)

# nopoll
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
    ${TOP_DIR}/src/base/websocket_client.c
)
add_library(nopoll STATIC ${NOPOLL_SRC})
target_compile_options(nopoll PRIVATE -DNOPOLL_HAVE_SYSUTILS_ENABLED -DNOPOLL_HAVE_MBEDTLS_ENABLED)

# GenieService_Unittest
set(GENIE_SERVICE_SRC ${TOP_DIR}/src/core/GenieService.c)
add_executable(GenieService_Unittest ${CMAKE_SOURCE_DIR}/GenieService_Unittest.c ${GENIE_SERVICE_SRC})
target_link_libraries(GenieService_Unittest tmallgenie_protocol nopoll sysutils pthread ${MBEDTLS_LIBS})

file(COPY ${CMAKE_SOURCE_DIR}/test.wav DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
