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

#ifndef __CUSTOMHEADER_H__
#define __CUSTOMHEADER_H__

#include <glib.h>

typedef struct _CustomHeader	CustomHeader;

#include "prefs_account.h"

struct _CustomHeader
{
	gint account_id;
	gchar *name;
	gchar *value;
};

void custom_header_read_config		(PrefsAccount	*ac);
void custom_header_write_config		(PrefsAccount	*ac);

gchar *custom_header_get_str		(CustomHeader	*ch);
CustomHeader *custom_header_read_str	(const gchar	*buf);
CustomHeader *custom_header_find	(GSList		*header_list,
					 const gchar	*header);
void custom_header_free			(CustomHeader	*ch);

#endif /* __CUSTOMHEADER_H__ */
