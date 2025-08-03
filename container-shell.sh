#!/bin/sh
exec podman run -it --rm -v $(pwd):/project -w /project hid-pico-builder:0.1.0
