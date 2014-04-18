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

#ifndef __PROCHEADER_H__
#define __PROCHEADER_H__

#include <glib.h>
#include <stdio.h>
#include <time.h>

#include "procmsg.h"

typedef struct _HeaderEntry	HeaderEntry;
typedef struct _Header		Header;

struct _HeaderEntry
{
	gchar	 *name;
	gchar	 *body;
	gboolean  unfold;
};

struct _Header
{
	gchar *name;
	gchar *body;
};

gint procheader_get_one_field		(gchar		*buf,
					 size_t		 len,
					 FILE		*fp,
					 HeaderEntry	 hentry[]);
gchar *procheader_get_unfolded_line	(gchar		*buf,
					 size_t		 len,
					 FILE		*fp);

GSList *procheader_get_header_list_from_file	(const gchar	*file);
GSList *procheader_get_header_list		(FILE		*fp);
GSList *procheader_get_header_list_from_msginfo	(MsgInfo	*msginfo);
GSList *procheader_add_header_list		(GSList		*hlist,
						 const gchar	*header_name,
						 const gchar	*body);
GSList *procheader_copy_header_list		(GSList		*hlist);
GSList *procheader_merge_header_list		(GSList		*hlist1,
						 GSList		*hlist2);
GSList *procheader_merge_header_list_dup	(GSList		*hlist1,
						 GSList		*hlist2);
gint procheader_find_header_list		(GSList		*hlist,
						 const gchar	*header_name);
void procheader_header_list_destroy		(GSList		*hlist);

GPtrArray *procheader_get_header_array		(FILE		*fp,
						 const gchar	*encoding);
GPtrArray *procheader_get_header_array_asis	(FILE		*fp,
						 const gchar	*encoding);
GPtrArray *procheader_get_header_array_for_display
						(FILE		*fp,
						 const gchar	*encoding);
void procheader_header_array_destroy		(GPtrArray	*harray);

void procheader_header_free			(Header		*header);

void procheader_get_header_fields	(FILE		*fp,
					 HeaderEntry	 hentry[]);
MsgInfo *procheader_parse_file		(const gchar	*file,
					 MsgFlags	 flags,
					 gboolean	 full);
MsgInfo *procheader_parse_str		(const gchar	*str,
					 MsgFlags	 flags,
					 gboolean	 full);
MsgInfo *procheader_parse_stream	(FILE		*fp,
					 MsgFlags	 flags,
					 gboolean	 full);

gchar *procheader_get_fromname		(const gchar	*str);
gchar *procheader_get_toname		(const gchar	*str);

stime_t procheader_date_parse		(gchar		*dest,
					 const gchar	*src,
					 gint		 len);
void procheader_date_get_localtime	(gchar		*dest,
					 gint		 len,
					 const stime_t	 timer);

#endif /* __PROCHEADER_H__ */
