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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkstatusbar.h>
#include <stdarg.h>

#include "statusbar.h"
#include "gtkutils.h"
#include "utils.h"

#define BUFFSIZE 1024

static GList *statusbar_list = NULL;

GtkWidget *statusbar_create(void)
{
	GtkWidget *statusbar;

	statusbar = gtk_statusbar_new();
	gtk_widget_set_size_request(statusbar, 1, -1);
	statusbar_list = g_list_append(statusbar_list, statusbar);

	set_log_show_status_func(statusbar_puts_all);

	return statusbar;
}

void statusbar_puts(GtkStatusbar *statusbar, const gchar *str)
{
	gint cid;
	gchar *buf;

	buf = g_strdup(str);
	strretchomp(buf);

	cid = gtk_statusbar_get_context_id(statusbar, "Standard Output");
	gtk_statusbar_pop(statusbar, cid);
	gtk_statusbar_push(statusbar, cid, buf);
	gtkut_widget_draw_now(GTK_WIDGET(statusbar));

	g_free(buf);
}

void statusbar_puts_all(const gchar *str)
{
	GList *cur;

	gdk_threads_enter();
	for (cur = statusbar_list; cur != NULL; cur = cur->next)
		statusbar_puts(GTK_STATUSBAR(cur->data), str);
	gdk_threads_leave();
}

void statusbar_print(GtkStatusbar *statusbar, const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	statusbar_puts(statusbar, buf);
}

void statusbar_print_all(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];
	GList *cur;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	for (cur = statusbar_list; cur != NULL; cur = cur->next)
		statusbar_puts(GTK_STATUSBAR(cur->data), buf);
}

void statusbar_pop_all(void)
{
	GList *cur;
	gint cid;

	for (cur = statusbar_list; cur != NULL; cur = cur->next) {
		cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(cur->data),
						   "Standard Output");
		gtk_statusbar_pop(GTK_STATUSBAR(cur->data), cid);
	}
}
