/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
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

#ifndef __SEND_MESSAGE_H__
#define __SEND_MESSAGE_H__

#include <glib.h>
#include <stdio.h>

typedef struct _QueueInfo	QueueInfo;

#include "prefs_account.h"
#include "folder.h"
#include "procmsg.h"

struct _QueueInfo
{
	gchar *from;
	gchar *server;
	GSList *to_list;
	PrefsAccount *ac;
	gchar *reply_target;
	gchar *forward_targets;
	FILE *fp;
};

gint send_message		(const gchar	*file,
				 PrefsAccount	*ac_prefs,
				 GSList		*to_list);

QueueInfo *send_get_queue_info	(const gchar	*file);
void send_queue_info_free	(QueueInfo	*qinfo);
gint send_message_queue		(QueueInfo	*qinfo);
gint send_message_queue_all	(FolderItem	*queue,
				 gboolean	 save_msgs,
				 gboolean	 filter_msgs);

gint send_message_set_reply_flag	(const gchar	*reply_target,
					 const gchar	*msgid);
gint send_message_set_forward_flags	(const gchar	*forward_targets);

#endif /* __SEND_H__ */
