dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.50)
AC_INIT(src/main.c)
PACKAGE=sylpheed

dnl version number
MAJOR_VERSION=3
MINOR_VERSION=7
MICRO_VERSION=0
INTERFACE_AGE=0
BINARY_AGE=0
EXTRA_VERSION=
BUILD_REVISION=1185
VERSION=$MAJOR_VERSION.$MINOR_VERSION.$MICRO_VERSION$EXTRA_VERSION

dnl define if this is a development release
dnl DEVEL_VERSION=1

dnl set $target
AC_CANONICAL_SYSTEM

dnl
AM_INIT_AUTOMAKE($PACKAGE, $VERSION, no-define)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
dnl AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
dnl AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(EXTRA_VERSION)
AC_SUBST(BUILD_REVISION)

dnl development or release version
AC_SUBST(DEVEL_VERSION)
if test "x$DEVEL_VERSION" != "x"; then
  AC_DEFINE(DEVEL_VERSION, 1, [Define if this is a development release])
fi

dnl libtool versioning
LT_RELEASE=$MAJOR_VERSION.$MINOR_VERSION
dnl LT_CURRENT=`expr 1 + $MICRO_VERSION - $INTERFACE_AGE`
dnl LT_REVISION=$INTERFACE_AGE
dnl LT_AGE=`expr $BINARY_AGE - $INTERFACE_AGE`
LT_CURRENT=4
LT_REVISION=0
LT_AGE=3
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Specify a header configuration file
AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

dnl Checks for programs.
dnl AC_ARG_PROGRAM
AC_PROG_CC
AM_DISABLE_STATIC
AC_ISC_POSIX
AM_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CPP
dnl AC_PROG_RANLIB
AM_PROG_LEX
AC_PROG_YACC
AM_PROG_LIBTOOL

SYLPHEED_ACLOCAL_INCLUDE(ac)

native_win32=no
os_osx=no

case "$target" in
*-darwin*)
	os_osx=yes
	CFLAGS="$CFLAGS -no-cpp-precomp -fno-common"
	LIBSYLPH_CFLAGS="$LIBSYLPH_CFLAGS -xobjective-c"
	;;
*-*-mingw*)
	native_win32=yes
	CFLAGS="$CFLAGS -mms-bitfields -mwindows"
	LIBS="$LIBS"
	LIBSYLPH_LIBS="-lws2_32 -lwinmm"
	SYLPHEED_LIBS="-lole32 -Wl,-luuid"
	AC_CHECK_PROG(WINDRES, windres, windres)
	;;
esac

AC_SUBST(LIBSYLPH_CFLAGS)
AC_SUBST(LIBSYLPH_LIBS)

AM_CONDITIONAL(NATIVE_WIN32, test "$native_win32" = "yes")

dnl Checks for libraries.
AM_PATH_GLIB_2_0(2.8.0,, AC_MSG_ERROR(Test for GLib failed. See the 'INSTALL' for help.), gobject gmodule)

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

AM_PATH_GTK_2_0(2.8.0,, AC_MSG_ERROR(Test for Gtk failed. See the 'INSTALL' for help.))

dnl --disable-deprecated switch for GTK2 purification
AC_ARG_ENABLE(deprecated, [  --disable-deprecated    Disable deprecated GTK functions. ],
			  [GTK_CFLAGS="$GTK_CFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"], [])

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

if test "$os_osx" == yes; then
	AC_CHECK_LIB(gtkmacintegration, gtkosx_application_ready, [ ac_have_gtkmacintegration=yes ], [ ac_have_gtkmacintegration=no ])
	AC_MSG_CHECKING([if gtkmacintegration is available])
	AC_MSG_RESULT($ac_have_gtkmacintegration)
	if test "$ac_have_gtkmacintegration" = yes; then
		SYLPHEED_LIBS="$SYLPHEED_LIBS -lgtkmacintegration"
		AC_DEFINE(USE_GTKMACINTEGRATION, 1, Define if you use gtkmacintegration library.)
	fi
fi

AC_SUBST(SYLPHEED_LIBS)

dnl checks for iconv
AM_ICONV

AC_CHECK_FUNCS(bind_textdomain_codeset)

dnl for gettext
ALL_LINGUAS="be bg cs da de el es et eu fi fr gl he hr hu it ja ko lt nl pl pt_BR ro ru sk sl sr sv tr uk vi zh_CN zh_TW"
GETTEXT_PACKAGE=sylpheed
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Define text domain.])

AM_GLIB_GNU_GETTEXT
dnl AM_GNU_GETTEXT
dnl AC_CHECK_FUNC(gettext, AC_CHECK_LIB(intl, gettext))

if test "x$DATADIRNAME" != "x"; then
	localedir='${prefix}/${DATADIRNAME}/locale'
else
	localedir='${datadir}/locale'
fi
AC_ARG_WITH(localedir,
	[  --with-localedir=DIR    Locale directory],
	[localedir="$withval"])
AC_SUBST(localedir)

manualdir='${datadir}/${PACKAGE}/manual'
AC_ARG_WITH(manualdir,
	[  --with-manualdir=DIR    Manual directory],
	[manualdir="$withval"])
AC_SUBST(manualdir)

faqdir='${datadir}/${PACKAGE}/faq'
AC_ARG_WITH(faqdir,
	[  --with-faqdir=DIR       FAQ directory],
	[faqdir="$withval"])
AC_SUBST(faqdir)

plugindir='${exec_prefix}/lib/${PACKAGE}/plugins'
AC_ARG_WITH(plugindir,
	[  --with-plugindir=DIR    Plug-in directory],
	[plugindir="$withval"])
AC_SUBST(plugindir)

themedir='${datadir}/${PACKAGE}/icons'
AC_ARG_WITH(themedir,
	[  --with-themedir=DIR     Icon theme directory],
	[themedir="$withval"])
AC_SUBST(themedir)

dnl GPGME is used to support OpenPGP 
AC_ARG_ENABLE(gpgme,
	[  --disable-gpgme         Disable GnuPG support using GPGME],
	[ac_cv_enable_gpgme=$enableval], [ac_cv_enable_gpgme=yes])
AC_MSG_CHECKING([whether to use GPGME])
if test $ac_cv_enable_gpgme = yes; then
	AC_MSG_RESULT(yes)
	AM_PATH_GPGME(1.0.0, AC_DEFINE(USE_GPGME, 1, Define if you use GPGME to support OpenPGP.),
		      [use_gpgme=no
		       ac_cv_enable_gpgme=no])
	if test $ac_cv_enable_gpgme = yes; then
		AC_CHECK_LIB(gpgme, gpgme_io_writen, AC_DEFINE(HAVE_GPGME_IO_WRITEN, 1, Define if GPGME has gpgme_io_writen function.))
	fi
else
	AC_MSG_RESULT(no)
fi

dnl for JPilot support in addressbook
dnl no check for libraries; these are dynamically loaded
AC_ARG_ENABLE(jpilot,
	[  --enable-jpilot         Enable JPilot support [[default=no]]],
	[ac_cv_enable_jpilot=$enableval], [ac_cv_enable_jpilot=no])
AC_MSG_CHECKING([whether to use JPilot])
if test "$ac_cv_enable_jpilot" = yes; then
	AC_MSG_RESULT(yes)
	AC_CHECK_HEADERS(pi-args.h pi-appinfo.h pi-address.h,
			 [ AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.) ],
			 [ ac_cv_enable_jpilot=no ])
	if test "$ac_cv_enable_jpilot" = no; then
		AC_CHECK_HEADERS(libpisock/pi-args.h libpisock/pi-appinfo.h libpisock/pi-address.h,
				 [ ac_cv_enable_jpilot=yes
				   AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.) ])
	fi
	AC_MSG_CHECKING([whether jpilot is available])
	AC_MSG_RESULT($ac_cv_enable_jpilot)

	if test "$ac_cv_enable_jpilot" = yes; then
		LIBS="$LIBS -lpisock"
	fi
else
	AC_MSG_RESULT(no)
fi

dnl for LDAP support in addressbook
dnl no check for libraries; dynamically loaded
AC_ARG_ENABLE(ldap,
	[  --enable-ldap           Enable LDAP support [[default=no]]],
	[ac_cv_enable_ldap=$enableval], [ac_cv_enable_ldap=no])
AC_MSG_CHECKING([whether to use LDAP])
if test "$ac_cv_enable_ldap" = yes; then
	AC_MSG_RESULT(yes)

	dnl check for available libraries, and pull them in
	AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="$LDAP_LIBS -lresolv")
	AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
	AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
	AC_CHECK_LIB(lber, ber_get_tag, LDAP_LIBS="$LDAP_LIBS -llber",,
		     $LDAP_LIBS)

	dnl we need libpthread for sylpheed ldap,  until we find
	dnl a better way to handle ldap requests asynchronously...
	AC_CHECK_LIB(pthread, pthread_create, LDAP_LIBS="$LDAP_LIBS -lpthread")

	AC_CHECK_HEADERS(ldap.h lber.h pthread.h,
			 [ ac_cv_enable_ldap=yes ],
			 [ ac_cv_enable_ldap=no ])

	if test "$ac_cv_enable_ldap" = yes; then
		AC_CHECK_LIB(ldap, ldap_open,
			     [ ac_cv_enable_ldap=yes ],
			     [ ac_cv_enable_ldap=no ],
			     $LDAP_LIBS)
	fi

	AC_MSG_CHECKING([whether ldap library is available])
	AC_MSG_RESULT($ac_cv_enable_ldap)

	if test "$ac_cv_enable_ldap" = yes; then
		CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gthread-2.0`"
		LDAP_LIBS="$LDAP_LIBS -lldap `$PKG_CONFIG --libs gthread-2.0`"
		AC_DEFINE(USE_LDAP, 1, Define if you want LDAP support in addressbook.)
		AC_DEFINE(USE_THREADS, 1, Whether to use multithread or not)
		AC_SUBST(LDAP_LIBS)
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Check for OpenSSL
AC_ARG_ENABLE(ssl,
	[  --disable-ssl           Disable SSL support using OpenSSL],
	[ac_cv_enable_ssl=$enableval], [ac_cv_enable_ssl=yes])
AC_MSG_CHECKING([whether to use OpenSSL])
if test $ac_cv_enable_ssl = yes; then
	AC_MSG_RESULT(yes)
	dnl First check with pkg-config
	PKG_CHECK_MODULES(OPENSSL, openssl, [:], [:])
	if test "x$OPENSSL_LIBS" != "x"; then
		CFLAGS="$CFLAGS $OPENSSL_CFLAGS"
		LIBS="$LIBS $OPENSSL_LIBS"
		AC_MSG_CHECKING([if openssl is available])
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_SSL, 1, Define if you use OpenSSL to support SSL.)
	else
		dnl If pkg-config fails, run compile/link test.
		AC_MSG_CHECKING([if openssl is available])
		ac_save_LIBS="$LIBS"
		if test "$native_win32" = yes; then
			LIBS="$LIBS -lssl32 -leay32"
		else
			LIBS="$LIBS -lssl -lcrypto"
		fi
		AC_TRY_LINK([
#include <openssl/opensslv.h>
],		[ return OPENSSL_VERSION_NUMBER; ],
		[ AC_MSG_RESULT(yes)
		  AC_DEFINE(USE_SSL, 1, Define if you use OpenSSL to support SSL.) ],
		[ AC_MSG_RESULT(no)
		  LIBS="$ac_save_LIBS"
		  ac_cv_enable_ssl=no ])
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Check for X-Face support
AC_ARG_ENABLE(compface,
	[  --disable-compface      Do not use compface (X-Face)],
	[ac_cv_enable_compface=$enableval], [ac_cv_enable_compface=yes])
if test "$ac_cv_enable_compface" = yes; then
	AC_CHECK_LIB(compface, uncompface,,[ac_cv_enable_compface=no])
fi

dnl Check for GtkSpell support
AC_MSG_CHECKING([whether to use GtkSpell])
AC_ARG_ENABLE(gtkspell,
	[  --disable-gtkspell      Do not use GtkSpell in editor],
	[ac_cv_enable_gtkspell=$enableval], [ac_cv_enable_gtkspell=yes])
if test "$ac_cv_enable_gtkspell" = yes; then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([whether GtkSpell is available])
	if $PKG_CONFIG gtkspell-2.0 ; then
		AC_MSG_RESULT(yes)
		CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gtkspell-2.0`"
		LIBS="$LIBS `$PKG_CONFIG --libs gtkspell-2.0`"
		if $PKG_CONFIG --atleast-version 2.0.13 gtkspell-2.0 ; then
			AC_DEFINE(USE_ENCHANT, 1, Use Enchant with GtkSpell)
			LIBS="$LIBS `$PKG_CONFIG --libs enchant`"

			dnl Workaroud for zemberek crash
			if $PKG_CONFIG --atleast-version 1.4.2 enchant ; then
				CFLAGS="$CFLAGS `$PKG_CONFIG --cflags dbus-glib-1`"
				LIBS="$LIBS `$PKG_CONFIG --libs dbus-glib-1`"
			fi
		elif $PKG_CONFIG --atleast-version 2.0.12 gtkspell-2.0 ; then
			LIBS="$LIBS -laspell"
		fi
		AC_DEFINE(USE_GTKSPELL, 1, Use GtkSpell in editor)
	else
		AC_MSG_RESULT(no)
		ac_cv_enable_gtkspell=no
	fi
else
	AC_MSG_RESULT(no)		
fi

dnl Check for Oniguruma support in message filtering
AC_ARG_ENABLE(oniguruma,
	[  --enable-oniguruma      Use Oniguruma instead of system's regex [[default=no]]],
	[ac_cv_enable_oniguruma=$enableval], [ac_cv_enable_oniguruma=no])
AC_MSG_CHECKING([whether to use Oniguruma])
if test "$ac_cv_enable_oniguruma" = yes; then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([whether Oniguruma is available])
        dnl AC_CHECK_PROG(ONIG_CONFIG, onig-config, onig-config)
	ONIG_CONFIG=onig-config
	ONIG_VERSION=`$ONIG_CONFIG --version 2>/dev/null`
	if test "x$ONIG_VERSION" != "x"; then
		AC_MSG_RESULT(yes)
		CFLAGS="$CFLAGS `$ONIG_CONFIG --cflags`"
		LIBS="$LIBS `$ONIG_CONFIG --libs`"
		AC_DEFINE(USE_ONIGURUMA, 1, Define if you want to use Oniguruma.)
	else
		AC_MSG_RESULT(no)
		ac_cv_enable_oniguruma=no
	fi
else
	AC_MSG_RESULT(no)
fi

dnl check additional libraries
AC_CHECK_LIB(xpg4, setlocale)
AC_CHECK_LIB(resolv, res_init)
AC_CHECK_LIB(socket, bind)
AC_CHECK_LIB(nsl, gethostbyname)

dnl for GThread support
AC_ARG_ENABLE(threads,
	[  --disable-threads       Disable multithread support],
	[use_threads=$enableval], [use_threads=yes])

AC_MSG_CHECKING([whether to use threads])
if test x"$use_threads" = xyes ; then
	AC_MSG_RESULT(yes)
	if $PKG_CONFIG --exists gthread-2.0 ; then
		CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gthread-2.0`"
		LIBS="$LIBS `$PKG_CONFIG --libs gthread-2.0`"
		AC_DEFINE(USE_THREADS, 1, Whether to use multithread or not)
	else
		AC_MSG_ERROR([Sylpheed requires GThread from GLib to enable threading.])
	fi
else
	AC_MSG_RESULT(no)
fi

dnl check for IPv6 option
AC_ARG_ENABLE(ipv6,
	[  --disable-ipv6          Disable IPv6 support],
	[ac_cv_enable_ipv6=$enableval], [ac_cv_enable_ipv6=yes])

dnl automated checks for IPv6 support.
AC_MSG_CHECKING([whether to use IPv6])
if test x"$ac_cv_enable_ipv6" = xyes; then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([for IPv6 support])
	if test "$native_win32" = yes; then
		AC_CACHE_VAL(ac_cv_ipv6,[
			AC_TRY_COMPILE([#define INET6
					#include <sys/types.h>
					#include <winsock2.h>
					#include <ws2tcpip.h>],
				[int x = IPPROTO_IPV6; struct in6_addr a;],
				ac_cv_ipv6=yes, ac_cv_ipv6=no)
		])
	else
		AC_CACHE_VAL(ac_cv_ipv6,[
			AC_TRY_COMPILE([#define INET6
					#include <sys/types.h>
					#include <netinet/in.h>],
				[int x = IPPROTO_IPV6; struct in6_addr a;],
				ac_cv_ipv6=yes, ac_cv_ipv6=no)
		])
	fi
	AC_MSG_RESULT($ac_cv_ipv6)
	if test $ac_cv_ipv6 = yes; then
		AC_DEFINE(INET6, 1, Define if you want IPv6 support.)
	else
		AC_MSG_WARN(*** IPv6 will not be supported ***)
		ac_cv_enable_ipv6=no
	fi
else
	AC_MSG_RESULT(no)
fi

dnl for update check feature
AC_ARG_ENABLE(updatecheck,
	[  --disable-updatecheck   Disable update check feature],
	[ac_cv_enable_updatecheck=$enableval], [ac_cv_enable_updatecheck=yes])
if test x"$ac_cv_enable_updatecheck" = xyes ; then
	AC_DEFINE(USE_UPDATE_CHECK, 1, Define if you want update check feature.)

  dnl for update check feature (requires update check enabled)
  AC_ARG_ENABLE(updatecheckplugin,
	[  --disable-updatecheckplugin   Disable update check plugin feature],
	[ac_cv_enable_updatecheckplugin=$enableval], [ac_cv_enable_updatecheckplugin=yes])
  if test x"$ac_cv_enable_updatecheckplugin" = xyes ; then
	AC_DEFINE(USE_UPDATE_CHECK_PLUGIN, 1, Define if you want update check plugin feature.)
  fi
fi

dnl Check for d_type member in struct dirent
AC_MSG_CHECKING([whether struct dirent has d_type member])
AC_CACHE_VAL(ac_cv_dirent_d_type,[
	AC_TRY_COMPILE([#include <dirent.h>],
		       [struct dirent d; d.d_type = DT_REG;],
		       ac_cv_dirent_d_type=yes, ac_cv_dirent_d_type=no)
])
AC_MSG_RESULT($ac_cv_dirent_d_type)
if test $ac_cv_dirent_d_type = yes; then
	AC_DEFINE(HAVE_DIRENT_D_TYPE, 1,
		  Define if `struct dirent' has `d_type' member.)
fi

dnl Checks for liblockfile
AC_CHECK_HEADERS(lockfile.h)
AC_CHECK_LIB(lockfile, lockfile_create)


dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h unistd.h paths.h \
		 sys/param.h sys/utsname.h sys/select.h \
		 netdb.h regex.h sys/mman.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

GNUPG_CHECK_TYPEDEF(u32, HAVE_U32_TYPEDEF, Used to test for a u32 typedef)
AC_CHECK_SIZEOF(unsigned short, 2)
AC_CHECK_SIZEOF(unsigned int, 4)
AC_CHECK_SIZEOF(unsigned long, 4)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(off_t)

if test "x$ac_cv_sizeof_time_t" = "x8"; then
	AC_DEFINE(HAVE_64BIT_TIME_T, 1, Define if time_t is 64-bit)
fi

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_CHECK_FUNCS(gethostname mkdir mktime socket strstr strchr \
	       uname flock lockf inet_aton inet_addr \
	       fchmod truncate getuid regcomp mlock fsync)

AC_OUTPUT([
Makefile
sylpheed.spec
ac/Makefile
libsylph/Makefile
src/version.h
src/sylpheed.rc
src/Makefile
src/icons/Makefile
plugin/Makefile
plugin/attachment_tool/Makefile
plugin/test/Makefile
po/Makefile.in
faq/Makefile
faq/de/Makefile
faq/en/Makefile
faq/es/Makefile
faq/fr/Makefile
faq/it/Makefile
manual/Makefile
manual/en/Makefile
manual/ja/Makefile
nsis/Makefile
macosx/Makefile
macosx/bundle/Makefile
])

dnl Output the configuration summary
echo ""
echo "$PACKAGE $VERSION"
echo ""
echo "GnuPG         : $ac_cv_enable_gpgme"
echo "JPilot        : $ac_cv_enable_jpilot"
echo "LDAP          : $ac_cv_enable_ldap"
echo "OpenSSL       : $ac_cv_enable_ssl"
echo "iconv         : $am_cv_func_iconv"
echo "compface      : $ac_cv_enable_compface"
echo "IPv6          : $ac_cv_enable_ipv6"
echo "GtkSpell      : $ac_cv_enable_gtkspell"
echo "Oniguruma     : $ac_cv_enable_oniguruma"
echo "GThread       : $use_threads"
echo ""
echo "The binary will be installed in $prefix/bin"
echo ""
echo "Configure finished, type 'make' to build."
