pico_generate_pio_header(amigahid-pico ${CMAKE_CURRENT_LIST_DIR}/keyboard.pio)

target_sources(amigahid-pico PRIVATE keyboard_serial_io.c quad_mouse.c)
