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

#if GTK_CHECK_VERSION(2, 10, 0)
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkprintoperation.h>

#include <stdio.h>

#include "mainwindow.h"
#include "procmsg.h"
#include "procheader.h"
#include "prefs_common.h"

typedef struct
{
	GSList *mlist;
	gboolean all_headers;
} PrintData;

static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context,
			gpointer data)
{
	PrintData *print_data = data;

	debug_print("begin_print\n");
	/* count pages */
	gtk_print_operation_set_n_pages(operation, 1);
}

static void draw_page(GtkPrintOperation *operation, GtkPrintContext *context,
		      gint page_nr, gpointer data)
{
	cairo_t *cr;
	PangoLayout *layout;
	gdouble width, text_height;
	gdouble font_size = 12.0;
	gint layout_w, layout_h;
	PangoFontDescription *desc;
	PrintData *print_data = data;
	MsgInfo *msginfo;
	FILE *fp;
	GPtrArray *headers;
	GString *str;
	gint i;
	gchar buf[BUFFSIZE];

	debug_print("draw_page: %d\n", page_nr);

	msginfo = (MsgInfo *)print_data->mlist->data;
	g_return_if_fail(msginfo != NULL);

	if ((fp = procmsg_open_message(msginfo)) == NULL)
		return;

	if (print_data->all_headers)
		headers = procheader_get_header_array_asis(fp, NULL);
	else
		headers = procheader_get_header_array_for_display(fp, NULL);

	fclose(fp);

	cr = gtk_print_context_get_cairo_context(context);
	width = gtk_print_context_get_width(context);

#if 0
	cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
	cairo_rectangle(cr, 0, 0, width, font_size * 5);
	cairo_fill(cr);
#endif

	cairo_set_source_rgb(cr, 0, 0, 0);

	layout = gtk_print_context_create_pango_layout(context);

	//desc = pango_font_description_from_string(prefs_common.textfont);
	desc = gtkut_get_default_font_desc();
	pango_font_description_set_size(desc, font_size * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	cairo_move_to(cr, 0, 0);

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

	procheader_header_array_destroy(headers);

	pango_layout_set_markup(layout, str->str, -1);
	g_string_free(str, TRUE);
	pango_cairo_show_layout(cr, layout);
	pango_layout_get_size(layout, &layout_w, &layout_h);
	cairo_rel_move_to(cr, 0, (double)layout_h / PANGO_SCALE);

	if ((fp = procmime_get_first_text_content(msginfo, NULL)) == NULL) {
		g_warning("Can't get text part\n");
		g_object_unref(layout);
		return;
	}

	pango_layout_set_attributes(layout, NULL);
	desc = pango_font_description_from_string(prefs_common_get()->textfont);
	pango_font_description_set_size(desc, font_size * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		pango_layout_set_text(layout, buf, -1);
		pango_cairo_show_layout(cr, layout);
		cairo_rel_move_to(cr, 0, font_size);
	}

	fclose(fp);

	g_object_unref(layout);
}

gint printing_print_messages(GSList *mlist, gboolean all_headers)
{
	GtkPrintOperation *op;
	GtkPrintOperationResult res;
	PrintData *print_data;

	debug_print("printing start\n");

	print_data = g_new0(PrintData, 1);
	print_data->mlist = mlist;
	print_data->all_headers = all_headers;

	op = gtk_print_operation_new();

	g_signal_connect(op, "begin-print", G_CALLBACK(begin_print),
			 print_data);
	g_signal_connect(op, "draw-page", G_CALLBACK(draw_page), print_data);

	gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);

	res = gtk_print_operation_run
		(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
		 GTK_WINDOW(main_window_get()->window), NULL);

	g_object_unref(op);

	g_free(print_data);

	debug_print("printing finished\n");
}

#endif
