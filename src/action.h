/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#ifndef __ACTION_H__
#define __ACTION_H__

#include <glib.h>
#include <gtk/gtkitemfactory.h>

#include "mainwindow.h"
#include "messageview.h"
#include "compose.h"

typedef enum
{
	ACTION_NONE		= 1 << 0,
	ACTION_PIPE_IN		= 1 << 1,
	ACTION_PIPE_OUT		= 1 << 2,
	ACTION_SINGLE		= 1 << 3,
	ACTION_MULTIPLE		= 1 << 4,
	ACTION_ASYNC		= 1 << 5,
	ACTION_USER_IN		= 1 << 6,
	ACTION_USER_HIDDEN_IN	= 1 << 7,
	ACTION_INSERT		= 1 << 8,
	ACTION_USER_STR		= 1 << 9,
	ACTION_USER_HIDDEN_STR	= 1 << 10,
	ACTION_SELECTION_STR	= 1 << 11,
	ACTION_ERROR		= 1 << 30
} ActionType;

ActionType action_get_type	(const gchar	*action_str);

void action_update_mainwin_menu	(GtkItemFactory	*ifactory, 
				 MainWindow	*mainwin);
void action_update_msgview_menu	(GtkItemFactory	*ifactory,
				 MessageView	*msgview);
void action_update_compose_menu	(GtkItemFactory	*ifactory, 
				 Compose	*compose);

#endif /* __ACTION_H__ */
