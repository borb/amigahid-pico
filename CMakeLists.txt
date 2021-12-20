cmake_minimum_required(VERSION 3.13)

set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/pico-sdk/")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(cmake/pico_sdk_import.cmake)

# add_compile_options(-DCFG_TUSB_DEBUG=3)

project(amigahid-pico C CXX ASM)
pico_sdk_init()

add_subdirectory(src)
