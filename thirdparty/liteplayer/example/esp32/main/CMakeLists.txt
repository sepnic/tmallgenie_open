set(COMPONENT_SRCS app_main.c)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -O3 -Wall)
