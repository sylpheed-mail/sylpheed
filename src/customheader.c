/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "customheader.h"
#include "utils.h"


gchar *custom_header_get_str(CustomHeader *ch)
{
	return g_strdup_printf("%i:%s: %s",
			       ch->account_id, ch->name,
			       ch->value ? ch->value : "");
}

CustomHeader *custom_header_read_str(const gchar *buf)
{
	CustomHeader *ch;
	gchar *account_id_str;
	gint id;
	gchar *name;
	gchar *value;
	gchar *tmp;

	Xstrdup_a(tmp, buf, return NULL);

	account_id_str = tmp;

	name = strchr(account_id_str, ':');
	if (!name)
		return NULL;
	else {
		gchar *endp;

		*name++ = '\0';
		id = strtol(account_id_str, &endp, 10);
		if (*endp != '\0') return NULL;
	}

	value = strchr(name, ':');
	if (!value) return NULL;

	*value++ = '\0';

	g_strstrip(name);
	g_strstrip(value);

	ch = g_new0(CustomHeader, 1);
	ch->account_id = id;
	ch->name = *name ? g_strdup(name) : NULL;
	ch->value = *value ? g_strdup(value) : NULL;

	return ch;
}

CustomHeader *custom_header_find(GSList *header_list, const gchar *header)
{
	GSList *cur;
	CustomHeader *chdr;

	for (cur = header_list; cur != NULL; cur = cur->next) {
		chdr = (CustomHeader *)cur->data;
		if (!strcasecmp(chdr->name, header))
			return chdr;
	}

	return NULL;
}

void custom_header_free(CustomHeader *ch)
{
	if (!ch) return;

	g_free(ch->name);
	g_free(ch->value);
	g_free(ch);
}
