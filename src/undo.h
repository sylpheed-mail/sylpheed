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

/* code ported from gedit */

#ifndef __UNDO_H__
#define __UNDO_H__

#include <glib.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkitemfactory.h>

typedef enum 
{
	UNDO_ACTION_INSERT,
	UNDO_ACTION_DELETE,
	UNDO_ACTION_REPLACE_INSERT,
	UNDO_ACTION_REPLACE_DELETE,
} UndoAction;

typedef enum 
{
	UNDO_STATE_TRUE,
	UNDO_STATE_FALSE,
	UNDO_STATE_UNCHANGED,
	UNDO_STATE_REFRESH,
} UndoState;

typedef struct _UndoMain UndoMain;

typedef void (*UndoChangeStateFunc)	(UndoMain	*undostruct,
					 gint		 undo_state,
					 gint		 redo_state,
					 gpointer	 data);

struct _UndoMain 
{
	GtkTextView *textview;

	GList *undo;
	GList *redo;

	UndoChangeStateFunc change_state_func;
	gpointer change_state_data;

	gboolean undo_state : 1;
	gboolean redo_state : 1;

	gint paste;
};

UndoMain *undo_init		(GtkWidget		*text);
void undo_destroy		(UndoMain		*undostruct);

void undo_set_change_state_func	(UndoMain		*undostruct,
				 UndoChangeStateFunc	 func,
				 gpointer		 data);

void undo_undo			(UndoMain		*undostruct); 
void undo_redo			(UndoMain		*undostruct); 
void undo_block			(UndoMain		*undostruct);
void undo_unblock		(UndoMain		*undostruct);

#endif /* __UNDO_H__ */
