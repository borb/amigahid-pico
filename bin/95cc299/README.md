# build 95cc299

## why?

i've not worked in earnest on the project for a while; it's not abandoned, but i'm considerably busier than i was when the project started.

since the source was uploaded, cmake has moved on and the old build files are somewhat incompatible. they need a mild refresh but i haven't had a sufficient chance to do this yet.

it'll come; i'm sorry, mostly my free time comes after 10pm. i still love and believe in this project.

## what is this?

this is a compiled build of commit 95cc299, the most recent and likely stable functioning build, dated march 20 2024.

## what is inside?

* `rev4/amigahid-pico.uf2`: binary for uploading via usb mass storage to an rp2040
* `rev4/amigahid-pico.elf`: elf binary for programming over swd with openocd; has debug info so can be poked around with `gdb`

## future builds?

i'm aiming to push releases up automatically via ci. but this is not ready yet; timeline is: mouse hid, test stability of development, merge to main, tag release version and have binary builds go to releases.

note that this project is unlikely to stay on github, and will likely move to gitlab.
