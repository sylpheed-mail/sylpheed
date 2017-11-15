/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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
#include "printing.h"

#include <glib.h>
#include <glib/gi18n.h>
#if GTK_CHECK_VERSION(2, 10, 0)
#include <gtk/gtkprintoperation.h>
#endif

#include <stdio.h>

#include "mainwindow.h"
#include "procmsg.h"
#include "procheader.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "utils.h"

#if GTK_CHECK_VERSION(2, 10, 0)

#define SPACING	2.0

typedef struct
{
	MsgInfo *msginfo;
	gint n_pages;
	gchar *hdr_data;
	gchar *msg_text_file;
	FILE *fp;
} MsgPrintInfo;

typedef struct
{
	gint page_nr_per_msg;
	MsgPrintInfo *mpinfo;
	glong pos;
	gint lines;
} PageInfo;

typedef struct
{
	GSList *mlist;
	gint n_msgs;
	MsgPrintInfo *msgs;
	GPtrArray *pages;
	gint n_pages;

	MimeInfo *partinfo;

	gdouble line_h;
	gint lines_per_page;

	gboolean all_headers;
} PrintData;

static GtkPrintSettings *settings = NULL;
static GtkPageSetup *page_setup = NULL;

static gint get_header_data(MsgPrintInfo *mpinfo, PrintData *print_data)
{
	MsgInfo *msginfo;
	FILE *fp;
	GPtrArray *headers;
	GString *str;
	gint i;

	msginfo = mpinfo->msginfo;

	if ((fp = procmsg_open_message(msginfo)) == NULL)
		return -1;

	if (print_data->all_headers)
		headers = procheader_get_header_array_asis(fp, NULL);
	else
		headers = procheader_get_header_array_for_display(fp, NULL);

	fclose(fp);

	str = g_string_new(NULL);

	for (i = 0; i < headers->len; i++) {
		Header *hdr;
		gchar *text;
		const gchar *body;

		hdr = g_ptr_array_index(headers, i);

		if (!g_ascii_strcasecmp(hdr->name, "Subject"))
			body = msginfo->subject;
		else if (!g_ascii_strcasecmp(hdr->name, "From"))
			body = msginfo->from;
		else if (!g_ascii_strcasecmp(hdr->name, "To"))
			body = msginfo->to;
		else if (!g_ascii_strcasecmp(hdr->name, "Cc")) {
			unfold_line(hdr->body);
			body = hdr->body;
			while (g_ascii_isspace(*body))
				body++;
		} else {
			body = hdr->body;
			while (g_ascii_isspace(*body))
				body++;
		}

		if (body && *body != '\0')
			text = g_markup_printf_escaped("<b>%s:</b> %s\n",
						       hdr->name, body);
		else
			text = g_markup_printf_escaped("<b>%s:</b> (none)\n",
						       hdr->name);
		g_string_append(str, text);
		g_free(text);
	}

	/* dummy character for correct separation */
	g_string_append_c(str, ' ');

	mpinfo->hdr_data = g_string_free(str, FALSE);
	procheader_header_array_destroy(headers);

	return 0;
}

static gint layout_set_headers(PangoLayout *layout, MsgPrintInfo *mpinfo,
			       PrintData *print_data)
{
	PangoFontDescription *desc;
	gint size;

	g_return_val_if_fail(mpinfo->hdr_data != NULL, -1);

	desc = pango_font_description_from_string(prefs_common_get()->textfont);
	size = pango_font_description_get_size(desc);
	pango_font_description_free(desc);
	desc = gtkut_get_default_font_desc();
	debug_print("layout_set_headers: using font %s (style %d, %g pt)\n", pango_font_description_get_family(desc), pango_font_description_get_style(desc), (gdouble)size / PANGO_SCALE);
	pango_font_description_set_size(desc, size);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_markup(layout, mpinfo->hdr_data, -1);

	return 0;
}

static gint message_count_page(MsgPrintInfo *mpinfo, GtkPrintContext *context,
			       PrintData *print_data)
{
	cairo_t *cr;
	gdouble width, height, line_h, hdr_h = 0.0, body_h;
	gdouble dpi_x, dpi_y;
	PangoLayout *layout;
	PangoFontDescription *desc;
	gint layout_h;
	gint lines_per_page, lines_left;
	gint n_pages = 1;
	PageInfo *pinfo;
	gint i;
	FILE *fp = NULL;
	gchar buf[BUFFSIZE];
	glong pos = 0;

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);
	dpi_x = gtk_print_context_get_dpi_x(context);
	dpi_y = gtk_print_context_get_dpi_y(context);

	layout = gtk_print_context_create_pango_layout(context);
	pango_layout_set_width(layout, width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_spacing(layout, (SPACING / 72 * dpi_y) * PANGO_SCALE);

	if (!print_data->partinfo) {
		if (get_header_data(mpinfo, print_data) < 0) {
			g_object_unref(layout);
			return 0;
		}
		layout_set_headers(layout, mpinfo, print_data);

		pango_layout_get_size(layout, NULL, &layout_h);
		hdr_h = (gdouble)layout_h / PANGO_SCALE;
	}

	pango_layout_set_attributes(layout, NULL);
	desc = pango_font_description_from_string(prefs_common_get()->textfont);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_text(layout, "Test", -1);
	pango_layout_get_size(layout, NULL, &layout_h);
	if (layout_h <= 0) {
		g_warning("invalid layout_h (%d) ! falling back to default height (%d)\n", layout_h, 12 * PANGO_SCALE);
		layout_h = 12 * PANGO_SCALE;
	}
	line_h = (gdouble)layout_h / PANGO_SCALE + (SPACING / 72 * dpi_y);
	print_data->line_h = line_h;
	lines_per_page = (height - line_h) / line_h;
	print_data->lines_per_page = lines_per_page;
	body_h = height - hdr_h - line_h;
	if (body_h < 0)
		body_h = 0.0;
	lines_left = body_h / line_h;

	debug_print("width = %g, height = %g\n", width, height);
	debug_print("dpi_x = %g, dpi_y = %g\n", dpi_x, dpi_y);
	debug_print("layout_h = %d, line_h = %g, lines_per_page = %d\n", layout_h, line_h, lines_per_page);
	debug_print("hdr_h = %g, body_h = %g, lines_left = %d\n", hdr_h, body_h, lines_left);

	if (print_data->partinfo) {
		FILE *msgfp;

		if ((msgfp = procmsg_open_message(mpinfo->msginfo)) == NULL)
			return -1;
		fp = procmime_get_text_content(print_data->partinfo, msgfp,
					       NULL);
		fclose(msgfp);
	} else {
		mpinfo->msg_text_file = get_tmp_file();
		if (procmsg_save_message_as_text(mpinfo->msginfo, mpinfo->msg_text_file, NULL, print_data->all_headers) == 0) {
			if ((fp = g_fopen(mpinfo->msg_text_file, "rb")) != NULL) {
				while (fgets(buf, sizeof(buf), fp) != NULL)
					if (buf[0] == '\r' || buf[0] == '\n')
						break;
			}
		}
	}
	if (!fp) {
		g_warning("Can't get text part\n");
		return -1;
	}

	i = 0;
	pos = ftell(fp);
	pinfo = g_new(PageInfo, 1);
	pinfo->page_nr_per_msg = 0;
	pinfo->mpinfo = mpinfo;
	pinfo->pos = pos;
	pinfo->lines = lines_left;
	g_ptr_array_add(print_data->pages, pinfo);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gint lines;
		gint line_offset = 0;

		strretchomp(buf);
		pango_layout_set_text(layout, buf, -1);
		lines = pango_layout_get_line_count(layout);

		while (lines_left < lines) {
			PangoLayoutLine *line;

			debug_print("page increment: %d: lines_left = %d, lines = %d\n", i, lines_left, lines);
			line_offset += lines_left;
			line = pango_layout_get_line(layout, line_offset);
			lines -= lines_left;
			lines_left = lines_per_page;

			pinfo = g_new(PageInfo, 1);
			pinfo->page_nr_per_msg = ++i;
			pinfo->mpinfo = mpinfo;
			pinfo->pos = pos + line->start_index;
			pinfo->lines = lines_left;
			g_ptr_array_add(print_data->pages, pinfo);
		}

		lines_left -= lines;
		pos = ftell(fp);
	}

	rewind(fp);
	mpinfo->fp = fp;

	g_object_unref(layout);

	n_pages = i + 1;
	debug_print("n_pages = %d\n", n_pages);

	return n_pages;
}

static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context,
			gpointer data)
{
	PrintData *print_data = data;
	gint n_pages = 0;
	gint i;

	debug_print("begin_print\n");

	for (i = 0; i < print_data->n_msgs; i++) {
		n_pages += message_count_page(&print_data->msgs[i], context,
					      print_data);
	}

	print_data->n_pages = n_pages;
	gtk_print_operation_set_n_pages(operation, n_pages);
}

static void draw_page(GtkPrintOperation *operation, GtkPrintContext *context,
		      gint page_nr, gpointer data)
{
	PrintData *print_data = data;
	PageInfo *pinfo;
	MsgPrintInfo *mpinfo;
	MsgInfo *msginfo;
	cairo_t *cr;
	PangoLayout *layout;
	gdouble width, height;
	gdouble dpi;
	gint layout_h;
	PangoFontDescription *desc;
	gint font_size;
	gchar buf[BUFFSIZE];
	gint count = 0;

	if (page_nr >= print_data->n_pages)
		return;

	pinfo = g_ptr_array_index(print_data->pages, page_nr);
	mpinfo = pinfo->mpinfo;
	msginfo = mpinfo->msginfo;

	debug_print("draw_page: %d (%d), pos = %ld\n", page_nr, pinfo->page_nr_per_msg, pinfo->pos);
	debug_print("msg pages = %d\n", mpinfo->n_pages);

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);
	dpi = gtk_print_context_get_dpi_y(context);

#if 0
	cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
	cairo_rectangle(cr, 0, 0, width, font_size * 5);
	cairo_fill(cr);
#endif

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, 0, 0);

	layout = gtk_print_context_create_pango_layout(context);
	pango_layout_set_width(layout, width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_spacing(layout, (SPACING / 72 * dpi) * PANGO_SCALE);

	if (pinfo->page_nr_per_msg == 0 && mpinfo->hdr_data) {
		if (layout_set_headers(layout, mpinfo, print_data) < 0) {
			g_object_unref(layout);
			return;
		}

		pango_cairo_show_layout(cr, layout);
		pango_layout_get_size(layout, NULL, &layout_h);
		cairo_rel_move_to(cr, 0, (double)layout_h / PANGO_SCALE);
	}

	pango_layout_set_attributes(layout, NULL);
	desc = pango_font_description_from_string(prefs_common_get()->textfont);
	font_size = pango_font_description_get_size(desc);
	debug_print("draw_page: using font %s (style %d, %g pt)\n", pango_font_description_get_family(desc), pango_font_description_get_style(desc), (gdouble)font_size / PANGO_SCALE);
	pango_font_description_set_size(desc, font_size);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	if (fseek(mpinfo->fp, pinfo->pos, SEEK_SET) < 0) {
		FILE_OP_ERROR("draw_page", "fseek");
		g_object_unref(layout);
		return;
	}

	while (count < pinfo->lines) {
		gint lines;
		gint i;
		PangoLayoutIter *iter;
		PangoLayoutLine *layout_line;
		gint baseline;
		gdouble x, y;

		if (fgets(buf, sizeof(buf), mpinfo->fp) == NULL)
			break;
		strretchomp(buf);
		if (buf[0] == '\0') {
			/* dummy character for correct empty line height */
			buf[0] = ' ';
			buf[1] = '\0';
		}

		pango_layout_set_text(layout, buf, -1);
		lines = pango_layout_get_line_count(layout);
		iter = pango_layout_get_iter(layout);
		cairo_get_current_point(cr, &x, &y);

		for (i = 0; i < lines; i++) {
			layout_line = pango_layout_iter_get_line(iter);
			baseline = pango_layout_iter_get_baseline(iter);
			cairo_move_to(cr, 0,
				      y + (gdouble)baseline / PANGO_SCALE);
			pango_cairo_show_layout_line(cr, layout_line);
			count++;
			if (count >= pinfo->lines && i + 1 < lines)
				break;
			pango_layout_iter_next_line(iter);
		}

		pango_layout_iter_free(iter);

		pango_layout_get_size(layout, NULL, &layout_h);
		cairo_move_to(cr, 0,
			      y + (gdouble)layout_h / PANGO_SCALE + (SPACING / 72 * dpi));
	}
	debug_print("count = %d\n", count);

	desc = gtkut_get_default_font_desc();
	pango_font_description_set_size(desc, font_size);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	g_snprintf(buf, sizeof(buf), "- %d -", pinfo->page_nr_per_msg + 1);
	pango_layout_set_text(layout, buf, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	cairo_move_to(cr, 0, height - (gdouble)font_size / PANGO_SCALE / 72 * dpi);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
}

static gboolean printing_load_settings(void)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	gchar *file;
	GtkPrintSettings *new_settings;

	debug_print("printing_load_settings: load settings\n");

	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "printingrc", NULL);
	if (!is_file_exist(file)) {
		debug_print("printing_load_settings: %s not exist\n", file);
		g_free(file);
		return FALSE;
	}

	if ((new_settings = gtk_print_settings_new_from_file(file, NULL)) == NULL) {
		g_warning("printing_load_settings: gtk_print_settings_new_from_file: %s: error", file);
		g_free(file);
		return FALSE;
	}
	g_free(file);

	if (settings)
		g_object_unref(settings);
	settings = new_settings;

	return TRUE;
#else
	return FALSE;
#endif /* GTK_CHECK_VERSION(2, 12, 0) */
}

static gboolean printing_save_settings(void)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	gchar *file;
	gboolean ret = TRUE;

	g_return_val_if_fail(settings != NULL, FALSE);

	debug_print("printing_save_settings: save settings\n");

	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "printingrc", NULL);
	if (gtk_print_settings_to_file(settings, file, NULL) != TRUE) {
		g_warning("printing_save_settings: gtk_print_settings_to_file: %s: error", file);
		ret = FALSE;
	}
	g_free(file);

	return ret;
#else
	return FALSE;
#endif /* GTK_CHECK_VERSION(2, 12, 0) */
}

static gboolean printing_load_page_setup(void)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	gchar *file;
	GtkPageSetup *new_setup;

	debug_print("printing_load_page_setup: load page setup\n");

	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "pagesetuprc", NULL);
	if (!is_file_exist(file)) {
		debug_print("printing_load_page_setup: %s not exist\n", file);
		g_free(file);
		return FALSE;
	}

	if ((new_setup = gtk_page_setup_new_from_file(file, NULL)) == NULL) {
		g_warning("printing_load_page_setup: gtk_page_setup_new_from_file: %s: error", file);
		g_free(file);
		return FALSE;
	}
	g_free(file);

	if (page_setup)
		g_object_unref(page_setup);
	page_setup = new_setup;

	return TRUE;
#else
	return FALSE;
#endif /* GTK_CHECK_VERSION(2, 12, 0) */
}

static gboolean printing_save_page_setup(void)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	gchar *file;
	gboolean ret = TRUE;

	g_return_val_if_fail(page_setup != NULL, FALSE);

	debug_print("printing_save_page_setup: save page setup\n");

	file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "pagesetuprc", NULL);
	if (gtk_page_setup_to_file(page_setup, file, NULL) != TRUE) {
		g_warning("printing_save_page_setup: gtk_page_setup_to_file: %s: error", file);
		ret = FALSE;
	}
	g_free(file);

	return ret;
#else
	return FALSE;
#endif /* GTK_CHECK_VERSION(2, 12, 0) */
}

gint printing_print_messages_gtk(GSList *mlist, MimeInfo *partinfo,
				 gboolean all_headers)
{
	GtkPrintOperation *op;
	GtkPrintOperationResult res;
	PrintData *print_data;
	GSList *cur;
	gchar *prev_dir;
	gint i;

	g_return_val_if_fail(mlist != NULL, -1);

	debug_print("printing start\n");

	prev_dir = g_get_current_dir();
	change_dir(get_document_dir());

	print_data = g_new0(PrintData, 1);
	print_data->mlist = mlist;
	print_data->n_msgs = g_slist_length(mlist);
	print_data->msgs = g_new0(MsgPrintInfo, print_data->n_msgs);
	for (i = 0, cur = mlist; cur != NULL; i++, cur = cur->next) {
		print_data->msgs[i].msginfo = (MsgInfo *)cur->data;
	}
	print_data->pages = g_ptr_array_new();
	print_data->n_pages = 0;
	print_data->partinfo = partinfo;
	print_data->line_h = 0.0;
	print_data->all_headers = all_headers;

	op = gtk_print_operation_new();

	/* GTK_UNIT_POINTS introduces text corruption bug, so use pixel unit */
	gtk_print_operation_set_unit(op, GTK_UNIT_PIXEL);

	g_signal_connect(op, "begin-print", G_CALLBACK(begin_print),
			 print_data);
	g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), print_data);

	if (!settings)
		printing_load_settings();
	if (settings)
		gtk_print_operation_set_print_settings(op, settings);
	if (!page_setup)
		printing_load_page_setup();
	if (page_setup)
		gtk_print_operation_set_default_page_setup(op, page_setup);

	res = gtk_print_operation_run
		(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
		 GTK_WINDOW(main_window_get()->window), NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (settings)
			g_object_unref(settings);
		settings = g_object_ref(gtk_print_operation_get_print_settings(op));
		printing_save_settings();
	}

	g_object_unref(op);
	for (i = 0; i < print_data->pages->len; i++) {
		PageInfo *pinfo;
		pinfo = g_ptr_array_index(print_data->pages, i);
		g_free(pinfo);
	}
	g_ptr_array_free(print_data->pages, TRUE);
	for (i = 0; i < print_data->n_msgs; i++) {
		g_free(print_data->msgs[i].hdr_data);
		if (print_data->msgs[i].fp)
			fclose(print_data->msgs[i].fp);
		if (print_data->msgs[i].msg_text_file) {
			g_unlink(print_data->msgs[i].msg_text_file);
			g_free(print_data->msgs[i].msg_text_file);
		}
	}
	g_free(print_data);

	change_dir(prev_dir);
	g_free(prev_dir);

	debug_print("printing finished\n");

	return 0;
}

void printing_page_setup_gtk(void)
{
	GtkPageSetup *new_page_setup;

	if (!settings)
		printing_load_settings();
	if (!settings)
		settings = gtk_print_settings_new();
	if (!page_setup)
		printing_load_page_setup();

	new_page_setup = gtk_print_run_page_setup_dialog
		(GTK_WINDOW(main_window_get()->window), page_setup, settings);

	if (page_setup)
		g_object_unref(page_setup);
	page_setup = new_page_setup;

	printing_save_page_setup();
}

#endif /* GTK_CHECK_VERSION(2, 10, 0) */

static gint check_command_line(const gchar *cmdline)
{
	gchar *msg;

	msg = g_strconcat
		(_("The message will be printed with the following command:"),
		 "\n\n", cmdline ? cmdline : _("(Default print command)"),
		 NULL);
	if (alertpanel(_("Print"), msg, GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL)
	    != G_ALERTDEFAULT) {
		g_free(msg);
		return -2;
	}
	g_free(msg);

	if (cmdline && str_find_format_times(cmdline, 's') != 1) {
		alertpanel_error(_("Print command line is invalid:\n`%s'"),
				 cmdline);
		return -1;
	}

	return 0;
}

gint printing_print_messages_with_command(GSList *mlist, gboolean all_headers,
					  const gchar *cmdline)
{
	MsgInfo *msginfo;
	GSList *cur;

	g_return_val_if_fail(mlist != NULL, -1);

	if (check_command_line(cmdline) < 0)
		return -1;

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (msginfo)
			procmsg_print_message(msginfo, cmdline, all_headers);
	}

	return 0;
}

gint printing_print_messages(GSList *mlist, gboolean all_headers)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	if (!prefs_common.use_print_cmd)
		return printing_print_messages_gtk(mlist, NULL, all_headers);
	else
#endif /* GTK_CHECK_VERSION(2, 10, 0) */
		return printing_print_messages_with_command
			(mlist, all_headers, prefs_common.print_cmd);
}

gint printing_print_message(MsgInfo *msginfo, gboolean all_headers)
{
	GSList mlist;

	mlist.data = msginfo;
	mlist.next = NULL;
	return printing_print_messages(&mlist, all_headers);
}

gint printing_print_message_part(MsgInfo *msginfo, MimeInfo *partinfo)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	if (!prefs_common.use_print_cmd) {
		GSList mlist;

		mlist.data = msginfo;
		mlist.next = NULL;
		return printing_print_messages_gtk(&mlist, partinfo, FALSE);
	}
#endif /* GTK_CHECK_VERSION(2, 10, 0) */
	if (check_command_line(prefs_common.print_cmd) < 0)
		return -1;
	procmsg_print_message_part(msginfo, partinfo, prefs_common.print_cmd,
				   FALSE);
	return 0;
}
