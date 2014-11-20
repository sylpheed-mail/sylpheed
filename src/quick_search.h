/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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

#ifndef __QUICK_SEARCH_H__
#define __QUICK_SEARCH_H__

#include <glib.h>

typedef struct _QuickSearch	QuickSearch;

#include "summaryview.h"

typedef enum
{
	QS_ALL,
	QS_UNREAD,
	QS_MARK,
	QS_CLABEL,
	QS_MIME,
	QS_W1DAY,
	QS_LAST5,
	QS_LAST7,
	QS_IN_ADDRESSBOOK,
	QS_LAST30
} QSearchCondType;

struct _QuickSearch
{
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *clear_btn;
	GtkWidget *status_label;

	SummaryView *summaryview;

	gboolean entry_entered;
};

QuickSearch *quick_search_create(SummaryView		*summaryview);

void quick_search_clear_entry	(QuickSearch		*qsearch);

GSList *quick_search_filter	(QuickSearch		*qsearch,
				 QSearchCondType	 type,
				 const gchar		*key);

#endif /* __QUICK_SEARCH_H__ */
