LOCAL_PATH := $(call my-dir)

## msgutils
include $(CLEAR_VARS)

TOP_DIR := ${LOCAL_PATH}/../..
WEBRTC_DIR := ${TOP_DIR}/thirdparty/webrtc

LOCAL_SRC_FILES := \
    ${WEBRTC_DIR}/src/signal_processing/energy.c \
    ${WEBRTC_DIR}/src/signal_processing/division_operations.c \
    ${WEBRTC_DIR}/src/signal_processing/get_scaling_square.c \
    ${WEBRTC_DIR}/src/signal_processing/resample.c \
    ${WEBRTC_DIR}/src/signal_processing/resample_48khz.c \
    ${WEBRTC_DIR}/src/signal_processing/resample_by_2.c \
    ${WEBRTC_DIR}/src/signal_processing/resample_by_2_internal.c \
    ${WEBRTC_DIR}/src/signal_processing/resample_fractional.c \
    ${WEBRTC_DIR}/src/vad/vad_core.c \
    ${WEBRTC_DIR}/src/vad/vad_filterbank.c \
    ${WEBRTC_DIR}/src/vad/vad_gmm.c \
    ${WEBRTC_DIR}/src/vad/vad_sp.c \
    ${WEBRTC_DIR}/src/vad/webrtc_vad.c \
    ${WEBRTC_DIR}/src/resampler/resampler.cc \
    ${TOP_DIR}/src/litevad.c

LOCAL_C_INCLUDES += ${WEBRTC_DIR}/inc ${TOP_DIR}/include

LOCAL_CFLAGS += -Wall -Werror

LOCAL_LDLIBS := -llog

LOCAL_MODULE := liblitevad

include $(BUILD_SHARED_LIBRARY)
