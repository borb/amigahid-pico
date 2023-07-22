#!/bin/sh
exec openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000; program build/src/amigahid-pico.elf verify reset exit" $@
