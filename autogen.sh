#!/bin/sh

ACLOCAL=aclocal-1.11
AUTOMAKE=automake-1.11

$ACLOCAL -I ac \
  && libtoolize --force --copy \
  && autoheader \
  && $AUTOMAKE --add-missing --foreign --copy \
  && autoconf \
  && ./configure $@
