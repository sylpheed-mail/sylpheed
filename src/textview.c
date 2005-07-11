/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkstock.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "summaryview.h"
#include "imageview.h"
#include "procheader.h"
#include "prefs_common.h"
#include "codeconv.h"
#include "statusbar.h"
#include "utils.h"
#include "gtkutils.h"
#include "procmime.h"
#include "account.h"
#include "html.h"
#include "compose.h"
#include "displayheader.h"
#include "filesel.h"
#include "alertpanel.h"

typedef struct _RemoteURI	RemoteURI;

struct _RemoteURI
{
	gchar *uri;

	gchar *filename;

	guint start;
	guint end;
};

static GdkColor quote_colors[3] = {
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0},
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0},
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0}
};

static GdkColor uri_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

static GdkColor emphasis_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0xcfff
};

#if 0
static GdkColor error_color = {
	(gulong)0,
	(gushort)0xefff,
	(gushort)0,
	(gushort)0
};
#endif

#if USE_GPGME
static GdkColor good_sig_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0xbfff,
	(gushort)0
};

static GdkColor untrusted_sig_color = {
	(gulong)0,
	(gushort)0xefff,
	(gushort)0,
	(gushort)0
};

static GdkColor nocheck_sig_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0xcfff
};

static GdkColor bad_sig_color = {
	(gulong)0,
	(gushort)0xefff,
	(gushort)0,
	(gushort)0
};
#endif

#define STATUSBAR_PUSH(textview, str)					    \
{									    \
	gtk_statusbar_push(GTK_STATUSBAR(textview->messageview->statusbar), \
			   textview->messageview->statusbar_cid, str);	    \
}

#define STATUSBAR_POP(textview)						   \
{									   \
	gtk_statusbar_pop(GTK_STATUSBAR(textview->messageview->statusbar), \
			  textview->messageview->statusbar_cid);	   \
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;


static void textview_add_part		(TextView	*textview,
					 MimeInfo	*mimeinfo,
					 FILE		*fp);
static void textview_add_parts		(TextView	*textview,
					 MimeInfo	*mimeinfo,
					 FILE		*fp);
static void textview_write_body		(TextView	*textview,
					 MimeInfo	*mimeinfo,
					 FILE		*fp,
					 const gchar	*charset);
static void textview_show_html		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);

static void textview_write_line		(TextView	*textview,
					 const gchar	*str,
					 CodeConverter	*conv);
static void textview_write_link		(TextView	*textview,
					 const gchar	*str,
					 const gchar	*uri,
					 CodeConverter	*conv);

static GPtrArray *textview_scan_header	(TextView	*textview,
					 FILE		*fp,
					 const gchar	*encoding);
static void textview_show_header	(TextView	*textview,
					 GPtrArray	*headers);

static gboolean textview_key_pressed		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 TextView	*textview);
static gboolean textview_event_after		(GtkWidget	*widget,
						 GdkEvent	*event,
						 TextView	*textview);
static gboolean textview_motion_notify		(GtkWidget	*widget,
						 GdkEventMotion	*event,
						 TextView	*textview);
static gboolean textview_leave_notify		(GtkWidget	  *widget,
						 GdkEventCrossing *event,
						 TextView	  *textview);
static gboolean textview_visibility_notify	(GtkWidget	*widget,
						 GdkEventVisibility *event,
						 TextView	*textview);

static void textview_populate_popup		(GtkWidget	*widget,
						 GtkMenu	*menu,
						 TextView	*textview);
static void textview_popup_menu_activate_open_uri_cb
						(GtkMenuItem	*menuitem,
						 gpointer	 data);
static void textview_popup_menu_activate_add_address_cb
						(GtkMenuItem	*menuitem,
						 gpointer	 data);
static void textview_popup_menu_activate_copy_cb(GtkMenuItem	*menuitem,
						 gpointer	 data);
static void textview_popup_menu_activate_image_cb
						(GtkMenuItem	*menuitem,
						 gpointer	 data);

static void textview_smooth_scroll_do		(TextView	*textview,
						 gfloat		 old_value,
						 gfloat		 last_value,
						 gint		 step);
static void textview_smooth_scroll_one_line	(TextView	*textview,
						 gboolean	 up);
static gboolean textview_smooth_scroll_page	(TextView	*textview,
						 gboolean	 up);

static gboolean textview_get_link_tag_bounds	(TextView	*textview,
						 GtkTextIter	*iter,
						 GtkTextIter	*start,
						 GtkTextIter	*end);
static RemoteURI *textview_get_uri		(TextView	*textview,
						 GtkTextIter	*start,
						 GtkTextIter	*end);
static void textview_show_uri			(TextView	*textview,
						 GtkTextIter	*start,
						 GtkTextIter	*end);
static void textview_set_cursor			(TextView	*textview,
						 GtkTextView	*text,
						 gint		 x,
						 gint		 y);

static gboolean textview_uri_security_check	(TextView	*textview,
						 RemoteURI	*uri);
static void textview_uri_list_remove_all	(GSList		*uri_list);


TextView *textview_create(void)
{
	TextView *textview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	debug_print(_("Creating text view...\n"));
	textview = g_new0(TextView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request
		(scrolledwin, prefs_common.mainview_width, -1);

	text = gtk_text_view_new();
	gtk_widget_add_events(text, GDK_LEAVE_NOTIFY_MASK);
	gtk_widget_show(text);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_add_selection_clipboard(buffer, clipboard);

	gtk_widget_ref(scrolledwin);

	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	g_signal_connect(G_OBJECT(text), "key-press-event",
			 G_CALLBACK(textview_key_pressed), textview);
	g_signal_connect(G_OBJECT(text), "event-after",
			 G_CALLBACK(textview_event_after), textview);
	g_signal_connect(G_OBJECT(text), "motion-notify-event",
			 G_CALLBACK(textview_motion_notify), textview);
	g_signal_connect(G_OBJECT(text), "leave-notify-event",
			 G_CALLBACK(textview_leave_notify), textview);
	g_signal_connect(G_OBJECT(text), "visibility-notify-event",
			 G_CALLBACK(textview_visibility_notify), textview);
	g_signal_connect(G_OBJECT(text), "populate-popup",
			 G_CALLBACK(textview_populate_popup), textview);

	gtk_widget_show(scrolledwin);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	gtk_widget_show(vbox);

	textview->vbox             = vbox;
	textview->scrolledwin      = scrolledwin;
	textview->text             = text;
	textview->uri_list         = NULL;
	textview->body_pos         = 0;
	textview->show_all_headers = FALSE;

	return textview;
}

static void textview_create_tags(TextView *textview)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	static PangoFontDescription *font_desc;

	if (!font_desc)
		font_desc = gtkut_get_default_font_desc();

	buffer = gtk_text_view_get_buffer(text);

	gtk_text_buffer_create_tag(buffer, "header",
				   "pixels-above-lines", 0,
				   "pixels-above-lines-set", TRUE,
				   "pixels-below-lines", 0,
				   "pixels-below-lines-set", TRUE,
				   "font-desc", font_desc,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "header_title",
				   "weight", PANGO_WEIGHT_BOLD,
				   NULL);

	textview->quote0_tag =
		gtk_text_buffer_create_tag(buffer, "quote0",
					   "foreground-gdk", &quote_colors[0],
					   NULL);
	textview->quote1_tag =
		gtk_text_buffer_create_tag(buffer, "quote1",
					   "foreground-gdk", &quote_colors[1],
					   NULL);
	textview->quote2_tag =
		gtk_text_buffer_create_tag(buffer, "quote2",
					   "foreground-gdk", &quote_colors[2],
					   NULL);
	textview->link_tag =
		gtk_text_buffer_create_tag(buffer, "link",
					   "foreground-gdk", &uri_color,
					   NULL);
	textview->hover_link_tag =
		gtk_text_buffer_create_tag(buffer, "hover-link",
					   "foreground-gdk", &uri_color,
					   "underline", PANGO_UNDERLINE_SINGLE,
					   NULL);

	gtk_text_buffer_create_tag(buffer, "emphasis",
				   "foreground-gdk", &emphasis_color,
				   NULL);
#if USE_GPGME
	gtk_text_buffer_create_tag(buffer, "good-signature",
				   "foreground-gdk", &good_sig_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "untrusted-signature",
				   "foreground-gdk", &untrusted_sig_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "bad-signature",
				   "foreground-gdk", &bad_sig_color,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "nocheck-signature",
				   "foreground-gdk", &nocheck_sig_color,
				   NULL);
#endif /* USE_GPGME */
}

void textview_init(TextView *textview)
{
	if (!hand_cursor)
		hand_cursor = gdk_cursor_new(GDK_HAND2);
	if (!regular_cursor)
		regular_cursor = gdk_cursor_new(GDK_XTERM);

	textview_create_tags(textview);
	textview_reflect_prefs(textview);
	textview_set_all_headers(textview, FALSE);
	textview_set_font(textview, NULL);
}

static void textview_update_message_colors(void)
{
	GdkColor black = {0, 0, 0, 0};

	if (prefs_common.enable_color) {
		/* grab the quote colors, converting from an int to a
		   GdkColor */
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level1_col,
					       &quote_colors[0]);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level2_col,
					       &quote_colors[1]);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level3_col,
					       &quote_colors[2]);
		gtkut_convert_int_to_gdk_color(prefs_common.uri_col,
					       &uri_color);
	} else {
		quote_colors[0] = quote_colors[1] = quote_colors[2] = 
			uri_color = emphasis_color = black;
	}
}

static void textview_update_tags(TextView *textview)
{
	g_object_set(textview->quote0_tag, "foreground-gdk", &quote_colors[0],
		     NULL);
	g_object_set(textview->quote1_tag, "foreground-gdk", &quote_colors[1],
		     NULL);
	g_object_set(textview->quote2_tag, "foreground-gdk", &quote_colors[2],
		     NULL);
	g_object_set(textview->link_tag, "foreground-gdk", &uri_color, NULL);
	g_object_set(textview->hover_link_tag, "foreground-gdk", &uri_color,
		     NULL);
}

void textview_reflect_prefs(TextView *textview)
{
	textview_update_message_colors();
	textview_update_tags(textview);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview->text),
					 prefs_common.textview_cursor_visible);
}

void textview_show_message(TextView *textview, MimeInfo *mimeinfo,
			   const gchar *file)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	FILE *fp;
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;

	buffer = gtk_text_view_get_buffer(text);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return;
	}

	if (textview->messageview->forced_charset)
		charset = textview->messageview->forced_charset;
	else if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else if (mimeinfo->charset)
		charset = mimeinfo->charset;

	textview_set_font(textview, charset);
	textview_clear(textview);

	if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0) perror("fseek");
	headers = textview_scan_header(textview, fp, charset);
	if (headers) {
		textview_show_header(textview, headers);
		procheader_header_array_destroy(headers);

		gtk_text_buffer_get_end_iter(buffer, &iter);
		textview->body_pos = gtk_text_iter_get_offset(&iter);
	}

	textview_add_parts(textview, mimeinfo, fp);

	fclose(fp);

	textview_set_position(textview, 0);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_view_scroll_mark_onscreen(text, mark);
}

void textview_show_part(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	gchar buf[BUFFSIZE];
	const gchar *boundary = NULL;
	gint boundary_len = 0;
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;
	gboolean is_rfc822_part = FALSE;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);

	if (mimeinfo->mime_type == MIME_MULTIPART) {
		textview_clear(textview);
		textview_add_parts(textview, mimeinfo, fp);
		return;
	}

	if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	if (textview->messageview->forced_charset)
		charset = textview->messageview->forced_charset;
	else if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else if (mimeinfo->charset)
		charset = mimeinfo->charset;

	if (!boundary && mimeinfo->mime_type == MIME_TEXT) {
		if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
			perror("fseek");
		headers = textview_scan_header(textview, fp, charset);
	} else {
		if (mimeinfo->mime_type == MIME_TEXT && mimeinfo->parent) {
			glong fpos;
			MimeInfo *parent = mimeinfo->parent;

			while (parent->parent) {
				if (parent->main &&
				    parent->main->mime_type ==
					MIME_MESSAGE_RFC822)
					break;
				parent = parent->parent;
			}

			if ((fpos = ftell(fp)) < 0)
				perror("ftell");
			else if (fseek(fp, parent->fpos, SEEK_SET) < 0)
				perror("fseek");
			else {
				headers = textview_scan_header
					(textview, fp, charset);
				if (fseek(fp, fpos, SEEK_SET) < 0)
					perror("fseek");
			}
		}
		/* skip MIME part headers */
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
	}

	/* display attached RFC822 single text message */
	if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
		if (headers) procheader_header_array_destroy(headers);
		if (!mimeinfo->sub) {
			textview_clear(textview);
			return;
		}
		headers = textview_scan_header(textview, fp, charset);
		mimeinfo = mimeinfo->sub;
		is_rfc822_part = TRUE;
	}

	textview_set_font(textview, charset);

	textview_clear(textview);

	if (headers) {
		GtkTextView *text = GTK_TEXT_VIEW(textview->text);
		GtkTextBuffer *buffer;
		GtkTextIter iter;

		textview_show_header(textview, headers);
		procheader_header_array_destroy(headers);

		buffer = gtk_text_view_get_buffer(text);
		gtk_text_buffer_get_end_iter(buffer, &iter);
		textview->body_pos = gtk_text_iter_get_offset(&iter);
		if (!mimeinfo->main)
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	if (mimeinfo->mime_type == MIME_MULTIPART || is_rfc822_part)
		textview_add_parts(textview, mimeinfo, fp);
	else
		textview_write_body(textview, mimeinfo, fp, charset);
}

static void textview_add_part(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar buf[BUFFSIZE];
	const gchar *boundary = NULL;
	gint boundary_len = 0;
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (mimeinfo->mime_type == MIME_MULTIPART) return;

	if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0) {
		perror("fseek");
		return;
	}

	if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	if (textview->messageview->forced_charset)
		charset = textview->messageview->forced_charset;
	else if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else if (mimeinfo->charset)
		charset = mimeinfo->charset;

	if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
		headers = textview_scan_header(textview, fp, charset);
		if (headers) {
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);
			textview_show_header(textview, headers);
			procheader_header_array_destroy(headers);
		}
		return;
	}

#if USE_GPGME
	if (mimeinfo->sigstatus)
		g_snprintf(buf, sizeof(buf), "\n[%s (%s)]\n",
			   mimeinfo->content_type, mimeinfo->sigstatus);
	else
#endif
	if (mimeinfo->filename || mimeinfo->name)
		g_snprintf(buf, sizeof(buf), "\n[%s  %s (%d bytes)]\n",
			   mimeinfo->filename ? mimeinfo->filename :
			   mimeinfo->name,
			   mimeinfo->content_type, mimeinfo->size);
	else
		g_snprintf(buf, sizeof(buf), "\n[%s (%d bytes)]\n",
			   mimeinfo->content_type, mimeinfo->size);

#if USE_GPGME
	if (mimeinfo->sigstatus) {
		const gchar *color;
		if (!strcmp(mimeinfo->sigstatus, _("Good signature")))
			color = "good-signature";
		else if (!strcmp(mimeinfo->sigstatus, _("Valid signature (untrusted key)")))
			color = "untrusted-signature";
		else if (!strcmp(mimeinfo->sigstatus, _("BAD signature")))
			color = "bad-signature";
		else
			color = "nocheck-signature";
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, buf, -1,
							 color, NULL);
	} else
#endif
	if (mimeinfo->mime_type != MIME_TEXT &&
	    mimeinfo->mime_type != MIME_TEXT_HTML) {
		gtk_text_buffer_insert(buffer, &iter, buf, -1);
		if (mimeinfo->mime_type == MIME_IMAGE &&
		    prefs_common.inline_image) {
			GdkPixbuf *pixbuf;
			GError *error = NULL;
			gchar *filename;
			RemoteURI *uri;
			gchar *uri_str;

			filename = procmime_get_tmp_file_name(mimeinfo);
			if (procmime_get_part_fp(filename, fp, mimeinfo) < 0) {
				g_warning("Can't get the image file.");
				g_free(filename);
				return;
			}

			pixbuf = gdk_pixbuf_new_from_file(filename, &error);
			if (error != NULL) {
				g_warning("%s\n", error->message);
				g_error_free(error);
			}
			if (!pixbuf) {
				g_warning("Can't load the image.");
				g_free(filename);
				return;
			}

			if (prefs_common.resize_image) {
				GdkPixbuf *scaled;

				scaled = imageview_get_resized_pixbuf
					(pixbuf, textview->text, 8);
				g_object_unref(pixbuf);
				pixbuf = scaled;
			}

			uri_str = g_filename_to_uri(filename, NULL, NULL);
			if (uri_str) {
				uri = g_new(RemoteURI, 1);
				uri->uri = uri_str;
				uri->filename =
					procmime_get_part_file_name(mimeinfo);
				uri->start = gtk_text_iter_get_offset(&iter);
				uri->end = uri->start + 1;
				textview->uri_list =
					g_slist_append(textview->uri_list, uri);
			}
			gtk_text_buffer_insert_pixbuf(buffer, &iter, pixbuf);
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);

			g_object_unref(pixbuf);
			g_free(filename);
		}
	} else {
		if (!mimeinfo->main &&
		    mimeinfo->parent &&
		    mimeinfo->parent->children != mimeinfo)
			gtk_text_buffer_insert(buffer, &iter, buf, -1);
		else
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);
		textview_write_body(textview, mimeinfo, fp, charset);
	}
}

static void textview_add_parts(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	gint level;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);

	level = mimeinfo->level;

	for (;;) {
		textview_add_part(textview, mimeinfo, fp);
		if (mimeinfo->parent && mimeinfo->parent->content_type &&
		    !g_ascii_strcasecmp(mimeinfo->parent->content_type,
					"multipart/alternative"))
			mimeinfo = mimeinfo->parent->next;
		else
			mimeinfo = procmime_mimeinfo_next(mimeinfo);
		if (!mimeinfo || mimeinfo->level <= level)
			break;
	}
}

void textview_show_error(TextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_insert(buffer, &iter,
			       _("This message can't be displayed.\n"), -1);
}

static void textview_write_body(TextView *textview, MimeInfo *mimeinfo,
				FILE *fp, const gchar *charset)
{
	FILE *tmpfp;
	gchar buf[BUFFSIZE];
	CodeConverter *conv;

	conv = conv_code_converter_new(charset, NULL);

	tmpfp = procmime_decode_content(NULL, fp, mimeinfo);
	if (tmpfp) {
		if (mimeinfo->mime_type == MIME_TEXT_HTML &&
		    prefs_common.render_html)
			textview_show_html(textview, tmpfp, conv);
		else
			while (fgets(buf, sizeof(buf), tmpfp) != NULL)
				textview_write_line(textview, buf, conv);
		fclose(tmpfp);
	}

	conv_code_converter_destroy(conv);
}

static void textview_show_html(TextView *textview, FILE *fp,
			       CodeConverter *conv)
{
	HTMLParser *parser;
	const gchar *str;

	parser = html_parser_new(fp, conv);
	g_return_if_fail(parser != NULL);

	while ((str = html_parse(parser)) != NULL) {
		if (parser->href != NULL)
			textview_write_link(textview, str, parser->href, NULL);
		else
			textview_write_line(textview, str, NULL);
	}
	textview_write_line(textview, "\n", NULL);

	html_parser_destroy(parser);
}

/* get_uri_part() - retrieves a URI starting from scanpos.
		    Returns TRUE if succesful */
static gboolean get_uri_part(const gchar *start, const gchar *scanpos,
			     const gchar **bp, const gchar **ep)
{
	const gchar *ep_;

	g_return_val_if_fail(start != NULL, FALSE);
	g_return_val_if_fail(scanpos != NULL, FALSE);
	g_return_val_if_fail(bp != NULL, FALSE);
	g_return_val_if_fail(ep != NULL, FALSE);

	*bp = scanpos;

	/* find end point of URI */
	for (ep_ = scanpos; *ep_ != '\0'; ep_++) {
		if (!isgraph(*(const guchar *)ep_) ||
		    !isascii(*(const guchar *)ep_) ||
		    strchr("()<>{}[]\"", *ep_))
			break;
	}

	/* no punctuation at end of string */

	/* FIXME: this stripping of trailing punctuations may bite with other URIs.
	 * should pass some URI type to this function and decide on that whether
	 * to perform punctuation stripping */

#define IS_REAL_PUNCT(ch)	(ispunct(ch) && ((ch) != '/')) 

	for (; ep_ - 1 > scanpos + 1 &&
	       IS_REAL_PUNCT(*(const guchar *)(ep_ - 1));
	     ep_--)
		;

#undef IS_REAL_PUNCT

	*ep = ep_;

	return TRUE;		
}

static gchar *make_uri_string(const gchar *bp, const gchar *ep)
{
	return g_strndup(bp, ep - bp);
}

/* valid mail address characters */
#define IS_RFC822_CHAR(ch) \
	(isascii(ch) && \
	 (ch) > 32   && \
	 (ch) != 127 && \
	 !isspace(ch) && \
	 !strchr("(),;<>\"", (ch)))

/* alphabet and number within 7bit ASCII */
#define IS_ASCII_ALNUM(ch)	(isascii(ch) && isalnum(ch))

/* get_email_part() - retrieves an email address. Returns TRUE if succesful */
static gboolean get_email_part(const gchar *start, const gchar *scanpos,
			       const gchar **bp, const gchar **ep)
{
	/* more complex than the uri part because we need to scan back and forward starting from
	 * the scan position. */
	gboolean result = FALSE;
	const gchar *bp_;
	const gchar *ep_;

	g_return_val_if_fail(start != NULL, FALSE);
	g_return_val_if_fail(scanpos != NULL, FALSE);
	g_return_val_if_fail(bp != NULL, FALSE);
	g_return_val_if_fail(ep != NULL, FALSE);

	/* scan start of address */
	for (bp_ = scanpos - 1;
	     bp_ >= start && IS_RFC822_CHAR(*(const guchar *)bp_); bp_--)
		;

	/* TODO: should start with an alnum? */
	bp_++;
	for (; bp_ < scanpos && !IS_ASCII_ALNUM(*(const guchar *)bp_); bp_++)
		;

	if (bp_ != scanpos) {
		/* scan end of address */
		for (ep_ = scanpos + 1;
		     *ep_ && IS_RFC822_CHAR(*(const guchar *)ep_); ep_++)
			;

		/* TODO: really should terminate with an alnum? */
		for (; ep_ > scanpos && !IS_ASCII_ALNUM(*(const guchar *)ep_);
		     --ep_)
			;
		ep_++;

		if (ep_ > scanpos + 1) {
			*ep = ep_;
			*bp = bp_;
			result = TRUE;
		}
	}

	return result;
}

#undef IS_ASCII_ALNUM
#undef IS_RFC822_CHAR

static gchar *make_email_string(const gchar *bp, const gchar *ep)
{
	/* returns a mailto: URI; mailto: is also used to detect the
	 * uri type later on in the button_pressed signal handler */
	gchar *tmp;
	gchar *result;

	tmp = g_strndup(bp, ep - bp);
	result = g_strconcat("mailto:", tmp, NULL);
	g_free(tmp);

	return result;
}

#define ADD_TXT_POS(bp_, ep_, pti_) \
	if ((last->next = alloca(sizeof(struct txtpos))) != NULL) { \
		last = last->next; \
		last->bp = (bp_); last->ep = (ep_); last->pti = (pti_); \
		last->next = NULL; \
	} else { \
		g_warning("alloc error scanning URIs\n"); \
		gtk_text_buffer_insert_with_tags_by_name \
			(buffer, &iter, linebuf, -1, fg_tag, NULL); \
		return; \
	}

/* textview_make_clickable_parts() - colorizes clickable parts */
static void textview_make_clickable_parts(TextView *textview,
					  const gchar *fg_tag,
					  const gchar *uri_tag,
					  const gchar *linebuf)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	/* parse table - in order of priority */
	struct table {
		const gchar *needle; /* token */

		/* token search function */
		gchar    *(*search)	(const gchar *haystack,
					 const gchar *needle);
		/* part parsing function */
		gboolean  (*parse)	(const gchar *start,
					 const gchar *scanpos,
					 const gchar **bp_,
					 const gchar **ep_);
		/* part to URI function */
		gchar    *(*build_uri)	(const gchar *bp,
					 const gchar *ep);
	};

	static struct table parser[] = {
		{"http://",  strcasestr, get_uri_part,   make_uri_string},
		{"https://", strcasestr, get_uri_part,   make_uri_string},
		{"ftp://",   strcasestr, get_uri_part,   make_uri_string},
		{"www.",     strcasestr, get_uri_part,   make_uri_string},
		{"mailto:",  strcasestr, get_uri_part,   make_uri_string},
		{"@",        strcasestr, get_email_part, make_email_string}
	};
	const gint PARSE_ELEMS = sizeof parser / sizeof parser[0];

	/* flags for search optimization */
	gboolean do_search[] = {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE};

	gint  n;
	const gchar *walk, *bp, *ep;

	struct txtpos {
		const gchar	*bp, *ep;	/* text position */
		gint		 pti;		/* index in parse table */
		struct txtpos	*next;		/* next */
	} head = {NULL, NULL, 0,  NULL}, *last = &head;

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	/* parse for clickable parts, and build a list of begin and
	   end positions  */
	for (walk = linebuf, n = 0;;) {
		gint last_index = PARSE_ELEMS;
		const gchar *scanpos = NULL;

		/* FIXME: this looks phony. scanning for anything in the
		   parse table */
		for (n = 0; n < PARSE_ELEMS; n++) {
			const gchar *tmp;

			if (do_search[n]) {
				tmp = parser[n].search(walk, parser[n].needle);
				if (tmp) {
					if (scanpos == NULL || tmp < scanpos) {
						scanpos = tmp;
						last_index = n;
					}
				} else
					do_search[n] = FALSE;
			}
		}

		if (scanpos) {
			/* check if URI can be parsed */
			if (parser[last_index].parse(walk, scanpos, &bp, &ep)
			    && (ep - bp - 1) > strlen(parser[last_index].needle)) {
					ADD_TXT_POS(bp, ep, last_index);
					walk = ep;
			} else
				walk = scanpos +
					strlen(parser[last_index].needle);
		} else
			break;
	}

	/* colorize this line */
	if (head.next) {
		const gchar *normal_text = linebuf;

		/* insert URIs */
		for (last = head.next; last != NULL;
		     normal_text = last->ep, last = last->next) {
			RemoteURI *uri;

			uri = g_new(RemoteURI, 1);
			if (last->bp - normal_text > 0)
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter,
					 normal_text,
					 last->bp - normal_text,
					 fg_tag, NULL);
			uri->uri = parser[last->pti].build_uri(last->bp,
							       last->ep);
			uri->filename = NULL;
			uri->start = gtk_text_iter_get_offset(&iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, last->bp, last->ep - last->bp,
				 uri_tag, fg_tag, NULL);
			uri->end = gtk_text_iter_get_offset(&iter);
			textview->uri_list =
				g_slist_append(textview->uri_list, uri);
		}

		if (*normal_text)
			gtkut_text_buffer_insert_with_tag_by_name
				(buffer, &iter, normal_text, -1, fg_tag);
	} else {
		gtkut_text_buffer_insert_with_tag_by_name
			(buffer, &iter, linebuf, -1, fg_tag);
	}
}

#undef ADD_TXT_POS

static void textview_write_line(TextView *textview, const gchar *str,
				CodeConverter *conv)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *buf;
	gchar *fg_color = NULL;
	gint quotelevel = -1;
	gchar quote_tag_str[10];

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (conv) {
		buf = conv_convert(conv, str);
		if (!buf)
			buf = conv_utf8todisp(str, NULL);
	} else
		buf = g_strdup(str);

	strcrchomp(buf);
	//if (prefs_common.conv_mb_alnum) conv_mb_alnum(buf);

	/* change color of quotation
	   >, foo>, _> ... ok, <foo>, foo bar>, foo-> ... ng
	   Up to 3 levels of quotations are detected, and each
	   level is colored using a different color. */
	if (prefs_common.enable_color && strchr(buf, '>')) {
		quotelevel = get_quote_level(buf);

		/* set up the correct foreground color */
		if (quotelevel > 2) {
			/* recycle colors */
			if (prefs_common.recycle_quote_colors)
				quotelevel %= 3;
			else
				quotelevel = 2;
		}
	}

	if (quotelevel != -1) {
		g_snprintf(quote_tag_str, sizeof(quote_tag_str),
			   "quote%d", quotelevel);
		fg_color = quote_tag_str;
	}

	if (prefs_common.enable_color)
		textview_make_clickable_parts(textview, fg_color, "link", buf);
	else
		textview_make_clickable_parts(textview, fg_color, NULL, buf);

	g_free(buf);
}

static void textview_write_link(TextView *textview, const gchar *str,
				const gchar *uri, CodeConverter *conv)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *buf;
	gchar *bufp;
	RemoteURI *r_uri;

	if (!str || *str == '\0')
		return;
	if (!uri)
		return;

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (conv) {
		buf = conv_convert(conv, str);
		if (!buf)
			buf = conv_utf8todisp(str, NULL);
	} else
		buf = g_strdup(str);

	if (g_utf8_validate(buf, -1, NULL) == FALSE) {
		g_free(buf);
		return;
	}

	strcrchomp(buf);

	for (bufp = buf; *bufp != '\0'; bufp = g_utf8_next_char(bufp)) {
		gunichar ch;

		ch = g_utf8_get_char(bufp);
		if (!g_unichar_isspace(ch))
			break;
	}
	if (bufp > buf)
		gtk_text_buffer_insert(buffer, &iter, buf, bufp - buf);

	r_uri = g_new(RemoteURI, 1);
	r_uri->uri = g_strdup(uri);
	r_uri->filename = NULL;
	r_uri->start = gtk_text_iter_get_offset(&iter);
	gtk_text_buffer_insert_with_tags_by_name
		(buffer, &iter, bufp, -1, "link", NULL);
	r_uri->end = gtk_text_iter_get_offset(&iter);
	textview->uri_list = g_slist_append(textview->uri_list, r_uri);

	g_free(buf);
}

void textview_clear(TextView *textview)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_set_text(buffer, "", -1);

	STATUSBAR_POP(textview);
	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;

	textview->body_pos = 0;
}

void textview_destroy(TextView *textview)
{
	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;
	g_free(textview);
}

void textview_set_all_headers(TextView *textview, gboolean all_headers)
{
	textview->show_all_headers = all_headers;
}

void textview_set_font(TextView *textview, const gchar *codeset)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);

	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;
		font_desc = pango_font_description_from_string
			(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(textview->text, font_desc);
			pango_font_description_free(font_desc);
		}
	}

	gtk_text_view_set_pixels_above_lines(text, prefs_common.line_space / 2);
	gtk_text_view_set_pixels_below_lines(text, prefs_common.line_space / 2);
}

void textview_set_position(TextView *textview, gint pos)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, pos);
	gtk_text_buffer_place_cursor(buffer, &iter);
}

static GPtrArray *textview_scan_header(TextView *textview, FILE *fp,
				       const gchar *encoding)
{
	gchar buf[BUFFSIZE];
	GPtrArray *headers, *sorted_headers;
	GSList *disphdr_list;
	Header *header;
	gint i;

	g_return_val_if_fail(fp != NULL, NULL);

	if (textview->show_all_headers)
		return procheader_get_header_array_asis(fp, encoding);

	if (!prefs_common.display_header) {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
		return NULL;
	}

	headers = procheader_get_header_array_asis(fp, encoding);

	sorted_headers = g_ptr_array_new();

	for (disphdr_list = prefs_common.disphdr_list; disphdr_list != NULL;
	     disphdr_list = disphdr_list->next) {
		DisplayHeaderProp *dp =
			(DisplayHeaderProp *)disphdr_list->data;

		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);

			if (!g_ascii_strcasecmp(header->name, dp->name)) {
				if (dp->hidden)
					procheader_header_free(header);
				else
					g_ptr_array_add(sorted_headers, header);

				g_ptr_array_remove_index(headers, i);
				i--;
			}
		}
	}

	if (prefs_common.show_other_header) {
		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);
			g_ptr_array_add(sorted_headers, header);
		}
		g_ptr_array_free(headers, TRUE);
	} else
		procheader_header_array_destroy(headers);


	return sorted_headers;
}

static void textview_show_header(TextView *textview, GPtrArray *headers)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	Header *header;
	gint i;

	g_return_if_fail(headers != NULL);

	buffer = gtk_text_view_get_buffer(text);

	for (i = 0; i < headers->len; i++) {
		header = g_ptr_array_index(headers, i);
		g_return_if_fail(header->name != NULL);

		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, header->name, -1,
			 "header_title", "header", NULL);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, ":", 1,
			 "header_title", "header", NULL);

		if (!g_ascii_strcasecmp(header->name, "Subject") ||
		    !g_ascii_strcasecmp(header->name, "From")    ||
		    !g_ascii_strcasecmp(header->name, "To")      ||
		    !g_ascii_strcasecmp(header->name, "Cc"))
			unfold_line(header->body);

		if (prefs_common.enable_color &&
		    (!strncmp(header->name, "X-Mailer", 8) ||
		     !strncmp(header->name, "X-Newsreader", 12)) &&
		    strstr(header->body, "Sylpheed") != NULL) {
			gtk_text_buffer_get_end_iter(buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, header->body, -1,
				 "header", "emphasis", NULL);
		} else if (prefs_common.enable_color) {
			textview_make_clickable_parts
				(textview, "header", "link", header->body);
		} else {
			textview_make_clickable_parts
				(textview, "header", NULL, header->body);
		}
		gtk_text_buffer_get_end_iter(buffer, &iter); //
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, "\n", 1, "header", NULL);
	}
}

gboolean textview_search_string(TextView *textview, const gchar *str,
				gboolean case_sens)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter, match_pos;
	GtkTextMark *mark;
	gint len;

	g_return_val_if_fail(str != NULL, FALSE);

	buffer = gtk_text_view_get_buffer(text);

	len = g_utf8_strlen(str, -1);
	g_return_val_if_fail(len >= 0, FALSE);

	gtk_text_buffer_get_selection_bounds(buffer, NULL, &iter);

	if (gtkut_text_buffer_find(buffer, &iter, str, case_sens,
				   &match_pos)) {
		GtkTextIter end = match_pos;

		gtk_text_iter_forward_chars(&end, len);
		/* place "insert" at the last character */
		gtk_text_buffer_select_range(buffer, &end, &match_pos);
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_view_scroll_to_mark(text, mark, 0.0, FALSE, 0.0, 0.0);
		return TRUE;
	}

	return FALSE;
}

gboolean textview_search_string_backward(TextView *textview, const gchar *str,
					 gboolean case_sens)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter, match_pos;
	GtkTextMark *mark;
	gint len;

	g_return_val_if_fail(str != NULL, FALSE);

	buffer = gtk_text_view_get_buffer(text);

	len = g_utf8_strlen(str, -1);
	g_return_val_if_fail(len >= 0, FALSE);

	gtk_text_buffer_get_selection_bounds(buffer, &iter, NULL);

	if (gtkut_text_buffer_find_backward(buffer, &iter, str, case_sens,
					    &match_pos)) {
		GtkTextIter end = match_pos;

		gtk_text_iter_forward_chars(&end, len);
		gtk_text_buffer_select_range(buffer, &match_pos, &end);
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_view_scroll_to_mark(text, mark, 0.0, FALSE, 0.0, 0.0);
		return TRUE;
	}

	return FALSE;
}

void textview_scroll_one_line(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;

	if (prefs_common.enable_smooth_scroll) {
		textview_smooth_scroll_one_line(textview, up);
		return;
	}

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			vadj->value += vadj->step_increment;
			vadj->value = MIN(vadj->value, upper);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		}
	} else {
		if (vadj->value > 0.0) {
			vadj->value -= vadj->step_increment;
			vadj->value = MAX(vadj->value, 0.0);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		}
	}
}

gboolean textview_scroll_page(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;
	gfloat page_incr;

	if (prefs_common.enable_smooth_scroll)
		return textview_smooth_scroll_page(textview, up);

	if (prefs_common.scroll_halfpage)
		page_incr = vadj->page_increment / 2;
	else
		page_incr = vadj->page_increment;

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			vadj->value += page_incr;
			vadj->value = MIN(vadj->value, upper);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		} else
			return FALSE;
	} else {
		if (vadj->value > 0.0) {
			vadj->value -= page_incr;
			vadj->value = MAX(vadj->value, 0.0);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		} else
			return FALSE;
	}

	return TRUE;
}

static void textview_smooth_scroll_do(TextView *textview,
				      gfloat old_value, gfloat last_value,
				      gint step)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gint change_value;
	gboolean up;
	gint i;

	if (old_value < last_value) {
		change_value = last_value - old_value;
		up = FALSE;
	} else {
		change_value = old_value - last_value;
		up = TRUE;
	}

	/* gdk_key_repeat_disable(); */

	for (i = step; i <= change_value; i += step) {
		vadj->value = old_value + (up ? -i : i);
		g_signal_emit_by_name(G_OBJECT(vadj), "value_changed", 0);
	}

	vadj->value = last_value;
	g_signal_emit_by_name(G_OBJECT(vadj), "value_changed", 0);

	/* gdk_key_repeat_restore(); */

	gtk_widget_queue_draw(GTK_WIDGET(text));
}

static void textview_smooth_scroll_one_line(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;
	gfloat old_value;
	gfloat last_value;

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			old_value = vadj->value;
			last_value = vadj->value + vadj->step_increment;
			last_value = MIN(last_value, upper);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	} else {
		if (vadj->value > 0.0) {
			old_value = vadj->value;
			last_value = vadj->value - vadj->step_increment;
			last_value = MAX(last_value, 0.0);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	}
}

static gboolean textview_smooth_scroll_page(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;
	gfloat page_incr;
	gfloat old_value;
	gfloat last_value;

	if (prefs_common.scroll_halfpage)
		page_incr = vadj->page_increment / 2;
	else
		page_incr = vadj->page_increment;

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			old_value = vadj->value;
			last_value = vadj->value + page_incr;
			last_value = MIN(last_value, upper);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	} else {
		if (vadj->value > 0.0) {
			old_value = vadj->value;
			last_value = vadj->value - page_incr;
			last_value = MAX(last_value, 0.0);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	}

	return TRUE;
}

#define KEY_PRESS_EVENT_STOP() \
	g_signal_stop_emission_by_name(G_OBJECT(widget), "key-press-event");

static gboolean textview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     TextView *textview)
{
	SummaryView *summaryview = NULL;
	MessageView *messageview = textview->messageview;

	if (!event) return FALSE;
	if (messageview->mainwin)
		summaryview = messageview->mainwin->summaryview;

	switch (event->keyval) {
	case GDK_Tab:
	case GDK_Home:
	case GDK_Left:
	case GDK_Up:
	case GDK_Right:
	case GDK_Down:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_End:
	case GDK_Control_L:
	case GDK_Control_R:
		break;
	case GDK_space:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		else
			textview_scroll_page
				(textview,
				 (event->state &
				  (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);
		break;
	case GDK_BackSpace:
		textview_scroll_page(textview, TRUE);
		break;
	case GDK_Return:
		textview_scroll_one_line
			(textview, (event->state &
				    (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);
		break;
	case GDK_Delete:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	case GDK_Escape:
		if (summaryview && textview == messageview->textview)
			gtk_widget_grab_focus(summaryview->treeview);
		else if (messageview->type == MVIEW_MIME &&
			 textview == messageview->mimeview->textview)
			gtk_widget_grab_focus(messageview->mimeview->treeview);
		break;
	case GDK_n:
	case GDK_N:
	case GDK_p:
	case GDK_P:
	case GDK_y:
	case GDK_t:
	case GDK_l:
		if (messageview->type == MVIEW_MIME &&
		    textview == messageview->mimeview->textview) {
			KEY_PRESS_EVENT_STOP();
			mimeview_pass_key_press_event(messageview->mimeview,
						      event);
			break;
		}
		/* fall through */
	default:
		if (summaryview &&
		    event->window != messageview->mainwin->window->window) {
			GdkEventKey tmpev = *event;

			tmpev.window = messageview->mainwin->window->window;
			KEY_PRESS_EVENT_STOP();
			gtk_widget_event(messageview->mainwin->window,
					 (GdkEvent *)&tmpev);
		}
		break;
	}

	return FALSE;
}

static gboolean textview_get_link_tag_bounds(TextView *textview,
					     GtkTextIter *iter,
					     GtkTextIter *start,
					     GtkTextIter *end)
{
	GSList *tags, *cur;
	gboolean on_link = FALSE;

	tags = gtk_text_iter_get_tags(iter);
	*start = *end = *iter;

	for (cur = tags; cur != NULL; cur = cur->next) {
		GtkTextTag *tag = cur->data;
		gchar *tag_name;

		g_object_get(G_OBJECT(tag), "name", &tag_name, NULL);
		if (tag_name && !strcmp(tag_name, "link")) {
			if (!gtk_text_iter_begins_tag(start, tag))
				gtk_text_iter_backward_to_tag_toggle
					(start, tag);
			if (!gtk_text_iter_ends_tag(end, tag))
				gtk_text_iter_forward_to_tag_toggle(end, tag);
			on_link = TRUE;
			g_free(tag_name);
			break;
		}
		if (tag_name)
			g_free(tag_name);
	}

	if (tags)
		g_slist_free(tags);

	return on_link;
}

static RemoteURI *textview_get_uri(TextView *textview, GtkTextIter *start,
				   GtkTextIter *end)
{
	gint start_pos, end_pos;
	GSList *cur;
	RemoteURI *uri = NULL;

	start_pos = gtk_text_iter_get_offset(start);
	end_pos = gtk_text_iter_get_offset(end);

	for (cur = textview->uri_list; cur != NULL; cur = cur->next) {
		RemoteURI *uri_ = (RemoteURI *)cur->data;

		if (start_pos == uri_->start && end_pos == uri_->end) {
			uri = uri_;
			break;
		}
	}

	return uri;
}

static gboolean textview_event_after(GtkWidget *widget, GdkEvent *event,
				     TextView *textview)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	GtkTextBuffer *buffer;
	GtkTextIter iter, start, end;
	gint x, y;
	gboolean on_link;
	RemoteURI *uri;

	if (event->type != GDK_BUTTON_RELEASE)
		return FALSE;
	if (bevent->button != 1 && bevent->button != 2)
		return FALSE;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));

	/* don't follow a link if the user has selected something */
	gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
	if (!gtk_text_iter_equal(&start, &end))
		return FALSE;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
					      GTK_TEXT_WINDOW_WIDGET,
					      bevent->x, bevent->y, &x, &y);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter, x, y);
	on_link = textview_get_link_tag_bounds(textview, &iter, &start, &end);
	if (!on_link)
		return FALSE;

	uri = textview_get_uri(textview, &start, &end);
	if (!uri)
		return FALSE;

	if (!g_ascii_strncasecmp(uri->uri, "mailto:", 7)) {
		PrefsAccount *ac = NULL;
		MsgInfo *msginfo = textview->messageview->msginfo;

		if (msginfo && msginfo->folder)
			ac = account_find_from_item(msginfo->folder);
		if (ac && ac->protocol == A_NNTP)
			ac = NULL;
		compose_new(ac, msginfo->folder, uri->uri + 7, NULL);
	} else if (textview_uri_security_check(textview, uri) == TRUE)
		open_uri(uri->uri, prefs_common.uri_cmd);

	return FALSE;
}

static void textview_show_uri(TextView *textview, GtkTextIter *start,
			      GtkTextIter *end)
{
	RemoteURI *uri;

	STATUSBAR_POP(textview);
	uri = textview_get_uri(textview, start, end);
	if (uri) {
		gchar *trimmed_uri;

		trimmed_uri = trim_string(uri->uri, 60);
		STATUSBAR_PUSH(textview, trimmed_uri);
		g_free(trimmed_uri);
	}
}

static void textview_set_cursor(TextView *textview, GtkTextView *text,
				gint x, gint y)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter start, end;
	GtkTextMark *start_mark, *end_mark;
	gboolean on_link = FALSE;

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_view_get_iter_at_location(text, &iter, x, y);
	on_link = textview_get_link_tag_bounds(textview, &iter, &start, &end);

	start_mark = gtk_text_buffer_get_mark(buffer, "hover-link-start");
	end_mark = gtk_text_buffer_get_mark(buffer, "hover-link-end");
	if (start_mark) {
		GtkTextIter prev_start, prev_end;

		gtk_text_buffer_get_iter_at_mark(buffer, &prev_start,
						 start_mark);
		gtk_text_buffer_get_iter_at_mark(buffer, &prev_end, end_mark);
		if (on_link) {
			if (gtk_text_iter_equal(&prev_start, &start))
				return;
		}

		gtk_text_buffer_get_iter_at_mark(buffer, &prev_start,
						 start_mark);
		gtk_text_buffer_get_iter_at_mark(buffer, &prev_end, end_mark);
		gtk_text_buffer_remove_tag_by_name(buffer, "hover-link",
						   &prev_start, &prev_end);
		gtk_text_buffer_delete_mark(buffer, start_mark);
		gtk_text_buffer_delete_mark(buffer, end_mark);
	} else {
		if (!on_link)
			return;
	}

	if (on_link) {
		gtk_text_buffer_create_mark
			(buffer, "hover-link-start", &start, FALSE);
		gtk_text_buffer_create_mark
			(buffer, "hover-link-end", &end, FALSE);
		gtk_text_buffer_apply_tag_by_name
			(buffer, "hover-link", &start, &end);
		gdk_window_set_cursor
			(gtk_text_view_get_window(text, GTK_TEXT_WINDOW_TEXT),
			 hand_cursor);
		textview_show_uri(textview, &start, &end);
	} else {
		gdk_window_set_cursor
			(gtk_text_view_get_window(text, GTK_TEXT_WINDOW_TEXT),
			 regular_cursor);
		STATUSBAR_POP(textview);
	}
}

static gboolean textview_motion_notify(GtkWidget *widget,
				       GdkEventMotion *event,
				       TextView *textview)
{
	gint x, y;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
					      GTK_TEXT_WINDOW_WIDGET,
					      event->x, event->y, &x, &y);
	textview_set_cursor(textview, GTK_TEXT_VIEW(widget), x, y);
	gdk_window_get_pointer(widget->window, NULL, NULL, NULL);

	return FALSE;
}

static gboolean textview_leave_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      TextView *textview)
{
	textview_set_cursor(textview, GTK_TEXT_VIEW(widget), 0, 0);

	return FALSE;
}

static gboolean textview_visibility_notify(GtkWidget *widget,
					   GdkEventVisibility *event,
					   TextView *textview)
{
	gint wx, wy, bx, by;
	GdkWindow *window;

	window = gtk_text_view_get_window(GTK_TEXT_VIEW(widget),
					  GTK_TEXT_WINDOW_TEXT);

	/* check if the event occurred for the text window part */
	if (window != event->window)
		return FALSE;

	gdk_window_get_pointer(widget->window, &wx, &wy, NULL);
	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
					      GTK_TEXT_WINDOW_WIDGET,
					      wx, wy, &bx, &by);
	textview_set_cursor(textview, GTK_TEXT_VIEW(widget), bx, by);

	return FALSE;
}

#define ADD_MENU(label, func)						\
{									\
	menuitem = gtk_menu_item_new_with_mnemonic(label);		\
	g_signal_connect(menuitem, "activate", G_CALLBACK(func), uri);	\
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);		\
	g_object_set_data(G_OBJECT(menuitem), "textview", textview);	\
	gtk_widget_show(menuitem);					\
}

static void textview_populate_popup(GtkWidget *widget, GtkMenu *menu,
				    TextView *textview)
{
	gint px, py, x, y;
	GtkWidget *separator, *menuitem;
	GtkTextBuffer *buffer;
	GtkTextIter iter, start, end;
	gboolean on_link;
	RemoteURI *uri;
	GdkPixbuf *pixbuf;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));

	gdk_window_get_pointer(widget->window, &px, &py, NULL);
	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
					      GTK_TEXT_WINDOW_WIDGET,
					      px, py, &x, &y);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter, x, y);
	if ((pixbuf = gtk_text_iter_get_pixbuf(&iter)) != NULL) {
		start = end = iter;
		gtk_text_iter_forward_char(&end);
		uri = textview_get_uri(textview, &start, &end);

		separator = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
		gtk_widget_show(separator);

		ADD_MENU(_("Sa_ve this image as..."),
			 textview_popup_menu_activate_image_cb);
	}
	on_link = textview_get_link_tag_bounds(textview, &iter, &start, &end);
	if (!on_link)
		return;

	uri = textview_get_uri(textview, &start, &end);
	if (!uri)
		return;

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
	gtk_widget_show(separator);

	if (!g_ascii_strncasecmp(uri->uri, "mailto:", 7)) {
		ADD_MENU(_("Compose _new message"),
			 textview_popup_menu_activate_open_uri_cb);
		ADD_MENU(_("Add to address _book..."),
			 textview_popup_menu_activate_add_address_cb);
		ADD_MENU(_("Copy this add_ress"),
			 textview_popup_menu_activate_copy_cb);
	} else {
		ADD_MENU(_("_Open with Web browser"),
			 textview_popup_menu_activate_open_uri_cb);
		ADD_MENU(_("Copy this _link"),
			 textview_popup_menu_activate_copy_cb);
	}
}

static void textview_popup_menu_activate_open_uri_cb(GtkMenuItem *menuitem,
						     gpointer data)
{
	RemoteURI *uri = (RemoteURI *)data;
	TextView *textview;

	g_return_if_fail(uri != NULL);

	if (!uri->uri)
		return;

	textview = g_object_get_data(G_OBJECT(menuitem), "textview");
	g_return_if_fail(textview != NULL);

	if (!g_ascii_strncasecmp(uri->uri, "mailto:", 7)) {
		PrefsAccount *ac = NULL;
		MsgInfo *msginfo = textview->messageview->msginfo;

		if (msginfo && msginfo->folder)
			ac = account_find_from_item(msginfo->folder);
		if (ac && ac->protocol == A_NNTP)
			ac = NULL;
		compose_new(ac, msginfo->folder, uri->uri + 7, NULL);
	} else if (textview_uri_security_check(textview, uri) == TRUE)
		open_uri(uri->uri, prefs_common.uri_cmd);
}

static void textview_popup_menu_activate_add_address_cb(GtkMenuItem *menuitem,
							gpointer data)
{
	RemoteURI *uri = (RemoteURI *)data;
	const gchar *addr;

	g_return_if_fail(uri != NULL);

	if (!uri->uri)
		return;

	if (!g_ascii_strncasecmp(uri->uri, "mailto:", 7))
		addr = uri->uri + 7;
	else
		addr = uri->uri;

	addressbook_add_contact(addr, addr, NULL);
}

static void textview_popup_menu_activate_copy_cb(GtkMenuItem *menuitem,
						 gpointer data)
{
	RemoteURI *uri = (RemoteURI *)data;
	const gchar *uri_string;
	GtkClipboard *clipboard;

	g_return_if_fail(uri != NULL);

	if (!uri->uri)
		return;

	if (!g_ascii_strncasecmp(uri->uri, "mailto:", 7))
		uri_string = uri->uri + 7;
	else
		uri_string = uri->uri;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, uri_string, -1);
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, uri_string, -1);
}

static void textview_popup_menu_activate_image_cb(GtkMenuItem *menuitem,
						  gpointer data)
{
	RemoteURI *uri = (RemoteURI *)data;
	gchar *src;
	gchar *dest;
	gchar *filename;

	g_return_if_fail(uri != NULL);

	if (!uri->uri)
		return;

	src = g_filename_from_uri(uri->uri, NULL, NULL);
	g_return_if_fail(src != NULL);

	filename = conv_filename_to_utf8(uri->filename);
	dest = filesel_save_as(filename);
	if (dest) {
		copy_file(src, dest, FALSE);
		g_free(dest);
	}
	g_free(filename);
	g_free(src);
}

static gboolean textview_uri_security_check(TextView *textview, RemoteURI *uri)
{
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
	gchar *visible_str;
	gboolean retval = TRUE;

	if (is_uri_string(uri->uri) == FALSE)
		return TRUE;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, uri->start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, uri->end);
	visible_str = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
					       FALSE);
	if (visible_str == NULL)
		return TRUE;

	if (strcmp(visible_str, uri->uri) != 0 && is_uri_string(visible_str)) {
		gchar *uri_path;
		gchar *visible_uri_path;

		uri_path = get_uri_path(uri->uri);
		visible_uri_path = get_uri_path(visible_str);
		if (strcmp(uri_path, visible_uri_path) != 0)
			retval = FALSE;
	}

	if (retval == FALSE) {
		gchar *msg;
		AlertValue aval;

		msg = g_strdup_printf(_("The real URL (%s) is different from\n"
					"the apparent URL (%s).\n"
					"Open it anyway?"),
				      uri->uri, visible_str);
		aval = alertpanel(_("Warning"), msg,
				  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		g_free(msg);
		if (aval == G_ALERTDEFAULT)
			retval = TRUE;
	}

	g_free(visible_str);

	return retval;
}

static void textview_uri_list_remove_all(GSList *uri_list)
{
	GSList *cur;

	for (cur = uri_list; cur != NULL; cur = cur->next) {
		RemoteURI *uri = (RemoteURI *)cur->data;

		if (uri) {
			g_free(uri->uri);
			g_free(uri->filename);
			g_free(uri);
		}
	}

	g_slist_free(uri_list);
}
