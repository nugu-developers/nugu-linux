# OOB Setup example

Unfortunately, authentication information is only available through the smartphone(iOS, Android) application. (Only T-ID integration is supported and OAuth is not supported currently.)

This OOB(Out-of-Box) program is an example for exchanging authentication information with a sample companion application. It provides only basic functions such as device information and authentication information.

This program is just an example and is not implemented for security or other exceptions. So **DO NOT USE** this sample in production environment.

The `nugu_oob_server.py` python script provides `GET` and `PUT` REST APIs for companion applications via `http://IP:8080/device` and `http://IP:8080/auth` endpoints.

## Install

### Install libnugu-examples package

The nugu_oob program is included in the `libnugu-examples` debian package.

    apt install libnugu-examples

### Register systemd service

    systemctl enable nugu_oob

## Configure

### How to configure the device information

Please modify the `/var/lib/nugu-device.json` file. If this file does not exist, it is created automatically when the nugu_oob service starts.
