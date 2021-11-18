add_executable(amigahid-pico
	main.c
)

pico_enable_stdio_uart(amigahid-pico 1)

pico_add_extra_outputs(amigahid-pico)

target_link_libraries(amigahid-pico pico_stdlib tinyusb_host tinyusb_board)
