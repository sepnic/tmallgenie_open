set(TOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(LITEPLAYER_DIR "${TOP_DIR}/thirdparty/liteplayer")

IF(CONFIG_IDF_TARGET STREQUAL "esp32")
    set(ESP_TARGET_CHIP "esp32")
ELSEIF(CONFIG_IDF_TARGET STREQUAL "esp32s3")
    set(ESP_TARGET_CHIP "esp32s3")
ENDIF()

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES nopoll sysutils speexogg)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_PRIV_INCLUDEDIRS ${TOP_DIR}/src ${LITEPLAYER_DIR}/adapter ${LITEPLAYER_DIR}/include)

set(COMPONENT_SRCS
    ${LITEPLAYER_DIR}/adapter/source_httpclient_wrapper.c
    ${LITEPLAYER_DIR}/adapter/source_file_wrapper.c
    ${TOP_DIR}/src/player/GenieUtpManager.c
    ${TOP_DIR}/src/player/GeniePlayer.c
    ${TOP_DIR}/src/player/vendorplayer/GenieVendorPlayer.c
    ${TOP_DIR}/src/recorder/GenieRecorder.c
    ${TOP_DIR}/src/GenieSdk.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -O3 -Wall -DGENIE_HAVE_SPEEXOGG_ENABLED)

target_link_libraries(${COMPONENT_TARGET} "-L ${TOP_DIR}/lib/${ESP_TARGET_CHIP} -L ${LITEPLAYER_DIR}/lib/${ESP_TARGET_CHIP}")

IF((IDF_VERSION_MAJOR GREATER 4) OR (IDF_VERSION_MAJOR EQUAL 4))
    add_prebuilt_library(tmallgenie_core "${TOP_DIR}/lib/${ESP_TARGET_CHIP}/libtmallgenie_core.a"
            PRIV_REQUIRES tmallgenie_open)
    add_prebuilt_library(liteplayer_core "${LITEPLAYER_DIR}/lib/${ESP_TARGET_CHIP}/libliteplayer_core.a"
            PRIV_REQUIRES tmallgenie_open)
ELSE()
    message(FATAL_ERROR "Only support IDF V4.4 and above, abort....")
    return()
ENDIF()

target_link_libraries(${COMPONENT_TARGET} "-Wl,--start-group" tmallgenie_core liteplayer_core "-Wl,--end-group")
