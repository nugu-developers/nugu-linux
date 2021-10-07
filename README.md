# NUGU SDK for Linux

![Platform](https://img.shields.io/badge/platform-Linux-lightgrey) [![GitHub release](https://img.shields.io/github/v/release/nugu-developers/nugu-linux?sort=semver)](https://github.com/nugu-developers/nugu-linux/releases) [![License](https://img.shields.io/github/license/nugu-developers/nugu-linux)](https://github.com/nugu-developers/nugu-linux/blob/master/LICENSE) [![Build Status](https://github.com/nugu-developers/nugu-linux/workflows/Push/badge.svg)](https://github.com/nugu-developers/nugu-linux/actions/workflows/push.yaml)

NUGU SDK is Linux based client library which is possible to use various NUGU Service by connecting to NUGU Platform. It also provides components and APIs for developing user application.

## Requirements

### Hardware

- Audio input/output(microphone and speakers)

### CPU Architecture

- amd64 (64bit x86)
- armhf
- arm64

### Linux Distribution

- Ubuntu Xenial (16.04), Bionic (18.04), Focal (20.04)

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

NUGU SDK provides sample application which is possible to test simple NUGU service by CLI. Before executing sample application, you have to get access token which is needed to use NUGU Service. You can get access token and authorization info by OAuth.

- [Sample Guide](https://github.com/nugu-developers/nugu-linux/wiki/Samples)

## Contributing

This project welcomes contributions and suggestions. The best way to contribute is to create issues or pull requests right here on Github. Please see [CONTRIBUTING.md](CONTRIBUTING.md)

## License

The contents of this repository is licensed under the
[Apache License, version 2.0](http://www.apache.org/licenses/LICENSE-2.0).
