/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
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

#ifndef __SYLMAIN_H__
#define __SYLMAIN_H__

#include <glib.h>
#include <glib-object.h>

/* SylApp object */

#define SYL_TYPE_APP	(syl_app_get_type())
#define SYL_APP(obj)         (G_TYPE_CHECK_INSTANCE_CAST((obj), SYL_TYPE_APP, SylApp))
#define SYL_IS_APP(obj)      (G_TYPE_CHECK_INSTANCE_CAST((obj), SYL_TYPE_APP))
#define SYL_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SYL_TYPE_APP, SylAppClass))
#define SYL_IS_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), SYL_TYPE_APP))
#define SYL_APP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS((obj), SYL_TYPE_APP, SylAppClass))

typedef struct _SylApp		SylApp;
typedef struct _SylAppClass	SylAppClass;

struct _SylApp
{
	GObject parent_instance;
};

struct _SylAppClass
{
	GObjectClass parent_class;
};

GObject *syl_app_create	(void);
GObject *syl_app_get	(void);

void syl_init		(void);
void syl_init_gettext	(const gchar *package, const gchar *dirname);
gint syl_setup_rc_dir	(void);
void syl_save_all_state	(void);
void syl_cleanup	(void);

#endif /* __SYLMAIN_H__ */
