set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../thirdparty/sysutils)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES pthread mbedtls)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_SRCS
    ${TOP_DIR}/osal/esp8266/os_log.c
    ${TOP_DIR}/osal/esp8266/os_memory.c
    ${TOP_DIR}/osal/esp8266/os_thread.c
    ${TOP_DIR}/osal/esp8266/os_time.c
    ${TOP_DIR}/osal/esp8266/os_timer.c
    ${TOP_DIR}/osal/esp8266/os_misc.c
    ${TOP_DIR}/source/cutils/memdbg.c
    ${TOP_DIR}/source/cutils/mlooper.c
    ${TOP_DIR}/source/cutils/mqueue.c
    ${TOP_DIR}/source/cutils/ringbuf.c
    ${TOP_DIR}/source/cutils/swtimer.c
    ${TOP_DIR}/source/httpclient/httpclient.c
    ${TOP_DIR}/source/json/cJSON.c
    ${TOP_DIR}/source/json/cJSON_Utils.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall -Werror -DOS_RTOS -DOS_FREERTOS_ESP32 -DSYSUTILS_HAVE_MBEDTLS_ENABLED)
