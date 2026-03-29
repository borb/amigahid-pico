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

execute_process(COMMAND sh -c "git log --pretty=format:%h -n 1"
                OUTPUT_VARIABLE GIT_REVISION
                ERROR_QUIET)
string(STRIP "${GIT_REVISION}" GIT_REVIS)

execute_process(COMMAND sh -c "git diff --quiet --exit-code || echo -dirty"
                OUTPUT_VARIABLE GIT_DIRTY
                ERROR_QUIET)
string(STRIP "${GIT_DIRTY}" GIT_DIRTY)

execute_process(COMMAND git rev-parse --abbrev-ref HEAD
                OUTPUT_VARIABLE GIT_BRANCH
                ERROR_QUIET)
string(STRIP "${GIT_BRANCH}" GIT_BRANCH)

set(_HP_VERSION "#define _HP_VERSION \"${GIT_REVISION}${GIT_DIRTY} (${GIT_BRANCH})\"")
file(WRITE "${CMAKE_SOURCE_DIR}/src/gen_version.h" "${_HP_VERSION}\n")

# configuration options

# set the board revision (changes which pins are mapped to which ports)
add_compile_definitions(${BOARD_TYPE})

# debugging for tinyusb - be warned that it can cause timing issues causing things to break
# add_compile_definitions(CFG_TUSB_DEBUG=2)

pico_sdk_init()

add_subdirectory(src)
