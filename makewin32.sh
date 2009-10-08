#!/bin/sh

./configure --prefix=$HOME/dist --with-localedir=lib/locale \
  --enable-oniguruma --enable-threads --disable-ipv6 \
  'CC=gcc -mtune=pentium3' CFLAGS=-O3 \
  && make \
  && make install-strip
