/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#ifndef __DISPLAYHEADER_H__
#define __DISPLAYHEADER_H__

#include <glib.h>

typedef struct _DisplayHeaderProp DisplayHeaderProp;

struct _DisplayHeaderProp
{
	gchar *name;
	gboolean hidden;
};

gchar *display_header_prop_get_str		(DisplayHeaderProp *dp);
DisplayHeaderProp *display_header_prop_read_str	(gchar		   *buf);
void display_header_prop_free			(DisplayHeaderProp *dp);

#endif /* __DISPLAYHEADER_H__ */
