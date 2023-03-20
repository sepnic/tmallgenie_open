set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(ADAPTER_DIR ${TOP_DIR}/adapter)

IF(CONFIG_IDF_TARGET STREQUAL "esp32")
    set(ESP_TARGET_CHIP "esp32")
ELSEIF(CONFIG_IDF_TARGET STREQUAL "esp32s3")
    set(ESP_TARGET_CHIP "esp32s3")
ENDIF()

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils driver tone_partition)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include ${ADAPTER_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(COMPONENT_PRIV_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_SRCS
    ${ADAPTER_DIR}/source_httpclient_wrapper.c
    ${ADAPTER_DIR}/source_file_wrapper.c
    source_esp32_flashtone_wrapper.c
    sink_esp32_i2s_wrapper.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -O3 -Wall)

target_link_libraries(${COMPONENT_TARGET} "-L ${TOP_DIR}/lib/${ESP_TARGET_CHIP}")

IF(IDF_VER MATCHES "v4.")
    add_prebuilt_library(liteplayer_core "${TOP_DIR}/lib/${ESP_TARGET_CHIP}/libliteplayer_core.a"
            PRIV_REQUIRES liteplayer)
ENDIF()

target_link_libraries(${COMPONENT_TARGET} "-Wl,--start-group" liteplayer_core "-Wl,--end-group")

