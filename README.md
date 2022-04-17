# amigahid-pico

## **important stuff**

* current kicad files are for r4 pcb, pin mappings are not yet in the source tree! generate at your own risk. i am holding off putting those mappings in until i have received the next prototypes and built one for test. the usb test pad attachment is potentially risky and may not align, in spite of spending a while with a magnifying glass and calipers.

* usb hubs and hotplug are currently only functional using [this fork](https://github.com/Ryzee119/tinyusb/tree/multi-hub) of tinyusb; i have not switched the submodule over to it because i am holding out for it being integrated into tinyusb upstream. without this branch, joint keyboard and mouse support will only be usable with a combination which share a single usb transceiver/cable.

* **always read the [errata](#errata) section for the current pcb layout before deciding whether or not to build.**

## introduction

amigahid-pico is a rewrite of the [amigahid](https://github.com/borb/amigahid) project to the rp2040 microcontroller, the raspberry pi pico in the short term, possibly something more bespoke if the complete project goals are reached. at present, keyboard and mouse functionality are working and stable.

**this is a work in progress.** if it builds, that's useful.

![rev 4 pcb](./images/board-rev-4.jpg)

## why?

i like old computers. particularly the amiga. it was amazing when checkmate produced a modern update to the checkmate a1500 case, the a1500+ with mountings for a variety of amigas. a optional extra for the case was a mechanism for mounting the standard amiga keyboard in an external metal housing. given i had ordered a black case, having a cream and grey keyboard in a black housing seemed out of keeping with the aesthetic of this recased beast; i needed something which fit the appearance, and in this modern era, lots of keyboards which match the appearance are available.

so at first i looked to the avr range of chips, for which a popular usb choice is the max3421e usb host controller. unfortunately, using commodity boards (i.e. arduino and clones), available usb "shields" are in dwindling supply, and arduino's own usb-capable arduino adk has been discontinued. this version was named amigahid, and is mostly hampered by that lack of available usb support.

enter the pi pico. dual core. significantly smaller than the mega2560 and with software switchable usb otg, it was an ideal choice. except for the 3.3v signal levels. this board uses fets to handle voltage translation, though this may change to a suitable ic in future; the txs0108 and txb0108 were tested and found to be unsuitable for the amiga's power requirements.

because the hardware for this is capable of so much more, the project has been extended to support mouse emulation and soon hopefully controller support (see roadmap below).

the entire amiga range uses a 6502-derived keyboard mcu. it seemed fitting that this project stands as a replacement for that keyboard mcu and the connection side of things.

after the big-box conversion of one of my amiga 500s, i obtained another amiga 500 with a sigificantly and irreversibly damaged keyboard. with several of my peers in the amiga community seeking replacement keyboards, it seemed obvious that this project was needed now more than ever. wouldn't you like to use your amiga with a tactile, blue-switched mechanical keyboard? i can tell you that it's a lot nicer than the stock amiga 500 keyboard.

## what is the latency of this?

no idea.

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

you have two options. swd or bootsel. swd will require usage of a raspberry pi (or pi pico with swd shim code installed), and you can flash the binary via openocd.

the easiest option is bootsel: take a powered down pico, hold the bootsel button and attach a micro usb cable. copy the `amigahid-pico.uf2` file from `build/src/` to the mounted usb storage volume. the pico will spontaneously disconnect, and when it comes back, it will be running the amigahid-pico code. disconnect usb and attach an otg adapter.

## how do i attach this to my amiga?

the board is designed to be mounted atop the amiga 500 keyboard header. mount a strip of eight 2.54mm pitch sockets with sufficient clearance from the nearby custom chips (cia, mostly). the drill holes are m3 - use some nylon supports to support the board and stop it flexing.

for other amigas, you will need to refer to your model's keyboard socket. instead of a socket on the underside, mount 2.54mm pitch idc pin strip to the top of the board and use dupont connections to attach to the amiga in your favoured manner. most amigas except for the a1000 (which uses an rj11 connector) use some form of din plug. the signalling is identical across the entire range.

the amiga 600 and 1200 have a surface mounted keyboard mcu; until a design has been put together, you can tap kclk and kdat from the chip (use an upturned plcc socket). find the pin information at [amigapcb.org](https://www.amigapcb.org/). please be **really** careful attaching to these machines.

the pcb has a key for signals; only kclk, kdat, power and ground are required. reset is not required for big-box amigas, which use clock to signal reset.

### amiga 500 keyboard header

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

## roadmap

crazy talk:
* non-amiga support
* non-hidbp support
* analogue/paddle emulation
* fix some tinyusb issues (hotplug, timing-related instability)

done, former roadmap items:
* quadrature mouse emulation
* usb hub support

## errata

this project is still in the relatively early stages in terms of hardware design and i'm learning things like kicad as i go along. it's likely i'll make mistakes, so this section documents those mistakes.

### board revision 1

revision 1 was based around using the txs0108 bidirectional level shifter, which proved to be a mistake: whilst it worked for keyboard functionality, the voltage drop once connected to a controller port was so significant the amiga was held in reset. this would not have been an issue for big-box amigas as they have no dedicated reset line on the keyboard port, but it would have proved difficult for the amiga 500, 600 and 1200.

### board revision 2

revision 2 of the board was the first successful test. this will change in the near future as some quirks were discovered during assembly. the design was changed to be mosfet-centric using the bss138; this may change in future. long-term, i'd like to use an ic rather than a series of (at present) 17 mosfets, but i have not yet found a part which provides all of the desired features.

the pico is attached using castellated edges to pads on the surface. ensure the alignment is straight and flood the edges with solder, ensuring the solder makes contact with both the edge and the pad (check). buzz the attachment out with a multimeter to ensure it's worked and not shorted.

for the amiga 500, the keyboard connector can use 2.54mm pitch sockets on the seven pin header and attach directly to the motherboard, though for simplicity of placement you may want to use short dupont wires for attachment.

the programming & debug header provides serial wire debug and uart connections. you can program either via swdio or the usb port & bootsel button.

the four pin i<sup>2</sup>c header is suitable for the generally available ssd1306 displays to mount directly to, assuming the pin layout is identical. the display is not currently used but will be used in future versions.

attach a usb otg adapter to the pico's usb port. usb hubs are supported, but only at a single level. multiple mice and keyboards can be attached and used concurrently, allowing you to attach a keyboard for desk and one for remote usage (e.g. sofa).

the controller ports mapping will change after revision 2: the assumption was made that 2x5 pin idc connectors were wired the same way as d-sub 9-pin connectors, and this was wrong. the numerical wiring works 1-2-3-4-5-6-7-8-9 vs 1-6-2-7-3-8-4-9-5, making ribbon attachment difficult in revision 2. this will be changed in revision 3.

### board revision 3 (not in version control)

was a reworking of revision two but with the intention of being smaller and introducing a castellated edge for attaching to the usb test pads; this version was never put into version control and has the same idc numbering problem as revision 2.

### board revision 4

warning: don't build this version; it's just for testing.

sticking with the mosfet design, this version fixes the idc to d-sub pin wiring so a flat 1.27mm pitch can be used.

downside: there are vias beneath the pico's keepouts for the bootsel/ground/power test points. if your board manufacturer doesn't coat the vias with solder mask sufficiently then these could make contact with the test points. if this happens, either use some polyamide (kapton) tape or clear nail varnish to insulate the test points before mounting the pico.

## license

on the fence at the moment, but the current license choice is Eclipse Public License 2.0 (EPL-2.0).

## whuh... who?

nine <[nine@aphlor.org](mailto:nine@aphlor.org)>

