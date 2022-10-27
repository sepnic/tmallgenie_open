set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../thirdparty/litevad)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_PRIV_INCLUDEDIRS ${TOP_DIR}/thirdparty/webrtc/inc)

set(COMPONENT_SRCS
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/energy.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/division_operations.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/get_scaling_square.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_48khz.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_by_2.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_by_2_internal.c
    ${TOP_DIR}/thirdparty/webrtc/src/signal_processing/resample_fractional.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_core.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_filterbank.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_gmm.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/vad_sp.c
    ${TOP_DIR}/thirdparty/webrtc/src/vad/webrtc_vad.c
    ${TOP_DIR}/thirdparty/webrtc/src/resampler/resampler.cc
    ${TOP_DIR}/src/litevad.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall -D__pnacl__ -DLITEVAD_HAVE_SYSUTILS_ENABLED)
