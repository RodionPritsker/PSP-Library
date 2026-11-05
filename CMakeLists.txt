cmake_minimum_required(VERSION 3.11)

project(psplibrary C)

add_executable(${PROJECT_NAME}
    src/psp/main.c
    src/psp/ui.c
    src/core/core.c
    src/core/storage.c
    src/core/demo.c
)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_compile_options(${PROJECT_NAME} PRIVATE -O2 -G0 -Wall -Wextra -std=c99)
target_link_libraries(${PROJECT_NAME} PRIVATE
    pspdebug
    pspctrl
    pspdisplay
    pspge
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "PSP Library"
    VERSION 01.00
)
