/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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
