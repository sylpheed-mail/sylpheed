#!/bin/sh

aclocal -I ac \
  && libtoolize --force --copy \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf \
  && ./configure $@
