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

#ifndef __ENUMS_H__
#define __ENUMS_H__

typedef enum
{
	TOOLBAR_NONE,
	TOOLBAR_ICON,
	TOOLBAR_TEXT,
	TOOLBAR_BOTH
} ToolbarStyle;

typedef enum
{
	S_COL_MARK,
	S_COL_UNREAD,
	S_COL_MIME,
	S_COL_SUBJECT,
	S_COL_FROM,
	S_COL_DATE,
	S_COL_SIZE,
	S_COL_NUMBER,

	S_COL_MSG_INFO,

	S_COL_LABEL,
	S_COL_TO,

	S_COL_FOREGROUND,
	S_COL_BOLD,

	N_SUMMARY_COLS
} SummaryColumnType;

#define N_SUMMARY_VISIBLE_COLS  S_COL_MSG_INFO

#endif /* __ENUMS_H__ */
