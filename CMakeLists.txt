cmake_minimum_required(VERSION 3.13)

include(cmake/pico_sdk_import.cmake)

project(amigahid-pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(src)
