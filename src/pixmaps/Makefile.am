BUILT_SOURCES = \
	stock_mail_receive_all.h \
	stock_mail_send_queue.h \
	stock_addressbook.h \
	stock_attach.h \
	stock_delete.h \
	stock_delete_16.h \
	stock_inbox.h \
	stock_insert-file.h \
	stock_mail-compose.h \
	stock_mail-compose_16.h \
	stock_mail-forward.h \
	stock_mail-receive.h \
	stock_mail-reply-to-all.h \
	stock_mail-reply.h \
	stock_mail-send.h \
	stock_mail.h \
	stock_outbox.h \
	stock_spam.h

EXTRA_DIST = \
	stock_mail_receive_all.png \
	stock_mail_send_queue.png \
	stock_addressbook.png \
	stock_attach.png \
	stock_delete.png \
	stock_delete_16.png \
	stock_inbox.png \
	stock_insert-file.png \
	stock_mail-compose.png \
	stock_mail-compose_16.png \
	stock_mail-forward.png \
	stock_mail-receive.png \
	stock_mail-reply-to-all.png \
	stock_mail-reply.png \
	stock_mail-send.png \
	stock_mail.png \
	stock_outbox.png \
	stock_spam.png

%.h: %.png
	gdk-pixbuf-csource --name=`echo $(*F)|sed s/-/_/g` $< > $@

clean:
	rm -f $(BUILT_SOURCES)
