# installation

## configuration

open [CMakeLists.txt](/CMakeLists.txt) and look for the configuration options. comment and uncomment as needed.

## building the source code

prior to installation, ensure you have cmake, gnu make and gcc targetted to arm-none-eabi. you should be able to find these in debian. homebrew in macos has a cask for prebuilt gcc 10 binaries from arm. under windows, you're probably best off using wsl.

clone the repository. once cloned, you'll need to initialise the submodule for the pico sdk.

```shell
$ git submodule update --init --recursive
```

the tinyusb component of the pico-sdk has a lot of its own submodules so this could take some time.

when it's ready, start cmake off:

```shell
$ cmake -B build/ -S .
```

then build:

```shell
$ cd build && make
```

see next section for installing on a pico.

## installing on a pi pico (or similar) board

for the simplest option, pick usb. swd is good for remote debugging and reprogramming without detaching devices.

### usb (bootsel)

take a powered down pico, hold the bootsel button and attach a micro usb cable. copy the `amigahid-pico.uf2` file from `build/src/` to the mounted usb storage volume. the pico will spontaneously disconnect, and when it comes back, it will be running the amigahid-pico code. disconnect usb and attach an otg adapter.

### serial wire debug (swd)

swd uses openocd to program the flash memory attached to the rp2040.

to use swd to install the firmware, see chapter 5 of [getting started with raspberry pi pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf#%5B%7B%22num%22%3A21%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C115%2C841.89%2Cnull%5D).
