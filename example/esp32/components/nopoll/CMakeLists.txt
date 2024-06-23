set(TOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES mbedtls sysutils)

set(COMPONENT_PRIV_INCLUDEDIRS ${TOP_DIR}/thirdparty/nopoll/src)

set(COMPONENT_SRCS
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_conn_opts.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_decl.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_listener.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_loop.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_conn.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_ctx.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_io.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_log.c
    ${TOP_DIR}/thirdparty/nopoll/src/nopoll_msg.c
)

register_component()

MESSAGE(STATUS "IDF_VERSION_MAJOR: ${IDF_VERSION_MAJOR}")

target_compile_options(${COMPONENT_TARGET} PRIVATE
    -O3 -Wall
    -DNOPOLL_HAVE_MBEDTLS_ENABLED -DNOPOLL_HAVE_LWIP_ENABLED -DNOPOLL_HAVE_SYSUTILS_ENABLED
)
