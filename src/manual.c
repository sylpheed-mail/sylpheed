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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <string.h>

#include "prefs_common.h"
#include "manual.h"
#include "utils.h"

static gchar *get_lang_str(ManualLang lang);

static gchar *get_lang_str(ManualLang lang)
{
	switch (lang) {
	case MANUAL_LANG_DE:
		return "de";
	case MANUAL_LANG_EN:
		return "en";
	case MANUAL_LANG_ES:
		return "es";
	case MANUAL_LANG_FR:
		return "fr";
	case MANUAL_LANG_IT:
		return "it";
	case MANUAL_LANG_JA:
		return "ja";
	default:
		return NULL;
	}
}

void manual_open(ManualLang lang)
{
	gchar *lang_str;
	gchar *file_uri;

	lang_str = get_lang_str(lang);
	if (!lang_str) return;

	file_uri = g_strconcat("file://",
#ifdef G_OS_WIN32
			       get_startup_dir(),
			       G_DIR_SEPARATOR_S "doc" G_DIR_SEPARATOR_S
			       "manual",
#elif defined(__APPLE__)
			       get_startup_dir(),
			       G_DIR_SEPARATOR_S "Contents" G_DIR_SEPARATOR_S
			       "Resources" G_DIR_SEPARATOR_S
			       "share" G_DIR_SEPARATOR_S
			       "sylpheed" G_DIR_SEPARATOR_S
			       "manual",
#else
			       MANUALDIR,
#endif
			       G_DIR_SEPARATOR_S, lang_str, G_DIR_SEPARATOR_S,
			       MANUAL_HTML_INDEX, NULL);
	debug_print("Opening manual: %s\n", file_uri);
	open_uri(file_uri, prefs_common.uri_cmd);
	g_free(file_uri);
}

void faq_open(ManualLang lang)
{
	gchar *lang_str;
	gchar *file_uri;

	lang_str = get_lang_str(lang);
	if (!lang_str) return;

	file_uri = g_strconcat("file://",
#ifdef G_OS_WIN32
			       get_startup_dir(),
			       G_DIR_SEPARATOR_S "doc" G_DIR_SEPARATOR_S "faq",
#elif defined(__APPLE__)
			       get_startup_dir(),
			       G_DIR_SEPARATOR_S "Contents" G_DIR_SEPARATOR_S
			       "Resources" G_DIR_SEPARATOR_S
			       "share" G_DIR_SEPARATOR_S
			       "sylpheed" G_DIR_SEPARATOR_S
			       "faq",
#else
			       FAQDIR,
#endif
			       G_DIR_SEPARATOR_S, lang_str, G_DIR_SEPARATOR_S,
			       FAQ_HTML_INDEX, NULL);
	debug_print("Opening FAQ: %s\n", file_uri);
	open_uri(file_uri, prefs_common.uri_cmd);
	g_free(file_uri);
}
