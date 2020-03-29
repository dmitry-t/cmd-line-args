cmake_minimum_required(VERSION 3.5)

project(CmdLineArgs)

include (CMakeDependentOption)

set(CMAKE_CXX_STANDARD 14)

add_library(cmd-line-args INTERFACE)

target_sources(cmd-line-args INTERFACE
    "${CMAKE_SOURCE_DIR}/cmd_line_args/cmd_line_args.h"
)

target_include_directories(cmd-line-args INTERFACE
    "${CMAKE_SOURCE_DIR}"
)

cmake_dependent_option(CMD_LINE_ARGS_DEV "Build sample" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

if(CMD_LINE_ARGS_DEV)
    add_custom_target(cmd-line-args-sources SOURCES
        cmd_line_args/cmd_line_args.h
        .clang-format
        LICENSE
    )

    source_group("cmd_line_args" FILES cmd_line_args/cmd_line_args.h)

    add_executable(cmd-line-args-sample
        sample/main.cpp
    )

    source_group("\\" FILES sample/main.cpp)

    target_link_libraries(cmd-line-args-sample
        cmd-line-args
    )
endif()
