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

#ifndef __PRINTING_H__
#define __PRINTING_H__

#include <gtk/gtkversion.h>

#include <glib.h>

#include "procmsg.h"
#include "procmime.h"

#if GTK_CHECK_VERSION(2, 10, 0)
gint printing_print_messages_gtk		(GSList		*mlist,
						 MimeInfo	*partinfo,
						 gboolean	 all_headers);

void printing_page_setup_gtk			(void);
#endif

gint printing_print_messages_with_command	(GSList		*mlist,
						 gboolean	 all_headers,
						 const gchar	*cmdline);

gint printing_print_messages			(GSList		*mlist,
						 gboolean	 all_headers);
gint printing_print_message			(MsgInfo	*msginfo,
						 gboolean	 all_headers);
gint printing_print_message_part		(MsgInfo	*msginfo,
						 MimeInfo	*partinfo);

#endif /* __PRINTING_H__ */
