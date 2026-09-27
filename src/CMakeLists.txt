add_executable(amigahid-pico
  input_bridge.c
  main.c
  usb_hid.c
)

# Refresh the commit ID even when an incremental build does not rerun CMake.
# The script leaves the header untouched when its contents have not changed.
add_custom_target(firmware_version
  COMMAND "${CMAKE_COMMAND}"
    "-DAMIGAHID_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DAMIGAHID_VERSION_HEADER=${CMAKE_CURRENT_BINARY_DIR}/generated/firmware_version.h"
    -P "${CMAKE_SOURCE_DIR}/cmake/firmware_version.cmake"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/generated/firmware_version.h"
  VERBATIM
)
add_dependencies(amigahid-pico firmware_version)
target_include_directories(amigahid-pico PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

if (ENABLE_BLUETOOTH_HID)
  target_sources(amigahid-pico PRIVATE
    bt_hid.c
  )
endif ()

add_subdirectory(platform)
add_subdirectory(util)
add_subdirectory(display)

# make the src subdirectory availabe in the include path so tinyusb can find tusb_config.h
target_include_directories(amigahid-pico PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_uart(amigahid-pico 1)

target_link_libraries(amigahid-pico PUBLIC
  pico_stdlib
  pico_util
  pico_multicore
  hardware_dma
  hardware_gpio
  hardware_i2c
  hardware_pio
  tinyusb_host
  tinyusb_board
)

if (ENABLE_BLUETOOTH_HID)
  target_compile_options(amigahid-pico PRIVATE
    -Wno-error=maybe-uninitialized
  )
  target_link_libraries(amigahid-pico PUBLIC
    pico_cyw43_arch_none
    pico_btstack_ble
    pico_btstack_classic
    pico_btstack_cyw43
  )
endif ()

if (ENABLE_BLUETOOTH_HID AND COMMAND suppress_btstack_warnings)
  suppress_btstack_warnings()
endif ()

add_compile_definitions(PICO_USE_MALLOC_MUTEX=0)

pico_add_extra_outputs(amigahid-pico)
