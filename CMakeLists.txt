cmake_minimum_required(VERSION 3.5)

project(CmdLineArgs)

include (CMakeDependentOption)

set(CMAKE_CXX_STANDARD 14)

add_library(cmd-line-args INTERFACE)

target_sources(cmd-line-args INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/over9000/cmd_line_args/parser.h"
)

target_include_directories(cmd-line-args INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

cmake_dependent_option(CMD_LINE_ARGS_DEV "Build tests and sample" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

if(CMD_LINE_ARGS_DEV)
    add_custom_target(cmd-line-args-sources SOURCES
        over9000/cmd_line_args/parser.h
        .clang-format
        LICENSE
    )

    source_group("cmd_line_args" FILES
        over9000/cmd_line_args/cmd_line_args.h
    )

    add_executable(cmd-line-args-sample
        sample/main.cpp
    )
    source_group("\\" FILES
        sample/main.cpp
    )
    target_link_libraries(cmd-line-args-sample
        cmd-line-args
    )

    set (gtest_force_shared_crt ON CACHE BOOL "Use /MD and /MDd" FORCE)
    add_subdirectory(third_party/googletest)

    add_executable(cmd-line-args-tests
        tests/main.cpp
        tests/tests.cpp
    )
    source_group("\\" FILES
        tests/main.cpp
        tests/tests.cpp
    )
    target_link_libraries(cmd-line-args-tests
        cmd-line-args
        gtest
    )
endif()
