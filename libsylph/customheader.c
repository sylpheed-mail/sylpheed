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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "customheader.h"
#include "prefs.h"
#include "prefs_account.h"
#include "utils.h"


void custom_header_read_config(PrefsAccount *ac)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	CustomHeader *ch;

	debug_print("Reading custom header configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     CUSTOM_HEADER_RC, NULL);
	if ((fp = g_fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		ac->customhdr_list = NULL;
		return;
	}
	g_free(rcpath);

	/* remove all previous headers list */
	while (ac->customhdr_list != NULL) {
		ch = (CustomHeader *)ac->customhdr_list->data;
		custom_header_free(ch);
		ac->customhdr_list = g_slist_remove(ac->customhdr_list, ch);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		ch = custom_header_read_str(buf);
		if (ch) {
			if (ch->account_id == ac->account_id) {
				ac->customhdr_list =
					g_slist_append(ac->customhdr_list, ch);
			} else
				custom_header_free(ch);
		}
	}

	fclose(fp);
}

void custom_header_write_config(PrefsAccount *ac)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;
	gchar buf[PREFSBUFSIZE];
	FILE * fp;
	CustomHeader *ch;

	GSList *all_hdrs = NULL;

	debug_print("Writing custom header configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     CUSTOM_HEADER_RC, NULL);

	if ((fp = g_fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
	} else {
		all_hdrs = NULL;

		while (fgets(buf, sizeof(buf), fp) != NULL) {
			ch = custom_header_read_str(buf);
			if (ch) {
				if (ch->account_id != ac->account_id)
					all_hdrs =
						g_slist_append(all_hdrs, ch);
				else
					custom_header_free(ch);
			}
		}

		fclose(fp);
	}

	if ((pfile = prefs_file_open(rcpath)) == NULL) {
		g_warning("failed to write configuration to file\n");
		g_free(rcpath);
		return;
	}

	for (cur = all_hdrs; cur != NULL; cur = cur->next) {
 		CustomHeader *hdr = (CustomHeader *)cur->data;
		gchar *chstr;

		chstr = custom_header_get_str(hdr);
		if (fputs(chstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_file_close_revert(pfile);
			g_free(rcpath);
			g_free(chstr);
			return;
		}
		g_free(chstr);
	}

	for (cur = ac->customhdr_list; cur != NULL; cur = cur->next) {
 		CustomHeader *hdr = (CustomHeader *)cur->data;
		gchar *chstr;

		chstr = custom_header_get_str(hdr);
		if (fputs(chstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_file_close_revert(pfile);
			g_free(rcpath);
			g_free(chstr);
			return;
		}
		g_free(chstr);
	}

	g_free(rcpath);

 	while (all_hdrs != NULL) {
 		ch = (CustomHeader *)all_hdrs->data;
 		custom_header_free(ch);
 		all_hdrs = g_slist_remove(all_hdrs, ch);
 	}

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write configuration to file\n");
		return;
	}
}

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
		if (!g_ascii_strcasecmp(chdr->name, header))
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
