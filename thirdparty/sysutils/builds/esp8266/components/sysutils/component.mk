#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/Makefile. By default,
# this will take the sources in the src/ directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

TOPDIR := ../../../..

COMPONENT_ADD_INCLUDEDIRS := \
    ${TOPDIR}/include

COMPONENT_SRCDIRS := \
    ${TOPDIR}/osal/esp8266 \
    ${TOPDIR}/source/cutils \
    ${TOPDIR}/source/cipher \
    ${TOPDIR}/source/httpclient

CFLAGS += -DOS_RTOS -DOS_FREERTOS_ESP8266 -DSYSUTILS_HAVE_MBEDTLS_ENABLED
