cmake_minimum_required(VERSION 3.4.1)
project(sysutils_test)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")

# include files
include_directories(${TOP_DIR}/include)

# source files
set (LIBS_SRC
    ${TOP_DIR}/osal/unix/os_log.c
    ${TOP_DIR}/osal/unix/os_memory.c
    ${TOP_DIR}/osal/unix/os_thread.c
    ${TOP_DIR}/osal/unix/os_time.c
    ${TOP_DIR}/osal/unix/os_timer.c
    ${TOP_DIR}/osal/unix/os_misc.c
    ${TOP_DIR}/source/cutils/memdbg.c
    ${TOP_DIR}/source/cutils/mlooper.c
    ${TOP_DIR}/source/cutils/mqueue.c
    ${TOP_DIR}/source/cutils/ringbuf.c
    ${TOP_DIR}/source/cutils/lockfree_ringbuf.c
    ${TOP_DIR}/source/cutils/swtimer.c
    ${TOP_DIR}/source/cipher/sha2.c
    ${TOP_DIR}/source/cipher/hmac_sha2.c
    ${TOP_DIR}/source/cipher/md5.c
    ${TOP_DIR}/source/cipher/base64.c
    ${TOP_DIR}/source/cipher/aes.c
    ${TOP_DIR}/source/httpclient/httpclient.c
    ${TOP_DIR}/source/json/cJSON.c
    ${TOP_DIR}/source/json/cJSON_Utils.c
)

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Werror -DOS_APPLE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -DOS_APPLE")

# SYSUTILS_HAVE_MBEDTLS_ENABLED
#set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS}   -DSYSUTILS_HAVE_MBEDTLS_ENABLED")

# SYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DSYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED")
# SYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DSYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSUTILS_HAVE_MEMORY_OVERFLOW_DETECT_ENABLED")

# sysutils lib
add_library(sysutils   SHARED ${LIBS_SRC})
add_library(sysutils_s STATIC ${LIBS_SRC})

# timer test
add_executable(timer_test ${CMAKE_SOURCE_DIR}/timer_test.c)
target_link_libraries(timer_test sysutils pthread)

# memdbg test
add_executable(memdbg_test ${CMAKE_SOURCE_DIR}/memdbg_test.cpp)
target_link_libraries(memdbg_test sysutils pthread)

# mqueue test
add_executable(mqueue_test ${CMAKE_SOURCE_DIR}/mqueue_test.c)
target_link_libraries(mqueue_test sysutils pthread)

# mlooper test
add_executable(mlooper_test ${CMAKE_SOURCE_DIR}/mlooper_test.c)
target_link_libraries(mlooper_test sysutils pthread)
