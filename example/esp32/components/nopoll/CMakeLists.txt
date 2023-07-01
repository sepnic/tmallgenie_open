set(TOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(NOPOLL_DIR "${TOP_DIR}/thirdparty/nopoll")
set(TMALLGENIE_DIR "${TOP_DIR}/src")

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES mbedtls sysutils)

set(COMPONENT_PRIV_INCLUDEDIRS ${NOPOLL_DIR}/src)

set(COMPONENT_SRCS
    ${NOPOLL_DIR}/src/nopoll.c
    ${NOPOLL_DIR}/src/nopoll_conn_opts.c
    ${NOPOLL_DIR}/src/nopoll_decl.c
    ${NOPOLL_DIR}/src/nopoll_listener.c
    ${NOPOLL_DIR}/src/nopoll_loop.c
    ${NOPOLL_DIR}/src/nopoll_conn.c
    ${NOPOLL_DIR}/src/nopoll_ctx.c
    ${NOPOLL_DIR}/src/nopoll_io.c
    ${NOPOLL_DIR}/src/nopoll_log.c
    ${NOPOLL_DIR}/src/nopoll_msg.c
    ${TMALLGENIE_DIR}/base/websocket_client.c
)

register_component()

MESSAGE(STATUS "IDF_VERSION_MAJOR: ${IDF_VERSION_MAJOR}")

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall
    -DNOPOLL_HAVE_MBEDTLS_ENABLED -DNOPOLL_HAVE_LWIP_ENABLED -DNOPOLL_HAVE_SYSUTILS_ENABLED
)
