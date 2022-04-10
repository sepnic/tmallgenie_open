set(TOPDIR ../../../..)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES pthread mbedtls)

set(COMPONENT_ADD_INCLUDEDIRS ${TOPDIR}/include)

set(COMPONENT_SRCS
    ${TOPDIR}/osal/esp8266/os_log.c
    ${TOPDIR}/osal/esp8266/os_memory.c
    ${TOPDIR}/osal/esp8266/os_thread.c
    ${TOPDIR}/osal/esp8266/os_time.c
    ${TOPDIR}/osal/esp8266/os_timer.c
    ${TOPDIR}/osal/esp8266/os_misc.c
    ${TOPDIR}/source/cutils/memdbg.c
    ${TOPDIR}/source/cutils/mlooper.c
    ${TOPDIR}/source/cutils/mqueue.c
    ${TOPDIR}/source/cutils/ringbuf.c
    ${TOPDIR}/source/cutils/swtimer.c
    ${TOPDIR}/source/cipher/sha2.c
    ${TOPDIR}/source/cipher/hmac_sha2.c
    ${TOPDIR}/source/cipher/md5.c
    ${TOPDIR}/source/cipher/base64.c
    ${TOPDIR}/source/httpclient/httpclient.c
)

register_component()

target_compile_options(${COMPONENT_LIB} PRIVATE -DOS_RTOS -DOS_FREERTOS_ESP8266 -DSYSUTILS_HAVE_MBEDTLS_ENABLED)
