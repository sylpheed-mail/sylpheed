#!/bin/sh

ACLOCAL=aclocal-1.15
AUTOMAKE=automake-1.15

$ACLOCAL -I ac \
  && libtoolize --force --copy \
  && autoheader \
  && $AUTOMAKE --add-missing --foreign --copy \
  && autoconf \
  && ./configure $@
