#!/bin/sh

export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/usr/local/include

./configure --prefix=$HOME/dist \
  --with-localedir=share/locale \
  --with-themedir=share/icons \
  --enable-oniguruma --enable-threads \
  'CC=gcc -mtune=core2' CFLAGS=-O3 \
  && make \
  && make install-strip \
  && (cd plugin/attachment_tool; make install-plugin) \
  && strip $HOME/dist/lib/sylpheed/plugins/attachment_tool.dll
