#!/bin/sh

./configure --with-localedir=share/locale --enable-oniguruma --disable-ipv6 \
  'CC=gcc -mtune=pentium3' CFLAGS=-O3 \
  && make \
  && make install-strip prefix=$HOME/dist
