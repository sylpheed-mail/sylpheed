/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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

#include "stock_pixmap.h"
#include "gtkutils.h"

#include "pixmaps/address.xpm"
#include "pixmaps/book.xpm"
#include "pixmaps/category.xpm"
#include "pixmaps/checkbox_off.xpm"
#include "pixmaps/checkbox_on.xpm"
#include "pixmaps/clip.xpm"
#include "pixmaps/complete.xpm"
#include "pixmaps/continue.xpm"
#include "pixmaps/deleted.xpm"
#include "pixmaps/dir-close.xpm"
#include "pixmaps/dir-open.xpm"
#include "pixmaps/dir-noselect.xpm"
#include "pixmaps/error.xpm"
#include "pixmaps/forwarded.xpm"
#include "pixmaps/group.xpm"
#include "pixmaps/inbox.xpm"
#include "pixmaps/interface.xpm"
#include "pixmaps/jpilot.xpm"
#include "pixmaps/ldap.xpm"
#include "pixmaps/linewrap.xpm"
#include "pixmaps/mark.xpm"
#include "pixmaps/new.xpm"
#include "pixmaps/outbox.xpm"
#include "pixmaps/replied.xpm"
#include "pixmaps/stock_close.xpm"
#include "pixmaps/stock_down_arrow.xpm"
#include "pixmaps/stock_exec.xpm"
#include "pixmaps/stock_mail.xpm"
#include "pixmaps/stock_mail_attach.xpm"
#include "pixmaps/stock_mail_compose.xpm"
#include "pixmaps/stock_mail_forward.xpm"
#include "pixmaps/stock_mail_receive.xpm"
#include "pixmaps/stock_mail_receive_all.xpm"
#include "pixmaps/stock_mail_reply.xpm"
#include "pixmaps/stock_mail_reply_to_all.xpm"
#include "pixmaps/stock_mail_send.xpm"
#include "pixmaps/stock_mail_send_queue.xpm"
#include "pixmaps/stock_paste.xpm"
#include "pixmaps/stock_preferences.xpm"
#include "pixmaps/stock_properties.xpm"
#include "pixmaps/sylpheed-logo.xpm"
#include "pixmaps/tb_address_book.xpm"
#include "pixmaps/trash.xpm"
#include "pixmaps/unread.xpm"
#include "pixmaps/vcard.xpm"
#include "pixmaps/online.xpm"
#include "pixmaps/offline.xpm"
#include "pixmaps/mail.xpm"

typedef struct _StockPixmapData	StockPixmapData;

struct _StockPixmapData
{
	gchar **data;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
};

static StockPixmapData pixmaps[] =
{
	{address_xpm			, NULL, NULL},
	{book_xpm			, NULL, NULL},
	{category_xpm			, NULL, NULL},
	{checkbox_off_xpm		, NULL, NULL},
	{checkbox_on_xpm		, NULL, NULL},
	{clip_xpm			, NULL, NULL},
	{complete_xpm			, NULL, NULL},
	{continue_xpm			, NULL, NULL},
	{deleted_xpm			, NULL, NULL},
	{dir_close_xpm			, NULL, NULL},
	{dir_open_xpm			, NULL, NULL},
	{dir_noselect_xpm		, NULL, NULL},
	{error_xpm			, NULL, NULL},
	{forwarded_xpm			, NULL, NULL},
	{group_xpm			, NULL, NULL},
	{inbox_xpm			, NULL, NULL},
	{interface_xpm			, NULL, NULL},
	{jpilot_xpm			, NULL, NULL},
	{ldap_xpm			, NULL, NULL},
	{linewrap_xpm			, NULL, NULL},
	{mark_xpm			, NULL, NULL},
	{new_xpm			, NULL, NULL},
	{outbox_xpm			, NULL, NULL},
	{replied_xpm			, NULL, NULL},
	{stock_close_xpm		, NULL, NULL},
	{stock_down_arrow_xpm		, NULL, NULL},
	{stock_exec_xpm			, NULL, NULL},
	{stock_mail_xpm			, NULL, NULL},
	{stock_mail_attach_xpm		, NULL, NULL},
	{stock_mail_compose_xpm		, NULL, NULL},
	{stock_mail_forward_xpm		, NULL, NULL},
	{stock_mail_receive_xpm		, NULL, NULL},
	{stock_mail_receive_all_xpm	, NULL, NULL},
	{stock_mail_reply_xpm		, NULL, NULL},
	{stock_mail_reply_to_all_xpm	, NULL, NULL},
	{stock_mail_send_xpm		, NULL, NULL},
	{stock_mail_send_queue_xpm	, NULL, NULL},
	{stock_paste_xpm		, NULL, NULL},
	{stock_preferences_xpm		, NULL, NULL},
	{stock_properties_xpm		, NULL, NULL},
	{sylpheed_logo_xpm		, NULL, NULL},
	{tb_address_book_xpm		, NULL, NULL},
	{trash_xpm			, NULL, NULL},
	{unread_xpm			, NULL, NULL},
	{vcard_xpm			, NULL, NULL},
	{online_xpm			, NULL, NULL},
	{offline_xpm			, NULL, NULL},
	{mail_xpm			, NULL, NULL},
};

/* return newly constructed GtkPixmap from GdkPixmap */
GtkWidget *stock_pixmap_widget(GtkWidget *window, StockPixmap icon)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	g_return_val_if_fail(window != NULL, NULL);
	g_return_val_if_fail(icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);

	stock_pixmap_gdk(window, icon, &pixmap, &mask);
	return gtk_pixmap_new(pixmap, mask);
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

	if (!pix_d->pixmap) {
		PIXMAP_CREATE(window, pix_d->pixmap, pix_d->mask,
			      pix_d->data);
	}

	if (pixmap) *pixmap = pix_d->pixmap;
	if (mask)   *mask   = pix_d->mask;

	return 0;
}
