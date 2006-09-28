/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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

#ifndef __TRAYICON_H__
#define __TRAYICON_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkversion.h>

#if GTK_CHECK_VERSION(2, 10, 0)
#  include <gtk/gtkstatusicon.h>
#endif

typedef struct _TrayIcon	TrayIcon;

#include "mainwindow.h"
#include "stock_pixmap.h"

struct _TrayIcon
{
#if GTK_CHECK_VERSION(2, 10, 0)
	GtkStatusIcon *status_icon;
#else
	GtkWidget *widget;
#endif
};

TrayIcon *trayicon_create	(MainWindow	*mainwin);
void trayicon_show		(TrayIcon	*tray_icon);
void trayicon_hide		(TrayIcon	*tray_icon);
void trayicon_destroy		(TrayIcon	*tray_icon);
void trayicon_set_tooltip	(const gchar	*text);
void trayicon_set_notify	(gboolean	 enabled);
void trayicon_set_stock_icon	(StockPixmap	 icon);

#endif /* __TRAYICON_H__ */
