SUBDIRS = ac po intl src manual faq

EXTRA_DIST = config.rpath  \
	ChangeLog.jp \
	INSTALL.jp \
	README.jp \
	TODO.jp \
	sylpheed.desktop \
	sylpheed.spec \
	sylpheed.spec.in \
	sylpheed.png \
	sylpheed-128x128.png \
	sylpheed-64x64.png \
	autogen.sh

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
