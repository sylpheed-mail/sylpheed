/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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

#ifndef __HEADERVIEW_H__
#define __HEADERVIEW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktooltips.h>

#include "procmsg.h"

typedef struct _HeaderView	HeaderView;

struct _HeaderView
{
	GtkWidget *hbox;

	GtkWidget *from_header_label;
	GtkWidget *from_body_label;
	GtkWidget *to_header_label;
	GtkWidget *to_body_label;
	GtkWidget *cc_header_label;
	GtkWidget *cc_body_label;
	GtkWidget *ng_header_label;
	GtkWidget *ng_body_label;
	GtkWidget *subject_header_label;
	GtkWidget *subject_body_label;

	GtkWidget *image;

	GtkTooltips *tip;
};

HeaderView *headerview_create	(void);
void headerview_init		(HeaderView	*headerview);
void headerview_show		(HeaderView	*headerview,
				 MsgInfo	*msginfo);
void headerview_clear		(HeaderView	*headerview);
void headerview_set_visibility	(HeaderView	*headerview,
				 gboolean	 visibility);
void headerview_destroy		(HeaderView	*headerview);

#endif /* __HEADERVIEW_H__ */
