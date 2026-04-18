cmake_minimum_required(VERSION 3.13..3.27)

set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/pico-sdk/")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# amigahid-pico build target; see src/config.h BOARD_* defines; default to rev4
if (NOT BOARD_TYPE)
	set(BOARD_TYPE BOARD_HIDPICO_REV4)
endif ()

# which chip/pico board version is being used; unfortunately this can't be driven by config.h so needs to be set here
if (NOT PICO_PLATFORM)
	set(PICO_PLATFORM rp2040)
endif ()
if (NOT PICO_BOARD)
	set(PICO_BOARD pico)
endif ()

# use sdk from git submodule
include(pico-sdk/pico_sdk_init.cmake)

project(amigahid-pico C CXX ASM)

add_compile_options(-Wall -Werror)

# configuration options

# uncomment for debug slurry
add_compile_definitions(DEBUG_MESSAGES=1)

# enable per-report mouse debug only when explicitly requested
option(DEBUG_MOUSE "Enable per-report mouse debug output" OFF)
if (DEBUG_MOUSE)
	add_compile_definitions(DEBUG_MOUSE=1)
endif ()

# enable per-key keyboard debug only when explicitly requested
option(DEBUG_KEYBOARD "Enable per-key keyboard debug output" OFF)
if (DEBUG_KEYBOARD)
	add_compile_definitions(DEBUG_KEYBOARD=1)
endif ()

# set the board revision (changes which pins are mapped to which ports)
add_compile_definitions(${BOARD_TYPE})

# debugging for tinyusb - be warned that it can cause timing issues causing things to break
# add_compile_definitions(CFG_TUSB_DEBUG=2)

pico_sdk_init()

add_subdirectory(src)
