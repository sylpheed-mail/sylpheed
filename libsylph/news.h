/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
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
	gboolean subscribed;
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
