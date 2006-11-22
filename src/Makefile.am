SUBDIRS = icons

bin_PROGRAMS = sylpheed

sylpheed_SOURCES = \
	version.h \
	main.c main.h \
	mainwindow.c mainwindow.h \
	folderview.c folderview.h \
	summaryview.c summaryview.h \
	messageview.c messageview.h \
	headerview.c headerview.h \
	textview.c textview.h \
	imageview.c imageview.h \
	mimeview.c mimeview.h \
	query_search.c query_search.h \
	message_search.c message_search.h \
	colorlabel.c colorlabel.h \
	action.c action.h \
	compose.c compose.h \
	gtkshruler.c gtkshruler.h \
	menu.c menu.h \
	stock_pixmap.c stock_pixmap.h \
	prefs_ui.c prefs_ui.h \
	prefs_common_dialog.c prefs_common_dialog.h \
	prefs_filter.c prefs_filter.h \
	prefs_filter_edit.c prefs_filter_edit.h \
	prefs_account_dialog.c prefs_account_dialog.h \
	prefs_folder_item.c prefs_folder_item.h \
	prefs_display_header.c prefs_display_header.h \
	prefs_customheader.c prefs_customheader.h \
	prefs_summary_column.c prefs_summary_column.h \
	prefs_template.c prefs_template.h \
	prefs_actions.c prefs_actions.h \
	prefs_search_folder.c prefs_search_folder.h \
	account_dialog.c account_dialog.h \
	template.c template.h \
	addressbook.c addressbook.h \
	addr_compl.c addr_compl.h \
	addressitem.h \
	addritem.c addritem.h \
	addrcache.c addrcache.h \
	addrbook.c addrbook.h \
	addrindex.c addrindex.h \
	mgutils.c mgutils.h \
	vcard.c vcard.h \
	ldif.c ldif.h \
	importldif.c importldif.h \
	jpilot.c jpilot.h \
	syldap.c syldap.h \
	editbook.c editbook.h \
	editgroup.c editgroup.h \
	editaddress.c editaddress.h \
	editvcard.c editvcard.h \
	editjpilot.c editjpilot.h \
	editldap.c editldap.h \
	editldap_basedn.c editldap_basedn.h \
	addressadd.c addressadd.h \
	filesel.c filesel.h \
	foldersel.c foldersel.h \
	statusbar.c statusbar.h \
	logwindow.c logwindow.h \
	sourcewindow.c sourcewindow.h \
	manage_window.c manage_window.h \
	undo.c undo.h \
	alertpanel.c alertpanel.h \
	inputdialog.c inputdialog.h \
	progressdialog.c progressdialog.h \
	subscribedialog.c subscribedialog.h \
	about.c about.h \
	setup.c setup.h \
	gtkutils.c gtkutils.h \
	send_message.c send_message.h \
	inc.c inc.h \
	import.c import.h \
	export.c export.h \
	rfc2015.c rfc2015.h \
	passphrase.c passphrase.h \
	select-keys.c select-keys.h \
	sigstatus.c sigstatus.h \
	simple-gettext.c \
	manual.c manual.h \
	eggtrayicon.c eggtrayicon.h \
	trayicon.c trayicon.h \
	printing.c printing.h \
	quote_fmt_lex.l quote_fmt_lex.h \
	quote_fmt_parse.y quote_fmt.h \
	sylpheed-marshal.c sylpheed-marshal.h

BUILT_SOURCES = \
	quote_fmt_lex.c \
	quote_fmt_parse.c \
	quote_fmt_parse.h \
	sylpheed-marshal.c \
	sylpheed-marshal.h

EXTRA_DIST = \
	quote_fmt_parse.h \
	sylpheed-marshal.list \
	version.h.in \
	sylpheed.rc.in

INCLUDES = \
	-DG_LOG_DOMAIN=\"Sylpheed\" \
	$(GTK_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GPGME_CFLAGS) \
	-I$(includedir) \
	-I$(top_srcdir)/libsylph

if HAVE_WINDRES
SYLPHEED_RES = sylpheed.res
#sylpheed.rc:
#	echo "SYLPHEED ICON icons/sylpheed.ico" >$@
sylpheed.res: sylpheed.rc
	$(WINDRES) $< -O coff -o $@
else
SYLPHEED_RES = 
endif

sylpheed_LDADD = \
	$(INTLLIBS) \
	$(GTK_LIBS) \
	$(GPGME_LIBS) \
	$(LDAP_LIBS) \
	$(LIBICONV) \
	../libsylph/libsylph.la \
	$(SYLPHEED_RES)

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DMANUALDIR=\""$(manualdir)"\" \
	-DFAQDIR=\""$(faqdir)"\" \
	-DTARGET_ALIAS=\""$(target_triplet)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\"

AM_YFLAGS = -d

sylpheed-marshal.h: sylpheed-marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=sylpheed_marshal > $@

sylpheed-marshal.c: sylpheed-marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=sylpheed_marshal > $@
