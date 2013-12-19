SUBDIRS = ac libsylph src plugin po manual faq nsis macosx

EXTRA_DIST = config.rpath  \
	ChangeLog.ja \
	INSTALL.ja \
	README.es \
	README.ja \
	TODO.ja \
	ChangeLog-1.0 \
	ChangeLog-1.0.ja \
	NEWS-1.0 \
	NEWS-2.0 \
	LICENSE \
	PLUGIN.txt \
	PLUGIN.ja.txt \
	sylpheed.desktop \
	sylpheed.spec \
	sylpheed.spec.in \
	sylpheed.png \
	sylpheed-128x128.png \
	sylpheed-64x64.png \
	autogen.sh \
	makeosx.sh \
	makewin32.sh \
	mime.types \
	sylpheed-mailto-protocol_admin.reg \
	sylpheed-mailto-protocol_user.reg

BZIP2_ENV =

release: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(AMTAR) chozf $(distdir).tar.gz $(distdir)
	rm -f $(distdir)/sylpheed.spec
	sed '/^Source:/s/\.tar\.gz$$/\.tar\.bz2/' sylpheed.spec \
	  > $(distdir)/sylpheed.spec
	BZIP2=$(BZIP2_ENV) $(AMTAR) --bzip2 -chof $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

#ACLOCAL_AMFLAGS = -I m4

install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps; \
	$(INSTALL_DATA) $(srcdir)/sylpheed-64x64.png $(DESTDIR)$(datadir)/pixmaps/sylpheed.png;

desktop_DATA = sylpheed.desktop
desktopdir = $(datadir)/applications
