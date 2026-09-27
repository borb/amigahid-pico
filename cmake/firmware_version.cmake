find_package(Git QUIET)
set(revision "unknown")

if (GIT_FOUND AND EXISTS "${AMIGAHID_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --long --abbrev=7
        WORKING_DIRECTORY "${AMIGAHID_SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE git_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if (result EQUAL 0)
        set(revision "${git_revision}")
        # Board-design edits and local notes do not change the firmware.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" diff --quiet --ignore-submodules=untracked HEAD --
                CMakeLists.txt cmake src .gitmodules pico-sdk lib
            WORKING_DIRECTORY "${AMIGAHID_SOURCE_DIR}"
            RESULT_VARIABLE dirty
            ERROR_QUIET
        )
        if (NOT dirty EQUAL 0)
            string(APPEND revision "*")
        endif ()
    endif ()
endif ()

set(header "#pragma once\n#define AMIGAHID_FIRMWARE_VERSION \"${revision}\"\n")
if (EXISTS "${AMIGAHID_VERSION_HEADER}")
    file(READ "${AMIGAHID_VERSION_HEADER}" previous_header)
endif ()
if (NOT "${header}" STREQUAL "${previous_header}")
    get_filename_component(header_dir "${AMIGAHID_VERSION_HEADER}" DIRECTORY)
    file(MAKE_DIRECTORY "${header_dir}")
    file(WRITE "${AMIGAHID_VERSION_HEADER}" "${header}")
endif ()
