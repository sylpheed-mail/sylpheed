/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2008 Hiroyuki Yamamoto
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

#ifndef __PREFS_H__
#define __PREFS_H__

#include <glib.h>
#include <stdio.h>

typedef struct _PrefParam	PrefParam;
typedef struct _PrefFile	PrefFile;

#define PREFSBUFSIZE		8192

typedef enum
{
	P_STRING,
	P_INT,
	P_BOOL,
	P_ENUM,
	P_USHORT,
	P_OTHER
} PrefType;

typedef void (*DataSetFunc)   (PrefParam *pparam);
typedef void (*WidgetSetFunc) (PrefParam *pparam);

struct _PrefParam {
	gchar	      *name;
	gchar	      *defval;
	gpointer       data;
	PrefType       type;
	gpointer       ui_data;
};

struct _PrefFile {
	FILE *fp;
	gchar *path;
};

GHashTable *prefs_param_table_get	(PrefParam	*param);
void prefs_param_table_destroy		(GHashTable	*param_table);

void prefs_read_config		(PrefParam	*param,
				 const gchar	*label,
				 const gchar	*rcfile,
				 const gchar	*encoding);
void prefs_write_config		(PrefParam	*param,
				 const gchar	*label,
				 const gchar	*rcfile);

PrefFile *prefs_file_open		(const gchar	*path);
gint prefs_file_write_param		(PrefFile	*pfile,
					 PrefParam	*param);
void prefs_file_set_backup_generation	(PrefFile	*pfile,
					 gint		 generation);
gint prefs_file_get_backup_generation	(PrefFile	*pfile);
gint prefs_file_close			(PrefFile	*pfile);
gint prefs_file_close_revert		(PrefFile	*pfile);

void prefs_set_default		(PrefParam	*param);
void prefs_free			(PrefParam	*param);

#endif /* __PREFS_H__ */
