#!/bin/sh

export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/usr/local/include

./configure --prefix=$HOME/dist --with-localedir=lib/locale \
  --enable-oniguruma --enable-threads --disable-ipv6 \
  'CC=gcc -mtune=pentium3' CFLAGS=-O3 \
  && make \
  && make install-strip
