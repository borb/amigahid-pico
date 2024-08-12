cmake_minimum_required(VERSION 3.13)

set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/pico-sdk/")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(PICO_TINYUSB_PATH "${CMAKE_SOURCE_DIR}/tinyusb/")

include(cmake/pico_sdk_import.cmake)

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
add_compile_definitions(HIDPICO_REVISION=4)

# debugging for tinyusb - be warned that it can cause timing issues causing things to break
# add_compile_definitions(CFG_TUSB_DEBUG=2)

project(amigahid-pico C CXX ASM)

pico_sdk_init()

add_subdirectory(src)
