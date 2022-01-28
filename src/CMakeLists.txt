add_executable(amigahid-pico
  main.c
  usb_hid.c
)

add_subdirectory(platform)
add_subdirectory(util)

# make the src subdirectory availabe in the include path so tinyusb can find tusb_config.h
target_include_directories(amigahid-pico PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_uart(amigahid-pico 1)

target_link_libraries(amigahid-pico PUBLIC pico_stdlib hardware_pio tinyusb_host tinyusb_board)

pico_add_extra_outputs(amigahid-pico)

