set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(SYSUTILS_DIR ${TOP_DIR}/thirdparty/sysutils)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES pthread mbedtls)

set(COMPONENT_ADD_INCLUDEDIRS ${SYSUTILS_DIR}/include)

set(COMPONENT_SRCS
    ${SYSUTILS_DIR}/osal/esp8266/os_log.c
    ${SYSUTILS_DIR}/osal/esp8266/os_memory.c
    ${SYSUTILS_DIR}/osal/esp8266/os_thread.c
    ${SYSUTILS_DIR}/osal/esp8266/os_time.c
    ${SYSUTILS_DIR}/osal/esp8266/os_timer.c
    ${SYSUTILS_DIR}/osal/esp8266/os_misc.c
    ${SYSUTILS_DIR}/source/cutils/memdbg.c
    ${SYSUTILS_DIR}/source/cutils/mlooper.c
    ${SYSUTILS_DIR}/source/cutils/mqueue.c
    ${SYSUTILS_DIR}/source/cutils/ringbuf.c
    ${SYSUTILS_DIR}/source/cutils/swtimer.c
    ${SYSUTILS_DIR}/source/httpclient/httpclient.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall -Werror -DOS_RTOS -DOS_FREERTOS_ESP32 -DSYSUTILS_HAVE_MBEDTLS_ENABLED)
