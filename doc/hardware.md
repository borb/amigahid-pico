# hardware

## how do i attach this to my amiga?

### a500

the standard board is designed to be mounted atop the amiga 500 keyboard header. mount a strip of eight 2.54mm pitch sockets with sufficient clearance from the nearby custom chips (cia, mostly). the drill holes are m3 - use some nylon supports to support the board and stop it flexing.

for other amigas, you will need to refer to your model's keyboard socket. instead of a socket on the underside, mount 2.54mm pitch idc pin strip to the top of the board and use dupont connections to attach to the amiga in your favoured manner. most amigas except for the a1000 (which uses an rj11 connector) use some form of din plug. the signalling is identical across the entire range.

the pcb has a key for signals; only kclk, kdat, power and ground are required. reset is not required for big-box amigas, which use clock to signal reset.

### a600/a1200

the amiga 600 and 1200 have a surface mounted keyboard mcu; until a design has been put together, you can tap kclk, kdat and reset from the chip (use an upturned plcc socket). find the pin information at [amigapcb.org](https://www.amigapcb.org/). please be **really** careful attaching to these machines.

attach these lines to the standard amigahid-pico header in the correct position.

### a1000/a2000/a3000/a4000/cdtv/cd32

the din socket (rj11 on the a1000) for these models should map the +5v, kclk and kdat. these models do not have a reset signal, instead they rely on holding kclk low for a period to signal the hard reset signal to the amiga.

map these pins to the corresponding signals on the a500 keyboard header on the amigahid-pico pcb.

## amiga 500 keyboard header

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

## what about controllers?

please see the silkscreen for the short-term connection notes. the wiring guide is detailed there for simplicity.

using 2.54mm pitch idc pin plug (female type), insert 9 strands of 1.25mm pitch ribbon cable, aligning to pin one of the connector (usually with a red stripe on the ribbon), pressing with a vise or suitable ribbon cabling tool. using a ribbon-type d-sub, align pin 1 of the ribbon to pin 1 of the d-sub, and complete the attachment using the vise/ribbon cabling tool.

whilst the idc header has an interleave meaning the ribbon is ordered incrementally (1-2-3-4-5-6-7-8-9), the d-sub ribbon ordering goes 1-6-2-7-3-8-4-9-5; this is compensated for by the board design.

if you have a r2 pcb, i'm very sorry, you'll need to read the silkscreen and build a custom attachment, likely from dupont pins. i would strongly suggest switching to r4 or later.
