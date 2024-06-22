set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(THIRDPARTY_DIR ${TOP_DIR}/thirdparty)
set(ADAPTER_DIR ${TOP_DIR}/adapter)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils driver)

set(COMPONENT_ADD_INCLUDEDIRS ${ADAPTER_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(COMPONENT_PRIV_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_SRCS
    ${ADAPTER_DIR}/source_httpclient_wrapper.c
    ${ADAPTER_DIR}/source_file_wrapper.c
    sink_esp32_i2s_wrapper.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -O3 -Wall)
