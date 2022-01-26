# amigahid-pico

## introduction

amigahid-pico is a rewrite of the [amigahid](https://github.com/borb/amigahid) project to the rp2040 microcontroller, the raspberry pi pico in the short term, possibly something more bespoke if the complete project goals are reached.

**this is a work in progress.** if it builds, that's useful.

![alpha rev 1 pcb](./images/board-alpha-rev-1.png)

## why?

amigahid was written for the avr and specifically, the max3421e usb host controller attached to the avr. the rp2040 combines an on-the-go usb controller with a dual-core cortex m0 cpu, and independant state machines for gpio; in effect, hid output events can be farmed off to a small routine and the processor can go back to processing other input from additional devices. this opens the possibility of handling many different hid devices concurrently (keyboard, mouse, game controller, etc.) without devoting significant time to converting the output signal to the destination hardware requirements.

## what is the latency of this?

no idea.

## building

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

the resulting binaries will be in `build/src/` once built.

## installing on a pi pico (or similar) board

you have two options. swd or bootsel. swd will require usage of a raspberry pi (or pi pico with swd shim code installed), and you can flash the binary via openocd.

the easiest option is bootsel: take a powered down pico, hold the bootsel button and attach a micro usb cable. copy the `amigahid-pico.uf2` file from `build/src/` to the mounted usb storage volume. the pico will spontaneously disconnect, and when it comes back, it will be running the amigahid-pico code. disconnect usb and attach an otg adapter.

## how do i attach this to my amiga?

please be patient; phase one is keyboard and this will be a four-wire attachment (five including 5v).

the intention is to provide a board design which will simplify connection to the amiga, however the obstacle of how to handle game controller port connection has not been considered yet.

the amiga 600 and 1200 have differing keyboard connection arrangements to other amigas, in that the keyboard controller is mounted to the system board rather than a keyboard-local controller board. the 6502-alike mcu will need to be "tapped" and deactivated so that it does not interfere with keyboard communication.

joystick connection will either have to go to the db9 ports, or to the multiplexer input on the denise chip (alice on aga machines). buttons are fed into one of the cia chips. yes, that's right, a complete input replacement will require tendrils everywhere.

you'll need a level shifter at the moment. pick your favourite and try it. because of this ambiguity i can't give you detailed instructions of how to attach that shifter.

pico pins:

the intention here is to align to the amiga 500's keyboard header layout.

| pin# | signal     | meaning | notes |
|------|------------|---------|-------|
| 16   | amiga clk  | clock   | gp12  |
| 15   | amiga kdat | data    | gp11  |
| 14   | amiga res  | reset   | gp10  |

use any ground pin; the above pins will have to be fed into a level shifter so find a pin which is convenient. it's probably quite practical to use the same ground connection as the txs/mosfet shifter.

amiga 500 keyboard header:

if you don't have an amiga 500, this should map to an a1000/2000/3000/4000 keyboard connector. find your favourite pinout resource for this information.

all signals are active low.

| pin# | signal | meaning  | notes |
|------|--------|----------|-------|
| 1    | kclk   | clock    |       |
| 2    | kdat   | data     |       |
| 3    | /res   | reset    | issues a hard reset |
| 4    | +5v    | 5v power |       |
| 5    | nc     | nc       | not connected (often physically absent) |
| 6    | gnd    | ground   | though if powering arduino/avr from psu, may be able to use that ground line |
| 7    | pwr    | power    | provides power to keyboard power led to indicate amiga is on/audio filter status |
| 8    | drv    | drive    | indicates floppy drive activity |

7 and 8 are not connected to the pico in any way, so you may want to investigate another way of indicating floppy drive and power status.

## configuration

open [CMakeLists.txt](/CMakeLists.txt) and look for the configuration options. comment and uncomment as needed.

## roadmap

* tinyusb has some unfortunate stability issues on the rp2040/pico, not least limited to:
    * device removal/reinsertion (either or both are problematic)
    * timing-related stability (e.g. debug messages over uart can throw out usb response timing)
    * caps lock led via `tuh_hid_set_report()` plainly _does not work_ and in some cases actaully causes a stack panic
    * no functioning usb hub support, meaning unless you have a multiple endpoint single usb device (e.g. combined wireless keyboard and mouse) then at least in the short term, this will be single-device

* quadrature mouse emulation

* joystick emulation
    * remappable 'up' to support button jumping
    * cd32 pad support via serial shifter
    * amiga controller ports each have three bidirectional pins, opening up the potential for an amiga-side control panel through the controller port

* status display (ssd1306 or similar)

* investigation of keymap support (non us/gb layouts)

* amiga reset warning as per amiga developer cd 2.1

* finalise pcb design:
    * go double sided
    * ground plane to reduce signal noise
    * add controller pin mapping

* potential for non-hidbp mode
    * hidbp is limited to six concurrent keypresses - itself, not a major issue, but if this could be changed to full hid mode that would be ideal

* analogue sticks via opamp? i've never seen any amiga titles which support analogue sticks

* potential for non-amiga support? many systems used atari-style joysticks, though oddly never the pc

* gotek control? eliminate the need for rotary encoders, etc., and supplant the difficult to use microswitches

## license

on the fence at the moment, but the current license choice is Eclipse Public License 2.0 (EPL-2.0).

## whuh... who?

nine <[nine@aphlor.org](mailto:nine@aphlor.org)>

