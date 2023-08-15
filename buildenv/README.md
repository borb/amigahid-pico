# build container

## what is it?

this is a container used to create a reproducible build environment for hid-pico be built in, without relying on excessive numbers of packages being installed and kept working on the host environment. furthermore, the build environment will ensure that newer library versions in distributions do not inhibit the ability to build the source.

## usage

the file [Containerfile](./Containerfile) contains the build information. i recommend using `podman` to build, otherwise `docker` if you don't have `podman` available.

to build:

```shell
$ cd buildenv && podman build -t picobuild ./
```

else, using docker:

```shell
$ cd buildenv && docker build -f Containerfile -t picobuild ./
```

once built, from the source root:

```shell
$ podman run -it --rm -v $(pwd):/project picobuild
```

from here, the build instructions should be as per the [main readme file](../README.md).
