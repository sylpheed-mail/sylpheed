
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"LibSylph\"

INCLUDES = $(GLIB_CFLAGS) -I$(top_srcdir) -I$(includedir)

#lib_LTLIBRARIES = libsylph.la
noinst_LTLIBRARIES = libsylph.la

libsylph_la_SOURCES = \
	defs.h \
	base64.c base64.h \
	codeconv.c codeconv.h \
	html.c html.h \
	prefs.c prefs.h \
	quoted-printable.c quoted-printable.h \
	session.c session.h \
	socket.c socket.h \
	ssl.c ssl.h \
	stringtable.c stringtable.h \
	unmime.c unmime.h \
	utils.c utils.h \
	uuencode.c uuencode.h \
	xml.c xml.h

libsylph_la_LDFLAGS =
libsylph_la_LIBADD = $(GLIB_LIBS)
