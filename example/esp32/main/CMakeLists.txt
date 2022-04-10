set(COMPONENT_SRCS ./GenieMain.c ./GenieAdapter.c ./GenieVoiceEngine.c)
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -O3 -Wall -Werror)
