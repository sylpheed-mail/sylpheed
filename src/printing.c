/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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

#if GTK_CHECK_VERSION(2, 10, 0)

#define SPACING	2.0

typedef struct
{
	gint page_nr_per_msg;
	MsgInfo *msginfo;
	gint lines;
} PageInfo;

typedef struct
{
	GSList *mlist;
	GPtrArray *pages;
	gint n_pages;
	gboolean all_headers;
	FILE *fp;
	gchar cont_buf[BUFFSIZE];
} PrintData;


static gint layout_set_headers(PangoLayout *layout, MsgInfo *msginfo,
			       PrintData *print_data)
{
	FILE *fp;
	GPtrArray *headers;
	GString *str;
	gint i;
	PangoFontDescription *desc;

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

		text = g_markup_printf_escaped("<b>%s:</b> %s\n",
					       hdr->name, body);
		g_string_append(str, text);
	}

	desc = gtkut_get_default_font_desc();
	pango_font_description_set_size(desc, 12.0 * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_markup(layout, str->str, -1);
	g_string_free(str, TRUE);
	procheader_header_array_destroy(headers);

	return 0;
}

static gint message_get_body_lines(MsgInfo *msginfo, PangoLayout *layout)
{
	FILE *fp;
	gchar buf[BUFFSIZE];
	gint lines = 0;

	if ((fp = procmime_get_first_text_content(msginfo, NULL)) == NULL) {
		g_warning("Can't get text part\n");
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		pango_layout_set_text(layout, buf, -1);
		lines += pango_layout_get_line_count(layout);
	}

	g_print("lines = %d\n", lines);
	return lines;
}

static gint message_count_page(MsgInfo *msginfo, GtkPrintContext *context,
			       PrintData *print_data)
{
	cairo_t *cr;
	gdouble width, height, line_h, hdr_h, body_h;
	PangoLayout *layout;
	PangoFontDescription *desc;
	gint layout_h;
	gint lines_per_page, lines_left, body_lines;
	gint n_pages = 1;
	PageInfo *pinfo;
	gint i;

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);
	height = gtk_print_context_get_height(context);
	g_print("w = %g, h = %g\n", width, height);

	layout = gtk_print_context_create_pango_layout(context);
	pango_layout_set_width(layout, width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_spacing(layout, SPACING * PANGO_SCALE);

	layout_set_headers(layout, msginfo, print_data);
	pango_layout_get_size(layout, NULL, &layout_h);
	hdr_h = (gdouble)layout_h / PANGO_SCALE;
	body_h = height - hdr_h;

	desc = pango_font_description_from_string(prefs_common_get()->textfont);
	pango_font_description_set_size(desc, 12.0 * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_text(layout, "Test", -1);
	pango_layout_get_size(layout, NULL, &layout_h);
	line_h = (gdouble)layout_h / PANGO_SCALE + SPACING;
	lines_per_page = height / line_h;
	lines_left = body_h / line_h;

	g_print("layout_h = %d, line_h = %g, lines = %d\n", layout_h, line_h, lines_per_page);
	g_print("hdr_h = %g, body_h = %g, lines_left = %d\n", hdr_h, body_h, lines_left);

	body_lines = message_get_body_lines(msginfo, layout);

	g_object_unref(layout);

	if (body_lines > lines_left) {
		n_pages += (body_lines - lines_left) / lines_per_page;
		if (((body_lines - lines_left) % lines_per_page) != 0)
			n_pages++;
	}
	g_print("n_pages = %d\n", n_pages);

	for (i = 0; i < n_pages; i++) {
		pinfo = g_new(PageInfo, 1);
		pinfo->page_nr_per_msg = i;
		pinfo->msginfo = msginfo;
		if (i == 0)
			pinfo->lines = lines_left;
		else
			pinfo->lines = lines_per_page;
		g_ptr_array_add(print_data->pages, pinfo);
	}

	return n_pages;
}

static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context,
			gpointer data)
{
	PrintData *print_data = data;
	GSList *cur;
	gint n_pages = 0;
	gint i;

	debug_print("begin_print\n");

	for (cur = print_data->mlist, i = 0; cur != NULL;
	     cur = cur->next, ++i) {
		n_pages += message_count_page((MsgInfo *)cur->data, context,
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
	MsgInfo *msginfo;
	cairo_t *cr;
	PangoLayout *layout;
	gdouble width, text_height;
	gdouble font_size = 12.0;
	gint layout_w, layout_h;
	PangoFontDescription *desc;
	gchar buf[BUFFSIZE];
	gint count = 0;

	pinfo = g_ptr_array_index(print_data->pages, page_nr);
	msginfo = pinfo->msginfo;

	debug_print("draw_page: %d (%d)\n", page_nr, pinfo->page_nr_per_msg);

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);

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
	pango_layout_set_spacing(layout, SPACING * PANGO_SCALE);

	if (pinfo->page_nr_per_msg == 0) {
		if (layout_set_headers(layout, msginfo, print_data) < 0) {
			g_object_unref(layout);
			return;
		}

		pango_cairo_show_layout(cr, layout);
		pango_layout_get_size(layout, &layout_w, &layout_h);
		cairo_rel_move_to(cr, 0, (double)layout_h / PANGO_SCALE);

		if (print_data->fp) {
			fclose(print_data->fp);
			print_data->fp = NULL;
		}

		if ((print_data->fp =
			procmime_get_first_text_content(msginfo, NULL))
		    == NULL) {
			g_warning("Can't get text part\n");
			g_object_unref(layout);
			return;
		}
	} else if (print_data->fp == NULL) {
		g_object_unref(layout);
		return;
	}

	pango_layout_set_attributes(layout, NULL);
	desc = pango_font_description_from_string(prefs_common_get()->textfont);
	pango_font_description_set_size(desc, font_size * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	while (fgets(buf, sizeof(buf), print_data->fp) != NULL) {
		strretchomp(buf);
		pango_layout_set_text(layout, buf, -1);
		pango_cairo_show_layout(cr, layout);
		pango_layout_get_size(layout, NULL, &layout_h);
		cairo_rel_move_to(cr, 0,
				  (double)layout_h / PANGO_SCALE + SPACING);
		count += pango_layout_get_line_count(layout);
		if (count >= pinfo->lines)
			break;
	}

	g_object_unref(layout);
}

gint printing_print_messages_gtk(GSList *mlist, gboolean all_headers)
{
	static GtkPrintSettings *settings = NULL;
	GtkPrintOperation *op;
	GtkPrintOperationResult res;
	PrintData *print_data;
	gint i;

	g_return_val_if_fail(mlist != NULL, -1);

	debug_print("printing start\n");

	print_data = g_new0(PrintData, 1);
	print_data->mlist = mlist;
	print_data->pages = g_ptr_array_new();
	print_data->n_pages = 0;
	print_data->all_headers = all_headers;
	print_data->fp = NULL;
	print_data->cont_buf[0] == '\0';

	op = gtk_print_operation_new();

	gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);

	g_signal_connect(op, "begin-print", G_CALLBACK(begin_print),
			 print_data);
	g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), print_data);

	if (settings)
		gtk_print_operation_set_print_settings(op, settings);

	res = gtk_print_operation_run
		(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
		 GTK_WINDOW(main_window_get()->window), NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		g_print("save settings\n");
		if (settings)
			g_object_unref(settings);
		settings = g_object_ref
			(gtk_print_operation_get_print_settings(op));
	}

	g_object_unref(op);
	for (i = 0; i < print_data->n_pages; i++) {
		PageInfo *pinfo;
		pinfo = g_ptr_array_index(print_data->pages, i);
		g_free(pinfo);
	}
	g_ptr_array_free(print_data->pages, TRUE);
	if (print_data->fp)
		fclose(print_data->fp);
	g_free(print_data);

	debug_print("printing finished\n");

	return 0;
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
		return printing_print_messages_gtk(mlist, all_headers);
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
	if (check_command_line(prefs_common.print_cmd) < 0)
		return -1;
	procmsg_print_message_part(msginfo, partinfo, prefs_common.print_cmd,
				   FALSE);
	return 0;
}
