
AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"LibSylph\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\"

INCLUDES = $(GLIB_CFLAGS) -I$(top_srcdir) -I$(includedir)

#lib_LTLIBRARIES = libsylph.la
noinst_LTLIBRARIES = libsylph.la

libsylph_la_SOURCES = \
	defs.h \
	enums.h \
	account.c account.h \
	base64.c base64.h \
	codeconv.c codeconv.h \
	customheader.c customheader.h \
	filter.c filter.h \
	folder.c folder.h \
	html.c html.h \
	imap.c imap.h \
	mbox.c mbox.h \
	md5.c md5.h \
	mh.c mh.h \
	news.c news.h \
	nntp.c nntp.h \
	pop.c pop.h \
	prefs.c prefs.h \
	prefs_account.c prefs_account.h \
	prefs_common.c prefs_common.h \
	procheader.c procheader.h \
	procmime.c procmime.h \
	procmsg.c procmsg.h \
	quoted-printable.c quoted-printable.h \
	recv.c recv.h \
	session.c session.h \
	smtp.c smtp.h \
	socket.c socket.h \
	ssl.c ssl.h \
	stringtable.c stringtable.h \
	unmime.c unmime.h \
	utils.c utils.h \
	uuencode.c uuencode.h \
	xml.c xml.h

libsylph_la_LDFLAGS =
libsylph_la_LIBADD = $(GLIB_LIBS)
