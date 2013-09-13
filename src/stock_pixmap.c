/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkicontheme.h>

#include "stock_pixmap.h"
#include "gtkutils.h"

#include "icons/address.xpm"
#include "icons/category.xpm"
#include "icons/clip.xpm"
#include "icons/complete.xpm"
#include "icons/continue.xpm"
#include "icons/deleted.xpm"
#include "icons/error.xpm"
#include "icons/forwarded.xpm"
#include "icons/interface.xpm"
#include "icons/jpilot.xpm"
#include "icons/ldap.xpm"
#include "icons/linewrap.xpm"
#include "icons/mark.xpm"
#include "icons/new.xpm"
#include "icons/replied.xpm"
#include "icons/unread.xpm"
#include "icons/vcard.xpm"
#include "icons/online.xpm"
#include "icons/offline.xpm"
#include "icons/mail.xpm"
#include "icons/stock_inbox.h"
#include "icons/stock_outbox.h"
#include "icons/stock_mail.h"
#include "icons/stock_attach.h"
#include "icons/stock_mail-compose.h"
#include "icons/stock_mail-compose_16.h"
#include "icons/stock_mail-forward.h"
#include "icons/stock_mail-receive.h"
#include "icons/stock_mail_receive_all.h"
#include "icons/stock_mail-reply.h"
#include "icons/stock_mail-reply-to-all.h"
#include "icons/stock_mail-send.h"
#include "icons/stock_mail_send_queue.h"
#include "icons/stock_insert-file.h"
#include "icons/stock_addressbook.h"
#include "icons/stock_delete.h"
#include "icons/stock_delete_16.h"
#include "icons/stock_spam.h"
#include "icons/stock_spam_16.h"
#include "icons/stock_notspam.h"
#include "icons/stock_hand-signed.h"
#include "icons/stock_sylpheed.h"
#include "icons/stock_sylpheed_16.h"
#include "icons/stock_sylpheed_32.h"
#include "icons/stock_sylpheed_newmail.h"
#include "icons/stock_sylpheed_newmail_16.h"
#include "icons/sylpheed-logo.h"
#include "icons/stock_person.h"
#include "icons/stock_book.h"
#include "icons/folder-close.h"
#include "icons/folder-open.h"
#include "icons/folder-noselect.h"
#include "icons/folder-search.h"
#include "icons/group.h"
#include "icons/html.h"

typedef struct _StockPixmapData	StockPixmapData;

struct _StockPixmapData
{
	gchar **data;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;
	const guint8 *pixbuf_data;
	gint pixbuf_data_len;
	gchar *icon_name;
	gint size;
};

static StockPixmapData pixmaps[] =
{
	{address_xpm	 , NULL, NULL},
	{NULL, NULL, NULL, NULL, stock_book, sizeof(stock_book), "stock_book", 16},
	{category_xpm	 , NULL, NULL},
	{clip_xpm	 , NULL, NULL},
	{complete_xpm	 , NULL, NULL},
	{continue_xpm	 , NULL, NULL},
	{deleted_xpm	 , NULL, NULL},
	{NULL, NULL, NULL, NULL, folder_close, sizeof(folder_close), "folder-close", 0},
	{NULL, NULL, NULL, NULL, folder_open, sizeof(folder_open), "folder-open", 0},
	{NULL, NULL, NULL, NULL, folder_noselect, sizeof(folder_noselect), "folder-noselect", 0},
	{error_xpm	 , NULL, NULL},
	{forwarded_xpm	 , NULL, NULL},
	{NULL, NULL, NULL, NULL, group, sizeof(group), "group", 0},
	{NULL, NULL, NULL, NULL, html, sizeof(html), "html", 0},
	{interface_xpm	 , NULL, NULL},
	{jpilot_xpm	 , NULL, NULL},
	{ldap_xpm	 , NULL, NULL},
	{linewrap_xpm	 , NULL, NULL},
	{mark_xpm	 , NULL, NULL},
	{new_xpm	 , NULL, NULL},
	{replied_xpm	 , NULL, NULL},
	{unread_xpm	 , NULL, NULL},
	{vcard_xpm	 , NULL, NULL},
	{online_xpm	 , NULL, NULL},
	{offline_xpm	 , NULL, NULL},
	{mail_xpm	 , NULL, NULL},

	{NULL, NULL, NULL, NULL, stock_inbox, sizeof(stock_inbox), "stock_inbox", 16},
	{NULL, NULL, NULL, NULL, stock_outbox, sizeof(stock_outbox), "stock_outbox", 16},
	{NULL, NULL, NULL, NULL, stock_mail_compose_16, sizeof(stock_mail_compose_16), "stock_mail-compose", 16},
	{NULL, NULL, NULL, NULL, stock_delete_16, sizeof(stock_delete_16), GTK_STOCK_DELETE, 16},
	{NULL, NULL, NULL, NULL, stock_mail, sizeof(stock_mail), "stock_mail", 24},
	{NULL, NULL, NULL, NULL, stock_attach, sizeof(stock_attach), "stock_attach", 24},
	{NULL, NULL, NULL, NULL, stock_mail_compose, sizeof(stock_mail_compose), "stock_mail-compose", 24},
	{NULL, NULL, NULL, NULL, stock_mail_forward, sizeof(stock_mail_forward), "stock_mail-forward", 24},
	{NULL, NULL, NULL, NULL, stock_mail_receive, sizeof(stock_mail_receive), "stock_mail-receive", 24},
	{NULL, NULL, NULL, NULL, stock_mail_receive_all, sizeof(stock_mail_receive_all), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_mail_reply, sizeof(stock_mail_reply), "stock_mail-reply", 24},
	{NULL, NULL, NULL, NULL, stock_mail_reply_to_all, sizeof(stock_mail_reply_to_all), "stock_mail-reply-to-all", 24},
	{NULL, NULL, NULL, NULL, stock_mail_send, sizeof(stock_mail_send), "stock_mail-send", 24},
	{NULL, NULL, NULL, NULL, stock_mail_send_queue, sizeof(stock_mail_send_queue), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_insert_file, sizeof(stock_insert_file), "stock_insert-file", 24},
	{NULL, NULL, NULL, NULL, stock_addressbook, sizeof(stock_addressbook), "stock_addressbook", 24},
	{NULL, NULL, NULL, NULL, stock_delete, sizeof(stock_delete), GTK_STOCK_DELETE, 24},
	{NULL, NULL, NULL, NULL, stock_spam, sizeof(stock_spam), "stock_spam", 24},
	{NULL, NULL, NULL, NULL, stock_notspam, sizeof(stock_notspam), "stock_notspam", 24},
	{NULL, NULL, NULL, NULL, stock_hand_signed, sizeof(stock_hand_signed), "stock_hand-signed", 24},
	{NULL, NULL, NULL, NULL, stock_sylpheed, sizeof(stock_sylpheed), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_sylpheed_16, sizeof(stock_sylpheed_16), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_sylpheed_32, sizeof(stock_sylpheed_32), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_sylpheed_newmail, sizeof(stock_sylpheed_newmail), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_sylpheed_newmail_16, sizeof(stock_sylpheed_newmail_16), NULL, 0},
	{NULL, NULL, NULL, NULL, sylpheed_logo, sizeof(sylpheed_logo), NULL, 0},
	{NULL, NULL, NULL, NULL, stock_person, sizeof(stock_person), "stock_person", 16},
	{NULL, NULL, NULL, NULL, folder_search, sizeof(folder_search), "folder-search", 0},
	{NULL, NULL, NULL, NULL, stock_spam_16, sizeof(stock_spam_16), "stock_spam", 16},
};


GtkWidget *stock_pixbuf_widget(GtkWidget *window, StockPixmap icon)
{
	GdkPixbuf *pixbuf;

	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);

	stock_pixbuf_gdk(window, icon, &pixbuf);
	return gtk_image_new_from_pixbuf(pixbuf);
}

GtkWidget *stock_pixbuf_widget_scale(GtkWidget *window, StockPixmap icon,
				     gint width, gint height)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GtkWidget *image;

	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);

	stock_pixbuf_gdk(window, icon, &pixbuf);
	scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, width, height,
						GDK_INTERP_HYPER);
	image = gtk_image_new_from_pixbuf(scaled_pixbuf);
	g_object_unref(scaled_pixbuf);

	return image;
}

/* create GdkPixmap if it has not created yet */
gint stock_pixmap_gdk(GtkWidget *window, StockPixmap icon,
		      GdkPixmap **pixmap, GdkBitmap **mask)
{
	StockPixmapData *pix_d;

	if (pixmap) *pixmap = NULL;
	if (mask)   *mask   = NULL;

	g_return_val_if_fail(window != NULL, -1);
	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, -1);

	pix_d = &pixmaps[icon];

	g_return_val_if_fail(pix_d->data != NULL, -1);

	if (!pix_d->pixmap) {
		PIXMAP_CREATE(window, pix_d->pixmap, pix_d->mask,
			      pix_d->data);
	}

	if (pixmap) *pixmap = pix_d->pixmap;
	if (mask)   *mask   = pix_d->mask;

	return 0;
}

gint stock_pixbuf_gdk(GtkWidget *window, StockPixmap icon, GdkPixbuf **pixbuf)
{
	StockPixmapData *pix_d;

	if (pixbuf)
		*pixbuf = NULL;

	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, -1);

	pix_d = &pixmaps[icon];

	if (!pix_d->pixbuf && pix_d->pixbuf_data)
		pix_d->pixbuf = gdk_pixbuf_new_from_inline
			(pix_d->pixbuf_data_len, pix_d->pixbuf_data,
			 FALSE, NULL);
	if (!pix_d->pixbuf && pix_d->icon_name)
		pix_d->pixbuf = gtk_icon_theme_load_icon
			(gtk_icon_theme_get_default(),
			 pix_d->icon_name, pix_d->size, 0, NULL);
	if (!pix_d->pixbuf && pix_d->data)
		pix_d->pixbuf = gdk_pixbuf_new_from_xpm_data
			((const gchar **)pix_d->data);
	if (!pix_d->pixbuf) {
		g_warning("can't read image %d\n", icon);
		return -1;
	}

	if (pixbuf)
		*pixbuf = pix_d->pixbuf;

	return 0;
}
