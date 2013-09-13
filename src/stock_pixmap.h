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

#ifndef __STOCK_PIXMAP_H__
#define __STOCK_PIXMAP_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef enum
{
	STOCK_PIXMAP_ADDRESS,
	STOCK_PIXMAP_BOOK,
	STOCK_PIXMAP_CATEGORY,
	STOCK_PIXMAP_CLIP,
	STOCK_PIXMAP_COMPLETE,
	STOCK_PIXMAP_CONTINUE,
	STOCK_PIXMAP_DELETED,
	STOCK_PIXMAP_FOLDER_CLOSE,
	STOCK_PIXMAP_FOLDER_OPEN,
	STOCK_PIXMAP_FOLDER_NOSELECT,
	STOCK_PIXMAP_ERROR,
	STOCK_PIXMAP_FORWARDED,
	STOCK_PIXMAP_GROUP,
	STOCK_PIXMAP_HTML,
	STOCK_PIXMAP_INTERFACE,
	STOCK_PIXMAP_JPILOT,
	STOCK_PIXMAP_LDAP,
	STOCK_PIXMAP_LINEWRAP,
	STOCK_PIXMAP_MARK,
	STOCK_PIXMAP_NEW,
	STOCK_PIXMAP_REPLIED,
	STOCK_PIXMAP_UNREAD,
	STOCK_PIXMAP_VCARD,
	STOCK_PIXMAP_ONLINE,
	STOCK_PIXMAP_OFFLINE,
	STOCK_PIXMAP_MAIL_SMALL,

	STOCK_PIXMAP_INBOX,
	STOCK_PIXMAP_OUTBOX,
	STOCK_PIXMAP_DRAFT,
	STOCK_PIXMAP_TRASH,
	STOCK_PIXMAP_MAIL,
	STOCK_PIXMAP_MAIL_ATTACH,
	STOCK_PIXMAP_MAIL_COMPOSE,
	STOCK_PIXMAP_MAIL_FORWARD,
	STOCK_PIXMAP_MAIL_RECEIVE,
	STOCK_PIXMAP_MAIL_RECEIVE_ALL,
	STOCK_PIXMAP_MAIL_REPLY,
	STOCK_PIXMAP_MAIL_REPLY_TO_ALL,
	STOCK_PIXMAP_MAIL_SEND,
	STOCK_PIXMAP_MAIL_SEND_QUEUE,
	STOCK_PIXMAP_INSERT_FILE,
	STOCK_PIXMAP_ADDRESS_BOOK,
	STOCK_PIXMAP_DELETE,
	STOCK_PIXMAP_SPAM,
	STOCK_PIXMAP_NOTSPAM,
	STOCK_PIXMAP_SIGN,
	STOCK_PIXMAP_SYLPHEED,
	STOCK_PIXMAP_SYLPHEED_SMALL,
	STOCK_PIXMAP_SYLPHEED_32,
	STOCK_PIXMAP_SYLPHEED_NEWMAIL,
	STOCK_PIXMAP_SYLPHEED_NEWMAIL_SMALL,
	STOCK_PIXMAP_SYLPHEED_LOGO,
	STOCK_PIXMAP_PERSON,
	STOCK_PIXMAP_FOLDER_SEARCH,
	STOCK_PIXMAP_SPAM_SMALL,

	N_STOCK_PIXMAPS
} StockPixmap;

GtkWidget *stock_pixbuf_widget	(GtkWidget	 *window,
				 StockPixmap	  icon);

GtkWidget *stock_pixbuf_widget_scale	(GtkWidget	*window,
					 StockPixmap	 icon,
					 gint		 width,
					 gint		 height);

gint stock_pixmap_gdk		(GtkWidget	 *window,
				 StockPixmap	  icon,
				 GdkPixmap	**pixmap,
				 GdkBitmap	**mask);
gint stock_pixbuf_gdk		(GtkWidget	 *window,
				 StockPixmap	  icon,
				 GdkPixbuf	**pixbuf);

#endif /* __STOCK_PIXMAP_H__ */
