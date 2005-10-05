#!/bin/sh

./configure --with-localedir=share/locale \
  && make
  && make install-strip prefix=$HOME/dist
