#!/bin/sh

./configure --prefix=$HOME/gtk/inst \
  --with-localedir=Contents/Resources/share/locale \
  --with-themedir=Contents/Resources/share/icons \
  && make \
  && make install-strip \
  && (cd plugin/attachment_tool; make install-plugin)
