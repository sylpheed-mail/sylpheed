dnl Autoconf macros for libgpgme
dnl $Id: gpgme.m4,v 1.5 2002/08/23 19:48:30 werner Exp $


dnl AM_PATH_GPGME([MINIMUM-VERSION,
dnl               [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]]])
dnl Test for libgpgme and define GPGME_CFLAGS and GPGME_LIBS
dnl
AC_DEFUN(AM_PATH_GPGME,
[ AC_ARG_WITH(gpgme-prefix,
            AC_HELP_STRING([--with-gpgme-prefix=PFX],
                           [prefix where GPGME is installed (optional)]),
     gpgme_config_prefix="$withval", gpgme_config_prefix="")
  if test x$gpgme_config_prefix != x ; then
     gpgme_config_args="$gpgme_config_args --prefix=$gpgme_config_prefix"
     if test x${GPGME_CONFIG+set} != xset ; then
        GPGME_CONFIG=$gpgme_config_prefix/bin/gpgme-config
     fi
  fi

  AC_PATH_PROG(GPGME_CONFIG, gpgme-config, no)
  min_gpgme_version=ifelse([$1], ,0.3.9,$1)
  AC_MSG_CHECKING(for GPGME - version >= $min_gpgme_version)
  ok=no
  if test "$GPGME_CONFIG" != "no" ; then
    req_major=`echo $min_gpgme_version | \
               sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\1/'`
    req_minor=`echo $min_gpgme_version | \
               sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\2/'`
    req_micro=`echo $min_gpgme_version | \
               sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\3/'`
    gpgme_config_version=`$GPGME_CONFIG $gpgme_config_args --version`
    major=`echo $gpgme_config_version | \
               sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    minor=`echo $gpgme_config_version | \
               sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    micro=`echo $gpgme_config_version | \
               sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`
    if test "$major" -gt "$req_major"; then
        ok=yes
    else 
        if test "$major" -eq "$req_major"; then
            if test "$minor" -gt "$req_minor"; then
               ok=yes
            else
               if test "$minor" -eq "$req_minor"; then
                   if test "$micro" -ge "$req_micro"; then
                     ok=yes
                   fi
               fi
            fi
        fi
    fi
  fi
  if test $ok = yes; then
    GPGME_CFLAGS=`$GPGME_CONFIG $gpgme_config_args --cflags`
    GPGME_LIBS=`$GPGME_CONFIG $gpgme_config_args --libs`
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])
  else
    GPGME_CFLAGS=""
    GPGME_LIBS=""
    AC_MSG_RESULT(no)
    ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GPGME_CFLAGS)
  AC_SUBST(GPGME_LIBS)
])

