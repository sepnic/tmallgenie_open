set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../thirdparty/speex)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include)

file(GLOB COMPONENT_SRCS src ${TOP_DIR}/libspeex/*.c ${TOP_DIR}/libogg/*.c)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall -Werror
    -DFIXED_POINT -DUSE_KISS_FFT -DEXPORT= -DSPEEX_HAVE_SYSUTILS_ENABLED)
