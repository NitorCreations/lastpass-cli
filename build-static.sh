#!/bin/bash

CURL_VERSION=7.47.0
LIBXML_VERSION=2.9.3
if ! [ -d curl-$CURL_VERSION ]; then
  wget -O - https://curl.haxx.se/download/curl-$CURL_VERSION.tar.gz | tar -xvzf -
fi
if ! [ -d libxml2-$LIBXML_VERSION ]; then
  wget -O - http://xmlsoft.org/sources/libxml2-$LIBXML_VERSION.tar.gz | tar -xvzf -
fi
(cd curl-$CURL_VERSION && ./configure --enable-static && make -j 8) &
(cd libxml2-$LIBXML_VERSION && ./configure --enable-static && make -j 8) &
wait
export CURL_VERSION LIBXML_VERSION
make -j 8
