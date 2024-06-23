set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../thirdparty/liteplayer)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include ${TOP_DIR}/adapter)

set(COMPONENT_PRIV_INCLUDEDIRS
    ${TOP_DIR}/thirdparty/codecs
    ${TOP_DIR}/thirdparty/codecs/pvmp3/include
    ${TOP_DIR}/thirdparty/codecs/pvmp3/src
    ${TOP_DIR}/thirdparty/codecs/pvaac
    ${TOP_DIR}/src
)

file(GLOB CODECS_SRCS src
    ${TOP_DIR}/thirdparty/codecs/pvmp3/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/pvaac/*.cpp
)

set(COMPONENT_SRCS
    ${CODECS_SRCS}
    ${TOP_DIR}/src/esp_adf/audio_element.c
    ${TOP_DIR}/src/esp_adf/audio_event_iface.c
    ${TOP_DIR}/src/audio_decoder/mp3_pvmp3_wrapper.c
    ${TOP_DIR}/src/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/src/audio_decoder/aac_pvaac_wrapper.c
    ${TOP_DIR}/src/audio_decoder/aac_decoder.c
    ${TOP_DIR}/src/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/src/audio_decoder/wav_decoder.c
    ${TOP_DIR}/src/audio_extractor/mp3_extractor.c
    ${TOP_DIR}/src/audio_extractor/aac_extractor.c
    ${TOP_DIR}/src/audio_extractor/m4a_extractor.c
    ${TOP_DIR}/src/audio_extractor/wav_extractor.c
    ${TOP_DIR}/src/liteplayer_adapter.c
    ${TOP_DIR}/src/liteplayer_source.c
    ${TOP_DIR}/src/liteplayer_parser.c
    ${TOP_DIR}/src/liteplayer_main.c
    ${TOP_DIR}/src/liteplayer_listplayer.c
    ${TOP_DIR}/src/liteplayer_ttsplayer.c
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c
    ${TOP_DIR}/adapter/source_file_wrapper.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall -Wno-error=narrowing
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE
    -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=
)
