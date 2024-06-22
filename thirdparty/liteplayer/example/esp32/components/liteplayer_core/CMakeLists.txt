set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(LITEPLAYER_DIR ${TOP_DIR}/src)
set(THIRDPARTY_DIR ${TOP_DIR}/thirdparty)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_PRIV_INCLUDEDIRS
    ${THIRDPARTY_DIR}/sysutils/include
    ${THIRDPARTY_DIR}/codecs
    ${THIRDPARTY_DIR}/codecs/pvmp3/include
    ${THIRDPARTY_DIR}/codecs/pvmp3/src
    ${THIRDPARTY_DIR}/codecs/pvaac
    ${LITEPLAYER_DIR}
)

file(GLOB CODECS_SRCS src
    ${THIRDPARTY_DIR}/codecs/pvmp3/src/*.cpp
    ${THIRDPARTY_DIR}/codecs/pvaac/*.cpp
)

set(COMPONENT_SRCS
    ${LITEPLAYER_DIR}/esp_adf/audio_element.c
    ${LITEPLAYER_DIR}/esp_adf/audio_event_iface.c
    ${LITEPLAYER_DIR}/audio_decoder/mp3_pvmp3_wrapper.c
    ${LITEPLAYER_DIR}/audio_decoder/mp3_decoder.c
    ${LITEPLAYER_DIR}/audio_decoder/aac_pvaac_wrapper.c
    ${LITEPLAYER_DIR}/audio_decoder/aac_decoder.c
    ${LITEPLAYER_DIR}/audio_decoder/m4a_decoder.c
    ${LITEPLAYER_DIR}/audio_decoder/wav_decoder.c
    ${LITEPLAYER_DIR}/audio_extractor/mp3_extractor.c
    ${LITEPLAYER_DIR}/audio_extractor/aac_extractor.c
    ${LITEPLAYER_DIR}/audio_extractor/m4a_extractor.c
    ${LITEPLAYER_DIR}/audio_extractor/wav_extractor.c
    ${LITEPLAYER_DIR}/liteplayer_adapter.c
    ${LITEPLAYER_DIR}/liteplayer_source.c
    ${LITEPLAYER_DIR}/liteplayer_parser.c
    ${LITEPLAYER_DIR}/liteplayer_main.c
    ${LITEPLAYER_DIR}/liteplayer_listplayer.c
    ${LITEPLAYER_DIR}/liteplayer_ttsplayer.c
    ${CODECS_SRCS}
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall -Wno-error=narrowing
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE
    -DOSCL_IMPORT_REF=
    -DOSCL_EXPORT_REF=
    -DOSCL_UNUSED_ARG=
)
