#!/bin/sh

./configure --with-localedir=share/locale 'CC=gcc -mcpu=pentium3' CFLAGS=-O3 \
  && make \
  && make install-strip prefix=$HOME/dist
