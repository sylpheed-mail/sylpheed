#!/bin/sh

ACLOCAL=aclocal-1.17
AUTOMAKE=automake-1.17

$ACLOCAL -I ac \
  && libtoolize --force --copy \
  && autoheader \
  && $AUTOMAKE --add-missing --foreign --copy \
  && autoconf \
  && ./configure $@
