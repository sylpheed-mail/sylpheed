/*
 * Sylpheed templates subsystem 
 * Copyright (C) 2001 Alexander Barinov
 * Copyright (C) 2001-2006 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>

#include "main.h"
#include "template.h"
#include "utils.h"

static GSList *template_list;

static Template *template_load(gchar *filename, guint tmplid)
{
	Template *tmpl;
	FILE *fp;
	gchar buf[BUFFSIZE];

	if ((fp = g_fopen(filename, "rb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		return NULL;
	}

	tmpl = g_new(Template, 1);
	tmpl->tmplid = tmplid;
	tmpl->name = NULL;
	tmpl->to = NULL;
	tmpl->cc = NULL;
	tmpl->bcc = NULL;
	tmpl->replyto = NULL;
	tmpl->subject = NULL;
	tmpl->value = NULL;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] == '\n')
			break;
		else if (!g_ascii_strncasecmp(buf, "Name:", 5))
			tmpl->name = g_strdup(g_strstrip(buf + 5));
		else if (!g_ascii_strncasecmp(buf, "To:", 3))
			tmpl->to = g_strdup(g_strstrip(buf + 3));
		else if (!g_ascii_strncasecmp(buf, "Cc:", 3))
			tmpl->cc = g_strdup(g_strstrip(buf + 3));
		else if (!g_ascii_strncasecmp(buf, "Bcc:", 3))
			tmpl->bcc = g_strdup(g_strstrip(buf + 4));
		else if (!g_ascii_strncasecmp(buf, "Reply-To:", 9))
			tmpl->replyto = g_strdup(g_strstrip(buf + 9));
		else if (!g_ascii_strncasecmp(buf, "Subject:", 8))
			tmpl->subject = g_strdup(g_strstrip(buf + 8));
	}

	if (!tmpl->name) {
		g_warning("wrong template format\n");
		template_free(tmpl);
		fclose(fp);
		return NULL;
	}

	tmpl->value = file_read_stream_to_str(fp);
	if (!tmpl->value) {
		g_warning("cannot read template body\n");
		template_free(tmpl);
		fclose(fp);
		return NULL;
	}
	fclose(fp);

	return tmpl;
}

void template_free(Template *tmpl)
{
	g_free(tmpl->name);
	g_free(tmpl->to);
	g_free(tmpl->cc);
	g_free(tmpl->subject);
	g_free(tmpl->value);
	g_free(tmpl);
}

void template_clear_config(GSList *tmpl_list)
{
	GSList *cur;
	Template *tmpl;

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = (Template *)cur->data;
		template_free(tmpl);
	}
	g_slist_free(tmpl_list);
}

static gint template_compare_id(gconstpointer a, gconstpointer b)
{
	const Template *ta, *tb;

	ta = a;
	tb = b;
	return (ta->tmplid - tb->tmplid);
}

GSList *template_read_config(void)
{
	const gchar *path;
	gchar *filename;
	GDir *dir;
	const gchar *dir_name;
	GStatBuf s;
	Template *tmpl;
	guint tmplid;
	GSList *tmpl_list = NULL;

	path = get_template_dir();
	debug_print("%s:%d reading templates dir %s\n",
		    __FILE__, __LINE__, path);

	if (!is_dir_exist(path)) {
		if (make_dir(path) < 0)
			return NULL;
	}

	if ((dir = g_dir_open(path, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s\n", path);
		return NULL;
	}

	while ((dir_name = g_dir_read_name(dir)) != NULL) {
		tmplid = atoi(dir_name);
		if (tmplid <= 0) {
			continue;
		}

		filename = g_strconcat(path, G_DIR_SEPARATOR_S,
				       dir_name, NULL);

		if (g_stat(filename, &s) != 0 || !S_ISREG(s.st_mode) ) {
			debug_print("%s:%d %s is not an ordinary file\n",
				    __FILE__, __LINE__, filename);
			g_free(filename);
			continue;
		}

		tmpl = template_load(filename, tmplid);
		if (tmpl)
			tmpl_list = g_slist_insert_sorted(tmpl_list, tmpl,
							  template_compare_id);

		g_free(filename);
	}

	g_dir_close(dir);

	return tmpl_list;
}

void template_write_config(GSList *tmpl_list)
{
	const gchar *path;
	GSList *cur;
	Template *tmpl;
	FILE *fp;

	debug_print("%s:%d writing templates\n", __FILE__, __LINE__);

	path = get_template_dir();

	if (!is_dir_exist(path)) {
		if (is_file_exist(path)) {
			g_warning(_("file %s already exists\n"), path);
			return;
		}
		if (make_dir(path) < 0)
			return;
	}

	remove_all_files(path);

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		gchar *filename;

		tmpl = cur->data;

		filename = g_strconcat(path, G_DIR_SEPARATOR_S,
				       itos(tmpl->tmplid), NULL);

		if ((fp = g_fopen(filename, "wb")) == NULL) {
			FILE_OP_ERROR(filename, "fopen");
			g_free(filename);
			return;
		}

		fprintf(fp, "Name: %s\n", tmpl->name);
		if (tmpl->to && *tmpl->to != '\0')
			fprintf(fp, "To: %s\n", tmpl->to);
		if (tmpl->cc && *tmpl->cc != '\0')
			fprintf(fp, "Cc: %s\n", tmpl->cc);
		if (tmpl->bcc && *tmpl->bcc != '\0')
			fprintf(fp, "Bcc: %s\n", tmpl->bcc);
		if (tmpl->replyto && *tmpl->replyto != '\0')
			fprintf(fp, "Reply-To: %s\n", tmpl->replyto);
		if (tmpl->subject && *tmpl->subject != '\0')
			fprintf(fp, "Subject: %s\n", tmpl->subject);
		fputs("\n", fp);
		fwrite(tmpl->value, sizeof(gchar) * strlen(tmpl->value), 1, fp);

		fclose(fp);
		g_free(filename);
	}
}

GSList *template_get_config(void)
{
	if (!template_list)
		template_list = template_read_config();

	return template_list;
}

void template_set_config(GSList *tmpl_list)
{
	template_clear_config(template_list);
	template_write_config(tmpl_list);
	template_list = tmpl_list;
}
