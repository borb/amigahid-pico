# hardware

thoughts and justifications.

## raspberry pi pico

the raspberry pi pico is a miniature development board featuring the rp2040, a bootsel button (readable within software as an input), an led, a buck converter supplying 3.3v to the circuit from the 5v input, a micro usb port (suitable for otg usage), 2.54mm pitch through-hole pins and castellated edges and a flat, silkscreened underside suitable for surface-mount soldering to a pcb.

fundamentally, it provides all of the required features for amigahid-pico without making hand soldering difficult; if size is a consideration, then a custom design featuring the rp2040 may be required.

at present, the initial design considerations will be targetting two particular modes of usage:

1. amiga 500 - for the original case and for rehomed (e.g. checkmate a1500+ re-cased amiga 500 boards); intention is to have a design which can be mounted directly on top of the keyboard header (7-pin, 2.54mm pitch with a single pin gap between the left four and right three pins). since mouse support is a consideration for the first phase, a run-out for the mouse port will likely be used as opposed to "tapping" the cia and denise to interpose the mouse/joystick signals.

2. big-box amigas (and cd32) - a mouse-free design intended as a plain keyboard adapter for machines which typically have external keyboards. the power requirements should be modest enough to consume power from the amiga port (5v). the a2000-cr board design has no fuse and only a current limiting resistor on the 5v keyboard line, meaning 1A should be a feasible current from the port. if other models feature either a fuse or more modest current limits, then external power provision will be required, but this may be overcome with hardware modifications if this is a desirable method of powering the adapter. it's hoped that some form of board-on-board design could accomodate the pico, a txs0108, voltage regulator and whichever socket is required to pass through to the amiga and fit within a small piece of upvc tubing for housing the pcb sandwich.

### pio

the rp2040's eight pio units should provide sufficient features for offloading timing-restrictive features to separate units without impacting performance on the arm cortex-m cores. the keyboard serial routine uses a clock channel to synchronise transfer but strict timings on pauses between signals will need to be adhered to so that any communication retains compatibility with the usual communication between the amiga and the mos-6570 normally featured in the amiga keyboard circuit. by offloading to a pio core and state machine loop, strict sequenced and timed throughput can be maintained whilst the arm cores handle communication input from the usb host controller.

the same timing-sensitive features will be required for handling mouse signalling. at first, the intention is to implement this using the second arm core (core1) to abstract the difference between usb input and specifically-timed amiga output, and thus negate issues caused by the high-dpi output of modern mice and the per-frame counter-based pixel movement of the amiga. the hardware reference details how the standard amiga mouse operates at 200dpi, given this value, if a mouse moves at over 38 inches in a single second, the counter will wrap around and pointer movement will become erratic. given modern inexpensive usb mice have a typical dpi of 4800, then this distance is shrunk to an extremely modest 1.6 inches, meaning dpi abstraction within the 200dpi timings is vital in order to provide a suitable adaption. after this abstraction is stabilised, porting the mouse motion from rp2040.core1 to a pio unit will mean the second core is freed up for other computationally intensive work.

### pin usage

the current pin usage of amigahid-pico within the design of the pico pcb is as follows (each signal output is passed through a txs0108 lv i/o with a 3.3v reference, 5v reference to the amiga); items in bold will be routed to the amiga:

amiga    | pico       | pin# | pcb     | pin# | pico     | amiga
---------|------------|------|---------|------|----------|---------------
\-       | uart tx    | 1    | \<pcb\> | 40   | vbus     | \-
\-       | uart rx    | 2    | \<pcb\> | 39   | vsys     | \-
\-       | gnd        | 3    | \<pcb\> | 38   | gnd      | \-
\-       |            | 4    | \<pcb\> | 37   | 3.3v in  | \-
\-       |            | 5    | \<pcb\> | 36   | 3.3v out | \-
\-       | i2c0 data  | 6    | \<pcb\> | 35   | vref     | \-
\-       | i2c0 clock | 7    | \<pcb\> | 34   | **gp28** | mouse2 button3
\-       | gnd        | 8    | \<pcb\> | 33   | gnd      | \-
mouse vq | **gp6**    | 9    | \<pcb\> | 32   | **gp27** | mouse button3
mouse hq | **gp7**    | 10   | \<pcb\> | 31   | **gp26** | mouse button2
mouse v  | **gp8**    | 11   | \<pcb\> | 30   | run      | \-
mouse h  | **gp9**    | 12   | \<pcb\> | 29   | **gp22** | mouse button1
\-       | gnd        | 13   | \<pcb\> | 28   | gnd      | \-
kbreset  | **gp10**   | 14   | \<pcb\> | 27   | **gp21** | mouse2 vq
kbdata   | **gp11**   | 15   | \<pcb\> | 26   | **gp20** | mouse2 hq
kbclock  | **gp12**   | 16   | \<pcb\> | 25   | **gp19** | mouse2 v
\-       |            | 17   | \<pcb\> | 24   | **gp18** | mouse2 h
\-       | gnd        | 18   | \<pcb\> | 23   | gnd      | \-
\-       |            | 19   | \<pcb\> | 22   | **gp17** | mouse2 button1
\-       |            | 20   | \<pcb\> | 21   | **gp16** | mouse2 button2
