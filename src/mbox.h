/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __MBOX_H__
#define __MBOX_H__

#include <glib.h>

#include "folder.h"

typedef enum {
	LOCK_FILE,
	LOCK_FLOCK
} LockType;

gint proc_mbox		(FolderItem	*dest,
			 const gchar	*mbox,
			 GHashTable	*folder_table);
gint lock_mbox		(const gchar	*base,
			 LockType	 type);
gint unlock_mbox	(const gchar	*base,
			 gint		 fd,
			 LockType	 type);
gint copy_mbox		(const gchar	*src,
			 const gchar	*dest);
void empty_mbox		(const gchar	*mbox);

gint export_to_mbox	(FolderItem	*src,
			 const gchar	*mbox);

#endif /* __MBOX_H__ */
