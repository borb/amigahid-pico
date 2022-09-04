cmake_minimum_required(VERSION 3.13)

set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/pico-sdk/")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(PICO_TINYUSB_PATH "${CMAKE_SOURCE_DIR}/tinyusb/")

include(cmake/pico_sdk_import.cmake)

# configuration options

# uncomment for debug slurry
add_compile_options(-DDEBUG_MESSAGES=1)

# set the board revision (changes which pins are mapped to which ports)
add_compile_options(-DHIDPICO_REVISION=4)

# debugging for tinyusb - be warned that it can cause timing issues causing things to break
# add_compile_options(-DCFG_TUSB_DEBUG=2)

project(amigahid-pico C CXX ASM)
pico_sdk_init()

add_subdirectory(src)
