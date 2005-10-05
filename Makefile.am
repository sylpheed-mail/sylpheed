SUBDIRS = ac libsylph src po manual faq

EXTRA_DIST = config.rpath  \
	ChangeLog.ja \
	INSTALL.ja \
	README.ja \
	TODO.ja \
	ChangeLog-1.0 \
	ChangeLog-1.0.ja \
	NEWS-1.0 \
	NEWS-2.0 \
	LICENSE \
	sylpheed.desktop \
	sylpheed.spec \
	sylpheed.spec.in \
	sylpheed.png \
	sylpheed-128x128.png \
	sylpheed-64x64.png \
	autogen.sh \
	makewin32.sh

BZIP2_ENV =

release: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	rm -f $(distdir)/sylpheed.spec
	sed '/^Source:/s/\.tar\.gz$$/\.tar\.bz2/' sylpheed.spec \
	  > $(distdir)/sylpheed.spec
	BZIP2=$(BZIP2_ENV) $(TAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

ACLOCAL_AMFLAGS = -I m4
