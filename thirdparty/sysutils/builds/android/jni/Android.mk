LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

TOP_DIR := ${LOCAL_PATH}/../../..

LOCAL_SRC_FILES := \
    ${TOP_DIR}/osal/unix/os_log.c \
    ${TOP_DIR}/osal/unix/os_memory.c \
    ${TOP_DIR}/osal/unix/os_thread.c \
    ${TOP_DIR}/osal/unix/os_time.c \
    ${TOP_DIR}/osal/unix/os_timer.c \
    ${TOP_DIR}/osal/unix/os_misc.c \
    ${TOP_DIR}/source/cutils/memdbg.c \
    ${TOP_DIR}/source/cutils/mlooper.c \
    ${TOP_DIR}/source/cutils/mqueue.c \
    ${TOP_DIR}/source/cutils/ringbuf.c \
    ${TOP_DIR}/source/cutils/lockfree_ringbuf.c \
    ${TOP_DIR}/source/cutils/swtimer.c \
    ${TOP_DIR}/source/cipher/sha2.c \
    ${TOP_DIR}/source/cipher/hmac_sha2.c \
    ${TOP_DIR}/source/cipher/md5.c \
    ${TOP_DIR}/source/cipher/base64.c \
    ${TOP_DIR}/source/cipher/aes.c \
    ${TOP_DIR}/source/httpclient/httpclient.c \
    ${TOP_DIR}/source/json/cJSON.c \
    ${TOP_DIR}/source/json/cJSON_Utils.c \

LOCAL_C_INCLUDES += ${TOP_DIR}/include

LOCAL_CFLAGS += -Wall -Werror -DOS_ANDROID

# SYSUTILS_HAVE_MBEDTLS_ENABLED
#LOCAL_CFLAGS += -DSYSUTILS_HAVE_MBEDTLS_ENABLED

LOCAL_CPPFLAGS += -Wall -Werror -DOS_ANDROID -std=c++11

LOCAL_LDLIBS := -llog

LOCAL_MODULE := libsysutils

include $(BUILD_SHARED_LIBRARY)
