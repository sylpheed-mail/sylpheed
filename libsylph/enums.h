/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#ifndef __ENUMS_H__
#define __ENUMS_H__

typedef enum
{
	TOOLBAR_NONE,
	TOOLBAR_ICON,
	TOOLBAR_TEXT,
	TOOLBAR_BOTH,
	TOOLBAR_BOTH_HORIZ
} ToolbarStyle;

typedef enum
{
	LAYOUT_NORMAL,
	LAYOUT_VERTICAL,
	LAYOUT_VERTICAL_DOUBLE,
	LAYOUT_WIDE_MESSAGE,
	LAYOUT_WIDE_SUMMARY
} LayoutType;

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
	S_COL_TO,

	S_COL_MSG_INFO,

	S_COL_LABEL,
	S_COL_TDATE,

	S_COL_FOREGROUND,
	S_COL_BOLD,

	N_SUMMARY_COLS
} SummaryColumnType;

#define N_SUMMARY_VISIBLE_COLS  S_COL_MSG_INFO

#endif /* __ENUMS_H__ */
