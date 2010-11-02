/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
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

#ifndef __UPDATE_CHECK_H__
#define __UPDATE_CHECK_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_UPDATE_CHECK

#include <glib.h>

void update_check			(gboolean	 show_dialog_always);
void update_check_plugin		(gboolean	 show_dialog_always);
#ifdef G_OS_WIN32
#ifdef USE_UPDATE_CHECK_PLUGIN
void update_check_spawn_plugin_updater(void);
#endif
#endif

void update_check_set_check_url		(const gchar	*url);
const gchar *update_check_get_check_url	(void);
void update_check_set_download_url	(const gchar	*url);
const gchar *update_check_get_download_url(void);
void update_check_set_jump_url		(const gchar	*url);
const gchar *update_check_get_jump_url	(void);

#ifdef USE_UPDATE_CHECK_PLUGIN
void update_check_set_check_plugin_url	(const gchar	*url);
const gchar *update_check_get_check_plugin_url(void);
void update_check_set_jump_plugin_url	(const gchar	*url);
const gchar *update_check_get_jump_plugin_url(void);
#endif /* USE_UPDATE_CHECK_PLUGIN */

#endif /* USE_UPDATE_CHECK */

#endif /* __UPDATE_CHECK_H__ */
