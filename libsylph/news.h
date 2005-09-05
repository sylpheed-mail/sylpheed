/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#ifndef __NEWS_H__
#define __NEWS_H__

#include <glib.h>
#include <stdio.h>

#include "folder.h"

typedef struct _NewsFolder	NewsFolder;
typedef struct _NewsGroupInfo	NewsGroupInfo;

#define NEWS_FOLDER(obj)	((NewsFolder *)obj)

struct _NewsFolder
{
	RemoteFolder rfolder;

	gboolean use_auth;
};

struct _NewsGroupInfo
{
	gchar *name;
	guint first;
	guint last;
	gchar type;
};

FolderClass *news_get_class		(void);

GSList *news_get_group_list		(Folder		*folder);
void news_group_list_free		(GSList		*group_list);
void news_remove_group_list_cache	(Folder		*folder);

gint news_post				(Folder		*folder,
					 const gchar	*file);
gint news_post_stream			(Folder		*folder,
					 FILE		*fp);

#endif /* __NEWS_H__ */
