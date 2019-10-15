# NUGU SDK for Linux

![Platform](https://img.shields.io/badge/platform-Linux-lightgrey) ![License](https://img.shields.io/badge/license-Apache%202.0-yellowgreen.svg) [![Build Status](https://travis-ci.org/nugu-developers/nugu-linux.svg?branch=master)](https://travis-ci.org/nugu-developers/nugu-linux)

NUGU SDK is Linux based client library which is possible to use various NUGU Service by connecting to NUGU Platform. It also provides components and APIs for developing user application.

## Requirements

### Hardware

- Audio input/output(microphone and speakers)

### CPU Architecture

- amd64 (64bit x86)
- armhf
- arm64

### Linux Distribution

- Ubuntu Xenial (16.04), Bionic (18.04)

### Third-party Packages

- Glib
- SSL
- zlib
- Alsa
- PortAudio
- Opus
- GStreamer

### Vendor Components

- KeyWord Detector(KWD) : detecting wakeup word
- EndPoint Detector(EPD) : recognizing speech start and end point

## Build

Before building NUGU SDK, you need to check whether your device and os are satisfied with above requirements. If satisfied, progress next guide step by step.

- [Build Guide](https://github.com/nugu-developers/nugu-linux/wiki/Build)

## Run

NUGU SDK provides sample application which is possible to test simple NUGU service by CLI. Before executing sample application, you have to get access token which is needed to use NUGU Service. You can get access token and authorization info by OAuth. Also, for using wakeup detection and speech recognizion, you have to get required asset files. (*It needs business partership.*)

- [Sample Guide](https://github.com/nugu-developers/nugu-linux/wiki/Samples)

## Contributing

This project welcomes contributions and suggestions. The best way to contribute is to create issues or pull requests right here on Github. Please see [Contributing.md](doc/contributing.md)

## License

The contents of this repository is licensed under the
[Apache License, version 2.0](http://www.apache.org/licenses/LICENSE-2.0).
