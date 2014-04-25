#!/bin/sh

export PATH=$PATH:/target/bin
export LIBRARY_PATH=$LIBRARY_PATH:/target/lib:/usr/local/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/target/include:/usr/local/include

(

./configure --prefix=$HOME/dist \
  --with-localedir=share/locale \
  --with-themedir=share/icons \
  --enable-oniguruma --enable-threads \
  'CC=gcc -mthreads -mtune=core2 -static-libgcc' CFLAGS=-O3 \
  && make \
  && make install-strip \
  && (cd plugin/attachment_tool; make install-plugin) \
  && strip $HOME/dist/lib/sylpheed/plugins/attachment_tool.dll

) 2>&1 | tee sylpheed-build.log
