/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#ifndef __PREFS_FILTER_H__
#define __PREFS_FILTER_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "procmsg.h"

typedef enum
{
	FILTER_BY_NONE,
	FILTER_BY_AUTO,
	FILTER_BY_FROM,
	FILTER_BY_TO,
	FILTER_BY_SUBJECT
} PrefsFilterType;

void prefs_filter_read_config	(void);
void prefs_filter_write_config	(void);

void prefs_filter_open		(MsgInfo	*msginfo,
				 const gchar	*header);

GSList *prefs_filter_get_header_list		(void);
GSList *prefs_filter_get_user_header_list	(void);

gchar *prefs_filter_get_msg_header_field	(const gchar	*header_name);

void prefs_filter_set_user_header_list		(GSList		*list);
void prefs_filter_set_msg_header_list		(MsgInfo	*msginfo);

void prefs_filter_rename_path	(const gchar	*old_path,
				 const gchar	*new_path);
void prefs_filter_delete_path	(const gchar	*path);

#endif /* __PREFS_FILTER_H__ */
