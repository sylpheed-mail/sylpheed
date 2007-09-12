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

#ifndef __PREFS_SUMMARY_COLUMN_H__
#define __PREFS_SUMMARY_COLUMN_H__

#include "summaryview.h"

void prefs_summary_column_open(gboolean sent_folder);

SummaryColumnState *prefs_summary_column_get_config(gboolean sent_folder);
void prefs_summary_column_set_config(SummaryColumnState *state,
				     gboolean sent_folder);

#endif /* __PREFS_SUMMARY_COLUMN_H__ */
