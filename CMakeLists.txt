cmake_minimum_required(VERSION 3.14)
project(chttp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(chttp STATIC
    src/chttp_server.c
    src/chttp_parser.c
    src/chttp_response.c
    src/chttp_utils.c
)

target_include_directories(chttp
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(chttp PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wformat=2
    -fstack-protector-strong
)
