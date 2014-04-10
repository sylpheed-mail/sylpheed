
AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"LibSylph\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS) $(LIBSYLPH_CFLAGS)

lib_LTLIBRARIES = libsylph-0.la
#noinst_LTLIBRARIES = libsylph.la

libsylph_0_la_SOURCES = \
	account.c \
	base64.c \
	codeconv.c \
	customheader.c \
	displayheader.c \
	filter.c \
	folder.c \
	html.c \
	imap.c \
	mbox.c \
	md5.c \
	md5_hmac.c \
	mh.c \
	news.c \
	nntp.c \
	pop.c \
	prefs.c \
	prefs_account.c \
	prefs_common.c \
	procheader.c \
	procmime.c \
	procmsg.c \
	quoted-printable.c \
	recv.c \
	session.c \
	smtp.c \
	socket.c \
	socks.c \
	ssl.c \
	ssl_hostname_validation.c \
	stringtable.c \
	sylmain.c \
	unmime.c \
	utils.c \
	uuencode.c \
	virtual.c \
	xml.c \
	syl-marshal.c

libsylph_0includedir=$(includedir)/sylpheed/sylph
libsylph_0include_HEADERS = \
	defs.h \
	enums.h \
	account.h \
	base64.h \
	codeconv.h \
	customheader.h \
	displayheader.h \
	filter.h \
	folder.h \
	html.h \
	imap.h \
	mbox.h \
	md5.h \
	md5_hmac.h \
	mh.h \
	news.h \
	nntp.h \
	pop.h \
	prefs.h \
	prefs_account.h \
	prefs_common.h \
	procheader.h \
	procmime.h \
	procmsg.h \
	quoted-printable.h \
	recv.h \
	session.h \
	smtp.h \
	socket.h \
	socks.h \
	ssl.h \
	ssl_hostname_validation.h \
	stringtable.h \
	sylmain.h \
	unmime.h \
	utils.h \
	uuencode.h \
	virtual.h \
	xml.h \
	syl-marshal.h

BUILT_SOURCES = \
	syl-marshal.c \
	syl-marshal.h

EXTRA_DIST = \
	syl-marshal.list \
	libsylph-0.def

if NATIVE_WIN32
no_undefined = -no-undefined
export_symbols = -export-symbols libsylph-0.def
endif

libsylph_0_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libsylph_0_la_LIBADD = $(GLIB_LIBS) $(LIBICONV) $(LIBSYLPH_LIBS)

syl-marshal.h: syl-marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=syl_marshal > $@

syl-marshal.c: syl-marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=syl_marshal > $@
