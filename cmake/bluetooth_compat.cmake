# Keep the SDK's upstream submodules intact. Build the Bluetooth dependencies
# from copies with the compatibility fixes required by this application.
find_package(Git REQUIRED)
set(AMIGAHID_COMPAT_PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}/patches")

function(amigahid_patch_dependency name source patch)
    set(destination "${CMAKE_BINARY_DIR}/bluetooth-compat/${name}")
    set(patch "${AMIGAHID_COMPAT_PATCH_DIR}/${patch}")
    file(MAKE_DIRECTORY "${destination}")

    if (name STREQUAL "btstack")
        set(directories src platform chipset 3rd-party tool)
        set(patched_files src/btstack_hid.h src/classic/hid_host.c)
    else ()
        set(directories src firmware)
        set(patched_files src/cyw43_ctrl.c)
    endif ()

    foreach(directory IN LISTS directories)
        file(COPY "${source}/${directory}" DESTINATION "${destination}")
    endforeach()
    # Restore just the patched files before applying, also on reconfiguration.
    foreach(path IN LISTS patched_files)
        configure_file("${source}/${path}" "${destination}/${path}" COPYONLY)
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --unsafe-paths "--directory=${destination}" "${patch}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error
    )
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot apply ${name} compatibility patch: ${error}")
    endif ()
    set(${name}_compat_path "${destination}" PARENT_SCOPE)
endfunction()

if (NOT PICO_BTSTACK_PATH)
    if (DEFINED ENV{PICO_BTSTACK_PATH})
        set(PICO_BTSTACK_PATH "$ENV{PICO_BTSTACK_PATH}")
    else ()
        set(PICO_BTSTACK_PATH "${PICO_SDK_PATH}/lib/btstack")
    endif ()
endif ()
if (NOT PICO_CYW43_DRIVER_PATH)
    set(PICO_CYW43_DRIVER_PATH "$ENV{PICO_CYW43_DRIVER_PATH}")
endif ()

amigahid_patch_dependency(btstack "${PICO_BTSTACK_PATH}" btstack-hid-boot-fallback.patch)
amigahid_patch_dependency(cyw43 "${PICO_CYW43_DRIVER_PATH}" cyw43-debug-format.patch)
set(PICO_BTSTACK_PATH "${btstack_compat_path}")
set(PICO_CYW43_DRIVER_PATH "${cyw43_compat_path}")
