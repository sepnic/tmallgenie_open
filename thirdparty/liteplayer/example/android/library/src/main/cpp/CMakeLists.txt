cmake_minimum_required(VERSION 3.4.1)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../../../../../..")
set(LIB_DIR "${TOP_DIR}/lib/android")

# cflags: compile paramters
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu99 -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror")

# mbedtls
file(GLOB MBEDTLS_SRC src ${TOP_DIR}/thirdparty/mbedtls/library/*.c)
add_library(mbedtls STATIC ${MBEDTLS_SRC})
target_compile_options(mbedtls PRIVATE -D_SOCKLEN_T)
target_include_directories(mbedtls PRIVATE ${TOP_DIR}/thirdparty/mbedtls/include)

# sysutils
set(SYSUTILS_SRC
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c
    ${TOP_DIR}/thirdparty/sysutils/source/httpclient/httpclient.c)
add_library(sysutils STATIC ${SYSUTILS_SRC})
target_compile_options(sysutils PRIVATE -DOS_ANDROID -DSYSUTILS_HAVE_MBEDTLS_ENABLED)
target_include_directories(sysutils PRIVATE
    ${TOP_DIR}/thirdparty/sysutils/include ${TOP_DIR}/thirdparty/mbedtls/include)

# liteplayer_adapter
set(LITEPLAYER_ADAPTER_SRC
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c
    ${TOP_DIR}/adapter/source_file_wrapper.c
    ${TOP_DIR}/adapter/sink_opensles_wrapper.cpp)
add_library(liteplayer_adapter STATIC ${LITEPLAYER_ADAPTER_SRC})
target_include_directories(liteplayer_adapter PRIVATE ${TOP_DIR}/thirdparty/sysutils/include ${TOP_DIR}/include)

# liteplayer_core
add_library(liteplayer_core STATIC IMPORTED)
set_target_properties(liteplayer_core PROPERTIES IMPORTED_LOCATION "${LIB_DIR}/${ANDROID_ABI}/libliteplayer_core.a")

# liteplayer-jni
add_library(liteplayer-jni SHARED liteplayer-jni.cpp)
target_include_directories(liteplayer-jni PRIVATE ${TOP_DIR}/include ${TOP_DIR}/adapter)
target_link_libraries(liteplayer-jni liteplayer_core liteplayer_adapter sysutils mbedtls android log)
