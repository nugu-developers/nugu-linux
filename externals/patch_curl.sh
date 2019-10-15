#!/bin/sh

patch -N --silent -p1 < ../0001_curl_find_nghttp2.patch
patch -N --silent -p1 < ../0002_curl_have_poll_h.patch
patch -N --silent -p1 < ../0003_curl_ca_variables.patch

echo "Patched!"
