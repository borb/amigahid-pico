add_executable(amigahid-pico
  main.c
  hid_app.c
)

# make the src subdirectory availabe in the include path so tinyusb can find tusb_config.h
target_include_directories(amigahid-pico PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_uart(amigahid-pico 1)

pico_add_extra_outputs(amigahid-pico)

target_link_libraries(amigahid-pico pico_stdlib tinyusb_host tinyusb_board)
